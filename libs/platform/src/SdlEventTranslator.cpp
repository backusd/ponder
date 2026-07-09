#include "SdlEventTranslator.hpp"
#include "StringValidation.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>

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

struct InputWindowResolution final
{
    bool valid{};
    std::optional<WindowId> windowId;
};

[[nodiscard]] InputWindowResolution ResolveInputWindowId(
    std::uint32_t backendWindowId, const EventTranslationContext& context)
{
    if (backendWindowId == 0)
    {
        return InputWindowResolution{true, std::nullopt};
    }
    if (context.resolveWindowId == nullptr)
    {
        return {};
    }

    const std::optional<WindowId> id =
        context.resolveWindowId(context.context, backendWindowId);
    if (!id.has_value() || !id->IsValid())
    {
        return {};
    }

    return InputWindowResolution{true, *id};
}

[[nodiscard]] PhysicalKey TranslatePhysicalKey(SDL_Scancode scancode) noexcept
{
    switch (scancode)
    {
    case SDL_SCANCODE_TAB:
        return PhysicalKey::Tab;
    case SDL_SCANCODE_LEFT:
        return PhysicalKey::ArrowLeft;
    case SDL_SCANCODE_RIGHT:
        return PhysicalKey::ArrowRight;
    case SDL_SCANCODE_UP:
        return PhysicalKey::ArrowUp;
    case SDL_SCANCODE_DOWN:
        return PhysicalKey::ArrowDown;
    case SDL_SCANCODE_PAGEUP:
        return PhysicalKey::PageUp;
    case SDL_SCANCODE_PAGEDOWN:
        return PhysicalKey::PageDown;
    case SDL_SCANCODE_HOME:
        return PhysicalKey::Home;
    case SDL_SCANCODE_END:
        return PhysicalKey::End;
    case SDL_SCANCODE_INSERT:
        return PhysicalKey::Insert;
    case SDL_SCANCODE_DELETE:
        return PhysicalKey::Delete;
    case SDL_SCANCODE_BACKSPACE:
        return PhysicalKey::Backspace;
    case SDL_SCANCODE_SPACE:
        return PhysicalKey::Space;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_RETURN2:
        return PhysicalKey::Enter;
    case SDL_SCANCODE_ESCAPE:
        return PhysicalKey::Escape;
    case SDL_SCANCODE_LCTRL:
        return PhysicalKey::LeftControl;
    case SDL_SCANCODE_LSHIFT:
        return PhysicalKey::LeftShift;
    case SDL_SCANCODE_LALT:
        return PhysicalKey::LeftAlt;
    case SDL_SCANCODE_LGUI:
        return PhysicalKey::LeftSuper;
    case SDL_SCANCODE_RCTRL:
        return PhysicalKey::RightControl;
    case SDL_SCANCODE_RSHIFT:
        return PhysicalKey::RightShift;
    case SDL_SCANCODE_RALT:
        return PhysicalKey::RightAlt;
    case SDL_SCANCODE_RGUI:
        return PhysicalKey::RightSuper;
    case SDL_SCANCODE_APPLICATION:
    case SDL_SCANCODE_MENU:
        return PhysicalKey::Menu;
    case SDL_SCANCODE_0:
        return PhysicalKey::Digit0;
    case SDL_SCANCODE_1:
        return PhysicalKey::Digit1;
    case SDL_SCANCODE_2:
        return PhysicalKey::Digit2;
    case SDL_SCANCODE_3:
        return PhysicalKey::Digit3;
    case SDL_SCANCODE_4:
        return PhysicalKey::Digit4;
    case SDL_SCANCODE_5:
        return PhysicalKey::Digit5;
    case SDL_SCANCODE_6:
        return PhysicalKey::Digit6;
    case SDL_SCANCODE_7:
        return PhysicalKey::Digit7;
    case SDL_SCANCODE_8:
        return PhysicalKey::Digit8;
    case SDL_SCANCODE_9:
        return PhysicalKey::Digit9;
    case SDL_SCANCODE_A:
        return PhysicalKey::A;
    case SDL_SCANCODE_B:
        return PhysicalKey::B;
    case SDL_SCANCODE_C:
        return PhysicalKey::C;
    case SDL_SCANCODE_D:
        return PhysicalKey::D;
    case SDL_SCANCODE_E:
        return PhysicalKey::E;
    case SDL_SCANCODE_F:
        return PhysicalKey::F;
    case SDL_SCANCODE_G:
        return PhysicalKey::G;
    case SDL_SCANCODE_H:
        return PhysicalKey::H;
    case SDL_SCANCODE_I:
        return PhysicalKey::I;
    case SDL_SCANCODE_J:
        return PhysicalKey::J;
    case SDL_SCANCODE_K:
        return PhysicalKey::K;
    case SDL_SCANCODE_L:
        return PhysicalKey::L;
    case SDL_SCANCODE_M:
        return PhysicalKey::M;
    case SDL_SCANCODE_N:
        return PhysicalKey::N;
    case SDL_SCANCODE_O:
        return PhysicalKey::O;
    case SDL_SCANCODE_P:
        return PhysicalKey::P;
    case SDL_SCANCODE_Q:
        return PhysicalKey::Q;
    case SDL_SCANCODE_R:
        return PhysicalKey::R;
    case SDL_SCANCODE_S:
        return PhysicalKey::S;
    case SDL_SCANCODE_T:
        return PhysicalKey::T;
    case SDL_SCANCODE_U:
        return PhysicalKey::U;
    case SDL_SCANCODE_V:
        return PhysicalKey::V;
    case SDL_SCANCODE_W:
        return PhysicalKey::W;
    case SDL_SCANCODE_X:
        return PhysicalKey::X;
    case SDL_SCANCODE_Y:
        return PhysicalKey::Y;
    case SDL_SCANCODE_Z:
        return PhysicalKey::Z;
    case SDL_SCANCODE_F1:
        return PhysicalKey::F1;
    case SDL_SCANCODE_F2:
        return PhysicalKey::F2;
    case SDL_SCANCODE_F3:
        return PhysicalKey::F3;
    case SDL_SCANCODE_F4:
        return PhysicalKey::F4;
    case SDL_SCANCODE_F5:
        return PhysicalKey::F5;
    case SDL_SCANCODE_F6:
        return PhysicalKey::F6;
    case SDL_SCANCODE_F7:
        return PhysicalKey::F7;
    case SDL_SCANCODE_F8:
        return PhysicalKey::F8;
    case SDL_SCANCODE_F9:
        return PhysicalKey::F9;
    case SDL_SCANCODE_F10:
        return PhysicalKey::F10;
    case SDL_SCANCODE_F11:
        return PhysicalKey::F11;
    case SDL_SCANCODE_F12:
        return PhysicalKey::F12;
    case SDL_SCANCODE_F13:
        return PhysicalKey::F13;
    case SDL_SCANCODE_F14:
        return PhysicalKey::F14;
    case SDL_SCANCODE_F15:
        return PhysicalKey::F15;
    case SDL_SCANCODE_F16:
        return PhysicalKey::F16;
    case SDL_SCANCODE_F17:
        return PhysicalKey::F17;
    case SDL_SCANCODE_F18:
        return PhysicalKey::F18;
    case SDL_SCANCODE_F19:
        return PhysicalKey::F19;
    case SDL_SCANCODE_F20:
        return PhysicalKey::F20;
    case SDL_SCANCODE_F21:
        return PhysicalKey::F21;
    case SDL_SCANCODE_F22:
        return PhysicalKey::F22;
    case SDL_SCANCODE_F23:
        return PhysicalKey::F23;
    case SDL_SCANCODE_F24:
        return PhysicalKey::F24;
    case SDL_SCANCODE_APOSTROPHE:
        return PhysicalKey::Apostrophe;
    case SDL_SCANCODE_COMMA:
        return PhysicalKey::Comma;
    case SDL_SCANCODE_MINUS:
        return PhysicalKey::Minus;
    case SDL_SCANCODE_PERIOD:
        return PhysicalKey::Period;
    case SDL_SCANCODE_SLASH:
        return PhysicalKey::Slash;
    case SDL_SCANCODE_SEMICOLON:
        return PhysicalKey::Semicolon;
    case SDL_SCANCODE_EQUALS:
        return PhysicalKey::Equal;
    case SDL_SCANCODE_LEFTBRACKET:
        return PhysicalKey::LeftBracket;
    case SDL_SCANCODE_BACKSLASH:
        return PhysicalKey::Backslash;
    case SDL_SCANCODE_RIGHTBRACKET:
        return PhysicalKey::RightBracket;
    case SDL_SCANCODE_GRAVE:
        return PhysicalKey::GraveAccent;
    case SDL_SCANCODE_CAPSLOCK:
        return PhysicalKey::CapsLock;
    case SDL_SCANCODE_SCROLLLOCK:
        return PhysicalKey::ScrollLock;
    case SDL_SCANCODE_NUMLOCKCLEAR:
        return PhysicalKey::NumLock;
    case SDL_SCANCODE_PRINTSCREEN:
        return PhysicalKey::PrintScreen;
    case SDL_SCANCODE_PAUSE:
        return PhysicalKey::Pause;
    case SDL_SCANCODE_KP_0:
        return PhysicalKey::Keypad0;
    case SDL_SCANCODE_KP_1:
        return PhysicalKey::Keypad1;
    case SDL_SCANCODE_KP_2:
        return PhysicalKey::Keypad2;
    case SDL_SCANCODE_KP_3:
        return PhysicalKey::Keypad3;
    case SDL_SCANCODE_KP_4:
        return PhysicalKey::Keypad4;
    case SDL_SCANCODE_KP_5:
        return PhysicalKey::Keypad5;
    case SDL_SCANCODE_KP_6:
        return PhysicalKey::Keypad6;
    case SDL_SCANCODE_KP_7:
        return PhysicalKey::Keypad7;
    case SDL_SCANCODE_KP_8:
        return PhysicalKey::Keypad8;
    case SDL_SCANCODE_KP_9:
        return PhysicalKey::Keypad9;
    case SDL_SCANCODE_KP_PERIOD:
        return PhysicalKey::KeypadDecimal;
    case SDL_SCANCODE_KP_DIVIDE:
        return PhysicalKey::KeypadDivide;
    case SDL_SCANCODE_KP_MULTIPLY:
        return PhysicalKey::KeypadMultiply;
    case SDL_SCANCODE_KP_MINUS:
        return PhysicalKey::KeypadSubtract;
    case SDL_SCANCODE_KP_PLUS:
        return PhysicalKey::KeypadAdd;
    case SDL_SCANCODE_KP_ENTER:
        return PhysicalKey::KeypadEnter;
    case SDL_SCANCODE_KP_EQUALS:
        return PhysicalKey::KeypadEqual;
    case SDL_SCANCODE_AC_BACK:
        return PhysicalKey::BrowserBack;
    case SDL_SCANCODE_AC_FORWARD:
        return PhysicalKey::BrowserForward;
    case SDL_SCANCODE_NONUSBACKSLASH:
        return PhysicalKey::NonUsBackslash;
    default:
        return PhysicalKey::Unknown;
    }
}

