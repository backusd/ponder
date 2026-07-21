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

#include <concepts>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <variant>

namespace pond::platform
{
struct QuitRequestedEvent final
{
    Timestamp timestamp{};

    [[nodiscard]] friend bool operator==(const QuitRequestedEvent& lhs,
                                         const QuitRequestedEvent& rhs) = default;
};

struct WindowCloseRequestedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowCloseRequestedEvent& lhs,
                                         const WindowCloseRequestedEvent& rhs) = default;
};

struct WindowMovedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    ScreenPosition position;

    [[nodiscard]] friend bool operator==(const WindowMovedEvent& lhs,
                                         const WindowMovedEvent& rhs) = default;
};

struct WindowLogicalSizeChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    LogicalSize logicalSize;

    [[nodiscard]] friend bool operator==(const WindowLogicalSizeChangedEvent& lhs,
                                         const WindowLogicalSizeChangedEvent& rhs) = default;
};

struct WindowPixelSizeChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    PixelSize pixelSize;

    [[nodiscard]] friend bool operator==(const WindowPixelSizeChangedEvent& lhs,
                                         const WindowPixelSizeChangedEvent& rhs) = default;
};

struct WindowFocusChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    bool focused{};

    [[nodiscard]] friend bool operator==(const WindowFocusChangedEvent& lhs,
                                         const WindowFocusChangedEvent& rhs) = default;
};

struct WindowVisibilityChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    bool visible{};

    [[nodiscard]] friend bool operator==(const WindowVisibilityChangedEvent& lhs,
                                         const WindowVisibilityChangedEvent& rhs) = default;
};

struct WindowStateChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    WindowState state{WindowState::Normal};

    [[nodiscard]] friend bool operator==(const WindowStateChangedEvent& lhs,
                                         const WindowStateChangedEvent& rhs) = default;
};

struct WindowPresentationChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    WindowPresentation presentation{WindowPresentation::Windowed};

    [[nodiscard]] friend bool operator==(const WindowPresentationChangedEvent& lhs,
                                         const WindowPresentationChangedEvent& rhs) = default;
};

struct WindowDisplayChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;
    std::optional<DisplayId> displayId;

    [[nodiscard]] friend bool operator==(const WindowDisplayChangedEvent& lhs,
                                         const WindowDisplayChangedEvent& rhs) = default;
};

struct WindowDisplayScaleChangedEvent final
{
    Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowDisplayScaleChangedEvent& lhs,
                                         const WindowDisplayScaleChangedEvent& rhs) = default;
};

struct WindowPointerEnteredEvent final
{
    Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowPointerEnteredEvent& lhs,
                                         const WindowPointerEnteredEvent& rhs) = default;
};

struct WindowPointerLeftEvent final
{
    Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowPointerLeftEvent& lhs,
                                         const WindowPointerLeftEvent& rhs) = default;
};

struct DisplayAddedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayAddedEvent& lhs,
                                         const DisplayAddedEvent& rhs) = default;
};

struct DisplayRemovedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayRemovedEvent& lhs,
                                         const DisplayRemovedEvent& rhs) = default;
};

struct DisplayMovedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayMovedEvent& lhs,
                                         const DisplayMovedEvent& rhs) = default;
};

struct DisplayDesktopModeChangedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;
    std::optional<ScreenExtent> extent;

    [[nodiscard]] friend bool operator==(const DisplayDesktopModeChangedEvent& lhs,
                                         const DisplayDesktopModeChangedEvent& rhs) = default;
};

struct DisplayCurrentModeChangedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;
    std::optional<ScreenExtent> extent;

    [[nodiscard]] friend bool operator==(const DisplayCurrentModeChangedEvent& lhs,
                                         const DisplayCurrentModeChangedEvent& rhs) = default;
};

struct DisplayOrientationChangedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;
    DisplayOrientation orientation{DisplayOrientation::Unknown};

    [[nodiscard]] friend bool operator==(const DisplayOrientationChangedEvent& lhs,
                                         const DisplayOrientationChangedEvent& rhs) = default;
};

