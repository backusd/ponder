#include <ponder/core/Result.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace
{
namespace core = pond::core;
namespace io = pond::io;
namespace platform = pond::platform;

struct Options final
{
    std::optional<platform::PlatformTimestamp::Duration> autoCloseAfter;
    std::optional<std::string> externalUri;
    std::optional<std::filesystem::path> dialogLocation;
    std::string clipboardText{"Ponder UTF-8 sample: H2O -> \xCE\x94G"};
    bool showHelp{};
};

enum class DialogKind : std::uint8_t
{
    OpenFile,
    SaveFile,
    OpenFolder
};

struct PendingDialog final
{
    platform::DialogRequestId id;
    DialogKind kind{DialogKind::OpenFile};
    platform::PlatformTimestamp requestedAt{};
    std::string label;
    bool parented{};
};

struct WindowSlot final
{
    platform::Window window;
    bool titleUpdateFailureReported{};
};

struct AppState final
{
    platform::PlatformRuntime& runtime;
    const Options& options;
    std::vector<WindowSlot>& windows;
    platform::PlatformTimestamp startTimestamp;
    std::vector<PendingDialog> pendingDialogs;
    std::uint64_t eventCount{};
    bool shutdownRequested{};
    bool clipboardCaptureAttempted{};
    bool clipboardModified{};
    std::optional<std::string> originalClipboardText;
    std::string lastAction{"ready"};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0},
                       std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::cout
        << "Usage: " << executableName << " [options]\n\n"
        << "Options:\n"
        << "  --auto-close-ms <milliseconds>  Exit after a short idle run.\n"
        << "  --clipboard-text <text>         Text copied by the C command.\n"
        << "  --dialog-location <path>        Default dialog location.\n"
        << "  --uri <uri>                     URI opened only after pressing U.\n"
        << "  --help                          Print this help text.\n\n"
        << "Controls:\n"
        << "  F1            Print this help text.\n"
        << "  C / E / R     Copy sample text, copy empty text, or read clipboard.\n"
        << "  B             Restore the clipboard captured before the first write.\n"
        << "  U             Open the URI supplied with --uri.\n"
        << "  O / M         Open parented single-file or unparented multi-file dialog.\n"
        << "  S / F         Open parented save-file or unparented folder dialog.\n"
        << "  A             Launch a concurrent three-dialog batch.\n"
        << "  Q / Escape    Request shutdown; pending dialogs are still consumed.\n";
}

[[nodiscard]] core::Result<platform::PlatformTimestamp::Duration> ParseMilliseconds(
    std::string_view text)
{
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end)
    {
        return core::Result<platform::PlatformTimestamp::Duration>::FromError(
            MakeOptionError("Expected a non-negative integer millisecond value."));
    }

    using Milliseconds = std::chrono::milliseconds;
    constexpr auto kMaxMilliseconds = static_cast<std::uint64_t>(
        std::numeric_limits<Milliseconds::rep>::max());
    if (value > kMaxMilliseconds)
    {
        return core::Result<platform::PlatformTimestamp::Duration>::FromError(
            MakeOptionError("Auto-close duration is too large."));
    }

    return platform::PlatformTimestamp::Duration{
        Milliseconds{static_cast<Milliseconds::rep>(value)}};
}

[[nodiscard]] core::Result<Options> ParseOptions(int argc, char** argv)
{
    Options options{};
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
        }
        else if (argument == "--auto-close-ms")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--auto-close-ms requires a value."));
            }

            ++index;
            auto duration = ParseMilliseconds(argv[index]);
            if (!duration)
            {
                return core::Result<Options>::FromError(std::move(duration).GetError());
            }
            options.autoCloseAfter = std::move(duration).GetValue();
        }
        else if (argument == "--clipboard-text")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--clipboard-text requires a value."));
            }
            options.clipboardText = argv[++index];
        }
        else if (argument == "--dialog-location")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--dialog-location requires a value."));
            }
            options.dialogLocation = io::PathFromUtf8(argv[++index]);
        }
        else if (argument == "--uri")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--uri requires a value."));
            }

            options.externalUri = argv[++index];
            if (options.externalUri->empty())
            {
                return core::Result<Options>::FromError(MakeOptionError("--uri cannot be empty."));
            }
        }
        else
        {
            return core::Result<Options>::FromError(
                MakeOptionError("Unknown option: " + std::string{argument}));
        }
    }

    return options;
}

