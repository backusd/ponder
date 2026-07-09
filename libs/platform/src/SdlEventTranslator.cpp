#include "SdlEventTranslator.hpp"

#include <SDL3/SDL_events.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>

namespace pond::platform::detail
{
namespace
{
[[nodiscard]] std::optional<PlatformTimestamp> TranslateTimestamp(
    std::uint64_t timestamp) noexcept
{
    constexpr auto kMaximumTimestamp =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (timestamp > kMaximumTimestamp)
    {
        return std::nullopt;
    }

    return PlatformTimestamp{
        std::chrono::nanoseconds{static_cast<std::int64_t>(timestamp)}};
}

[[nodiscard]] std::optional<WindowId> ResolveWindowId(
    const SDL_WindowEvent& event, const EventTranslationContext& context)
{
    if (event.windowID == 0 || context.resolveWindowId == nullptr)
    {
        return std::nullopt;
    }

    std::optional<WindowId> id = context.resolveWindowId(
        context.context, static_cast<std::uint32_t>(event.windowID));
    return id.has_value() && id->IsValid() ? id : std::nullopt;
}

[[nodiscard]] std::optional<DisplayId> ResolveRequiredDisplayId(
    std::uint32_t backendDisplayId, const EventTranslationContext& context)
{
    if (backendDisplayId == 0 || context.resolveDisplayId == nullptr)
    {
        return std::nullopt;
    }

    std::optional<DisplayId> id =
        context.resolveDisplayId(context.context, backendDisplayId);
    return id.has_value() && id->IsValid() ? id : std::nullopt;
}

[[nodiscard]] std::optional<DisplayId> ResolveOptionalDisplayId(
    std::uint32_t backendDisplayId, const EventTranslationContext& context)
{
    return ResolveRequiredDisplayId(backendDisplayId, context);
}

[[nodiscard]] std::optional<ScreenExtent> TranslateModeExtent(
    std::int32_t width, std::int32_t height) noexcept
{
    if (width <= 0 || height <= 0)
    {
        return std::nullopt;
    }

    return ScreenExtent{static_cast<std::uint32_t>(width),
                        static_cast<std::uint32_t>(height)};
}

[[nodiscard]] DisplayOrientation TranslateOrientation(std::int32_t orientation) noexcept
{
    switch (orientation)
    {
    case SDL_ORIENTATION_LANDSCAPE:
        return DisplayOrientation::Landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return DisplayOrientation::LandscapeFlipped;
    case SDL_ORIENTATION_PORTRAIT:
        return DisplayOrientation::Portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return DisplayOrientation::PortraitFlipped;
    case SDL_ORIENTATION_UNKNOWN:
        return DisplayOrientation::Unknown;
    }

    return DisplayOrientation::Unknown;
}
} // namespace

std::optional<PlatformEvent> TranslateSdlEvent(
    const SDL_Event& event, const EventTranslationContext& context)
{
    const std::optional<PlatformTimestamp> timestamp =
        TranslateTimestamp(event.common.timestamp);
    if (!timestamp.has_value())
    {
        return std::nullopt;
    }

    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        return PlatformEvent{QuitRequestedEvent{*timestamp}};

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    {
        if ((event.type == SDL_EVENT_WINDOW_RESIZED ||
             event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) &&
            (event.window.data1 < 0 || event.window.data2 < 0))
        {
            return std::nullopt;
        }

        const std::optional<WindowId> windowId = ResolveWindowId(event.window, context);
        if (!windowId.has_value())
        {
            return std::nullopt;
        }

        switch (event.type)
        {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            return PlatformEvent{WindowCloseRequestedEvent{*timestamp, *windowId}};
        case SDL_EVENT_WINDOW_MOVED:
            return PlatformEvent{WindowMovedEvent{
                *timestamp, *windowId,
                ScreenPosition{event.window.data1, event.window.data2}}};
        case SDL_EVENT_WINDOW_RESIZED:
            return PlatformEvent{WindowLogicalSizeChangedEvent{
                *timestamp, *windowId,
                LogicalSize{static_cast<std::uint32_t>(event.window.data1),
                            static_cast<std::uint32_t>(event.window.data2)}}};
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            return PlatformEvent{WindowPixelSizeChangedEvent{
                *timestamp, *windowId,
                PixelSize{static_cast<std::uint32_t>(event.window.data1),
                          static_cast<std::uint32_t>(event.window.data2)}}};
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            return PlatformEvent{WindowFocusChangedEvent{*timestamp, *windowId, true}};
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            return PlatformEvent{WindowFocusChangedEvent{*timestamp, *windowId, false}};
        case SDL_EVENT_WINDOW_SHOWN:
            return PlatformEvent{
                WindowVisibilityChangedEvent{*timestamp, *windowId, true}};
        case SDL_EVENT_WINDOW_HIDDEN:
            return PlatformEvent{
                WindowVisibilityChangedEvent{*timestamp, *windowId, false}};
        case SDL_EVENT_WINDOW_MINIMIZED:
            return PlatformEvent{WindowStateChangedEvent{
                *timestamp, *windowId, WindowState::Minimized}};
        case SDL_EVENT_WINDOW_MAXIMIZED:
            return PlatformEvent{WindowStateChangedEvent{
                *timestamp, *windowId, WindowState::Maximized}};
        case SDL_EVENT_WINDOW_RESTORED:
            return PlatformEvent{
                WindowStateChangedEvent{*timestamp, *windowId, WindowState::Normal}};
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            return PlatformEvent{WindowPresentationChangedEvent{
                *timestamp, *windowId, WindowPresentation::DesktopFullscreen}};
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            return PlatformEvent{WindowPresentationChangedEvent{
                *timestamp, *windowId, WindowPresentation::Windowed}};
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            return PlatformEvent{WindowDisplayChangedEvent{
                *timestamp, *windowId,
                ResolveOptionalDisplayId(
                    static_cast<std::uint32_t>(event.window.data1), context)}};
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            return PlatformEvent{WindowDisplayScaleChangedEvent{*timestamp, *windowId}};
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            return PlatformEvent{WindowPointerEnteredEvent{*timestamp, *windowId}};
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            return PlatformEvent{WindowPointerLeftEvent{*timestamp, *windowId}};
        default:
            return std::nullopt;
        }
    }

    case SDL_EVENT_DISPLAY_ADDED:
    case SDL_EVENT_DISPLAY_REMOVED:
    case SDL_EVENT_DISPLAY_MOVED:
    case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
    case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
    case SDL_EVENT_DISPLAY_ORIENTATION:
    case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    case SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED:
    {
        const std::optional<DisplayId> displayId = ResolveRequiredDisplayId(
            static_cast<std::uint32_t>(event.display.displayID), context);
        if (!displayId.has_value())
        {
            return std::nullopt;
        }

        switch (event.type)
        {
        case SDL_EVENT_DISPLAY_ADDED:
            return PlatformEvent{DisplayAddedEvent{*timestamp, *displayId}};
        case SDL_EVENT_DISPLAY_REMOVED:
            return PlatformEvent{DisplayRemovedEvent{*timestamp, *displayId}};
        case SDL_EVENT_DISPLAY_MOVED:
            return PlatformEvent{DisplayMovedEvent{*timestamp, *displayId}};
        case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
            return PlatformEvent{DisplayDesktopModeChangedEvent{
                *timestamp, *displayId,
                TranslateModeExtent(event.display.data1, event.display.data2)}};
        case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
            return PlatformEvent{DisplayCurrentModeChangedEvent{
                *timestamp, *displayId,
                TranslateModeExtent(event.display.data1, event.display.data2)}};
        case SDL_EVENT_DISPLAY_ORIENTATION:
            return PlatformEvent{DisplayOrientationChangedEvent{
                *timestamp, *displayId,
                TranslateOrientation(event.display.data1)}};
        case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
            return PlatformEvent{DisplayContentScaleChangedEvent{*timestamp, *displayId}};
        case SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED:
            return PlatformEvent{DisplayUsableBoundsChangedEvent{*timestamp, *displayId}};
        default:
            return std::nullopt;
        }
    }

    default:
        return std::nullopt;
    }
}
} // namespace pond::platform::detail
