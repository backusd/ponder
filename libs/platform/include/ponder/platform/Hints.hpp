#pragma once

#include <ponder/platform/Mouse.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace ponder::platform::hints
{
enum class EventLoggingLevel : std::uint8_t
{
    Disabled,
    Common,
    Verbose
};

enum class ImeUiCapabilities : std::uint8_t
{
    None,
    Composition,
    Candidates,
    CompositionAndCandidates
};

enum class FullscreenFocusLossBehavior : std::uint8_t
{
    Automatic,
    Minimize,
    KeepFullscreen
};

#define PONDER_DECLARE_HINT_TYPE(Type, ValueType, DefaultValue)                                                                                      \
    struct Type final                                                                                                                                \
    {                                                                                                                                                \
        ValueType value = DefaultValue;                                                                                                              \
                                                                                                                                                     \
        [[nodiscard]] friend bool operator==(const Type&, const Type&) = default;                                                                    \
    }

PONDER_DECLARE_HINT_TYPE(AllowAltTabWhileGrabbed, bool, false);
PONDER_DECLARE_HINT_TYPE(EventLogging, EventLoggingLevel, EventLoggingLevel::Disabled);
PONDER_DECLARE_HINT_TYPE(ImeImplementedUi, ImeUiCapabilities, ImeUiCapabilities::None);
PONDER_DECLARE_HINT_TYPE(PollSentinel, bool, true);
PONDER_DECLARE_HINT_TYPE(QuitOnLastWindowClose, bool, true);

PONDER_DECLARE_HINT_TYPE(VideoAllowScreensaver, bool, false);
PONDER_DECLARE_HINT_TYPE(VideoDoubleBuffer, bool, false);
PONDER_DECLARE_HINT_TYPE(VideoDriver, std::string, std::string{});
PONDER_DECLARE_HINT_TYPE(VideoForceEgl, bool, false);
PONDER_DECLARE_HINT_TYPE(VideoMinimizeOnFocusLoss, FullscreenFocusLossBehavior, FullscreenFocusLossBehavior::Automatic);
PONDER_DECLARE_HINT_TYPE(VideoSyncWindowOperations, bool, false);

PONDER_DECLARE_HINT_TYPE(WindowActivateWhenRaised, bool, true);
PONDER_DECLARE_HINT_TYPE(WindowActivateWhenShown, bool, true);
PONDER_DECLARE_HINT_TYPE(WindowAllowTopmost, bool, true);
PONDER_DECLARE_HINT_TYPE(WindowFrameUsableWhileCursorHidden, bool, true);

PONDER_DECLARE_HINT_TYPE(MouseAutoCapture, bool, false);
PONDER_DECLARE_HINT_TYPE(MouseDefaultSystemCursor, SystemCursorShape, SystemCursorShape::Default);
PONDER_DECLARE_HINT_TYPE(MouseDoubleClickRadius, std::uint32_t, 0U);
PONDER_DECLARE_HINT_TYPE(MouseDoubleClickTime, std::chrono::milliseconds, std::chrono::milliseconds{500});
PONDER_DECLARE_HINT_TYPE(MouseDpiScaleCursors, bool, false);
PONDER_DECLARE_HINT_TYPE(MouseEmulateWarpWithRelative, bool, true);
PONDER_DECLARE_HINT_TYPE(MouseFocusClickThrough, bool, true);
PONDER_DECLARE_HINT_TYPE(MouseNormalSpeedScale, float, 1.0F);
PONDER_DECLARE_HINT_TYPE(MouseRelativeCursorVisible, bool, false);
PONDER_DECLARE_HINT_TYPE(MouseRelativeModeCenter, bool, true);
PONDER_DECLARE_HINT_TYPE(MouseRelativeSpeedScale, float, 1.0F);
PONDER_DECLARE_HINT_TYPE(MouseRelativeSystemScale, bool, false);
PONDER_DECLARE_HINT_TYPE(MouseRelativeWarpMotion, bool, false);
PONDER_DECLARE_HINT_TYPE(MouseTouchEvents, bool, false);
PONDER_DECLARE_HINT_TYPE(PenMouseEvents, bool, true);
PONDER_DECLARE_HINT_TYPE(PenTouchEvents, bool, true);
PONDER_DECLARE_HINT_TYPE(TouchMouseEvents, bool, true);
PONDER_DECLARE_HINT_TYPE(TrackpadIsTouchOnly, bool, false);

#if defined(__APPLE__)
PONDER_DECLARE_HINT_TYPE(MacCtrlClickEmulatesRightClick, bool, false);
PONDER_DECLARE_HINT_TYPE(MacScrollMomentum, bool, true);
PONDER_DECLARE_HINT_TYPE(VideoMacFullscreenSpaces, bool, true);
#endif

#if defined(_WIN32)
PONDER_DECLARE_HINT_TYPE(WindowsCloseOnAltF4, bool, true);
PONDER_DECLARE_HINT_TYPE(WindowsEnableMenuMnemonics, bool, true);
PONDER_DECLARE_HINT_TYPE(WindowsGameInput, bool, true);
PONDER_DECLARE_HINT_TYPE(WindowsRawKeyboard, bool, false);
PONDER_DECLARE_HINT_TYPE(WindowsRawKeyboardExcludeHotkeys, bool, false);
PONDER_DECLARE_HINT_TYPE(WindowsRawKeyboardInputSink, bool, false);
PONDER_DECLARE_HINT_TYPE(WindowsRawMouseNoLegacy, bool, false);
#endif

#if defined(__linux__)
PONDER_DECLARE_HINT_TYPE(VideoDisplayPriority, std::string, std::string{});
PONDER_DECLARE_HINT_TYPE(VideoWaylandAllowLibdecor, bool, true);
PONDER_DECLARE_HINT_TYPE(VideoWaylandModeEmulation, bool, true);
PONDER_DECLARE_HINT_TYPE(VideoWaylandPreferLibdecor, bool, false);
PONDER_DECLARE_HINT_TYPE(VideoWaylandScaleToDisplay, bool, true);
PONDER_DECLARE_HINT_TYPE(VideoX11NetWmBypassCompositor, bool, true);
PONDER_DECLARE_HINT_TYPE(VideoX11Xrandr, bool, true);
#endif

#undef PONDER_DECLARE_HINT_TYPE
} // namespace ponder::platform::hints

