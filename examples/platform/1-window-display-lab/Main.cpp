#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
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
struct OptionalDisplayId final
{
    const std::optional<pond::platform::DisplayId>& value;
};

struct OptionalRefreshRate final
{
    const std::optional<float>& value;
};
} // namespace

namespace std
{
template <>
struct formatter<pond::platform::WindowId> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowId id, FormatContext& context) const
    {
        if (!id.IsValid())
        {
            return formatter<string>::format("invalid", context);
        }

        return formatter<string>::format(std::to_string(id.GetValue()), context);
    }
};

template <>
struct formatter<pond::platform::DisplayId> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::DisplayId id, FormatContext& context) const
    {
        if (!id.IsValid())
        {
            return formatter<string>::format("invalid", context);
        }

        return formatter<string>::format(std::to_string(id.GetValue()), context);
    }
};

template <>
struct formatter<OptionalDisplayId> : formatter<string>
{
    template <typename FormatContext>
    auto format(const OptionalDisplayId& id, FormatContext& context) const
    {
        if (!id.value)
        {
            return formatter<string>::format("none", context);
        }

        return formatter<string>::format(std::format("{}", *id.value), context);
    }
};

template <>
struct formatter<OptionalRefreshRate> : formatter<string>
{
    template <typename FormatContext>
    auto format(const OptionalRefreshRate& refreshRate, FormatContext& context) const
    {
        if (!refreshRate.value)
        {
            return formatter<string>::format("unavailable", context);
        }

        return formatter<string>::format(std::format("{} Hz", *refreshRate.value), context);
    }
};
} // namespace std

namespace
{
namespace core = pond::core;
namespace platform = pond::platform;

struct Options final
{
    std::optional<platform::Duration> autoCloseAfter;
    bool exerciseState{};
    bool showHelp{};
};

struct WindowSlot final
{
    platform::Window window;
    std::string label;
    bool titleUpdateFailureReported{};
};

struct AppState final
{
    platform::PlatformRuntime& runtime;
    std::vector<WindowSlot>& windows;
    platform::Timestamp startTimestamp;
    std::uint64_t eventCount{};
    bool quitRequested{};
    bool snapshotRequested{};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0},
                       std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::println("Usage: {} [--auto-close-ms <milliseconds>] [--exercise-state]", executableName);
    std::println("\nOptions:");
    std::println("  --auto-close-ms <milliseconds>  Exit automatically after a short run.");
    std::println("  --exercise-state                Also try intrusive window state changes.");
    std::println("  --help                          Print this help text.\n");
}

[[nodiscard]] core::Result<platform::Duration> ParseMilliseconds(
    std::string_view text)
{
    using ResultType = core::Result<platform::Duration>;

    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end)
    {
        return ResultType::FromError(
            MakeOptionError("Expected a non-negative integer millisecond value."));
    }

    using Milliseconds = std::chrono::milliseconds;
    constexpr auto kMaxMilliseconds = static_cast<std::uint64_t>(
        std::numeric_limits<Milliseconds::rep>::max());
    if (value > kMaxMilliseconds)
    {
        return ResultType::FromError(
            MakeOptionError("Auto-close duration is too large."));
    }

    return platform::Duration{
        Milliseconds{static_cast<Milliseconds::rep>(value)}};
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
        else if (argument == "--exercise-state")
        {
            options.exerciseState = true;
        }
        else if (argument == "--auto-close-ms")
        {
            if (index + 1 >= argc)
            {
                return ResultType::FromError(
                    MakeOptionError("--auto-close-ms requires a value."));
            }

            ++index;
            auto duration = ParseMilliseconds(argv[index]);
            RETURN_ERROR_IF_FAILED(duration);
            options.autoCloseAfter = std::move(duration).GetValue();
        }
        else
        {
            return ResultType::FromError(
                MakeOptionError("Unknown option: " + std::string{argument}));
        }
    }

    return options;
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

    PrintError(operation, result.GetError());
}

template <typename Value, typename Formatter>
void PrintQueryResult(std::string_view label, const core::Result<Value>& result,
                      Formatter formatter)
{
    if (result)
    {
        std::println("  {}: {}", label, formatter(result.GetValue()));
        return;
    }

    std::println("  {}: error: {}", label, result.GetError());
}

