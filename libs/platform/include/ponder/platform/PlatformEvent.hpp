#pragma once

#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Timing.hpp>
#include <ponder/platform/WindowState.hpp>

#include <optional>
#include <variant>

namespace pond::platform
{
struct QuitRequestedEvent final
{
    PlatformTimestamp timestamp{};

    [[nodiscard]] friend bool operator==(const QuitRequestedEvent& lhs,
                                         const QuitRequestedEvent& rhs) = default;
};

struct WindowCloseRequestedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowCloseRequestedEvent& lhs,
                                         const WindowCloseRequestedEvent& rhs) = default;
};

struct WindowMovedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    ScreenPosition position;

    [[nodiscard]] friend bool operator==(const WindowMovedEvent& lhs,
                                         const WindowMovedEvent& rhs) = default;
};

struct WindowLogicalSizeChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    LogicalSize logicalSize;

    [[nodiscard]] friend bool operator==(
        const WindowLogicalSizeChangedEvent& lhs,
        const WindowLogicalSizeChangedEvent& rhs) = default;
};

struct WindowPixelSizeChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    PixelSize pixelSize;

    [[nodiscard]] friend bool operator==(const WindowPixelSizeChangedEvent& lhs,
                                         const WindowPixelSizeChangedEvent& rhs) = default;
};

struct WindowFocusChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    bool focused{};

    [[nodiscard]] friend bool operator==(const WindowFocusChangedEvent& lhs,
                                         const WindowFocusChangedEvent& rhs) = default;
};

struct WindowVisibilityChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    bool visible{};

    [[nodiscard]] friend bool operator==(
        const WindowVisibilityChangedEvent& lhs,
        const WindowVisibilityChangedEvent& rhs) = default;
};

struct WindowStateChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    WindowState state{WindowState::Normal};

    [[nodiscard]] friend bool operator==(const WindowStateChangedEvent& lhs,
                                         const WindowStateChangedEvent& rhs) = default;
};

struct WindowPresentationChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    WindowPresentation presentation{WindowPresentation::Windowed};

    [[nodiscard]] friend bool operator==(
        const WindowPresentationChangedEvent& lhs,
        const WindowPresentationChangedEvent& rhs) = default;
};

struct WindowDisplayChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;
    std::optional<DisplayId> displayId;

    [[nodiscard]] friend bool operator==(const WindowDisplayChangedEvent& lhs,
                                         const WindowDisplayChangedEvent& rhs) = default;
};

struct WindowDisplayScaleChangedEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(
        const WindowDisplayScaleChangedEvent& lhs,
        const WindowDisplayScaleChangedEvent& rhs) = default;
};

struct WindowPointerEnteredEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowPointerEnteredEvent& lhs,
                                         const WindowPointerEnteredEvent& rhs) = default;
};

struct WindowPointerLeftEvent final
{
    PlatformTimestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowPointerLeftEvent& lhs,
                                         const WindowPointerLeftEvent& rhs) = default;
};

struct DisplayAddedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayAddedEvent& lhs,
                                         const DisplayAddedEvent& rhs) = default;
};

struct DisplayRemovedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayRemovedEvent& lhs,
                                         const DisplayRemovedEvent& rhs) = default;
};

struct DisplayMovedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayMovedEvent& lhs,
                                         const DisplayMovedEvent& rhs) = default;
};

struct DisplayDesktopModeChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;
    std::optional<ScreenExtent> extent;

    [[nodiscard]] friend bool operator==(
        const DisplayDesktopModeChangedEvent& lhs,
        const DisplayDesktopModeChangedEvent& rhs) = default;
};

struct DisplayCurrentModeChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;
    std::optional<ScreenExtent> extent;

    [[nodiscard]] friend bool operator==(
        const DisplayCurrentModeChangedEvent& lhs,
        const DisplayCurrentModeChangedEvent& rhs) = default;
};

struct DisplayOrientationChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;
    DisplayOrientation orientation{DisplayOrientation::Unknown};

    [[nodiscard]] friend bool operator==(
        const DisplayOrientationChangedEvent& lhs,
        const DisplayOrientationChangedEvent& rhs) = default;
};

struct DisplayContentScaleChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(
        const DisplayContentScaleChangedEvent& lhs,
        const DisplayContentScaleChangedEvent& rhs) = default;
};

struct DisplayUsableBoundsChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(
        const DisplayUsableBoundsChangedEvent& lhs,
        const DisplayUsableBoundsChangedEvent& rhs) = default;
};

using PlatformEvent = std::variant<
    QuitRequestedEvent,
    WindowCloseRequestedEvent,
    WindowMovedEvent,
    WindowLogicalSizeChangedEvent,
    WindowPixelSizeChangedEvent,
    WindowFocusChangedEvent,
    WindowVisibilityChangedEvent,
    WindowStateChangedEvent,
    WindowPresentationChangedEvent,
    WindowDisplayChangedEvent,
    WindowDisplayScaleChangedEvent,
    WindowPointerEnteredEvent,
    WindowPointerLeftEvent,
    DisplayAddedEvent,
    DisplayRemovedEvent,
    DisplayMovedEvent,
    DisplayDesktopModeChangedEvent,
    DisplayCurrentModeChangedEvent,
    DisplayOrientationChangedEvent,
    DisplayContentScaleChangedEvent,
    DisplayUsableBoundsChangedEvent>;
} // namespace pond::platform