[[nodiscard]] std::optional<NamedKey> TranslateNamedKey(
    SDL_Keycode keycode, SDL_Scancode scancode) noexcept
{
    switch (scancode)
    {
    case SDL_SCANCODE_KP_0:
        return NamedKey::Keypad0;
    case SDL_SCANCODE_KP_1:
        return NamedKey::Keypad1;
    case SDL_SCANCODE_KP_2:
        return NamedKey::Keypad2;
    case SDL_SCANCODE_KP_3:
        return NamedKey::Keypad3;
    case SDL_SCANCODE_KP_4:
        return NamedKey::Keypad4;
    case SDL_SCANCODE_KP_5:
        return NamedKey::Keypad5;
    case SDL_SCANCODE_KP_6:
        return NamedKey::Keypad6;
    case SDL_SCANCODE_KP_7:
        return NamedKey::Keypad7;
    case SDL_SCANCODE_KP_8:
        return NamedKey::Keypad8;
    case SDL_SCANCODE_KP_9:
        return NamedKey::Keypad9;
    case SDL_SCANCODE_KP_PERIOD:
        return NamedKey::KeypadDecimal;
    case SDL_SCANCODE_KP_DIVIDE:
        return NamedKey::KeypadDivide;
    case SDL_SCANCODE_KP_MULTIPLY:
        return NamedKey::KeypadMultiply;
    case SDL_SCANCODE_KP_MINUS:
        return NamedKey::KeypadSubtract;
    case SDL_SCANCODE_KP_PLUS:
        return NamedKey::KeypadAdd;
    case SDL_SCANCODE_KP_ENTER:
        return NamedKey::KeypadEnter;
    case SDL_SCANCODE_KP_EQUALS:
        return NamedKey::KeypadEqual;
    default:
        break;
    }

    switch (keycode)
    {
    case SDLK_TAB:
    case SDLK_LEFT_TAB:
    case SDLK_KP_TAB:
        return NamedKey::Tab;
    case SDLK_LEFT:
        return NamedKey::ArrowLeft;
    case SDLK_RIGHT:
        return NamedKey::ArrowRight;
    case SDLK_UP:
        return NamedKey::ArrowUp;
    case SDLK_DOWN:
        return NamedKey::ArrowDown;
    case SDLK_PAGEUP:
        return NamedKey::PageUp;
    case SDLK_PAGEDOWN:
        return NamedKey::PageDown;
    case SDLK_HOME:
        return NamedKey::Home;
    case SDLK_END:
        return NamedKey::End;
    case SDLK_INSERT:
        return NamedKey::Insert;
    case SDLK_DELETE:
        return NamedKey::Delete;
    case SDLK_BACKSPACE:
    case SDLK_KP_BACKSPACE:
        return NamedKey::Backspace;
    case SDLK_RETURN:
    case SDLK_RETURN2:
        return NamedKey::Enter;
    case SDLK_ESCAPE:
        return NamedKey::Escape;
    case SDLK_LCTRL:
        return NamedKey::LeftControl;
    case SDLK_LSHIFT:
        return NamedKey::LeftShift;
    case SDLK_LALT:
        return NamedKey::LeftAlt;
    case SDLK_LGUI:
        return NamedKey::LeftSuper;
    case SDLK_RCTRL:
        return NamedKey::RightControl;
    case SDLK_RSHIFT:
        return NamedKey::RightShift;
    case SDLK_RALT:
        return NamedKey::RightAlt;
    case SDLK_RGUI:
        return NamedKey::RightSuper;
    case SDLK_APPLICATION:
    case SDLK_MENU:
        return NamedKey::Menu;
    case SDLK_F1:
        return NamedKey::F1;
    case SDLK_F2:
        return NamedKey::F2;
    case SDLK_F3:
        return NamedKey::F3;
    case SDLK_F4:
        return NamedKey::F4;
    case SDLK_F5:
        return NamedKey::F5;
    case SDLK_F6:
        return NamedKey::F6;
    case SDLK_F7:
        return NamedKey::F7;
    case SDLK_F8:
        return NamedKey::F8;
    case SDLK_F9:
        return NamedKey::F9;
    case SDLK_F10:
        return NamedKey::F10;
    case SDLK_F11:
        return NamedKey::F11;
    case SDLK_F12:
        return NamedKey::F12;
    case SDLK_F13:
        return NamedKey::F13;
    case SDLK_F14:
        return NamedKey::F14;
    case SDLK_F15:
        return NamedKey::F15;
    case SDLK_F16:
        return NamedKey::F16;
    case SDLK_F17:
        return NamedKey::F17;
    case SDLK_F18:
        return NamedKey::F18;
    case SDLK_F19:
        return NamedKey::F19;
    case SDLK_F20:
        return NamedKey::F20;
    case SDLK_F21:
        return NamedKey::F21;
    case SDLK_F22:
        return NamedKey::F22;
    case SDLK_F23:
        return NamedKey::F23;
    case SDLK_F24:
        return NamedKey::F24;
    case SDLK_CAPSLOCK:
        return NamedKey::CapsLock;
    case SDLK_SCROLLLOCK:
        return NamedKey::ScrollLock;
    case SDLK_NUMLOCKCLEAR:
        return NamedKey::NumLock;
    case SDLK_PRINTSCREEN:
        return NamedKey::PrintScreen;
    case SDLK_PAUSE:
        return NamedKey::Pause;
    case SDLK_KP_0:
        return NamedKey::Keypad0;
    case SDLK_KP_1:
        return NamedKey::Keypad1;
    case SDLK_KP_2:
        return NamedKey::Keypad2;
    case SDLK_KP_3:
        return NamedKey::Keypad3;
    case SDLK_KP_4:
        return NamedKey::Keypad4;
    case SDLK_KP_5:
        return NamedKey::Keypad5;
    case SDLK_KP_6:
        return NamedKey::Keypad6;
    case SDLK_KP_7:
        return NamedKey::Keypad7;
    case SDLK_KP_8:
        return NamedKey::Keypad8;
    case SDLK_KP_9:
        return NamedKey::Keypad9;
    case SDLK_KP_PERIOD:
        return NamedKey::KeypadDecimal;
    case SDLK_KP_DIVIDE:
        return NamedKey::KeypadDivide;
    case SDLK_KP_MULTIPLY:
        return NamedKey::KeypadMultiply;
    case SDLK_KP_MINUS:
        return NamedKey::KeypadSubtract;
    case SDLK_KP_PLUS:
        return NamedKey::KeypadAdd;
    case SDLK_KP_ENTER:
        return NamedKey::KeypadEnter;
    case SDLK_KP_EQUALS:
        return NamedKey::KeypadEqual;
    case SDLK_AC_BACK:
        return NamedKey::BrowserBack;
    case SDLK_AC_FORWARD:
        return NamedKey::BrowserForward;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] LogicalKey TranslateLogicalKey(
    SDL_Keycode keycode, SDL_Scancode scancode) noexcept
{
    const std::optional<NamedKey> namedKey =
        TranslateNamedKey(keycode, scancode);
    if (namedKey.has_value())
    {
        return LogicalKey::FromNamed(*namedKey);
    }

    return LogicalKey::FromCharacter(static_cast<char32_t>(keycode));
}

[[nodiscard]] KeyModifiers TranslateKeyModifiers(SDL_Keymod modifiers) noexcept
{
    KeyModifiers translated = KeyModifiers::None;
    if ((modifiers & SDL_KMOD_LSHIFT) != 0)
    {
        translated |= KeyModifiers::LeftShift;
    }
    if ((modifiers & SDL_KMOD_RSHIFT) != 0)
    {
        translated |= KeyModifiers::RightShift;
    }
    if ((modifiers & SDL_KMOD_LEVEL5) != 0)
    {
        translated |= KeyModifiers::Level5Shift;
    }
    if ((modifiers & SDL_KMOD_LCTRL) != 0)
    {
        translated |= KeyModifiers::LeftControl;
    }
    if ((modifiers & SDL_KMOD_RCTRL) != 0)
    {
        translated |= KeyModifiers::RightControl;
    }
    if ((modifiers & SDL_KMOD_LALT) != 0)
    {
        translated |= KeyModifiers::LeftAlt;
    }
    if ((modifiers & SDL_KMOD_RALT) != 0)
    {
        translated |= KeyModifiers::RightAlt;
    }
    if ((modifiers & SDL_KMOD_LGUI) != 0)
    {
        translated |= KeyModifiers::LeftSuper;
    }
    if ((modifiers & SDL_KMOD_RGUI) != 0)
    {
        translated |= KeyModifiers::RightSuper;
    }
    if ((modifiers & SDL_KMOD_NUM) != 0)
    {
        translated |= KeyModifiers::NumLock;
    }
    if ((modifiers & SDL_KMOD_CAPS) != 0)
    {
        translated |= KeyModifiers::CapsLock;
    }
    if ((modifiers & SDL_KMOD_MODE) != 0)
    {
        translated |= KeyModifiers::AltGraph;
    }
    if ((modifiers & SDL_KMOD_SCROLL) != 0)
    {
        translated |= KeyModifiers::ScrollLock;
    }
    return translated;
}

[[nodiscard]] MouseButton TranslateMouseButton(std::uint8_t button) noexcept
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:
        return MouseButton::Left;
    case SDL_BUTTON_RIGHT:
        return MouseButton::Right;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::Middle;
    case SDL_BUTTON_X1:
        return MouseButton::X1;
    case SDL_BUTTON_X2:
        return MouseButton::X2;
    default:
        return MouseButton::Unknown;
    }
}