template <typename Id>
[[nodiscard]] std::string FormatId(Id id)
{
    if (!id.IsValid())
    {
        return "invalid";
    }
    return std::to_string(id.GetValue());
}

[[nodiscard]] std::string FormatDuration(platform::PlatformTimestamp::Duration duration)
{
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return std::to_string(milliseconds.count()) + " ms";
}

[[nodiscard]] std::string FormatTimestamp(platform::PlatformTimestamp timestamp)
{
    return std::to_string(timestamp.GetTimeSinceEpoch().count()) + " ns";
}

[[nodiscard]] std::string QuoteText(std::string_view text)
{
    std::ostringstream stream;
    stream << '"';
    for (unsigned char character : text)
    {
        switch (character)
        {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            if (character < 0x20U)
            {
                stream << "\\x" << std::hex << static_cast<int>(character) << std::dec;
            }
            else
            {
                stream << static_cast<char>(character);
            }
            break;
        }
    }
    stream << '"';
    return stream.str();
}

[[nodiscard]] std::string Shorten(std::string_view text, std::size_t maxLength)
{
    if (text.size() <= maxLength)
    {
        return std::string{text};
    }
    return std::string{text.substr(0, maxLength - 3U)} + "...";
}

[[nodiscard]] std::string FormatPath(const std::filesystem::path& path)
{
    return QuoteText(io::PathToUtf8(path));
}

[[nodiscard]] std::string_view ToString(DialogKind kind) noexcept
{
    switch (kind)
    {
    case DialogKind::OpenFile:
        return "open-file";
    case DialogKind::SaveFile:
        return "save-file";
    case DialogKind::OpenFolder:
        return "open-folder";
    }

    return "unrecognized";
}

void PrintError(std::string_view operation, const core::Error& error)
{
    std::cout << operation << " failed: " << core::FormatError(error) << '\n';
}

[[nodiscard]] bool IsPlatformError(const core::Error& error, platform::PlatformErrorCode code)
{
    return error.GetCode() == platform::ToErrorCode(code);
}

void PrintOperationResult(std::string_view operation, const core::VoidResult& result)
{
    if (result)
    {
        std::cout << operation << " succeeded.\n";
        return;
    }

    if (IsPlatformError(result.GetError(), platform::PlatformErrorCode::Unsupported))
    {
        std::cout << operation << " is unsupported on this host: "
                  << core::FormatError(result.GetError()) << '\n';
        return;
    }

    PrintError(operation, result.GetError());
}

[[nodiscard]] std::optional<std::filesystem::path> GetDialogLocation(const Options& options)
{
    if (options.dialogLocation)
    {
        return options.dialogLocation;
    }

    std::error_code error;
    std::filesystem::path location = std::filesystem::current_path(error);
    if (error)
    {
        return std::nullopt;
    }
    return location;
}

[[nodiscard]] std::vector<platform::DialogFileFilter> MakeMoleculeFilters()
{
    return {
        platform::DialogFileFilter{.name = "Molecule files", .pattern = "sdf;mol;mol2;pdb"},
        platform::DialogFileFilter{.name = "JSON files", .pattern = "json"},
        platform::DialogFileFilter{.name = "All files", .pattern = "*"},
    };
}

[[nodiscard]] std::vector<platform::DialogFileFilter> MakeSaveFilters()
{
    return {
        platform::DialogFileFilter{.name = "SDF molecule", .pattern = "sdf"},
        platform::DialogFileFilter{.name = "All files", .pattern = "*"},
    };
}

[[nodiscard]] std::optional<platform::WindowId> GetParentWindowId(const AppState& state)
{
    if (state.windows.empty())
    {
        return std::nullopt;
    }
    return state.windows.front().window.GetId();
}

[[nodiscard]] bool CanStartDesktopWork(const AppState& state, std::string_view label)
{
    if (state.shutdownRequested)
    {
        std::cout << label << " ignored: shutdown has been requested.\n";
        return false;
    }

    if (state.windows.empty())
    {
        std::cout << label << " ignored: parent window is no longer available.\n";
        return false;
    }

    return true;
}

void UpdateLastAction(AppState& state, std::string text)
{
    state.lastAction = Shorten(text, 96);
}

