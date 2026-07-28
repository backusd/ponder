#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Keyboard.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/TextInput.hpp>
#include <ponder/platform/WindowState.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace ponder::platform
{
enum class DialogKind : std::uint8_t
{
    OpenFile,
    SaveFile,
    OpenFolder
};

struct DialogRequestInfo final
{
    DialogRequestId id;
    DialogKind kind{DialogKind::OpenFile};
    ponder::core::Timestamp requestedAt{};
    std::optional<WindowId> parentWindowId;
    std::size_t filterCount{};
    bool allowMultipleSelection{};

    [[nodiscard]] friend bool operator==(const DialogRequestInfo& lhs, const DialogRequestInfo& rhs) = default;
};

struct DialogSelection final
{
    std::vector<std::filesystem::path> paths;
    std::optional<std::size_t> selectedFilterIndex;

    [[nodiscard]] friend bool operator==(const DialogSelection& lhs, const DialogSelection& rhs) = default;
};

struct DialogCancellation final
{
    [[nodiscard]] friend constexpr bool operator==(DialogCancellation, DialogCancellation) noexcept = default;
};

struct DialogFailure final
{
    ponder::core::Error error;

    [[nodiscard]] friend bool operator==(const DialogFailure& lhs, const DialogFailure& rhs) noexcept
    {
        return lhs.error.GetCode() == rhs.error.GetCode() && lhs.error.GetMessage() == rhs.error.GetMessage();
    }
};

using DialogOutcome = std::variant<DialogSelection, DialogCancellation, DialogFailure>;

struct DialogCompletedEvent final
{
    ponder::core::Timestamp timestamp{};
    DialogRequestInfo request;
    DialogOutcome outcome;

    [[nodiscard]] friend bool operator==(const DialogCompletedEvent& lhs, const DialogCompletedEvent& rhs) = default;
};

struct QuitRequestedEvent final
{
    ponder::core::Timestamp timestamp{};

    [[nodiscard]] friend bool operator==(const QuitRequestedEvent& lhs, const QuitRequestedEvent& rhs) = default;
};

struct WindowCloseRequestedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowCloseRequestedEvent& lhs, const WindowCloseRequestedEvent& rhs) = default;
};

struct WindowMovedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    ScreenPosition position;

    [[nodiscard]] friend bool operator==(const WindowMovedEvent& lhs, const WindowMovedEvent& rhs) = default;
};

struct WindowLogicalSizeChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    LogicalSize logicalSize;

    [[nodiscard]] friend bool operator==(const WindowLogicalSizeChangedEvent& lhs, const WindowLogicalSizeChangedEvent& rhs) = default;
};

struct WindowPixelSizeChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    PixelSize pixelSize;

    [[nodiscard]] friend bool operator==(const WindowPixelSizeChangedEvent& lhs, const WindowPixelSizeChangedEvent& rhs) = default;
};

struct WindowFocusChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    bool focused{};

    [[nodiscard]] friend bool operator==(const WindowFocusChangedEvent& lhs, const WindowFocusChangedEvent& rhs) = default;
};

struct WindowVisibilityChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    bool visible{};

    [[nodiscard]] friend bool operator==(const WindowVisibilityChangedEvent& lhs, const WindowVisibilityChangedEvent& rhs) = default;
};

struct WindowStateChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    WindowState state{WindowState::Normal};

    [[nodiscard]] friend bool operator==(const WindowStateChangedEvent& lhs, const WindowStateChangedEvent& rhs) = default;
};

struct WindowPresentationChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    WindowPresentation presentation{WindowPresentation::Windowed};

    [[nodiscard]] friend bool operator==(const WindowPresentationChangedEvent& lhs, const WindowPresentationChangedEvent& rhs) = default;
};

struct WindowDisplayChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;
    std::optional<DisplayId> displayId;

    [[nodiscard]] friend bool operator==(const WindowDisplayChangedEvent& lhs, const WindowDisplayChangedEvent& rhs) = default;
};

struct WindowDisplayScaleChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowDisplayScaleChangedEvent& lhs, const WindowDisplayScaleChangedEvent& rhs) = default;
};

struct WindowPointerEnteredEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowPointerEnteredEvent& lhs, const WindowPointerEnteredEvent& rhs) = default;
};

struct WindowPointerLeftEvent final
{
    ponder::core::Timestamp timestamp{};
    WindowId windowId;

    [[nodiscard]] friend bool operator==(const WindowPointerLeftEvent& lhs, const WindowPointerLeftEvent& rhs) = default;
};

struct DisplayAddedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayAddedEvent& lhs, const DisplayAddedEvent& rhs) = default;
};