void PrintDisplayInfo(const platform::DisplayInfo& display)
{
    std::println("Display {}", display.id);
    std::println("  name: {}", display.name);
    std::println("  bounds: {}", display.bounds);
    std::println("  usable bounds: {}", display.usableBounds);
    std::println("  refresh: {}", OptionalRefreshRate{display.refreshRateHertz});
    std::println("  orientation: {}", display.orientation);
    std::println("  content scale: {}", display.contentScale);
}

void PrintDisplays(platform::PlatformRuntime& runtime)
{
    std::println("\nDisplay snapshot");
    auto displays = runtime.EnumerateDisplays();
    RETURN_VOID_IF_FAILED_FN(displays,
                             [](const pond::core::Error& e)
                             {
                                 PrintError("PlatformRuntime::EnumerateDisplays", e);
                             });

    if (displays.GetValue().empty())
    {
        std::println("  No connected displays were reported.");
        return;
    }

    for (const platform::DisplayInfo& display : displays.GetValue())
    {
        PrintDisplayInfo(display);

        auto lookup = runtime.GetDisplayInfo(display.id);
        RETURN_VOID_IF_FAILED_FN(lookup,
                                 [](const pond::core::Error& e)
                                 {
                                     PrintError("PlatformRuntime::GetDisplayInfo", e);
                                 });
    }
}

void PrintWindowSnapshot(const WindowSlot& slot)
{
    const platform::Window& window = slot.window;

    std::println("\nWindow snapshot: {} (id {})", slot.label, window.GetId());
    std::println("  title: {}", window.GetTitle());
    std::println("  graphics compatibility: {}", window.GetGraphicsCompatibility());
    std::println("  mouse grabbed: {}", window.IsMouseGrabbed());
    std::println("  relative mouse mode: {}", window.IsRelativeMouseModeEnabled());
    std::println("  position: {}", window.GetPosition());
    std::println("  logical size: {}", window.GetLogicalSize());
    std::println("  pixel size: {}", window.GetPixelSize());
    std::println("  display id: {}", window.GetDisplayId());
    std::println("  pixel density: {}", window.GetPixelDensity());
    std::println("  display scale: {}", window.GetDisplayScale());
    std::println("  presentation: {}", window.GetPresentation());
    std::println("  decoration: {}", window.GetDecoration());
    std::println("  state: {}", window.GetState());
    std::println("  visible: {}", window.IsVisible());
    std::println("  resizable: {}", window.IsResizable());
    std::println("  focused: {}", window.IsFocused());
    std::println("  always on top: {}", window.IsAlwaysOnTop());
}

void PrintAllWindowSnapshots(const std::vector<WindowSlot>& windows)
{
    for (const WindowSlot& slot : windows)
    {
        PrintWindowSnapshot(slot);
    }
}

[[nodiscard]] core::Result<WindowSlot> CreateWindowSlot(platform::PlatformRuntime& runtime,
                                                        const platform::WindowDesc& desc,
                                                        std::string label)
{
    auto window = runtime.CreateWindow(desc);
    RETURN_ERROR_IF_FAILED(window);

    return WindowSlot{std::move(window).GetValue(), std::move(label)};
}

[[nodiscard]] pond::core::Result<platform::WindowId> CreateAndReleaseProbeWindow(
    platform::PlatformRuntime& runtime)
{
    const platform::WindowDesc desc{
        .title = "Ponder Platform Lab - released probe",
        .logicalSize = {240, 160},
        .visible = false,
        .resizable = false,
        .highPixelDensity = true,
        .minimumLogicalSize = std::nullopt,
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };

    auto probe = runtime.CreateWindow(desc);
    RETURN_ERROR_IF_FAILED_FN(probe,
                              [](const pond::core::Error& e)
                              {
                                  PrintError("probe CreateWindow", e);
                              });

    const platform::WindowId id = probe->GetId();
    std::println("Created probe window id {} and releasing it immediately.", id);
    return id;
}