struct DisplayContentScaleChangedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayContentScaleChangedEvent& lhs,
                                         const DisplayContentScaleChangedEvent& rhs) = default;
};

struct DisplayUsableBoundsChangedEvent final
{
    Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayUsableBoundsChangedEvent& lhs,
                                         const DisplayUsableBoundsChangedEvent& rhs) = default;
};

struct KeyboardKeyEvent final
{
    Timestamp timestamp{};
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
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;

    [[nodiscard]] friend bool operator==(const TextInputEvent& lhs,
                                         const TextInputEvent& rhs) = default;
};

struct TextCompositionEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;
    std::optional<TextCompositionRange> selection;

    [[nodiscard]] friend bool operator==(const TextCompositionEvent& lhs,
                                         const TextCompositionEvent& rhs) = default;
};

struct MouseMotionEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    LogicalPoint relativeMovement{};

    [[nodiscard]] friend bool operator==(const MouseMotionEvent& lhs,
                                         const MouseMotionEvent& rhs) = default;
};

struct MouseButtonEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    MouseButton button{MouseButton::Unknown};
    bool pressed{};

    [[nodiscard]] friend bool operator==(const MouseButtonEvent& lhs,
                                         const MouseButtonEvent& rhs) = default;
};

struct MouseWheelEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    float horizontal{};
    float vertical{};

    [[nodiscard]] friend bool operator==(const MouseWheelEvent& lhs,
                                         const MouseWheelEvent& rhs) = default;
};

struct DropBeginEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropBeginEvent& lhs,
                                         const DropBeginEvent& rhs) = default;
};

struct DroppedFileEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::filesystem::path path;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DroppedFileEvent& lhs,
                                         const DroppedFileEvent& rhs) = default;
};

struct DroppedTextEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DroppedTextEvent& lhs,
                                         const DroppedTextEvent& rhs) = default;
};

struct DropPositionEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropPositionEvent& lhs,
                                         const DropPositionEvent& rhs) = default;
};

struct DropCompleteEvent final
{
    Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropCompleteEvent& lhs,
                                         const DropCompleteEvent& rhs) = default;
};

struct DialogCompletedEvent final
{
    Timestamp timestamp{};
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

namespace std
{
template <>
struct formatter<pond::platform::QuitRequestedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::QuitRequestedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("quit_requested(timestamp={})", event.timestamp), context);
    }
};

template <>
struct formatter<pond::platform::WindowCloseRequestedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowCloseRequestedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_close_requested(timestamp={}, window={})", event.timestamp,
                        event.windowId),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowMovedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowMovedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_moved(timestamp={}, window={}, position={})", event.timestamp,
                        event.windowId, event.position),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowLogicalSizeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowLogicalSizeChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_logical_size_changed(timestamp={}, window={}, size={})",
                        event.timestamp, event.windowId, event.logicalSize),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowPixelSizeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowPixelSizeChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_pixel_size_changed(timestamp={}, window={}, size={})",
                        event.timestamp, event.windowId, event.pixelSize),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowFocusChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowFocusChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_focus_changed(timestamp={}, window={}, focused={})",
                        event.timestamp, event.windowId, event.focused),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowVisibilityChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowVisibilityChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_visibility_changed(timestamp={}, window={}, visible={})",
                        event.timestamp, event.windowId, event.visible),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowStateChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowStateChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_state_changed(timestamp={}, window={}, state={})",
                        event.timestamp, event.windowId, event.state),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowPresentationChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowPresentationChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_presentation_changed(timestamp={}, window={}, presentation={})",
                        event.timestamp, event.windowId, event.presentation),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowDisplayChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowDisplayChangedEvent& event,
                FormatContext& context) const
    {
        const string display =
            event.displayId.has_value() ? std::format("{}", *event.displayId) : "none";
        return formatter<string>::format(
            std::format("window_display_changed(timestamp={}, window={}, display={})",
                        event.timestamp, event.windowId, display),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowDisplayScaleChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowDisplayScaleChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_display_scale_changed(timestamp={}, window={})",
                        event.timestamp, event.windowId),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowPointerEnteredEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowPointerEnteredEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_pointer_entered(timestamp={}, window={})", event.timestamp,
                        event.windowId),
            context);
    }
};

