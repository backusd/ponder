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
#include <format>
#include <filesystem>
#include <print>
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
struct OptionalWindowId final
{
    const std::optional<pond::platform::WindowId>& value;
};
} // namespace

namespace std
{
template <>
struct formatter<OptionalWindowId> : formatter<string>
{
    template <typename FormatContext>
    auto format(const OptionalWindowId& id, FormatContext& context) const
    {
        if (!id.value)
        {
            return formatter<string>::format("none", context);
        }

        return formatter<string>::format(std::format("{}", *id.value), context);
    }
};
} // namespace std

namespace
{
namespace core = pond::core;
namespace io = pond::io;
namespace platform = pond::platform;

struct Options final
{
    std::optional<platform::Duration> autoCloseAfter;
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
    platform::Timestamp requestedAt{};
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
    platform::Timestamp startTimestamp;
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
    std::print(
        "Usage: {} [options]\n\n"
        "Options:\n"
        "  --auto-close-ms <milliseconds>  Exit after a short idle run.\n"
        "  --clipboard-text <text>         Text copied by the C command.\n"
        "  --dialog-location <path>        Default dialog location.\n"
        "  --uri <uri>                     URI opened only after pressing U.\n"
        "  --help                          Print this help text.\n\n"
        "Controls:\n"
        "  F1            Print this help text.\n"
        "  C / E / R     Copy sample text, copy empty text, or read clipboard.\n"
        "  B             Restore the clipboard captured before the first write.\n"
        "  U             Open the URI supplied with --uri.\n"
        "  O / M         Open parented single-file or unparented multi-file dialog.\n"
        "  S / F         Open parented save-file or unparented folder dialog.\n"
        "  A             Launch a concurrent three-dialog batch.\n"
        "  Q / Escape    Request shutdown; pending dialogs are still consumed.\n",
        executableName);
}

[[nodiscard]] core::Result<platform::Duration> ParseMilliseconds(
    std::string_view text)
{
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end)
    {
        return core::Result<platform::Duration>::FromError(
            MakeOptionError("Expected a non-negative integer millisecond value."));
    }

    using Milliseconds = std::chrono::milliseconds;
    constexpr auto kMaxMilliseconds = static_cast<std::uint64_t>(
        std::numeric_limits<Milliseconds::rep>::max());
    if (value > kMaxMilliseconds)
    {
        return core::Result<platform::Duration>::FromError(
            MakeOptionError("Auto-close duration is too large."));
    }