[[nodiscard]] std::optional<std::string> CopyUtf8Text(const char* text)
{
    if (text == nullptr)
    {
        return std::nullopt;
    }

    std::string ownedText{text};
    if (!IsValidUtf8WithoutEmbeddedNull(ownedText))
    {
        return std::nullopt;
    }

    return ownedText;
}

struct OptionalUtf8Text final
{
    bool valid{};
    std::optional<std::string> text;
};

[[nodiscard]] OptionalUtf8Text CopyOptionalUtf8Text(const char* text)
{
    if (text == nullptr)
    {
        return OptionalUtf8Text{true, std::nullopt};
    }

    std::optional<std::string> ownedText = CopyUtf8Text(text);
    if (!ownedText.has_value())
    {
        return {};
    }

    return OptionalUtf8Text{true, std::move(*ownedText)};
}

[[nodiscard]] std::filesystem::path MakePathFromUtf8(const std::string& utf8Path)
{
    std::u8string pathText;
    pathText.reserve(utf8Path.size());
    for (const char character : utf8Path)
    {
        pathText.push_back(static_cast<char8_t>(
            static_cast<unsigned char>(character)));
    }

    return std::filesystem::path{pathText};
}

[[nodiscard]] std::optional<LogicalPoint> TranslateDropPosition(
    float x, float y) noexcept
{
    const LogicalPoint position{x, y};
    return IsValid(position) ? std::optional<LogicalPoint>{position} : std::nullopt;
}