struct DisplayRemovedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayRemovedEvent& lhs, const DisplayRemovedEvent& rhs) = default;
};

struct DisplayMovedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayMovedEvent& lhs, const DisplayMovedEvent& rhs) = default;
};

struct DisplayDesktopModeChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;
    std::optional<ScreenExtent> extent;

    [[nodiscard]] friend bool operator==(const DisplayDesktopModeChangedEvent& lhs, const DisplayDesktopModeChangedEvent& rhs) = default;
};

struct DisplayCurrentModeChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;
    std::optional<ScreenExtent> extent;

    [[nodiscard]] friend bool operator==(const DisplayCurrentModeChangedEvent& lhs, const DisplayCurrentModeChangedEvent& rhs) = default;
};

struct DisplayOrientationChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;
    DisplayOrientation orientation{DisplayOrientation::Unknown};

    [[nodiscard]] friend bool operator==(const DisplayOrientationChangedEvent& lhs, const DisplayOrientationChangedEvent& rhs) = default;
};

struct DisplayContentScaleChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayContentScaleChangedEvent& lhs, const DisplayContentScaleChangedEvent& rhs) = default;
};

struct DisplayUsableBoundsChangedEvent final
{
    ponder::core::Timestamp timestamp{};
    DisplayId displayId;

    [[nodiscard]] friend bool operator==(const DisplayUsableBoundsChangedEvent& lhs, const DisplayUsableBoundsChangedEvent& rhs) = default;
};

struct KeyboardKeyEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    PhysicalKey physicalKey{PhysicalKey::Unknown};
    LogicalKey logicalKey;
    KeyModifiers modifiers{KeyModifiers::None};
    bool pressed{};
    bool repeat{};

    [[nodiscard]] friend bool operator==(const KeyboardKeyEvent& lhs, const KeyboardKeyEvent& rhs) = default;
};

struct TextInputEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;

    [[nodiscard]] friend bool operator==(const TextInputEvent& lhs, const TextInputEvent& rhs) = default;
};

struct TextCompositionEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;
    std::optional<TextCompositionRange> selection;

    [[nodiscard]] friend bool operator==(const TextCompositionEvent& lhs, const TextCompositionEvent& rhs) = default;
};

struct MouseMotionEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    LogicalPoint relativeMovement{};

    [[nodiscard]] friend bool operator==(const MouseMotionEvent& lhs, const MouseMotionEvent& rhs) = default;
};

struct MouseButtonEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    MouseButton button{MouseButton::Unknown};
    bool pressed{};

    [[nodiscard]] friend bool operator==(const MouseButtonEvent& lhs, const MouseButtonEvent& rhs) = default;
};

struct MouseWheelEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    float horizontal{};
    float vertical{};

    [[nodiscard]] friend bool operator==(const MouseWheelEvent& lhs, const MouseWheelEvent& rhs) = default;
};

struct DropBeginEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropBeginEvent& lhs, const DropBeginEvent& rhs) = default;
};

struct DroppedFileEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::filesystem::path path;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DroppedFileEvent& lhs, const DroppedFileEvent& rhs) = default;
};

struct DroppedTextEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    std::string text;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DroppedTextEvent& lhs, const DroppedTextEvent& rhs) = default;
};

struct DropPositionEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropPositionEvent& lhs, const DropPositionEvent& rhs) = default;
};

struct DropCompleteEvent final
{
    ponder::core::Timestamp timestamp{};
    std::optional<WindowId> windowId;
    LogicalPoint position{};
    std::optional<std::string> sourceApplication;

    [[nodiscard]] friend bool operator==(const DropCompleteEvent& lhs, const DropCompleteEvent& rhs) = default;
};

using PlatformEvent =
    std::variant<QuitRequestedEvent, WindowCloseRequestedEvent, WindowMovedEvent, WindowLogicalSizeChangedEvent, WindowPixelSizeChangedEvent,
                 WindowFocusChangedEvent, WindowVisibilityChangedEvent, WindowStateChangedEvent, WindowPresentationChangedEvent,
                 WindowDisplayChangedEvent, WindowDisplayScaleChangedEvent, WindowPointerEnteredEvent, WindowPointerLeftEvent, DisplayAddedEvent,
                 DisplayRemovedEvent, DisplayMovedEvent, DisplayDesktopModeChangedEvent, DisplayCurrentModeChangedEvent,
                 DisplayOrientationChangedEvent, DisplayContentScaleChangedEvent, DisplayUsableBoundsChangedEvent, KeyboardKeyEvent, TextInputEvent,
                 TextCompositionEvent, MouseMotionEvent, MouseButtonEvent, MouseWheelEvent, DropBeginEvent, DroppedFileEvent, DroppedTextEvent,
                 DropPositionEvent, DropCompleteEvent, DialogCompletedEvent>;
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::DialogKind> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::DialogKind kind, FormatContext& context) const
    {
        using enum ponder::platform::DialogKind;
        switch (kind)
        {
        case OpenFile:
            return formatter<string_view>::format("open-file", context);
        case SaveFile:
            return formatter<string_view>::format("save-file", context);
        case OpenFolder:
            return formatter<string_view>::format("open-folder", context);
        }

        return formatter<string_view>::format("unrecognized", context);
    }
};