void DemonstrateRuntimeAlreadyActive(const platform::PlatformRuntimeDesc& desc)
{
    auto duplicateRuntime = platform::PlatformRuntime::Create(desc);
    RETURN_VOID_IF_FAILED_FN(duplicateRuntime,
        [](const pond::core::Error& e)
        {
            if (e == platform::PlatformErrorCode::RuntimeAlreadyActive)
            {
                std::println("Observed expected RuntimeAlreadyActive error: {}", e);
                return;
            }

            PrintError("duplicate PlatformRuntime::Create", e);
        });

    std::println("Unexpectedly created a second runtime; releasing it immediately.");
}

void ApplyBasicWindowTour(std::vector<WindowSlot>& windows, bool exerciseState)
{
    if (windows.empty())
    {
        return;
    }

    platform::Window& primary = windows.front().window;

    std::println("primary.SetTitle: {}", primary.SetTitle("Ponder Platform Lab - primary"));
    std::println("primary.SetLogicalSize: {}", primary.SetLogicalSize({960, 640}));
    std::println("primary.SetPosition: {}", primary.SetPosition({80, 80}));
    std::println("primary.SetPresentation(Windowed): {}",
                 primary.SetPresentation(platform::WindowPresentation::Windowed));
    std::println("primary.SetDecoration(System): {}",
                 primary.SetDecoration(platform::WindowDecoration::System));
    std::println("primary.Restore: {}", primary.Restore());

    if (windows.size() > 1U)
    {
        platform::Window& secondary = windows[1].window;
        std::println("secondary.SetTitle: {}", secondary.SetTitle("Ponder Platform Lab - secondary"));
        std::println("secondary.SetResizable(false): {}", secondary.SetResizable(false));
        std::println("secondary.SetResizable(true): {}", secondary.SetResizable(true));
        std::println("secondary.Hide: {}", secondary.Hide());
        std::println("secondary.Show: {}", secondary.Show());
    }

    if (!exerciseState)
    {
        std::println("Skipping intrusive state changes; pass --exercise-state to try them.");
        return;
    }

    std::println("primary.SetAlwaysOnTop(true): {}", primary.SetAlwaysOnTop(true));
    std::println("primary.SetAlwaysOnTop(false): {}", primary.SetAlwaysOnTop(false));
    std::println("primary.SetDecoration(Borderless): {}", primary.SetDecoration(platform::WindowDecoration::Borderless));
    std::println("primary.SetDecoration(System): {}", primary.SetDecoration(platform::WindowDecoration::System));
    std::println("primary.Maximize: {}", primary.Maximize());
    std::println("primary.Restore: {}", primary.Restore());
    std::println("primary.Minimize: {}", primary.Minimize());
    std::println("primary.Restore: {}", primary.Restore());
    std::println("primary.SetPresentation(DesktopFullscreen): {}", primary.SetPresentation(platform::WindowPresentation::DesktopFullscreen));
    std::println("primary.SetPresentation(Windowed): {}", primary.SetPresentation(platform::WindowPresentation::Windowed));
}

[[nodiscard]] WindowSlot* FindWindow(std::vector<WindowSlot>& windows, platform::WindowId id)
{
    const auto found = std::ranges::find_if(windows, [id](const WindowSlot& slot) {
        return slot.window.GetId() == id;
    });
    if (found == windows.end())
    {
        return nullptr;
    }
    return &*found;
}

void ReleaseWindow(std::vector<WindowSlot>& windows, platform::WindowId id)
{
    const auto originalSize = windows.size();
    std::erase_if(windows, [id](const WindowSlot& slot) { return slot.window.GetId() == id; });
    if (windows.size() != originalSize)
    {
        std::println("Released application-owned window id {}", id);
    }
}

