#include <ponder/core/Exception.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/Dialogs.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Runtime.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <print>
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
    const std::optional<ponder::platform::WindowId>& value;
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
namespace core = ponder::core;
namespace io = pond::io;
namespace platform = ponder::platform;

struct Options final
{
    std::optional<core::Duration> autoCloseAfter;
    std::optional<std::string> externalUri;
    std::optional<std::filesystem::path> dialogLocation;
    std::string clipboardText{"Ponder UTF-8 sample: H2O -> \xCE\x94G"};
    bool showHelp{};
};

struct WindowSlot final
{
    platform::Window window;
};

struct AppState final
{
    platform::Runtime& runtime;
    const Options& options;
    std::vector<WindowSlot>& windows;
    core::Timestamp startTimestamp;
    std::uint64_t eventCount{};
    bool shutdownRequested{};
    bool dialogApiFailed{};
    bool clipboardModified{};
    std::optional<std::string> originalClipboardText;
    std::string lastAction{"ready"};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0}, std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::print("Usage: {} [options]\n\n"
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

[[nodiscard]] core::Result<core::Duration> ParseMilliseconds(std::string_view text)
{
    using ResultType = core::Result<core::Duration>;

    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end)
    {
        return ResultType::FromError(MakeOptionError("Expected a non-negative integer millisecond value."));
    }

    using Milliseconds = std::chrono::milliseconds;
    constexpr auto kMaxMilliseconds = static_cast<std::uint64_t>(std::numeric_limits<Milliseconds::rep>::max());
    if (value > kMaxMilliseconds)
    {
        return ResultType::FromError(MakeOptionError("Auto-close duration is too large."));
    }

    return core::Duration{Milliseconds{static_cast<Milliseconds::rep>(value)}};
}

