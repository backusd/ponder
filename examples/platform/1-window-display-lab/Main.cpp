#include <ponder/core/Exception.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/Runtime.hpp>

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
    const std::optional<ponder::platform::DisplayId>& value;
};

struct OptionalRefreshRate final
{
    const std::optional<float>& value;
};
} // namespace

namespace std
{
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
namespace core = ponder::core;
namespace platform = ponder::platform;

struct Options final
{
    std::optional<core::Duration> autoCloseAfter;
    bool exerciseState{};
    bool showHelp{};
};

struct WindowSlot final
{
    platform::Window window;
    std::string label;
};

struct AppState final
{
    platform::Runtime& runtime;
    std::vector<WindowSlot>& windows;
    core::Timestamp startTimestamp;
    std::uint64_t eventCount{};
    bool quitRequested{};
    bool snapshotRequested{};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0}, std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::println("Usage: {} [--auto-close-ms <milliseconds>] [--exercise-state]", executableName);
    std::println("\nOptions:");
    std::println("  --auto-close-ms <milliseconds>  Exit automatically after a short run.");
    std::println("  --exercise-state                Also try intrusive window state changes.");
    std::println("  --help                          Print this help text.\n");
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
        else if (argument == "--exercise-state")
        {
            options.exerciseState = true;
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
        else
        {
            return ResultType::FromError(MakeOptionError("Unknown option: " + std::string{argument}));
        }
    }

    return options;
}