    return platform::Duration{
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
            RETURN_ERROR_IF_FAILED(duration);
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
    std::println("{} failed: {}", operation, error);
}

void PrintOperationResult(std::string_view operation, const core::VoidResult& result)
{
    if (result)
    {
        std::println("{} succeeded.", operation);
        return;
    }

    if (result.GetError() == platform::PlatformErrorCode::Unsupported)
    {
        std::println("{} is unsupported on this host: {}", operation, result.GetError());
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
        std::println("{} ignored: shutdown has been requested.", label);
        return false;
    }

    if (state.windows.empty())
    {
        std::println("{} ignored: parent window is no longer available.", label);
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
        std::println("Clipboard restoration will be unavailable for this run.");
        return;
    }

    state.originalClipboardText = std::move(original).GetValue();
    std::println("Captured original clipboard text for best-effort restoration ({} byte(s)).",
                 state.originalClipboardText->size());
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
    std::println("Clipboard text: {}", quoted);
    UpdateLastAction(state, "clipboard read " + quoted);
}

void RestoreClipboardText(AppState& state)
{
    if (!state.originalClipboardText)
    {
        std::println("No captured clipboard text is available to restore.");
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
        std::println("No URI configured. Pass --uri <uri>, then press U.");
        return;
    }

    std::println("Opening external URI after explicit key command: {}",
                 QuoteText(*state.options.externalUri));
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

    std::println(
        "{} accepted as request {}: descriptor validation, request registration, and backend "
        "invocation have completed. Await a later DialogCompletedEvent.",
        state.pendingDialogs.back().label, id);
    UpdateLastAction(state, std::format("registered dialog {}", id));
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

    std::println("Launching three dialog requests without assuming completion order.");

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

    std::println("Releasing parent window after {} pending dialog(s) remain.",
                 state.pendingDialogs.size());
    state.windows.clear();
}

void RequestShutdown(AppState& state, std::string_view reason)
{
    if (!state.shutdownRequested)
    {
        state.shutdownRequested = true;
        std::println("Shutdown requested by {}. New desktop work is disabled.", reason);
    }

    if (state.pendingDialogs.empty())
    {
        ReleaseParentWindow(state);
        return;
    }

    std::println("Waiting for {} pending dialog completion(s) before releasing the parent.",
                 state.pendingDialogs.size());
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
    std::println("  outcome: selection");
    if (selection.selectedFilterIndex)
    {
        std::println("  selected filter index: {}", *selection.selectedFilterIndex);
    }
    else
    {
        std::println("  selected filter index: none");
    }

    if (selection.paths.empty())
    {
        std::println("  selected paths: none");
        return;
    }

    for (std::size_t index = 0; index < selection.paths.size(); ++index)
    {
        std::println("  path[{}]: {}", index, FormatPath(selection.paths[index]));
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
            std::println("  outcome: cancellation (normal user choice)");
        }

        void operator()(const platform::DialogFailure& failure) const
        {
            std::println("  outcome: failure");
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
        std::println("DialogCompleted for unknown request {}; it may have been consumed already.",
                     event.requestId);
        return;
    }

    std::println(
        "DialogCompleted request={} kind={} label={} parented={}\n"
        "  callback timestamp: {} (+{})\n"
        "  elapsed since request registration: {}",
        event.requestId, ToString(request->kind), request->label, request->parented,
        event.timestamp, event.timestamp - state.startTimestamp,
        event.timestamp - request->requestedAt);
    PrintDialogOutcome(event.outcome);

    UpdateLastAction(state, std::format("completed dialog {}", event.requestId));
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

void PrintEventHeader(std::string_view name, platform::Timestamp timestamp,
                      const AppState& state)
{
    std::print("[event {}] {} at {} (+{})", state.eventCount, name, timestamp,
               timestamp - state.startTimestamp);
}

struct EventVisitor final
{
    AppState& state;

    void operator()(const platform::QuitRequestedEvent& event) const
    {
        PrintEventHeader("QuitRequested", event.timestamp, state);
        std::println();
        RequestShutdown(state, "quit event");
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::println(" window={}", event.windowId);
        RequestShutdown(state, "window close request");
    }

    void operator()(const platform::KeyboardKeyEvent& event) const
    {
        PrintEventHeader("KeyboardKey", event.timestamp, state);
        std::println(" window={} physical={} pressed={} repeat={}",
                     OptionalWindowId{event.windowId}, static_cast<int>(event.physicalKey),
                     event.pressed, event.repeat);

        if (event.pressed && !event.repeat)
        {
            HandleCommand(state, event.physicalKey);
        }
    }

    void operator()(const platform::DialogCompletedEvent& event) const
    {
        PrintEventHeader("DialogCompleted", event.timestamp, state);
        std::println();
        HandleDialogCompleted(state, event);
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Other platform event", event.timestamp, state);
        std::println(" observed while desktop services remain responsive.");
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
        std::println("Clipboard was modified, but no backup is available to restore.");
        return;
    }

    std::println("Restoring clipboard before runtime shutdown.");
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
    RETURN_ERROR_IF_FAILED(window);

    return WindowSlot{.window = std::move(window).GetValue()};
}

[[nodiscard]] core::VoidResult RunDesktopServicesWorkbench(int argc, char** argv)
{
    auto optionsResult = ParseOptions(argc, argv);
    RETURN_ERROR_IF_FAILED(optionsResult);

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
    RETURN_ERROR_IF_FAILED(runtimeResult);

    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    RETURN_ERROR_IF_FAILED(
        runtime.GetHintManager().PushHint(platform::hints::MouseFocusClickThrough{true}));
    RETURN_ERROR_IF_FAILED(
        runtime.GetHintManager().PushHint(platform::hints::MouseAutoCapture{false}));
    const platform::Timestamp start = runtime.Now();

    std::vector<WindowSlot> windows;
    windows.reserve(1);
    auto parent = CreateParentWindow(runtime);
    RETURN_ERROR_IF_FAILED(parent);
    windows.push_back(std::move(parent).GetValue());

    AppState state{.runtime = runtime,
                   .options = options,
                   .windows = windows,
                   .startTimestamp = start};

    PrintUsage(argc > 0 ? argv[0] : "ponder-platform-3-desktop-services-workbench");
    if (options.externalUri)
    {
        std::println("Configured URI is {}; press U to open it.",
                     QuoteText(*options.externalUri));
    }
    else
    {
        std::println("No URI configured; pass --uri <uri> to enable the U command.");
    }

    auto nextTitleUpdate = start;
    while (!state.windows.empty() || !state.pendingDialogs.empty())
    {
        DrainEvents(state);

        const platform::Timestamp now = runtime.Now();
        if (now - nextTitleUpdate >= std::chrono::milliseconds{250})
        {
            UpdateWindowTitle(state);
            nextTitleUpdate = now;
        }

        if (options.autoCloseAfter && !state.shutdownRequested &&
            now - start >= *options.autoCloseAfter)
        {
            std::println("Auto-close duration reached after {}.", now - start);
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
    try
    {
        const auto result = RunDesktopServicesWorkbench(argc, argv);
        if (!result)
        {
            std::println(stderr, "ponder-platform-3-desktop-services-workbench failed: {}",
                         result.GetError());
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::println(stderr,
                     "ponder-platform-3-desktop-services-workbench terminated with an exception: {}",
                     exception.what());
        return 1;
    }

    return 0;
}
