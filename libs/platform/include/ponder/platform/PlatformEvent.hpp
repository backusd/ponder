#pragma once

#include <ponder/platform/Dialog.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Keyboard.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/TextInput.hpp>
#include <ponder/platform/Timing.hpp>
#include <ponder/platform/WindowState.hpp>

#include <filesystem>
#include <optional>
#include <string>
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

    [[nodiscard]] friend bool operator==(const WindowLogicalSizeChangedEvent& lhs,
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

    [[nodiscard]] friend bool operator==(const WindowVisibilityChangedEvent& lhs,
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

    [[nodiscard]] friend bool operator==(const WindowPresentationChangedEvent& lhs,
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

    [[nodiscard]] friend bool operator==(const WindowDisplayScaleChangedEvent& lhs,
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

    [[nodiscard]] friend bool operator==(const DisplayDesktopModeChangedEvent& lhs,
                                         const DisplayDesktopModeChangedEvent& rhs) = default;
};

struct DisplayCurrentModeChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;
    std::optional<ScreenExtent> extent;

    [[nodiscard]] friend bool operator==(const DisplayCurrentModeChangedEvent& lhs,
                                         const DisplayCurrentModeChangedEvent& rhs) = default;
};

struct DisplayOrientationChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;
    DisplayOrientation orientation{DisplayOrientation::Unknown};

    [[nodiscard]] friend bool operator==(const DisplayOrientationChangedEvent& lhs,
                                         const DisplayOrientationChangedEvent& rhs) = default;
};

struct DisplayContentScaleChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayContentScaleChangedEvent& lhs,
                                         const DisplayContentScaleChangedEvent& rhs) = default;
};

struct DisplayUsableBoundsChangedEvent final
{
    PlatformTimestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayUsableBoundsChangedEvent& lhs,
                                         const DisplayUsableBoundsChangedEvent& rhs) = default;
};

struct KeyboardKeyEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    PhysicalKey physicalKey{PhysicalKey::Unknown};
    LogicalKey logicalKey;
    KeyModifiers modifiers{KeyModifiers::None};
    bool pressed{};
    bool repeat{};

    [[nodiscard]] friend bool operator==(const KeyboardKeyEvent& lhs,
                                         const KeyboardKeyEvent& rhs) = default;
};

struct TextInputEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;

    [[nodiscard]] friend bool operator==(const TextInputEvent& lhs,
                                         const TextInputEvent& rhs) = default;
};

struct TextCompositionEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;
    std::optional<TextCompositionRange> selection;

    [[nodiscard]] friend bool operator==(const TextCompositionEvent& lhs,
                                         const TextCompositionEvent& rhs) = default;
};

struct MouseMotionEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    LogicalPoint relativeMovement{};

    [[nodiscard]] friend bool operator==(const MouseMotionEvent& lhs,
                                         const MouseMotionEvent& rhs) = default;
};

struct MouseButtonEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    MouseButton button{MouseButton::Unknown};
    bool pressed{};

    [[nodiscard]] friend bool operator==(const MouseButtonEvent& lhs,
                                         const MouseButtonEvent& rhs) = default;
};

struct MouseWheelEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    float horizontal{};
    float vertical{};

    [[nodiscard]] friend bool operator==(const MouseWheelEvent& lhs,
                                         const MouseWheelEvent& rhs) = default;
};

struct DropBeginEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropBeginEvent& lhs,
                                         const DropBeginEvent& rhs) = default;
};

struct DroppedFileEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    std::filesystem::path path;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DroppedFileEvent& lhs,
                                         const DroppedFileEvent& rhs) = default;
};

struct DroppedTextEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DroppedTextEvent& lhs,
                                         const DroppedTextEvent& rhs) = default;
};

struct DropPositionEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropPositionEvent& lhs,
                                         const DropPositionEvent& rhs) = default;
};

struct DropCompleteEvent final
{
    PlatformTimestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropCompleteEvent& lhs,
                                         const DropCompleteEvent& rhs) = default;
};

struct DialogCompletedEvent final
{
    PlatformTimestamp timestamp{};
    DialogRequestId requestId;
    DialogOutcome outcome;

    [[nodiscard]] friend bool operator==(const DialogCompletedEvent& lhs,
                                         const DialogCompletedEvent& rhs) = default;
};

using PlatformEvent = std::variant<
    QuitRequestedEvent, WindowCloseRequestedEvent, WindowMovedEvent, WindowLogicalSizeChangedEvent,
    WindowPixelSizeChangedEvent, WindowFocusChangedEvent, WindowVisibilityChangedEvent,
    WindowStateChangedEvent, WindowPresentationChangedEvent, WindowDisplayChangedEvent,
    WindowDisplayScaleChangedEvent, WindowPointerEnteredEvent, WindowPointerLeftEvent,
    DisplayAddedEvent, DisplayRemovedEvent, DisplayMovedEvent, DisplayDesktopModeChangedEvent,
    DisplayCurrentModeChangedEvent, DisplayOrientationChangedEvent, DisplayContentScaleChangedEvent,
    DisplayUsableBoundsChangedEvent, KeyboardKeyEvent, TextInputEvent, TextCompositionEvent,
    MouseMotionEvent, MouseButtonEvent, MouseWheelEvent, DropBeginEvent, DroppedFileEvent,
    DroppedTextEvent, DropPositionEvent, DropCompleteEvent, DialogCompletedEvent>;
} // namespace pond::platform