void UpdateWindowTitles(AppState& state)
{
    const auto elapsed = state.runtime.Now() - state.startTimestamp;
    for (WindowSlot& slot : state.windows)
    {
        const std::string title = std::format("{} | events {} | {}", slot.label, state.eventCount,
            elapsed);
        auto result = slot.window.SetTitle(title);
        if (!result && !slot.titleUpdateFailureReported)
        {
            std::println("SetTitle during title update failed: {}", result.GetError());
            slot.titleUpdateFailureReported = true;
        }
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
        state.quitRequested = true;
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::println(" window={}", event.windowId);
        if (FindWindow(state.windows, event.windowId) == nullptr)
        {
            std::println("  Close request did not match an owned window.");
            return;
        }
        ReleaseWindow(state.windows, event.windowId);
    }

    void operator()(const platform::WindowMovedEvent& event) const
    {
        PrintEventHeader("WindowMoved", event.timestamp, state);
        std::println(" window={} position={}", event.windowId, event.position);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowLogicalSizeChangedEvent& event) const
    {
        PrintEventHeader("WindowLogicalSizeChanged", event.timestamp, state);
        std::println(" window={} logical={}", event.windowId, event.logicalSize);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowPixelSizeChangedEvent& event) const
    {
        PrintEventHeader("WindowPixelSizeChanged", event.timestamp, state);
        std::println(" window={} pixels={}", event.windowId, event.pixelSize);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowFocusChangedEvent& event) const
    {
        PrintEventHeader("WindowFocusChanged", event.timestamp, state);
        std::println(" window={} focused={}", event.windowId, event.focused);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowVisibilityChangedEvent& event) const
    {
        PrintEventHeader("WindowVisibilityChanged", event.timestamp, state);
        std::println(" window={} visible={}", event.windowId, event.visible);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowStateChangedEvent& event) const
    {
        PrintEventHeader("WindowStateChanged", event.timestamp, state);
        std::println(" window={} state={}", event.windowId, event.state);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowPresentationChangedEvent& event) const
    {
        PrintEventHeader("WindowPresentationChanged", event.timestamp, state);
        std::println(" window={} presentation={}", event.windowId, event.presentation);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowDisplayChangedEvent& event) const
    {
        PrintEventHeader("WindowDisplayChanged", event.timestamp, state);
        std::println(" window={} display={}", event.windowId,
            OptionalDisplayId{event.displayId});
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowDisplayScaleChangedEvent& event) const
    {
        PrintEventHeader("WindowDisplayScaleChanged", event.timestamp, state);
        std::println(" window={}", event.windowId);
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowPointerEnteredEvent& event) const
    {
        PrintEventHeader("WindowPointerEntered", event.timestamp, state);
        std::println(" window={}", event.windowId);
    }

    void operator()(const platform::WindowPointerLeftEvent& event) const
    {
        PrintEventHeader("WindowPointerLeft", event.timestamp, state);
        std::println(" window={}", event.windowId);
    }

    void operator()(const platform::DisplayAddedEvent& event) const
    {
        PrintEventHeader("DisplayAdded", event.timestamp, state);
        std::println(" display={}", event.displayId);
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayRemovedEvent& event) const
    {
        PrintEventHeader("DisplayRemoved", event.timestamp, state);
        std::println(" display={}", event.displayId);
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayMovedEvent& event) const
    {
        PrintEventHeader("DisplayMoved", event.timestamp, state);
        std::println(" display={}", event.displayId);
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayDesktopModeChangedEvent& event) const
    {
        PrintEventHeader("DisplayDesktopModeChanged", event.timestamp, state);
        if (event.extent)
        {
            std::println(" display={} extent={}", event.displayId, *event.extent);
        }
        else
        {
            std::println(" display={} extent=none", event.displayId);
        }
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayCurrentModeChangedEvent& event) const
    {
        PrintEventHeader("DisplayCurrentModeChanged", event.timestamp, state);
        if (event.extent)
        {
            std::println(" display={} extent={}", event.displayId, *event.extent);
        }
        else
        {
            std::println(" display={} extent=none", event.displayId);
        }
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayOrientationChangedEvent& event) const
    {
        PrintEventHeader("DisplayOrientationChanged", event.timestamp, state);
        std::println(" display={} orientation={}", event.displayId, event.orientation);
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayContentScaleChangedEvent& event) const
    {
        PrintEventHeader("DisplayContentScaleChanged", event.timestamp, state);
        std::println(" display={}", event.displayId);
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayUsableBoundsChangedEvent& event) const
    {
        PrintEventHeader("DisplayUsableBoundsChanged", event.timestamp, state);
        std::println(" display={}", event.displayId);
        state.snapshotRequested = true;
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Non-window/display event", event.timestamp, state);
        std::println(" forwarded to later platform examples.");
    }
};