[[nodiscard]] core::Result<Options> ParseOptions(int argc, char** argv)
{
    using ResultType = core::Result<Options>;

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
                return ResultType::FromError(MakeOptionError("--auto-close-ms requires a value."));
            }

            ++index;
            auto duration = ParseMilliseconds(argv[index]);
            if (!duration)
            {
                return ResultType::FromError(std::move(duration).GetError());
            }
            options.autoCloseAfter = std::move(duration).GetValue();
        }
        else if (argument == "--clipboard-text")
        {
            if (index + 1 >= argc)
            {
                return ResultType::FromError(MakeOptionError("--clipboard-text requires a value."));
            }
            options.clipboardText = argv[++index];
        }
        else if (argument == "--dialog-location")
        {
            if (index + 1 >= argc)
            {
                return ResultType::FromError(MakeOptionError("--dialog-location requires a value."));
            }
            options.dialogLocation = io::PathFromUtf8(argv[++index]);
        }
        else if (argument == "--uri")
        {
            if (index + 1 >= argc)
            {
                return ResultType::FromError(MakeOptionError("--uri requires a value."));
            }

            options.externalUri = argv[++index];
            if (options.externalUri->empty())
            {
                return ResultType::FromError(MakeOptionError("--uri cannot be empty."));
            }
        }
        else
        {
            return ResultType::FromError(MakeOptionError("Unknown option: " + std::string{argument}));
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

void PrintError(std::string_view operation, const core::Error& error)
{
    std::println("{} failed: {}", operation, error);
}

void PrintServiceResult(std::string_view operation, const core::VoidResult& result)
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

[[nodiscard]] std::vector<platform::dialogs::DialogFileFilter> MakeMoleculeFilters()
{
    return {
        platform::dialogs::DialogFileFilter{.name = "Molecule files", .pattern = "sdf;mol;mol2;pdb"},
        platform::dialogs::DialogFileFilter{.name = "JSON files", .pattern = "json"},
        platform::dialogs::DialogFileFilter{.name = "All files", .pattern = "*"},
    };
}

[[nodiscard]] std::vector<platform::dialogs::DialogFileFilter> MakeSaveFilters()
{
    return {
        platform::dialogs::DialogFileFilter{.name = "SDF molecule", .pattern = "sdf"},
        platform::dialogs::DialogFileFilter{.name = "All files", .pattern = "*"},
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

void ReportDialogApiError(AppState& state, std::string_view operation, const core::Error& error)
{
    PrintError(operation, error);
    state.dialogApiFailed = true;
    UpdateLastAction(state, std::string{operation} + " failed");
}

void SetClipboardText(AppState& state, std::string_view text, std::string_view label)
{
    if (!CanStartDesktopWork(state, label))
    {
        return;
    }

    std::optional<std::string> capturedOriginal;
    if (!state.clipboardModified && !state.originalClipboardText.has_value())
    {
        auto captureResult = state.runtime.ClipboardGetText();
        if (captureResult)
        {
            capturedOriginal = std::move(captureResult).GetValue();
        }
        else
        {
            PrintError("Runtime::ClipboardGetText(capture original)", captureResult.GetError());
        }
    }

    const core::VoidResult setResult = state.runtime.ClipboardSetText(text);
    if (!setResult)
    {
        PrintServiceResult(label, setResult);
        UpdateLastAction(state, std::string{label} + " failed");
        return;
    }

    if (capturedOriginal.has_value())
    {
        state.originalClipboardText = std::move(capturedOriginal);
        std::println("Captured original clipboard text for best-effort restoration ({} byte(s)).", state.originalClipboardText->size());
    }
    state.clipboardModified = true;
    UpdateLastAction(state, "clipboard set to " + QuoteText(text));
    std::println("{} succeeded.", label);
}

void ReadClipboardText(AppState& state)
{
    if (!CanStartDesktopWork(state, "ReadClipboard"))
    {
        return;
    }

    auto textResult = state.runtime.ClipboardGetText();
    if (!textResult)
    {
        PrintError("Runtime::ClipboardGetText", textResult.GetError());
        UpdateLastAction(state, "clipboard read failed");
        return;
    }

    const std::string text = std::move(textResult).GetValue();
    const std::string quoted = QuoteText(text);
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

    const core::VoidResult restoreResult = state.runtime.ClipboardSetText(*state.originalClipboardText);
    if (!restoreResult)
    {
        PrintServiceResult("Runtime::ClipboardSetText(restore)", restoreResult);
        UpdateLastAction(state, "clipboard restore failed");
        return;
    }

    state.clipboardModified = false;
    UpdateLastAction(state, "clipboard restored");
    std::println("Runtime::ClipboardSetText(restore) succeeded.");
}

void OpenConfiguredUri(AppState& state)
{
    if (!CanStartDesktopWork(state, "UriOpenExternal"))
    {
        return;
    }

    if (!state.options.externalUri)
    {
        std::println("No URI configured. Pass --uri <uri>, then press U.");
        return;
    }

    std::println("Opening external URI after explicit key command: {}", QuoteText(*state.options.externalUri));
    auto result = state.runtime.UriOpenExternal(*state.options.externalUri);
    if (result)
    {
        UpdateLastAction(state, "opened URI " + *state.options.externalUri);
    }
    PrintServiceResult("Runtime::UriOpenExternal", result);
    if (!result)
    {
        std::println("Fallback: copy {} and open it manually if host policy permits.", QuoteText(*state.options.externalUri));
    }
}

void ReportDialogRequest(AppState& state, std::string_view operation, core::Result<platform::dialogs::DialogRequestId> result)
{
    if (!result)
    {
        ReportDialogApiError(state, operation, result.GetError());
        return;
    }

    const platform::dialogs::DialogRequestId id = std::move(result).GetValue();
    std::println("{} accepted as request {}: descriptor validation, request registration, and backend "
                 "invocation have completed. {} dialog(s) are pending.",
                 operation, id, state.runtime.DialogGetPendingCount());
    UpdateLastAction(state, std::format("registered dialog {}", id));
}

void ShowParentedOpenFileDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "Runtime::DialogShowOpenFile(parented)"))
    {
        return;
    }

    const platform::dialogs::OpenFileDialogDesc desc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeMoleculeFilters(),
        .allowMultipleSelection = false,
    };
    ReportDialogRequest(state, "Runtime::DialogShowOpenFile(parented single)", state.runtime.DialogShowOpenFile(desc));
}

void ShowUnparentedMultiOpenFileDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "Runtime::DialogShowOpenFile(unparented multi)"))
    {
        return;
    }

    const platform::dialogs::OpenFileDialogDesc desc{
        .parentWindowId = std::nullopt,
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeMoleculeFilters(),
        .allowMultipleSelection = true,
    };
    ReportDialogRequest(state, "Runtime::DialogShowOpenFile(unparented multi)", state.runtime.DialogShowOpenFile(desc));
}

void ShowParentedSaveFileDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "Runtime::DialogShowSaveFile(parented)"))
    {
        return;
    }

    const platform::dialogs::SaveFileDialogDesc desc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeSaveFilters(),
    };
    ReportDialogRequest(state, "Runtime::DialogShowSaveFile(parented)", state.runtime.DialogShowSaveFile(desc));
}

void ShowUnparentedFolderDialog(AppState& state)
{
    if (!CanStartDesktopWork(state, "Runtime::DialogShowOpenFolder(unparented)"))
    {
        return;
    }

    const platform::dialogs::OpenFolderDialogDesc desc{
        .parentWindowId = std::nullopt,
        .defaultLocation = GetDialogLocation(state.options),
        .allowMultipleSelection = false,
    };
    ReportDialogRequest(state, "Runtime::DialogShowOpenFolder(unparented)", state.runtime.DialogShowOpenFolder(desc));
}

void LaunchConcurrentDialogBatch(AppState& state)
{
    if (!CanStartDesktopWork(state, "LaunchConcurrentDialogBatch"))
    {
        return;
    }

    std::println("Launching three dialog requests without assuming completion order.");

    const platform::dialogs::OpenFileDialogDesc openDesc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeMoleculeFilters(),
        .allowMultipleSelection = true,
    };
    ReportDialogRequest(state, "batch parented multi open-file", state.runtime.DialogShowOpenFile(openDesc));

    const platform::dialogs::SaveFileDialogDesc saveDesc{
        .parentWindowId = std::nullopt,
        .defaultLocation = GetDialogLocation(state.options),
        .filters = MakeSaveFilters(),
    };
    ReportDialogRequest(state, "batch unparented save-file", state.runtime.DialogShowSaveFile(saveDesc));

    const platform::dialogs::OpenFolderDialogDesc folderDesc{
        .parentWindowId = GetParentWindowId(state),
        .defaultLocation = GetDialogLocation(state.options),
        .allowMultipleSelection = true,
    };
    ReportDialogRequest(state, "batch parented multi folder", state.runtime.DialogShowOpenFolder(folderDesc));
}

void ReleaseParentWindow(AppState& state)
{
    if (state.windows.empty())
    {
        return;
    }

    std::println("Releasing parent window with {} pending dialog(s).", state.runtime.DialogGetPendingCount());
    state.windows.clear();
}