template <>
struct formatter<ponder::platform::DialogRequestInfo> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DialogRequestInfo& request, FormatContext& context) const
    {
        const string parent = request.parentWindowId.has_value() ? std::format("{}", *request.parentWindowId) : "none";
        return formatter<string>::format(std::format("dialog_request(id={}, kind={}, requestedAt={}, parent={}, filterCount={}, "
                                                     "allowMultipleSelection={})",
                                                     request.id, request.kind, request.requestedAt, parent, request.filterCount,
                                                     request.allowMultipleSelection),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::DialogSelection> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DialogSelection& selection, FormatContext& context) const
    {
        const string filterIndex = selection.selectedFilterIndex.has_value() ? std::format("{}", *selection.selectedFilterIndex) : "none";
        return formatter<string>::format(std::format("selection(pathCount={}, selectedFilter={})", selection.paths.size(), filterIndex), context);
    }
};

template <>
struct formatter<ponder::platform::DialogCancellation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::DialogCancellation, FormatContext& context) const
    {
        return formatter<string_view>::format("cancelled", context);
    }
};

template <>
struct formatter<ponder::platform::DialogFailure> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DialogFailure& failure, FormatContext& context) const
    {
        return formatter<string>::format(std::format("failure({})", failure.error), context);
    }
};

template <>
struct formatter<ponder::platform::DialogOutcome> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DialogOutcome& outcome, FormatContext& context) const
    {
        const string text = std::visit(
            [](const auto& value)
            {
                return std::format("{}", value);
            },
            outcome);
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::DialogCompletedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DialogCompletedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("dialog_completed(timestamp={}, request={}, outcome={})", event.timestamp, event.request, event.outcome), context);
    }
};

template <>
struct formatter<ponder::platform::QuitRequestedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::QuitRequestedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("quit_requested(timestamp={})", event.timestamp), context);
    }
};

template <>
struct formatter<ponder::platform::WindowCloseRequestedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowCloseRequestedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("window_close_requested(timestamp={}, window={})", event.timestamp, event.windowId), context);
    }
};

template <>
struct formatter<ponder::platform::WindowMovedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowMovedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_moved(timestamp={}, window={}, position={})", event.timestamp, event.windowId, event.position), context);
    }
};

template <>
struct formatter<ponder::platform::WindowLogicalSizeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowLogicalSizeChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_logical_size_changed(timestamp={}, window={}, size={})", event.timestamp, event.windowId, event.logicalSize),
            context);
    }
};

template <>
struct formatter<ponder::platform::WindowPixelSizeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowPixelSizeChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_pixel_size_changed(timestamp={}, window={}, size={})", event.timestamp, event.windowId, event.pixelSize), context);
    }
};

template <>
struct formatter<ponder::platform::WindowFocusChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowFocusChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_focus_changed(timestamp={}, window={}, focused={})", event.timestamp, event.windowId, event.focused), context);
    }
};

template <>
struct formatter<ponder::platform::WindowVisibilityChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowVisibilityChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_visibility_changed(timestamp={}, window={}, visible={})", event.timestamp, event.windowId, event.visible), context);
    }
};

template <>
struct formatter<ponder::platform::WindowStateChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowStateChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_state_changed(timestamp={}, window={}, state={})", event.timestamp, event.windowId, event.state), context);
    }
};

template <>
struct formatter<ponder::platform::WindowPresentationChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowPresentationChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("window_presentation_changed(timestamp={}, window={}, presentation={})", event.timestamp, event.windowId, event.presentation),
            context);
    }
};

template <>
struct formatter<ponder::platform::WindowDisplayChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowDisplayChangedEvent& event, FormatContext& context) const
    {
        const string display = event.displayId.has_value() ? std::format("{}", *event.displayId) : "none";
        return formatter<string>::format(
            std::format("window_display_changed(timestamp={}, window={}, display={})", event.timestamp, event.windowId, display), context);
    }
};