namespace std
{
template <>
struct formatter<ponder::platform::hints::EventLoggingLevel> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::hints::EventLoggingLevel value, FormatContext& context) const
    {
        string_view text;
        switch (value)
        {
        case ponder::platform::hints::EventLoggingLevel::Disabled:
            text = "0";
            break;
        case ponder::platform::hints::EventLoggingLevel::Common:
            text = "1";
            break;
        case ponder::platform::hints::EventLoggingLevel::Verbose:
            text = "2";
            break;
        default:
            text = "<invalid>";
            break;
        }
        return formatter<string_view>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::hints::ImeUiCapabilities> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::hints::ImeUiCapabilities value, FormatContext& context) const
    {
        string_view text;
        switch (value)
        {
        case ponder::platform::hints::ImeUiCapabilities::None:
            text = "none";
            break;
        case ponder::platform::hints::ImeUiCapabilities::Composition:
            text = "composition";
            break;
        case ponder::platform::hints::ImeUiCapabilities::Candidates:
            text = "candidates";
            break;
        case ponder::platform::hints::ImeUiCapabilities::CompositionAndCandidates:
            text = "composition,candidates";
            break;
        default:
            text = "<invalid>";
            break;
        }
        return formatter<string_view>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::hints::FullscreenFocusLossBehavior> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::hints::FullscreenFocusLossBehavior value, FormatContext& context) const
    {
        string_view text;
        switch (value)
        {
        case ponder::platform::hints::FullscreenFocusLossBehavior::Automatic:
            text = "auto";
            break;
        case ponder::platform::hints::FullscreenFocusLossBehavior::Minimize:
            text = "1";
            break;
        case ponder::platform::hints::FullscreenFocusLossBehavior::KeepFullscreen:
            text = "0";
            break;
        default:
            text = "<invalid>";
            break;
        }
        return formatter<string_view>::format(text, context);
    }
};
} // namespace std