void CaptureClipboardIfNeeded(AppState& state)
{
    if (state.clipboardCaptureAttempted)
    {
        return;
    }

    state.clipboardCaptureAttempted = true;
    auto original = state.runtime.GetClipboardText();
    if (!original)
    {
        PrintError("PlatformRuntime::GetClipboardText before first write", original.GetError());
        std::cout << "Clipboard restoration will be unavailable for this run.\n";
        return;
    }

    state.originalClipboardText = std::move(original).GetValue();
    std::cout << "Captured original clipboard text for best-effort restoration ("
              << state.originalClipboardText->size() << " byte(s)).\n";
}

void SetClipboardText(AppState& state, std::string_view text, std::string_view label)
{
    if (!CanStartDesktopWork(state, label))
    {
        return;
    }

    CaptureClipboardIfNeeded(state);
    auto result = state.runtime.SetClipboardText(text);
    if (result)
    {
        state.clipboardModified = true;
        UpdateLastAction(state, "clipboard set to " + QuoteText(text));
    }
    PrintOperationResult(label, result);
}

void ReadClipboardText(AppState& state)
{
    if (!CanStartDesktopWork(state, "ReadClipboard"))
    {
        return;
    }

    auto text = state.runtime.GetClipboardText();
    if (!text)
    {
        PrintError("PlatformRuntime::GetClipboardText", text.GetError());
        return;
    }

    const std::string quoted = QuoteText(text.GetValue());
    std::cout << "Clipboard text: " << quoted << '\n';
    UpdateLastAction(state, "clipboard read " + quoted);
}

void RestoreClipboardText(AppState& state)
{
    if (!state.originalClipboardText)
    {
        std::cout << "No captured clipboard text is available to restore.\n";
        return;
    }

    auto result = state.runtime.SetClipboardText(*state.originalClipboardText);
    if (result)
    {
        state.clipboardModified = false;
        UpdateLastAction(state, "clipboard restored");
    }
    PrintOperationResult("PlatformRuntime::SetClipboardText(restore)", result);
}

void OpenConfiguredUri(AppState& state)
{
    if (!CanStartDesktopWork(state, "OpenExternalUri"))
    {
        return;
    }

    if (!state.options.externalUri)
    {
        std::cout << "No URI configured. Pass --uri <uri>, then press U.\n";
        return;
    }

    std::cout << "Opening external URI after explicit key command: "
              << QuoteText(*state.options.externalUri) << '\n';
    auto result = state.runtime.OpenExternalUri(*state.options.externalUri);
    if (result)
    {
        UpdateLastAction(state, "opened URI " + *state.options.externalUri);
    }
    PrintOperationResult("PlatformRuntime::OpenExternalUri", result);
}

void RegisterDialog(AppState& state, DialogKind kind, std::string label, bool parented,
                    core::Result<platform::DialogRequestId> request)
{
    if (!request)
    {
        PrintError(label, request.GetError());
        return;
    }

    const platform::DialogRequestId id = request.GetValue();
    state.pendingDialogs.push_back(PendingDialog{
        .id = id,
        .kind = kind,
        .requestedAt = state.runtime.Now(),
        .label = std::move(label),
        .parented = parented,
    });

    std::cout << state.pendingDialogs.back().label << " accepted as request " << FormatId(id)
              << ": descriptor validation, request registration, and backend invocation "
                 "have completed. Await a later DialogCompletedEvent.\n";
    UpdateLastAction(state, "registered dialog " + FormatId(id));
}

void ShowParentedOpenFileDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "ShowOpenFileDialog(parented)"))
    {
        return;
    }

    const platform::OpenFileDialogDesc desc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeMoleculeFilters(),
        .allowMultipleSelection = false,
    };
    RegisterDialog(state, DialogKind::OpenFile, "ShowOpenFileDialog(parented single)", true,
                   state.runtime.ShowOpenFileDialog(desc));
}

void ShowUnparentedMultiOpenFileDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "ShowOpenFileDialog(unparented multi)"))
    {
        return;
    }

    const platform::OpenFileDialogDesc desc{
        .parentWindowId = std::nullopt,
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeMoleculeFilters(),
        .allowMultipleSelection = true,
    };
    RegisterDialog(state, DialogKind::OpenFile, "ShowOpenFileDialog(unparented multi)", false,
                   state.runtime.ShowOpenFileDialog(desc));
}

void ShowParentedSaveFileDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "ShowSaveFileDialog(parented)"))
    {
        return;
    }

    const platform::SaveFileDialogDesc desc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeSaveFilters(),
    };
    RegisterDialog(state, DialogKind::SaveFile, "ShowSaveFileDialog(parented)", true,
                   state.runtime.ShowSaveFileDialog(desc));
}

void ShowUnparentedFolderDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "ShowOpenFolderDialog(unparented)"))
    {
        return;
    }

    const platform::OpenFolderDialogDesc desc{
        .parentWindowId = std::nullopt,
        .defaultLocation = GetDialogLocation(state.options),
        .allowMultipleSelection = false,
    };
    RegisterDialog(state, DialogKind::OpenFolder, "ShowOpenFolderDialog(unparented)", false,
                   state.runtime.ShowOpenFolderDialog(desc));
}

void LaunchConcurrentDialogBatch(AppState& state)
{
    if (!CanStartDesktopWork(state, "LaunchConcurrentDialogBatch"))
    {
        return;
    }

    std::cout << "Launching three dialog requests without assuming completion order.\n";

    const platform::OpenFileDialogDesc openDesc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeMoleculeFilters(),
        .allowMultipleSelection = true,
    };
    RegisterDialog(state, DialogKind::OpenFile, "batch parented multi open-file", true,
                   state.runtime.ShowOpenFileDialog(openDesc));

    const platform::SaveFileDialogDesc saveDesc{
        .parentWindowId = std::nullopt,
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeSaveFilters(),
    };
    RegisterDialog(state, DialogKind::SaveFile, "batch unparented save-file", false,
                   state.runtime.ShowSaveFileDialog(saveDesc));

    const platform::OpenFolderDialogDesc folderDesc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .allowMultipleSelection = true,
    };
    RegisterDialog(state, DialogKind::OpenFolder, "batch parented multi folder", true,
                   state.runtime.ShowOpenFolderDialog(folderDesc));
}

void ReleaseParentWindow(AppState& state)
{
    if (state.windows.empty())
    {
        return;
    }

    std::cout << "Releasing parent window after " << state.pendingDialogs.size()
              << " pending dialog(s) remain.\n";
    state.windows.clear();
}

void RequestShutdown(AppState& state, std::string_view reason)
{
    if (!state.shutdownRequested)
    {
        state.shutdownRequested = true;
        std::cout << "Shutdown requested by " << reason << ". New desktop work is disabled.\n";
    }

    if (state.pendingDialogs.empty())
    {
        ReleaseParentWindow(state);
        return;
    }

    std::cout << "Waiting for " << state.pendingDialogs.size()
              << " pending dialog completion(s) before releasing the parent.\n";
}

[[nodiscard]] std::vector<PendingDialog>::iterator FindPendingDialog(
    AppState& state, platform::DialogRequestId id)
{
    return std::ranges::find_if(state.pendingDialogs, [id](const PendingDialog& request) {
        return request.id == id;
    });
}

void PrintDialogSelection(const platform::DialogSelection& selection)
{
    std::cout << "  outcome: selection\n";
    std::cout << "  selected filter index: ";
    if (selection.selectedFilterIndex)
    {
        std::cout << *selection.selectedFilterIndex;
    }
    else
    {
        std::cout << "none";
    }
    std::cout << '\n';

    if (selection.paths.empty())
    {
        std::cout << "  selected paths: none\n";
        return;
    }

    for (std::size_t index = 0; index < selection.paths.size(); ++index)
    {
        std::cout << "  path[" << index << "]: " << FormatPath(selection.paths[index]) << '\n';
    }
}

void PrintDialogOutcome(const platform::DialogOutcome& outcome)
{
    struct Visitor final
    {
        void operator()(const platform::DialogSelection& selection) const
        {
            PrintDialogSelection(selection);
        }

        void operator()(platform::DialogCancellation) const
        {
            std::cout << "  outcome: cancellation (normal user choice)\n";
        }

        void operator()(const platform::DialogFailure& failure) const
        {
            std::cout << "  outcome: failure\n";
            PrintError("  dialog completion", failure.error);
        }
    };

    std::visit(Visitor{}, outcome);
}