void DrainEvents(AppState& state)
{
    while (std::optional<platform::PlatformEvent> event = state.runtime.PollEvent())
    {
        ++state.eventCount;
        std::visit(EventVisitor{state}, *event);
    }

    if (state.snapshotRequested)
    {
        state.snapshotRequested = false;
        PrintDisplays(state.runtime);
        PrintAllWindowSnapshots(state.windows);
    }
}

[[nodiscard]] core::VoidResult RunWindowDisplayLab(int argc, char** argv)
{
    auto optionsResult = ParseOptions(argc, argv);
    RETURN_ERROR_IF_FAILED(optionsResult);

    const Options options = std::move(optionsResult).GetValue();
    if (options.showHelp)
    {
        PrintUsage(argc > 0 ? argv[0] : "ponder-platform-1-window-display-lab");
        return {};
    }

    const platform::PlatformRuntimeDesc runtimeDesc{
        .applicationName = "Ponder Platform Window Display Lab",
        .applicationVersion = std::string{"0.1.0"},
        .applicationIdentifier = std::string{"org.ponder.examples.platform.window-display-lab"},
    };

    auto runtimeResult = platform::PlatformRuntime::Create(runtimeDesc);
    RETURN_ERROR_IF_FAILED(runtimeResult);

    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    const platform::Timestamp start = runtime.Now();
    std::println("Platform runtime created at {}", start);
    DemonstrateRuntimeAlreadyActive(runtimeDesc);
    PrintDisplays(runtime);

    std::vector<WindowSlot> windows;
    windows.reserve(2);

    const platform::WindowDesc primaryDesc{
        .title = "Ponder Platform Lab - primary",
        .logicalSize = {900, 600},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .minimumLogicalSize = platform::LogicalSize{320, 240},
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };
    auto primary = CreateWindowSlot(runtime, primaryDesc, "primary");
    RETURN_ERROR_IF_FAILED(primary);
    windows.push_back(std::move(primary).GetValue());

    const auto releasedProbeId = CreateAndReleaseProbeWindow(runtime);

    const platform::WindowDesc secondaryDesc{
        .title = "Ponder Platform Lab - secondary",
        .logicalSize = {520, 360},
        .visible = true,
        .resizable = false,
        .highPixelDensity = true,
        .minimumLogicalSize = platform::LogicalSize{240, 180},
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };
    auto secondary = CreateWindowSlot(runtime, secondaryDesc, "secondary");
    RETURN_ERROR_IF_FAILED(secondary);
    windows.push_back(std::move(secondary).GetValue());

    if (releasedProbeId)
    {
        std::println("Released probe id {}; next live secondary id is {}.",
                     *releasedProbeId, windows.back().window.GetId());
    }

    ApplyBasicWindowTour(windows, options.exerciseState);
    PrintAllWindowSnapshots(windows);

    AppState state{.runtime = runtime, .windows = windows, .startTimestamp = start};
    auto nextTitleUpdate = start;

    std::println("\nEvent loop started. Close all windows to exit.");
    while (!state.quitRequested && !state.windows.empty())
    {
        DrainEvents(state);

        const platform::Timestamp now = runtime.Now();
        if (now - nextTitleUpdate >= std::chrono::milliseconds{500})
        {
            UpdateWindowTitles(state);
            nextTitleUpdate = now;
        }

        if (options.autoCloseAfter && now - start >= *options.autoCloseAfter)
        {
            std::println("Auto-close duration reached after {}.", now - start);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{16});
    }

    std::println("Shutting down with {} live window owner(s).", state.windows.size());
    state.windows.clear();
    return {};
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto result = RunWindowDisplayLab(argc, argv);
        if (!result)
        {
            std::println(stderr, "ponder-platform-1-window-display-lab failed: {}",
                         result.GetError());
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::println(stderr,
                     "ponder-platform-1-window-display-lab terminated with an exception: {}",
                     exception.what());
        return 1;
    }

    return 0;
}