void PrintError(std::string_view operation, const core::Error& error)
{
    std::println("{} failed: {}", operation, error);
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

void PrintDisplays(platform::Runtime& runtime)
{
    std::println("\nDisplay snapshot");
    const std::vector<platform::DisplayInfo> displays = runtime.DisplayEnumerate();

    if (displays.empty())
    {
        std::println("  No connected displays were reported.");
        return;
    }

    for (const platform::DisplayInfo& display : displays)
    {
        PrintDisplayInfo(display);

        auto lookup = runtime.DisplayGetInfo(display.id);
        if (!lookup)
        {
            PrintError("Runtime::DisplayGetInfo", lookup.GetError());
            std::println("  Display {} left the topology before it could be refreshed; "
                         "continuing with the remaining snapshot.",
                         display.id);
            continue;
        }
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

    auto displayId = window.GetDisplayId();
    if (displayId)
    {
        std::println("  display id: {}", displayId.GetValue());
    }
    else
    {
        std::println("  display id: unavailable after a topology change: {}", displayId.GetError());
    }

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

[[nodiscard]] WindowSlot CreateWindowSlot(platform::Runtime& runtime, const platform::WindowDesc& desc, std::string label)
{
    return WindowSlot{runtime.WindowCreate(desc), std::move(label)};
}

[[nodiscard]] platform::WindowId CreateAndReleaseProbeWindow(platform::Runtime& runtime)
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

    platform::Window probe = runtime.WindowCreate(desc);
    const platform::WindowId id = probe.GetId();
    std::println("Created probe window id {} and releasing it immediately.", id);
    return id;
}

void DemonstrateRuntimeAlreadyActive(const platform::RuntimeDesc& desc)
{
    try
    {
        [[maybe_unused]] auto duplicateRuntime = platform::Runtime::Create(desc);
        std::println("Unexpectedly created a second runtime; releasing it immediately.");
    }
    catch (const core::Exception& exception)
    {
        std::println("Observed expected second-runtime exception: {}", exception.GetMessage());
    }
}

void ApplyBasicWindowTour(std::vector<WindowSlot>& windows, bool exerciseState)
{
    if (windows.empty())
    {
        return;
    }

    platform::Window& primary = windows.front().window;

    primary.SetTitle("Ponder Platform Lab - primary");
    std::println("primary.SetTitle succeeded.");
    primary.SetLogicalSize({960, 640});
    std::println("primary.SetLogicalSize succeeded.");
    primary.SetPosition({80, 80});
    std::println("primary.SetPosition succeeded.");
    primary.SetPresentation(platform::WindowPresentation::Windowed);
    std::println("primary.SetPresentation(Windowed) succeeded.");
    primary.SetDecoration(platform::WindowDecoration::System);
    std::println("primary.SetDecoration(System) succeeded.");
    primary.Restore();
    std::println("primary.Restore succeeded.");

    if (windows.size() > 1U)
    {
        platform::Window& secondary = windows[1].window;
        secondary.SetTitle("Ponder Platform Lab - secondary");
        std::println("secondary.SetTitle succeeded.");
        secondary.SetResizable(true);
        std::println("secondary.SetResizable(true) succeeded.");
        secondary.Hide();
        std::println("secondary.Hide succeeded.");
        secondary.Show();
        std::println("secondary.Show succeeded.");
    }

    if (!exerciseState)
    {
        std::println("Skipping intrusive state changes; pass --exercise-state to try them.");
        return;
    }

    if (windows.size() > 1U)
    {
        platform::Window& secondary = windows[1].window;
        secondary.SetResizable(false);
        std::println("secondary.SetResizable(false) succeeded.");
        secondary.SetResizable(true);
        std::println("secondary.SetResizable(true) succeeded.");
    }

    primary.SetAlwaysOnTop(true);
    std::println("primary.SetAlwaysOnTop(true) succeeded.");
    primary.SetAlwaysOnTop(false);
    std::println("primary.SetAlwaysOnTop(false) succeeded.");
    primary.SetDecoration(platform::WindowDecoration::Borderless);
    std::println("primary.SetDecoration(Borderless) succeeded.");
    primary.SetDecoration(platform::WindowDecoration::System);
    std::println("primary.SetDecoration(System) succeeded.");
    primary.Maximize();
    std::println("primary.Maximize succeeded.");
    primary.Restore();
    std::println("primary.Restore succeeded.");
    primary.Minimize();
    std::println("primary.Minimize succeeded.");
    primary.Restore();
    std::println("primary.Restore succeeded.");
    primary.SetPresentation(platform::WindowPresentation::DesktopFullscreen);
    std::println("primary.SetPresentation(DesktopFullscreen) succeeded.");
    primary.SetPresentation(platform::WindowPresentation::Windowed);
    std::println("primary.SetPresentation(Windowed) succeeded.");
}

[[nodiscard]] WindowSlot* FindWindow(std::vector<WindowSlot>& windows, platform::WindowId id)
{
    const auto found = std::ranges::find_if(windows,
                                            [id](const WindowSlot& slot)
                                            {
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
    std::erase_if(windows,
                  [id](const WindowSlot& slot)
                  {
                      return slot.window.GetId() == id;
                  });
    if (windows.size() != originalSize)
    {
        std::println("Released application-owned window id {}", id);
    }
}

void UpdateWindowTitles(AppState& state)
{
    const auto elapsed = state.runtime.TimeNow() - state.startTimestamp;
    for (WindowSlot& slot : state.windows)
    {
        const std::string title = std::format("{} | events {} | {}", slot.label, state.eventCount, elapsed);
        slot.window.SetTitle(title);
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
        std::println(" window={} display={}", event.windowId, OptionalDisplayId{event.displayId});
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
    while (std::optional<platform::PlatformEvent> event = state.runtime.EventPoll())
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

[[nodiscard]] int RunWindowDisplayLab(int argc, char** argv)
{
    auto optionsResult = ParseOptions(argc, argv);
    if (!optionsResult)
    {
        std::println(stderr, "ponder-platform-1-window-display-lab failed: {}", optionsResult.GetError());
        return 1;
    }

    const Options options = std::move(optionsResult).GetValue();
    if (options.showHelp)
    {
        PrintUsage(argc > 0 ? argv[0] : "ponder-platform-1-window-display-lab");
        return 0;
    }

    const platform::RuntimeDesc runtimeDesc{
        .applicationName = "Ponder Platform Window Display Lab",
        .applicationVersion = std::string{"0.1.0"},
        .applicationIdentifier = std::string{"org.ponder.examples.platform.window-display-lab"},
        .configureHintsBeforeInitialization =
            [](platform::Runtime& runtime)
        {
            runtime.HintPush<platform::hints::MouseFocusClickThrough>(platform::hints::MouseFocusClickThrough{true});
            runtime.HintPush<platform::hints::MouseAutoCapture>(platform::hints::MouseAutoCapture{false});
        },
    };

    platform::Runtime runtime = platform::Runtime::Create(runtimeDesc);
    const core::Timestamp start = runtime.TimeNow();
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
    windows.push_back(CreateWindowSlot(runtime, primaryDesc, "primary"));

    const auto releasedProbeId = CreateAndReleaseProbeWindow(runtime);

    const platform::WindowDesc secondaryDesc{
        .title = "Ponder Platform Lab - secondary",
        .logicalSize = {520, 360},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .minimumLogicalSize = platform::LogicalSize{240, 180},
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };
    windows.push_back(CreateWindowSlot(runtime, secondaryDesc, "secondary"));

    std::println("Released probe id {}; next live secondary id is {}.", releasedProbeId, windows.back().window.GetId());

    ApplyBasicWindowTour(windows, options.exerciseState);
    PrintAllWindowSnapshots(windows);

    AppState state{.runtime = runtime, .windows = windows, .startTimestamp = start};
    auto nextTitleUpdate = start;

    std::println("\nEvent loop started. Close all windows to exit.");
    while (!state.quitRequested && !state.windows.empty())
    {
        DrainEvents(state);

        const core::Timestamp now = runtime.TimeNow();
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
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        return RunWindowDisplayLab(argc, argv);
    }
    catch (const core::Exception& exception)
    {
        std::println(stderr, "ponder-platform-1-window-display-lab terminated with a ponder exception: {}", exception.GetMessage());
        return 1;
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "ponder-platform-1-window-display-lab terminated with an exception: {}", exception.what());
        return 1;
    }
}