[[nodiscard]] std::optional<TextCompositionRange> TranslateCompositionRange(
    std::int32_t start, std::int32_t length) noexcept
{
    if (start < 0 || length < 0)
    {
        return std::nullopt;
    }

    return TextCompositionRange{static_cast<std::uint32_t>(start),
                                static_cast<std::uint32_t>(length)};
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

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    {
        const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
        if (event.key.down != pressed)
        {
            return std::nullopt;
        }

        const InputWindowResolution target = ResolveInputWindowId(
            static_cast<std::uint32_t>(event.key.windowID), context);
        if (!target.valid)
        {
            return std::nullopt;
        }

        return PlatformEvent{KeyboardKeyEvent{
            *timestamp,
            target.windowId,
            TranslatePhysicalKey(event.key.scancode),
            TranslateLogicalKey(event.key.key, event.key.scancode),
            TranslateKeyModifiers(event.key.mod),
            pressed,
            event.key.repeat}};
    }

    case SDL_EVENT_MOUSE_MOTION:
    {
        const LogicalPoint position{event.motion.x, event.motion.y};
        const LogicalPoint relativeMovement{event.motion.xrel, event.motion.yrel};
        if (!IsValid(position) || !IsValid(relativeMovement))
        {
            return std::nullopt;
        }

        const InputWindowResolution target = ResolveInputWindowId(
            static_cast<std::uint32_t>(event.motion.windowID), context);
        if (!target.valid)
        {
            return std::nullopt;
        }

        return PlatformEvent{MouseMotionEvent{
            *timestamp, target.windowId, position, relativeMovement}};
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        const bool pressed = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (event.button.down != pressed)
        {
            return std::nullopt;
        }

        const LogicalPoint position{event.button.x, event.button.y};
        if (!IsValid(position))
        {
            return std::nullopt;
        }

        const InputWindowResolution target = ResolveInputWindowId(
            static_cast<std::uint32_t>(event.button.windowID), context);
        if (!target.valid)
        {
            return std::nullopt;
        }

        return PlatformEvent{MouseButtonEvent{
            *timestamp,
            target.windowId,
            position,
            TranslateMouseButton(event.button.button),
            pressed}};
    }

    case SDL_EVENT_MOUSE_WHEEL:
    {
        float horizontal = event.wheel.x;
        float vertical = event.wheel.y;
        switch (event.wheel.direction)
        {
        case SDL_MOUSEWHEEL_NORMAL:
            break;
        case SDL_MOUSEWHEEL_FLIPPED:
            horizontal = -horizontal;
            vertical = -vertical;
            break;
        default:
            return std::nullopt;
        }

        const LogicalPoint position{event.wheel.mouse_x, event.wheel.mouse_y};
        if (!IsValid(position) || !IsValid(LogicalPoint{horizontal, vertical}))
        {
            return std::nullopt;
        }

        const InputWindowResolution target = ResolveInputWindowId(
            static_cast<std::uint32_t>(event.wheel.windowID), context);
        if (!target.valid)
        {
            return std::nullopt;
        }

        return PlatformEvent{MouseWheelEvent{
            *timestamp, target.windowId, position, horizontal, vertical}};
    }

    case SDL_EVENT_TEXT_INPUT:
    {
        const InputWindowResolution target = ResolveInputWindowId(
            static_cast<std::uint32_t>(event.text.windowID), context);
        if (!target.valid)
        {
            return std::nullopt;
        }

        std::optional<std::string> text = CopyUtf8Text(event.text.text);
        if (!text.has_value())
        {
            return std::nullopt;
        }

        return PlatformEvent{
            TextInputEvent{*timestamp, target.windowId, std::move(*text)}};
    }

    case SDL_EVENT_TEXT_EDITING:
    {
        const InputWindowResolution target = ResolveInputWindowId(
            static_cast<std::uint32_t>(event.edit.windowID), context);
        if (!target.valid)
        {
            return std::nullopt;
        }

        std::optional<std::string> text = CopyUtf8Text(event.edit.text);
        if (!text.has_value())
        {
            return std::nullopt;
        }

        return PlatformEvent{TextCompositionEvent{
            *timestamp,
            target.windowId,
            std::move(*text),
            TranslateCompositionRange(event.edit.start, event.edit.length)}};
    }

    case SDL_EVENT_DROP_BEGIN:
    case SDL_EVENT_DROP_FILE:
    case SDL_EVENT_DROP_TEXT:
    case SDL_EVENT_DROP_POSITION:
    case SDL_EVENT_DROP_COMPLETE:
    {
        OptionalUtf8Text sourceApplication =
            CopyOptionalUtf8Text(event.drop.source);
        if (!sourceApplication.valid)
        {
            return std::nullopt;
        }

        if (event.type == SDL_EVENT_DROP_BEGIN)
        {
            const InputWindowResolution target = ResolveInputWindowId(
                static_cast<std::uint32_t>(event.drop.windowID), context);
            if (!target.valid)
            {
                return std::nullopt;
            }

            return PlatformEvent{DropBeginEvent{
                *timestamp, target.windowId,
                std::move(sourceApplication.text)}};
        }

        const std::optional<LogicalPoint> position =
            TranslateDropPosition(event.drop.x, event.drop.y);
        if (!position.has_value())
        {
            return std::nullopt;
        }

        std::optional<std::string> payloadText;
        if (event.type == SDL_EVENT_DROP_FILE ||
            event.type == SDL_EVENT_DROP_TEXT)
        {
            payloadText = CopyUtf8Text(event.drop.data);
            if (!payloadText.has_value())
            {
                return std::nullopt;
            }
        }

        const InputWindowResolution target = ResolveInputWindowId(
            static_cast<std::uint32_t>(event.drop.windowID), context);
        if (!target.valid)
        {
            return std::nullopt;
        }

        switch (event.type)
        {
        case SDL_EVENT_DROP_FILE:
            return PlatformEvent{DroppedFileEvent{
                *timestamp, target.windowId, MakePathFromUtf8(*payloadText),
                *position, std::move(sourceApplication.text)}};
        case SDL_EVENT_DROP_TEXT:
            return PlatformEvent{DroppedTextEvent{
                *timestamp, target.windowId, std::move(*payloadText), *position,
                std::move(sourceApplication.text)}};
        case SDL_EVENT_DROP_POSITION:
            return PlatformEvent{DropPositionEvent{
                *timestamp, target.windowId, *position,
                std::move(sourceApplication.text)}};
        case SDL_EVENT_DROP_COMPLETE:
            return PlatformEvent{DropCompleteEvent{
                *timestamp, target.windowId, *position,
                std::move(sourceApplication.text)}};
        default:
            return std::nullopt;
        }
    }

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
