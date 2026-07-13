#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
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
namespace platform = pond::platform;

struct Options final
{
    std::optional<platform::PlatformTimestamp::Duration> autoCloseAfter;
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
    platform::PlatformTimestamp startTimestamp;
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
    std::cout << "Usage: " << executableName << " [--auto-close-ms <milliseconds>] "
              << "[--exercise-state]\n\n"
              << "Options:\n"
              << "  --auto-close-ms <milliseconds>  Exit automatically after a short run.\n"
              << "  --exercise-state                Also try intrusive window state changes.\n"
              << "  --help                          Print this help text.\n";
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
        else if (argument == "--exercise-state")
        {
            options.exerciseState = true;
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

template <typename Id>
[[nodiscard]] std::string FormatOptionalId(const std::optional<Id>& id)
{
    if (!id)
    {
        return "none";
    }
    return FormatId(*id);
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

[[nodiscard]] std::string FormatScreenPosition(platform::ScreenPosition position)
{
    return "(" + std::to_string(position.x) + ", " + std::to_string(position.y) + ")";
}

[[nodiscard]] std::string FormatScreenExtent(platform::ScreenExtent extent)
{
    return std::to_string(extent.width) + " x " + std::to_string(extent.height);
}

[[nodiscard]] std::string FormatScreenRectangle(platform::ScreenRectangle rectangle)
{
    return FormatScreenPosition(rectangle.position) + " / " + FormatScreenExtent(rectangle.extent);
}

[[nodiscard]] std::string FormatLogicalSize(platform::LogicalSize size)
{
    return std::to_string(size.width) + " x " + std::to_string(size.height);
}

[[nodiscard]] std::string FormatPixelSize(platform::PixelSize size)
{
    return std::to_string(size.width) + " x " + std::to_string(size.height);
}

[[nodiscard]] std::string FormatFloat(float value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

[[nodiscard]] std::string FormatOptionalRefreshRate(const std::optional<float>& refreshRateHertz)
{
    if (!refreshRateHertz)
    {
        return "unavailable";
    }
    return FormatFloat(*refreshRateHertz) + " Hz";
}

[[nodiscard]] std::string ToString(platform::DisplayOrientation orientation)
{
    switch (orientation)
    {
    case platform::DisplayOrientation::Unknown:
        return "unknown";
    case platform::DisplayOrientation::Landscape:
        return "landscape";
    case platform::DisplayOrientation::LandscapeFlipped:
        return "landscape-flipped";
    case platform::DisplayOrientation::Portrait:
        return "portrait";
    case platform::DisplayOrientation::PortraitFlipped:
        return "portrait-flipped";
    }
    return "unrecognized";
}

[[nodiscard]] std::string ToString(platform::WindowGraphicsCompatibility compatibility)
{
    switch (compatibility)
    {
    case platform::WindowGraphicsCompatibility::Default:
        return "default";
    case platform::WindowGraphicsCompatibility::Vulkan:
        return "vulkan";
    }
    return "unrecognized";
}

[[nodiscard]] std::string ToString(platform::WindowPresentation presentation)
{
    switch (presentation)
    {
    case platform::WindowPresentation::Windowed:
        return "windowed";
    case platform::WindowPresentation::DesktopFullscreen:
        return "desktop-fullscreen";
    }
    return "unrecognized";
}

[[nodiscard]] std::string ToString(platform::WindowDecoration decoration)
{
    switch (decoration)
    {
    case platform::WindowDecoration::System:
        return "system";
    case platform::WindowDecoration::Borderless:
        return "borderless";
    }
    return "unrecognized";
}

[[nodiscard]] std::string ToString(platform::WindowState state)
{
    switch (state)
    {
    case platform::WindowState::Normal:
        return "normal";
    case platform::WindowState::Minimized:
        return "minimized";
    case platform::WindowState::Maximized:
        return "maximized";
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

    PrintError(operation, result.GetError());
}

template <typename Value, typename Formatter>
void PrintQueryResult(std::string_view label, const core::Result<Value>& result,
                      Formatter formatter)
{
    std::cout << "  " << label << ": ";
    if (result)
    {
        std::cout << formatter(result.GetValue()) << '\n';
        return;
    }

    std::cout << "error: " << core::FormatError(result.GetError()) << '\n';
}

void PrintDisplayInfo(const platform::DisplayInfo& display)
{
    std::cout << "Display " << FormatId(display.id) << "\n"
              << "  name: " << display.name << "\n"
              << "  bounds: " << FormatScreenRectangle(display.bounds) << "\n"
              << "  usable bounds: " << FormatScreenRectangle(display.usableBounds) << "\n"
              << "  refresh: " << FormatOptionalRefreshRate(display.refreshRateHertz) << "\n"
              << "  orientation: " << ToString(display.orientation) << "\n"
              << "  content scale: " << FormatFloat(display.contentScale) << "\n";
}

void PrintDisplays(platform::PlatformRuntime& runtime)
{
    std::cout << "\nDisplay snapshot\n";
    auto displays = runtime.EnumerateDisplays();
    if (!displays)
    {
        PrintError("PlatformRuntime::EnumerateDisplays", displays.GetError());
        return;
    }

    if (displays.GetValue().empty())
    {
        std::cout << "  No connected displays were reported.\n";
        return;
    }

    for (const platform::DisplayInfo& display : displays.GetValue())
    {
        PrintDisplayInfo(display);

        auto lookup = runtime.GetDisplayInfo(display.id);
        if (!lookup)
        {
            PrintError("PlatformRuntime::GetDisplayInfo", lookup.GetError());
        }
    }
}

void PrintWindowSnapshot(const WindowSlot& slot)
{
    const platform::Window& window = slot.window;
    std::cout << "\nWindow snapshot: " << slot.label << " (id " << FormatId(window.GetId())
              << ")\n"
              << "  title: " << window.GetTitle() << "\n"
              << "  graphics compatibility: " << ToString(window.GetGraphicsCompatibility()) << "\n"
              << "  mouse grabbed: " << window.IsMouseGrabbed() << "\n"
              << "  relative mouse mode: " << window.IsRelativeMouseModeEnabled() << "\n";

    PrintQueryResult("position", window.GetPosition(), FormatScreenPosition);
    PrintQueryResult("logical size", window.GetLogicalSize(), FormatLogicalSize);
    PrintQueryResult("pixel size", window.GetPixelSize(), FormatPixelSize);
    PrintQueryResult("display id", window.GetDisplayId(), FormatId<platform::DisplayId>);
    PrintQueryResult("pixel density", window.GetPixelDensity(), FormatFloat);
    PrintQueryResult("display scale", window.GetDisplayScale(), FormatFloat);
    PrintQueryResult("presentation", window.GetPresentation(),
                     [](platform::WindowPresentation value) { return ToString(value); });
    PrintQueryResult("decoration", window.GetDecoration(),
                     [](platform::WindowDecoration value) { return ToString(value); });
    PrintQueryResult("state", window.GetState(),
                     [](platform::WindowState value) { return ToString(value); });
    PrintQueryResult("visible", window.IsVisible(),
                     [](bool value) { return value ? "true" : "false"; });
    PrintQueryResult("resizable", window.IsResizable(),
                     [](bool value) { return value ? "true" : "false"; });
    PrintQueryResult("focused", window.IsFocused(),
                     [](bool value) { return value ? "true" : "false"; });
    PrintQueryResult("always on top", window.IsAlwaysOnTop(),
                     [](bool value) { return value ? "true" : "false"; });
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
    if (!window)
    {
        return core::Result<WindowSlot>::FromError(std::move(window).GetError());
    }

    return WindowSlot{std::move(window).GetValue(), std::move(label)};
}

[[nodiscard]] std::optional<platform::WindowId> CreateAndReleaseProbeWindow(
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
    if (!probe)
    {
        PrintError("probe CreateWindow", probe.GetError());
        return std::nullopt;
    }

    const platform::WindowId id = probe->GetId();
    std::cout << "Created released probe window id " << FormatId(id)
              << "; it will leave scope now.\n";
    return id;
}

void DemonstrateRuntimeAlreadyActive(const platform::PlatformRuntimeDesc& desc)
{
    auto duplicateRuntime = platform::PlatformRuntime::Create(desc);
    if (!duplicateRuntime)
    {
        const core::Error& error = duplicateRuntime.GetError();
        if (IsPlatformError(error, platform::PlatformErrorCode::RuntimeAlreadyActive))
        {
            std::cout << "Observed expected RuntimeAlreadyActive error: "
                      << core::FormatError(error) << '\n';
            return;
        }

        PrintError("duplicate PlatformRuntime::Create", error);
        return;
    }

    std::cout << "Unexpectedly created a second runtime; releasing it immediately.\n";
}

void ApplyBasicWindowTour(std::vector<WindowSlot>& windows, bool exerciseState)
{
    if (windows.empty())
    {
        return;
    }

    platform::Window& primary = windows.front().window;
    PrintOperationResult("primary.SetTitle", primary.SetTitle("Ponder Platform Lab - primary"));
    PrintOperationResult("primary.SetLogicalSize", primary.SetLogicalSize({960, 640}));
    PrintOperationResult("primary.SetPosition", primary.SetPosition({80, 80}));
    PrintOperationResult("primary.SetPresentation(Windowed)",
                         primary.SetPresentation(platform::WindowPresentation::Windowed));
    PrintOperationResult("primary.SetDecoration(System)",
                         primary.SetDecoration(platform::WindowDecoration::System));
    PrintOperationResult("primary.Restore", primary.Restore());

    if (windows.size() > 1U)
    {
        platform::Window& secondary = windows[1].window;
        PrintOperationResult("secondary.SetTitle",
                             secondary.SetTitle("Ponder Platform Lab - secondary"));
        PrintOperationResult("secondary.SetResizable(false)", secondary.SetResizable(false));
        PrintOperationResult("secondary.SetResizable(true)", secondary.SetResizable(true));
        PrintOperationResult("secondary.Hide", secondary.Hide());
        PrintOperationResult("secondary.Show", secondary.Show());
    }

    if (!exerciseState)
    {
        std::cout << "Skipping intrusive state changes; pass --exercise-state to try them.\n";
        return;
    }

    PrintOperationResult("primary.SetAlwaysOnTop(true)", primary.SetAlwaysOnTop(true));
    PrintOperationResult("primary.SetAlwaysOnTop(false)", primary.SetAlwaysOnTop(false));
    PrintOperationResult("primary.SetDecoration(Borderless)",
                         primary.SetDecoration(platform::WindowDecoration::Borderless));
    PrintOperationResult("primary.SetDecoration(System)",
                         primary.SetDecoration(platform::WindowDecoration::System));
    PrintOperationResult("primary.Maximize", primary.Maximize());
    PrintOperationResult("primary.Restore", primary.Restore());
    PrintOperationResult("primary.Minimize", primary.Minimize());
    PrintOperationResult("primary.Restore", primary.Restore());
    PrintOperationResult("primary.SetPresentation(DesktopFullscreen)",
                         primary.SetPresentation(platform::WindowPresentation::DesktopFullscreen));
    PrintOperationResult("primary.SetPresentation(Windowed)",
                         primary.SetPresentation(platform::WindowPresentation::Windowed));
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
        std::cout << "Released application-owned window id " << FormatId(id) << ".\n";
    }
}

void UpdateWindowTitles(AppState& state)
{
    const auto elapsed = state.runtime.Now() - state.startTimestamp;
    for (WindowSlot& slot : state.windows)
    {
        const std::string title = slot.label + " | events " + std::to_string(state.eventCount) +
                                  " | " + FormatDuration(elapsed);
        auto result = slot.window.SetTitle(title);
        if (!result && !slot.titleUpdateFailureReported)
        {
            PrintError("SetTitle during title update", result.GetError());
            slot.titleUpdateFailureReported = true;
        }
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
        state.quitRequested = true;
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
        if (FindWindow(state.windows, event.windowId) == nullptr)
        {
            std::cout << "  Close request did not match an owned window.\n";
            return;
        }
        ReleaseWindow(state.windows, event.windowId);
    }

    void operator()(const platform::WindowMovedEvent& event) const
    {
        PrintEventHeader("WindowMoved", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId)
                  << " position=" << FormatScreenPosition(event.position) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowLogicalSizeChangedEvent& event) const
    {
        PrintEventHeader("WindowLogicalSizeChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId)
                  << " logical=" << FormatLogicalSize(event.logicalSize) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowPixelSizeChangedEvent& event) const
    {
        PrintEventHeader("WindowPixelSizeChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId)
                  << " pixels=" << FormatPixelSize(event.pixelSize) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowFocusChangedEvent& event) const
    {
        PrintEventHeader("WindowFocusChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << " focused=" << event.focused
                  << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowVisibilityChangedEvent& event) const
    {
        PrintEventHeader("WindowVisibilityChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << " visible=" << event.visible
                  << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowStateChangedEvent& event) const
    {
        PrintEventHeader("WindowStateChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << " state=" << ToString(event.state)
                  << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowPresentationChangedEvent& event) const
    {
        PrintEventHeader("WindowPresentationChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId)
                  << " presentation=" << ToString(event.presentation) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowDisplayChangedEvent& event) const
    {
        PrintEventHeader("WindowDisplayChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId)
                  << " display=" << FormatOptionalId(event.displayId) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowDisplayScaleChangedEvent& event) const
    {
        PrintEventHeader("WindowDisplayScaleChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::WindowPointerEnteredEvent& event) const
    {
        PrintEventHeader("WindowPointerEntered", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
    }

    void operator()(const platform::WindowPointerLeftEvent& event) const
    {
        PrintEventHeader("WindowPointerLeft", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
    }

    void operator()(const platform::DisplayAddedEvent& event) const
    {
        PrintEventHeader("DisplayAdded", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayRemovedEvent& event) const
    {
        PrintEventHeader("DisplayRemoved", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayMovedEvent& event) const
    {
        PrintEventHeader("DisplayMoved", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayDesktopModeChangedEvent& event) const
    {
        PrintEventHeader("DisplayDesktopModeChanged", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId) << " extent=";
        if (event.extent)
        {
            std::cout << FormatScreenExtent(*event.extent);
        }
        else
        {
            std::cout << "none";
        }
        std::cout << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayCurrentModeChangedEvent& event) const
    {
        PrintEventHeader("DisplayCurrentModeChanged", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId) << " extent=";
        if (event.extent)
        {
            std::cout << FormatScreenExtent(*event.extent);
        }
        else
        {
            std::cout << "none";
        }
        std::cout << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayOrientationChangedEvent& event) const
    {
        PrintEventHeader("DisplayOrientationChanged", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId)
                  << " orientation=" << ToString(event.orientation) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayContentScaleChangedEvent& event) const
    {
        PrintEventHeader("DisplayContentScaleChanged", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId) << '\n';
        state.snapshotRequested = true;
    }

    void operator()(const platform::DisplayUsableBoundsChangedEvent& event) const
    {
        PrintEventHeader("DisplayUsableBoundsChanged", event.timestamp, state);
        std::cout << " display=" << FormatId(event.displayId) << '\n';
        state.snapshotRequested = true;
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Non-window/display event", event.timestamp, state);
        std::cout << " forwarded to later platform examples.\n";
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
    if (!optionsResult)
    {
        return core::VoidResult::FromError(std::move(optionsResult).GetError());
    }

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
    if (!runtimeResult)
    {
        return core::VoidResult::FromError(std::move(runtimeResult).GetError());
    }

    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    const platform::PlatformTimestamp start = runtime.Now();
    std::cout << "Platform runtime created at " << FormatTimestamp(start) << ".\n";
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
    if (!primary)
    {
        return core::VoidResult::FromError(std::move(primary).GetError());
    }
    windows.push_back(std::move(primary).GetValue());

    const std::optional<platform::WindowId> releasedProbeId = CreateAndReleaseProbeWindow(runtime);

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
    if (!secondary)
    {
        return core::VoidResult::FromError(std::move(secondary).GetError());
    }
    windows.push_back(std::move(secondary).GetValue());

    if (releasedProbeId)
    {
        std::cout << "Released probe id " << FormatId(*releasedProbeId)
                  << "; next live secondary id is " << FormatId(windows.back().window.GetId())
                  << ".\n";
    }

    ApplyBasicWindowTour(windows, options.exerciseState);
    PrintAllWindowSnapshots(windows);

    AppState state{.runtime = runtime, .windows = windows, .startTimestamp = start};
    auto nextTitleUpdate = start;

    std::cout << "\nEvent loop started. Close all windows to exit.\n";
    while (!state.quitRequested && !state.windows.empty())
    {
        DrainEvents(state);

        const platform::PlatformTimestamp now = runtime.Now();
        if (now - nextTitleUpdate >= std::chrono::milliseconds{500})
        {
            UpdateWindowTitles(state);
            nextTitleUpdate = now;
        }

        if (options.autoCloseAfter && now - start >= *options.autoCloseAfter)
        {
            std::cout << "Auto-close duration reached after " << FormatDuration(now - start)
                      << ".\n";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{16});
    }

    std::cout << "Shutting down with " << state.windows.size() << " live window owner(s).\n";
    state.windows.clear();
    return {};
}
} // namespace

int main(int argc, char** argv)
{
    std::cout << std::boolalpha;

    try
    {
        const auto result = RunWindowDisplayLab(argc, argv);
        if (!result)
        {
            std::cerr << "ponder-platform-1-window-display-lab failed: "
                      << core::FormatError(result.GetError()) << '\n';
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ponder-platform-1-window-display-lab terminated with an exception: "
                  << exception.what() << '\n';
        return 1;
    }

    return 0;
}