template <>
struct formatter<ponder::platform::WindowDisplayScaleChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowDisplayScaleChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("window_display_scale_changed(timestamp={}, window={})", event.timestamp, event.windowId),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::WindowPointerEnteredEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowPointerEnteredEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("window_pointer_entered(timestamp={}, window={})", event.timestamp, event.windowId), context);
    }
};

template <>
struct formatter<ponder::platform::WindowPointerLeftEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowPointerLeftEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("window_pointer_left(timestamp={}, window={})", event.timestamp, event.windowId), context);
    }
};
} // namespace std
namespace std
{
template <>
struct formatter<ponder::platform::DisplayAddedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayAddedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("display_added(timestamp={}, display={})", event.timestamp, event.displayId), context);
    }
};

template <>
struct formatter<ponder::platform::DisplayRemovedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayRemovedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("display_removed(timestamp={}, display={})", event.timestamp, event.displayId), context);
    }
};

template <>
struct formatter<ponder::platform::DisplayMovedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayMovedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("display_moved(timestamp={}, display={})", event.timestamp, event.displayId), context);
    }
};

template <>
struct formatter<ponder::platform::DisplayDesktopModeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayDesktopModeChangedEvent& event, FormatContext& context) const
    {
        const string extent = event.extent.has_value() ? std::format("{}", *event.extent) : "none";
        return formatter<string>::format(
            std::format("display_desktop_mode_changed(timestamp={}, display={}, extent={})", event.timestamp, event.displayId, extent), context);
    }
};

template <>
struct formatter<ponder::platform::DisplayCurrentModeChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayCurrentModeChangedEvent& event, FormatContext& context) const
    {
        const string extent = event.extent.has_value() ? std::format("{}", *event.extent) : "none";
        return formatter<string>::format(
            std::format("display_current_mode_changed(timestamp={}, display={}, extent={})", event.timestamp, event.displayId, extent), context);
    }
};

template <>
struct formatter<ponder::platform::DisplayOrientationChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayOrientationChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("display_orientation_changed(timestamp={}, display={}, orientation={})", event.timestamp, event.displayId, event.orientation),
            context);
    }
};

template <>
struct formatter<ponder::platform::DisplayContentScaleChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayContentScaleChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("display_content_scale_changed(timestamp={}, display={})", event.timestamp, event.displayId),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::DisplayUsableBoundsChangedEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayUsableBoundsChangedEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("display_usable_bounds_changed(timestamp={}, display={})", event.timestamp, event.displayId),
                                         context);
    }
};
} // namespace std
namespace std
{
template <>
struct formatter<ponder::platform::KeyboardKeyEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::KeyboardKeyEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(std::format("keyboard_key(timestamp={}, window={}, physical={}, logical={}, "
                                                     "modifiers={}, pressed={}, repeat={})",
                                                     event.timestamp, window, event.physicalKey, event.logicalKey, event.modifiers, event.pressed,
                                                     event.repeat),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::TextInputEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::TextInputEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(std::format("text_input(timestamp={}, window={}, text='{}')", event.timestamp, window, event.text), context);
    }
};

template <>
struct formatter<ponder::platform::TextCompositionEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::TextCompositionEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string selection = event.selection.has_value() ? std::format("{}", *event.selection) : "none";
        return formatter<string>::format(
            std::format("text_composition(timestamp={}, window={}, text='{}', selection={})", event.timestamp, window, event.text, selection),
            context);
    }
};