void HandleDialogCompleted(AppState& state, const platform::DialogCompletedEvent& event)
{
    auto request = FindPendingDialog(state, event.requestId);
    if (request == state.pendingDialogs.end())
    {
        std::cout << "DialogCompleted for unknown request " << FormatId(event.requestId)
                  << "; it may have been consumed already.\n";
        return;
    }

    std::cout << "DialogCompleted request=" << FormatId(event.requestId)
              << " kind=" << ToString(request->kind) << " label=" << request->label
              << " parented=" << request->parented << '\n'
              << "  callback timestamp: " << FormatTimestamp(event.timestamp) << " (+"
              << FormatDuration(event.timestamp - state.startTimestamp) << ")\n"
              << "  elapsed since request registration: "
              << FormatDuration(event.timestamp - request->requestedAt) << '\n';
    PrintDialogOutcome(event.outcome);

    UpdateLastAction(state, "completed dialog " + FormatId(event.requestId));
    state.pendingDialogs.erase(request);

    if (state.shutdownRequested && state.pendingDialogs.empty())
    {
        ReleaseParentWindow(state);
    }
}

void HandleCommand(AppState& state, platform::PhysicalKey key)
{
    switch (key)
    {
    case platform::PhysicalKey::F1:
        PrintUsage("ponder-platform-3-desktop-services-workbench");
        return;
    case platform::PhysicalKey::C:
        SetClipboardText(state, state.options.clipboardText,
                         "PlatformRuntime::SetClipboardText(sample)");
        return;
    case platform::PhysicalKey::E:
        SetClipboardText(state, "", "PlatformRuntime::SetClipboardText(empty)");
        return;
    case platform::PhysicalKey::R:
        ReadClipboardText(state);
        return;
    case platform::PhysicalKey::B:
        RestoreClipboardText(state);
        return;
    case platform::PhysicalKey::U:
        OpenConfiguredUri(state);
        return;
    case platform::PhysicalKey::O:
        ShowParentedOpenFileDialog(state);
        return;
    case platform::PhysicalKey::M:
        ShowUnparentedMultiOpenFileDialog(state);
        return;
    case platform::PhysicalKey::S:
        ShowParentedSaveFileDialog(state);
        return;
    case platform::PhysicalKey::F:
        ShowUnparentedFolderDialog(state);
        return;
    case platform::PhysicalKey::A:
        LaunchConcurrentDialogBatch(state);
        return;
    case platform::PhysicalKey::Q:
    case platform::PhysicalKey::Escape:
        RequestShutdown(state, "keyboard");
        return;
    default:
        return;
    }
}

void PrintEventHeader(std::string_view name, platform::PlatformTimestamp timestamp,
                      const AppState& state)
{
    std::cout << "[event " << state.eventCount << "] " << name << " at "
              << FormatTimestamp(timestamp) << " (+"
              << FormatDuration(timestamp - state.startTimestamp) << ")";
}

struct EventVisitor final
{
    AppState& state;

    void operator()(const platform::QuitRequestedEvent& event) const
    {
        PrintEventHeader("QuitRequested", event.timestamp, state);
        std::cout << '\n';
        RequestShutdown(state, "quit event");
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
        RequestShutdown(state, "window close request");
    }

    void operator()(const platform::KeyboardKeyEvent& event) const
    {
        PrintEventHeader("KeyboardKey", event.timestamp, state);
        std::cout << " window=";
        if (event.windowId)
        {
            std::cout << FormatId(*event.windowId);
        }
        else
        {
            std::cout << "none";
        }
        std::cout << " physical=" << static_cast<int>(event.physicalKey)
                  << " pressed=" << event.pressed << " repeat=" << event.repeat << '\n';

        if (event.pressed && !event.repeat)
        {
            HandleCommand(state, event.physicalKey);
        }
    }

    void operator()(const platform::DialogCompletedEvent& event) const
    {
        PrintEventHeader("DialogCompleted", event.timestamp, state);
        std::cout << '\n';
        HandleDialogCompleted(state, event);
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Other platform event", event.timestamp, state);
        std::cout << " observed while desktop services remain responsive.\n";
    }
};

void DrainEvents(AppState& state)
{
    while (std::optional<platform::PlatformEvent> event = state.runtime.PollEvent())
    {
        ++state.eventCount;
        std::visit(EventVisitor{state}, *event);
    }
}