template <>
struct formatter<pond::platform::WindowPointerLeftEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowPointerLeftEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_pointer_left(timestamp={}, window={})", event.timestamp,
                        event.windowId),
            context);
    }
};
} // namespace std
namespace std
{
template <>
struct formatter<pond::platform::DisplayAddedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayAddedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("display_added(timestamp={}, display={})", event.timestamp,
                        event.displayId),
            context);
    }
};

template <>
struct formatter<pond::platform::DisplayRemovedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayRemovedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("display_removed(timestamp={}, display={})", event.timestamp,
                        event.displayId),
            context);
    }
};

template <>
struct formatter<pond::platform::DisplayMovedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayMovedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("display_moved(timestamp={}, display={})", event.timestamp,
                        event.displayId),
            context);
    }
};

template <>
struct formatter<pond::platform::DisplayDesktopModeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayDesktopModeChangedEvent& event,
                FormatContext& context) const
    {
        const string extent =
            event.extent.has_value() ? std::format("{}", *event.extent) : "none";
        return formatter<string>::format(
            std::format("display_desktop_mode_changed(timestamp={}, display={}, extent={})",
                        event.timestamp, event.displayId, extent),
            context);
    }
};

template <>
struct formatter<pond::platform::DisplayCurrentModeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayCurrentModeChangedEvent& event,
                FormatContext& context) const
    {
        const string extent =
            event.extent.has_value() ? std::format("{}", *event.extent) : "none";
        return formatter<string>::format(
            std::format("display_current_mode_changed(timestamp={}, display={}, extent={})",
                        event.timestamp, event.displayId, extent),
            context);
    }
};

template <>
struct formatter<pond::platform::DisplayOrientationChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayOrientationChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("display_orientation_changed(timestamp={}, display={}, orientation={})",
                        event.timestamp, event.displayId, event.orientation),
            context);
    }
};

template <>
struct formatter<pond::platform::DisplayContentScaleChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayContentScaleChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("display_content_scale_changed(timestamp={}, display={})",
                        event.timestamp, event.displayId),
            context);
    }
};

template <>
struct formatter<pond::platform::DisplayUsableBoundsChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DisplayUsableBoundsChangedEvent& event,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("display_usable_bounds_changed(timestamp={}, display={})",
                        event.timestamp, event.displayId),
            context);
    }
};
} // namespace std
namespace std
{
template <>
struct formatter<pond::platform::KeyboardKeyEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::KeyboardKeyEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(
            std::format("keyboard_key(timestamp={}, window={}, physical={}, logical={}, "
                        "modifiers={}, pressed={}, repeat={})",
                        event.timestamp, window, event.physicalKey, event.logicalKey,
                        event.modifiers, event.pressed, event.repeat),
            context);
    }
};

template <>
struct formatter<pond::platform::TextInputEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::TextInputEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(
            std::format("text_input(timestamp={}, window={}, text='{}')", event.timestamp,
                        window, event.text),
            context);
    }
};

template <>
struct formatter<pond::platform::TextCompositionEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::TextCompositionEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string selection =
            event.selection.has_value() ? std::format("{}", *event.selection) : "none";
        return formatter<string>::format(
            std::format("text_composition(timestamp={}, window={}, text='{}', selection={})",
                        event.timestamp, window, event.text, selection),
            context);
    }
};

template <>
struct formatter<pond::platform::MouseMotionEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::MouseMotionEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(
            std::format("mouse_motion(timestamp={}, window={}, position={}, relative={})",
                        event.timestamp, window, event.position, event.relativeMovement),
            context);
    }
};

template <>
struct formatter<pond::platform::MouseButtonEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::MouseButtonEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(
            std::format("mouse_button(timestamp={}, window={}, position={}, button={}, "
                        "pressed={})",
                        event.timestamp, window, event.position, event.button, event.pressed),
            context);
    }
};