void RequestShutdown(AppState& state, std::string_view reason)
{
    if (!state.shutdownRequested)
    {
        state.shutdownRequested = true;
        std::println("Shutdown requested by {}. New desktop work is disabled.", reason);
    }

    if (!state.runtime.DialogHasPending())
    {
        ReleaseParentWindow(state);
        return;
    }

    std::println("Waiting for {} pending dialog completion(s) before releasing the parent.", state.runtime.DialogGetPendingCount());
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

void HandleDialogCompleted(AppState& state, const platform::DialogCompletedEvent& event)
{
    std::println("DialogCompleted request={} kind={} parent={} filters={} allowMultipleSelection={}\n"
                 "  requested timestamp: {} (+{})\n"
                 "  callback timestamp: {} (+{})\n"
                 "  elapsed since request invocation: {}",
                 event.request.id, event.request.kind, OptionalWindowId{event.request.parentWindowId}, event.request.filterCount,
                 event.request.allowMultipleSelection, event.request.requestedAt, event.request.requestedAt - state.startTimestamp, event.timestamp,
                 event.timestamp - state.startTimestamp, event.timestamp - event.request.requestedAt);
    if (const auto* selection = std::get_if<platform::DialogSelection>(&event.outcome))
    {
        PrintDialogSelection(*selection);
    }
    else if (std::holds_alternative<platform::DialogCancellation>(event.outcome))
    {
        std::println("  outcome: cancellation (normal user choice)");
    }
    else
    {
        const auto& failure = std::get<platform::DialogFailure>(event.outcome);
        std::println("  outcome: asynchronous failure");
        PrintError("  dialog completion", failure.error);
    }
    std::println("  remaining pending dialogs: {}", state.runtime.DialogGetPendingCount());

    UpdateLastAction(state, std::format("completed dialog {}", event.request.id));

    if (state.shutdownRequested && !state.runtime.DialogHasPending())
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
        SetClipboardText(state, state.options.clipboardText, "Runtime::ClipboardSetText(sample)");
        return;
    case platform::PhysicalKey::E:
        SetClipboardText(state, "", "Runtime::ClipboardSetText(empty)");
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

void PrintEventHeader(std::string_view name, core::Timestamp timestamp, const AppState& state)
{
    std::print("[event {}] {} at {} (+{})", state.eventCount, name, timestamp, timestamp - state.startTimestamp);
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
        std::println(" window={} physical={} pressed={} repeat={}", OptionalWindowId{event.windowId}, event.physicalKey, event.pressed, event.repeat);

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
    while (std::optional<platform::PlatformEvent> event = state.runtime.EventPoll())
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

    const std::string title = "Desktop Services | pending " + std::to_string(state.runtime.DialogGetPendingCount()) + " | " + state.lastAction;
    state.windows.front().window.SetTitle(title);
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

[[nodiscard]] WindowSlot CreateParentWindow(platform::Runtime& runtime)
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

    return WindowSlot{.window = runtime.WindowCreate(desc)};
}

[[nodiscard]] int RunDesktopServicesWorkbench(int argc, char** argv)
{
    auto optionsResult = ParseOptions(argc, argv);
    if (!optionsResult)
    {
        std::println(stderr, "ponder-platform-3-desktop-services-workbench failed: {}", optionsResult.GetError());
        return 1;
    }

    const Options options = std::move(optionsResult).GetValue();
    if (options.showHelp)
    {
        PrintUsage(argc > 0 ? argv[0] : "ponder-platform-3-desktop-services-workbench");
        return 0;
    }

    platform::Runtime runtime = platform::Runtime::Create();
    runtime.HintPush(platform::hints::MouseFocusClickThrough{true});
    runtime.HintPush(platform::hints::MouseAutoCapture{false});
    runtime.Initialize("Ponder Platform Desktop Services Workbench", "0.1.0", "org.ponder.examples.platform.desktop-services-workbench");
    const core::Timestamp start = runtime.TimeNow();

    std::vector<WindowSlot> windows;
    windows.reserve(1);
    windows.push_back(CreateParentWindow(runtime));

    AppState state{.runtime = runtime, .options = options, .windows = windows, .startTimestamp = start};

    PrintUsage(argc > 0 ? argv[0] : "ponder-platform-3-desktop-services-workbench");
    if (options.externalUri)
    {
        std::println("Configured URI is {}; press U to open it.", QuoteText(*options.externalUri));
    }
    else
    {
        std::println("No URI configured; pass --uri <uri> to enable the U command.");
    }

    auto nextTitleUpdate = start;
    std::exception_ptr deferredFailure;
    while (!state.windows.empty() || state.runtime.DialogHasPending())
    {
        try
        {
            DrainEvents(state);

            const core::Timestamp now = state.runtime.TimeNow();
            if (deferredFailure == nullptr && now - nextTitleUpdate >= std::chrono::milliseconds{250})
            {
                UpdateWindowTitle(state);
                nextTitleUpdate = now;
            }

            if (options.autoCloseAfter && !state.shutdownRequested && now - start >= *options.autoCloseAfter)
            {
                std::println("Auto-close duration reached after {}.", now - start);
                RequestShutdown(state, "auto close");
            }
        }
        catch (...)
        {
            if (deferredFailure != nullptr)
            {
                throw;
            }

            deferredFailure = std::current_exception();
            state.shutdownRequested = true;
            if (state.runtime.DialogHasPending())
            {
                std::println(stderr,
                             "A synchronous failure occurred with {} accepted dialog request(s); new "
                             "desktop work is disabled while their completions are consumed.",
                             state.runtime.DialogGetPendingCount());
            }
            else
            {
                ReleaseParentWindow(state);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{8});
    }

    RestoreClipboardOnExit(state);
    const core::VoidResult shutdownResult = state.runtime.DialogShutdown();
    if (!shutdownResult)
    {
        ReportDialogApiError(state, "Runtime::DialogShutdown", shutdownResult.GetError());
    }

    if (deferredFailure != nullptr)
    {
        std::rethrow_exception(deferredFailure);
    }
    return state.dialogApiFailed ? 1 : 0;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        return RunDesktopServicesWorkbench(argc, argv);
    }
    catch (const core::Exception& exception)
    {
        std::println(stderr, "ponder-platform-3-desktop-services-workbench terminated with a ponder exception: {}", exception.GetMessage());
        return 1;
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "ponder-platform-3-desktop-services-workbench terminated with an exception: {}", exception.what());
        return 1;
    }
}