namespace ponder::platform::detail
{
template <typename Hint>
struct HintValueFormatter : std::formatter<std::string>
{
    template <typename FormatContext>
    auto format(const Hint& hint, FormatContext& context) const
    {
        using Value = std::remove_cvref_t<decltype(hint.value)>;

        std::string text;
        if constexpr (std::same_as<Value, bool>)
        {
            text = std::format("{}", hint.value ? 1 : 0);
        }
        else if constexpr (std::same_as<Value, std::chrono::milliseconds>)
        {
            text = std::format("{}", hint.value.count());
        }
        else
        {
            text = std::format("{}", hint.value);
        }
        return std::formatter<std::string>::format(text, context);
    }
};

struct MouseDefaultSystemCursorFormatter : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const hints::MouseDefaultSystemCursor& hint, FormatContext& context) const
    {
        std::string_view text;
        switch (hint.value)
        {
        case SystemCursorShape::Default:
            text = "0";
            break;
        case SystemCursorShape::TextInput:
            text = "1";
            break;
        case SystemCursorShape::Move:
            text = "9";
            break;
        case SystemCursorShape::ResizeNorthSouth:
            text = "8";
            break;
        case SystemCursorShape::ResizeEastWest:
            text = "7";
            break;
        case SystemCursorShape::ResizeNortheastSouthwest:
            text = "6";
            break;
        case SystemCursorShape::ResizeNorthwestSoutheast:
            text = "5";
            break;
        case SystemCursorShape::Pointer:
            text = "11";
            break;
        case SystemCursorShape::Wait:
            text = "2";
            break;
        case SystemCursorShape::Progress:
            text = "4";
            break;
        case SystemCursorShape::NotAllowed:
            text = "10";
            break;
        default:
            text = "<invalid>";
            break;
        }
        return std::formatter<std::string_view>::format(text, context);
    }
};
} // namespace ponder::platform::detail

namespace std
{
#define PONDER_DEFINE_HINT_FORMATTER(Type)                                                                                                           \
    template <>                                                                                                                                      \
    struct formatter<ponder::platform::hints::Type> : ponder::platform::detail::HintValueFormatter<ponder::platform::hints::Type>                    \
    {                                                                                                                                                \
    }

PONDER_DEFINE_HINT_FORMATTER(AllowAltTabWhileGrabbed);
PONDER_DEFINE_HINT_FORMATTER(EventLogging);
PONDER_DEFINE_HINT_FORMATTER(ImeImplementedUi);
PONDER_DEFINE_HINT_FORMATTER(PollSentinel);
PONDER_DEFINE_HINT_FORMATTER(QuitOnLastWindowClose);

PONDER_DEFINE_HINT_FORMATTER(VideoAllowScreensaver);
PONDER_DEFINE_HINT_FORMATTER(VideoDoubleBuffer);
PONDER_DEFINE_HINT_FORMATTER(VideoDriver);
PONDER_DEFINE_HINT_FORMATTER(VideoForceEgl);
PONDER_DEFINE_HINT_FORMATTER(VideoMinimizeOnFocusLoss);
PONDER_DEFINE_HINT_FORMATTER(VideoSyncWindowOperations);

PONDER_DEFINE_HINT_FORMATTER(WindowActivateWhenRaised);
PONDER_DEFINE_HINT_FORMATTER(WindowActivateWhenShown);
PONDER_DEFINE_HINT_FORMATTER(WindowAllowTopmost);
PONDER_DEFINE_HINT_FORMATTER(WindowFrameUsableWhileCursorHidden);

PONDER_DEFINE_HINT_FORMATTER(MouseAutoCapture);
template <>
struct formatter<ponder::platform::hints::MouseDefaultSystemCursor> : ponder::platform::detail::MouseDefaultSystemCursorFormatter
{
};
PONDER_DEFINE_HINT_FORMATTER(MouseDoubleClickRadius);
PONDER_DEFINE_HINT_FORMATTER(MouseDoubleClickTime);
PONDER_DEFINE_HINT_FORMATTER(MouseDpiScaleCursors);
PONDER_DEFINE_HINT_FORMATTER(MouseEmulateWarpWithRelative);
PONDER_DEFINE_HINT_FORMATTER(MouseFocusClickThrough);
PONDER_DEFINE_HINT_FORMATTER(MouseNormalSpeedScale);
PONDER_DEFINE_HINT_FORMATTER(MouseRelativeCursorVisible);
PONDER_DEFINE_HINT_FORMATTER(MouseRelativeModeCenter);
PONDER_DEFINE_HINT_FORMATTER(MouseRelativeSpeedScale);
PONDER_DEFINE_HINT_FORMATTER(MouseRelativeSystemScale);
PONDER_DEFINE_HINT_FORMATTER(MouseRelativeWarpMotion);
PONDER_DEFINE_HINT_FORMATTER(MouseTouchEvents);
PONDER_DEFINE_HINT_FORMATTER(PenMouseEvents);
PONDER_DEFINE_HINT_FORMATTER(PenTouchEvents);
PONDER_DEFINE_HINT_FORMATTER(TouchMouseEvents);
PONDER_DEFINE_HINT_FORMATTER(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_DEFINE_HINT_FORMATTER(MacCtrlClickEmulatesRightClick);
PONDER_DEFINE_HINT_FORMATTER(MacScrollMomentum);
PONDER_DEFINE_HINT_FORMATTER(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_DEFINE_HINT_FORMATTER(WindowsCloseOnAltF4);
PONDER_DEFINE_HINT_FORMATTER(WindowsEnableMenuMnemonics);
PONDER_DEFINE_HINT_FORMATTER(WindowsGameInput);
PONDER_DEFINE_HINT_FORMATTER(WindowsRawKeyboard);
PONDER_DEFINE_HINT_FORMATTER(WindowsRawKeyboardExcludeHotkeys);
PONDER_DEFINE_HINT_FORMATTER(WindowsRawKeyboardInputSink);
PONDER_DEFINE_HINT_FORMATTER(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_DEFINE_HINT_FORMATTER(VideoDisplayPriority);
PONDER_DEFINE_HINT_FORMATTER(VideoWaylandAllowLibdecor);
PONDER_DEFINE_HINT_FORMATTER(VideoWaylandModeEmulation);
PONDER_DEFINE_HINT_FORMATTER(VideoWaylandPreferLibdecor);
PONDER_DEFINE_HINT_FORMATTER(VideoWaylandScaleToDisplay);
PONDER_DEFINE_HINT_FORMATTER(VideoX11NetWmBypassCompositor);
PONDER_DEFINE_HINT_FORMATTER(VideoX11Xrandr);
#endif

#undef PONDER_DEFINE_HINT_FORMATTER
} // namespace std

namespace ponder::platform::hints
{
inline std::ostream& operator<<(std::ostream& output, EventLoggingLevel value)
{
    return output << std::format("{}", value);
}

inline std::ostream& operator<<(std::ostream& output, ImeUiCapabilities value)
{
    return output << std::format("{}", value);
}

inline std::ostream& operator<<(std::ostream& output, FullscreenFocusLossBehavior value)
{
    return output << std::format("{}", value);
}

#define PONDER_DEFINE_HINT_STREAM_OPERATOR(Type)                                                                                                     \
    inline std::ostream& operator<<(std::ostream& output, const Type& hint)                                                                          \
    {                                                                                                                                                \
        return output << std::format("{}", hint);                                                                                                    \
    }

PONDER_DEFINE_HINT_STREAM_OPERATOR(AllowAltTabWhileGrabbed);
PONDER_DEFINE_HINT_STREAM_OPERATOR(EventLogging);
PONDER_DEFINE_HINT_STREAM_OPERATOR(ImeImplementedUi);
PONDER_DEFINE_HINT_STREAM_OPERATOR(PollSentinel);
PONDER_DEFINE_HINT_STREAM_OPERATOR(QuitOnLastWindowClose);

PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoAllowScreensaver);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoDoubleBuffer);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoDriver);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoForceEgl);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoMinimizeOnFocusLoss);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoSyncWindowOperations);

PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowActivateWhenRaised);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowActivateWhenShown);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowAllowTopmost);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowFrameUsableWhileCursorHidden);

PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseAutoCapture);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseDefaultSystemCursor);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseDoubleClickRadius);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseDoubleClickTime);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseDpiScaleCursors);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseEmulateWarpWithRelative);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseFocusClickThrough);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseNormalSpeedScale);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseRelativeCursorVisible);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseRelativeModeCenter);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseRelativeSpeedScale);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseRelativeSystemScale);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseRelativeWarpMotion);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MouseTouchEvents);
PONDER_DEFINE_HINT_STREAM_OPERATOR(PenMouseEvents);
PONDER_DEFINE_HINT_STREAM_OPERATOR(PenTouchEvents);
PONDER_DEFINE_HINT_STREAM_OPERATOR(TouchMouseEvents);
PONDER_DEFINE_HINT_STREAM_OPERATOR(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_DEFINE_HINT_STREAM_OPERATOR(MacCtrlClickEmulatesRightClick);
PONDER_DEFINE_HINT_STREAM_OPERATOR(MacScrollMomentum);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowsCloseOnAltF4);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowsEnableMenuMnemonics);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowsGameInput);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowsRawKeyboard);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowsRawKeyboardExcludeHotkeys);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowsRawKeyboardInputSink);
PONDER_DEFINE_HINT_STREAM_OPERATOR(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoDisplayPriority);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoWaylandAllowLibdecor);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoWaylandModeEmulation);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoWaylandPreferLibdecor);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoWaylandScaleToDisplay);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoX11NetWmBypassCompositor);
PONDER_DEFINE_HINT_STREAM_OPERATOR(VideoX11Xrandr);
#endif

#undef PONDER_DEFINE_HINT_STREAM_OPERATOR
} // namespace ponder::platform::hints