template <>
struct formatter<pond::platform::MouseWheelEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::MouseWheelEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(
            std::format("mouse_wheel(timestamp={}, window={}, position={}, horizontal={}, "
                        "vertical={})",
                        event.timestamp, window, event.position, event.horizontal,
                        event.vertical),
            context);
    }
};
} // namespace std
namespace std
{
template <>
struct formatter<pond::platform::DropBeginEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DropBeginEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value()
                                  ? std::format("'{}'", *event.sourceApplication)
                                  : "none";
        return formatter<string>::format(
            std::format("drop_begin(timestamp={}, window={}, sourceApplication={})",
                        event.timestamp, window, source),
            context);
    }
};

template <>
struct formatter<pond::platform::DroppedFileEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DroppedFileEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value()
                                  ? std::format("'{}'", *event.sourceApplication)
                                  : "none";
        return formatter<string>::format(
            std::format("dropped_file(timestamp={}, window={}, path='{}', position={}, "
                        "sourceApplication={})",
                        event.timestamp, window, event.path.string(), event.position, source),
            context);
    }
};

template <>
struct formatter<pond::platform::DroppedTextEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DroppedTextEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value()
                                  ? std::format("'{}'", *event.sourceApplication)
                                  : "none";
        return formatter<string>::format(
            std::format("dropped_text(timestamp={}, window={}, text='{}', position={}, "
                        "sourceApplication={})",
                        event.timestamp, window, event.text, event.position, source),
            context);
    }
};

template <>
struct formatter<pond::platform::DropPositionEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DropPositionEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value()
                                  ? std::format("'{}'", *event.sourceApplication)
                                  : "none";
        return formatter<string>::format(
            std::format("drop_position(timestamp={}, window={}, position={}, "
                        "sourceApplication={})",
                        event.timestamp, window, event.position, source),
            context);
    }
};

template <>
struct formatter<pond::platform::DropCompleteEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DropCompleteEvent& event,
                FormatContext& context) const
    {
        const string window =
            event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value()
                                  ? std::format("'{}'", *event.sourceApplication)
                                  : "none";
        return formatter<string>::format(
            std::format("drop_complete(timestamp={}, window={}, position={}, "
                        "sourceApplication={})",
                        event.timestamp, window, event.position, source),
            context);
    }
};

template <>
struct formatter<pond::platform::DialogCompletedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DialogCompletedEvent& event,
                FormatContext& context) const
    {
        const string outcome = std::visit(
            [](const auto& value)
            {
                return std::format("{}", value);
            },
            event.outcome);
        return formatter<string>::format(
            std::format("dialog_completed(timestamp={}, request={}, outcome={})",
                        event.timestamp, event.requestId, outcome),
            context);
    }
};
} // namespace std
namespace pond::platform::detail
{
template <typename Type>
concept PlatformEventPayload =
    std::same_as<std::remove_cvref_t<Type>, QuitRequestedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowCloseRequestedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowMovedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowLogicalSizeChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowPixelSizeChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowFocusChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowVisibilityChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowStateChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowPresentationChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowDisplayChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowDisplayScaleChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowPointerEnteredEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowPointerLeftEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayAddedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayRemovedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayMovedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayDesktopModeChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayCurrentModeChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayOrientationChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayContentScaleChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayUsableBoundsChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, KeyboardKeyEvent> ||
    std::same_as<std::remove_cvref_t<Type>, TextInputEvent> ||
    std::same_as<std::remove_cvref_t<Type>, TextCompositionEvent> ||
    std::same_as<std::remove_cvref_t<Type>, MouseMotionEvent> ||
    std::same_as<std::remove_cvref_t<Type>, MouseButtonEvent> ||
    std::same_as<std::remove_cvref_t<Type>, MouseWheelEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DropBeginEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DroppedFileEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DroppedTextEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DropPositionEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DropCompleteEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DialogCompletedEvent>;
} // namespace pond::platform::detail

namespace pond::platform
{
template <detail::PlatformEventPayload Event>
inline std::ostream& operator<<(std::ostream& output, const Event& event)
{
    return output << std::format("{}", event);
}
} // namespace pond::platform