void UpdateWindowTitle(AppState& state)
{
    if (state.windows.empty())
    {
        return;
    }

    const std::string title = "Desktop Services | pending " +
                              std::to_string(state.pendingDialogs.size()) + " | " +
                              state.lastAction;
    auto result = state.windows.front().window.SetTitle(title);
    if (!result && !state.windows.front().titleUpdateFailureReported)
    {
        PrintError("SetTitle during title update", result.GetError());
        state.windows.front().titleUpdateFailureReported = true;
    }
}

void RestoreClipboardOnExit(AppState& state)
{
    if (!state.clipboardModified)
    {
        return;
    }

    if (!state.originalClipboardText)
    {
        std::cout << "Clipboard was modified, but no backup is available to restore.\n";
        return;
    }

    std::cout << "Restoring clipboard before runtime shutdown.\n";
    RestoreClipboardText(state);
}

[[nodiscard]] core::Result<WindowSlot> CreateParentWindow(platform::PlatformRuntime& runtime)
{
    const platform::WindowDesc desc{
        .title = "Ponder Desktop Services Workbench",
        .logicalSize = {860, 520},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .minimumLogicalSize = platform::LogicalSize{360, 240},
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };

    auto window = runtime.CreateWindow(desc);
    if (!window)
    {
        return core::Result<WindowSlot>::FromError(std::move(window).GetError());
    }

    return WindowSlot{.window = std::move(window).GetValue()};
}

[[nodiscard]] core::VoidResult RunDesktopServicesWorkbench(int argc, char** argv)
{
    auto optionsResult = ParseOptions(argc, argv);
    if (!optionsResult)
    {
        return core::VoidResult::FromError(std::move(optionsResult).GetError());
    }

    const Options options = std::move(optionsResult).GetValue();
    if (options.showHelp)
    {
        PrintUsage(argc > 0 ? argv[0] : "ponder-platform-3-desktop-services-workbench");
        return {};
    }

    const platform::PlatformRuntimeDesc runtimeDesc{
        .applicationName = "Ponder Platform Desktop Services Workbench",
        .applicationVersion = std::string{"0.1.0"},
        .applicationIdentifier = std::string{
            "org.ponder.examples.platform.desktop-services-workbench"},
    };

    auto runtimeResult = platform::PlatformRuntime::Create(runtimeDesc);
    if (!runtimeResult)
    {
        return core::VoidResult::FromError(std::move(runtimeResult).GetError());
    }

    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    const platform::PlatformTimestamp start = runtime.Now();

    std::vector<WindowSlot> windows;
    windows.reserve(1);
    auto parent = CreateParentWindow(runtime);
    if (!parent)
    {
        return core::VoidResult::FromError(std::move(parent).GetError());
    }
    windows.push_back(std::move(parent).GetValue());

    AppState state{.runtime = runtime,
                   .options = options,
                   .windows = windows,
                   .startTimestamp = start};

    PrintUsage(argc > 0 ? argv[0] : "ponder-platform-3-desktop-services-workbench");
    if (options.externalUri)
    {
        std::cout << "Configured URI is " << QuoteText(*options.externalUri)
                  << "; press U to open it.\n";
    }
    else
    {
        std::cout << "No URI configured; pass --uri <uri> to enable the U command.\n";
    }

    auto nextTitleUpdate = start;
    while (!state.windows.empty() || !state.pendingDialogs.empty())
    {
        DrainEvents(state);

        const platform::PlatformTimestamp now = runtime.Now();
        if (now - nextTitleUpdate >= std::chrono::milliseconds{250})
        {
            UpdateWindowTitle(state);
            nextTitleUpdate = now;
        }

        if (options.autoCloseAfter && !state.shutdownRequested &&
            now - start >= *options.autoCloseAfter)
        {
            std::cout << "Auto-close duration reached after " << FormatDuration(now - start)
                      << ".\n";
            RequestShutdown(state, "auto close");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{8});
    }

    RestoreClipboardOnExit(state);
    return {};
}
} // namespace

int main(int argc, char** argv)
{
    std::cout << std::boolalpha;

    try
    {
        const auto result = RunDesktopServicesWorkbench(argc, argv);
        if (!result)
        {
            std::cerr << "ponder-platform-3-desktop-services-workbench failed: "
                      << core::FormatError(result.GetError()) << '\n';
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ponder-platform-3-desktop-services-workbench terminated with an exception: "
                  << exception.what() << '\n';
        return 1;
    }

    return 0;
}