template <>
struct formatter<ponder::platform::MouseMotionEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::MouseMotionEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(std::format("mouse_motion(timestamp={}, window={}, position={}, relative={})", event.timestamp, window,
                                                     event.position, event.relativeMovement),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::MouseButtonEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::MouseButtonEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(std::format("mouse_button(timestamp={}, window={}, position={}, button={}, "
                                                     "pressed={})",
                                                     event.timestamp, window, event.position, event.button, event.pressed),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::MouseWheelEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::MouseWheelEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        return formatter<string>::format(std::format("mouse_wheel(timestamp={}, window={}, position={}, horizontal={}, "
                                                     "vertical={})",
                                                     event.timestamp, window, event.position, event.horizontal, event.vertical),
                                         context);
    }
};
} // namespace std
namespace std
{
template <>
struct formatter<ponder::platform::DropBeginEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DropBeginEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value() ? std::format("'{}'", *event.sourceApplication) : "none";
        return formatter<string>::format(std::format("drop_begin(timestamp={}, window={}, sourceApplication={})", event.timestamp, window, source),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::DroppedFileEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DroppedFileEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value() ? std::format("'{}'", *event.sourceApplication) : "none";
        return formatter<string>::format(std::format("dropped_file(timestamp={}, window={}, path='{}', position={}, "
                                                     "sourceApplication={})",
                                                     event.timestamp, window, event.path.string(), event.position, source),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::DroppedTextEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DroppedTextEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value() ? std::format("'{}'", *event.sourceApplication) : "none";
        return formatter<string>::format(std::format("dropped_text(timestamp={}, window={}, text='{}', position={}, "
                                                     "sourceApplication={})",
                                                     event.timestamp, window, event.text, event.position, source),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::DropPositionEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DropPositionEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value() ? std::format("'{}'", *event.sourceApplication) : "none";
        return formatter<string>::format(std::format("drop_position(timestamp={}, window={}, position={}, "
                                                     "sourceApplication={})",
                                                     event.timestamp, window, event.position, source),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::DropCompleteEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DropCompleteEvent& event, FormatContext& context) const
    {
        const string window = event.windowId.has_value() ? std::format("{}", *event.windowId) : "none";
        const string source = event.sourceApplication.has_value() ? std::format("'{}'", *event.sourceApplication) : "none";
        return formatter<string>::format(std::format("drop_complete(timestamp={}, window={}, position={}, "
                                                     "sourceApplication={})",
                                                     event.timestamp, window, event.position, source),
                                         context);
    }
};

} // namespace std
namespace ponder::platform::detail
{
template <typename Type>
concept PlatformEventPayload =
    std::same_as<std::remove_cvref_t<Type>, QuitRequestedEvent> || std::same_as<std::remove_cvref_t<Type>, WindowCloseRequestedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowMovedEvent> || std::same_as<std::remove_cvref_t<Type>, WindowLogicalSizeChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowPixelSizeChangedEvent> || std::same_as<std::remove_cvref_t<Type>, WindowFocusChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowVisibilityChangedEvent> || std::same_as<std::remove_cvref_t<Type>, WindowStateChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowPresentationChangedEvent> || std::same_as<std::remove_cvref_t<Type>, WindowDisplayChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowDisplayScaleChangedEvent> || std::same_as<std::remove_cvref_t<Type>, WindowPointerEnteredEvent> ||
    std::same_as<std::remove_cvref_t<Type>, WindowPointerLeftEvent> || std::same_as<std::remove_cvref_t<Type>, DisplayAddedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayRemovedEvent> || std::same_as<std::remove_cvref_t<Type>, DisplayMovedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayDesktopModeChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayCurrentModeChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayOrientationChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayContentScaleChangedEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DisplayUsableBoundsChangedEvent> || std::same_as<std::remove_cvref_t<Type>, KeyboardKeyEvent> ||
    std::same_as<std::remove_cvref_t<Type>, TextInputEvent> || std::same_as<std::remove_cvref_t<Type>, TextCompositionEvent> ||
    std::same_as<std::remove_cvref_t<Type>, MouseMotionEvent> || std::same_as<std::remove_cvref_t<Type>, MouseButtonEvent> ||
    std::same_as<std::remove_cvref_t<Type>, MouseWheelEvent> || std::same_as<std::remove_cvref_t<Type>, DropBeginEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DroppedFileEvent> || std::same_as<std::remove_cvref_t<Type>, DroppedTextEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DropPositionEvent> || std::same_as<std::remove_cvref_t<Type>, DropCompleteEvent> ||
    std::same_as<std::remove_cvref_t<Type>, DialogCompletedEvent>;
} // namespace ponder::platform::detail

namespace ponder::platform
{
inline std::ostream& operator<<(std::ostream& output, DialogKind kind)
{
    return output << std::format("{}", kind);
}

inline std::ostream& operator<<(std::ostream& output, const DialogRequestInfo& request)
{
    return output << std::format("{}", request);
}

inline std::ostream& operator<<(std::ostream& output, const DialogSelection& selection)
{
    return output << std::format("{}", selection);
}

inline std::ostream& operator<<(std::ostream& output, DialogCancellation cancellation)
{
    return output << std::format("{}", cancellation);
}

inline std::ostream& operator<<(std::ostream& output, const DialogFailure& failure)
{
    return output << std::format("{}", failure);
}

inline std::ostream& operator<<(std::ostream& output, const DialogOutcome& outcome)
{
    return output << std::format("{}", outcome);
}

inline std::ostream& operator<<(std::ostream& output, const DialogCompletedEvent& event)
{
    return output << std::format("{}", event);
}

template <detail::PlatformEventPayload Event>
inline std::ostream& operator<<(std::ostream& output, const Event& event)
{
    return output << std::format("{}", event);
}
} // namespace ponder::platform
