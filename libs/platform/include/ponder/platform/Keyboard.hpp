#pragma once

#include <cstdint>
#include <optional>

namespace pond::platform
{
enum class PhysicalKey : std::uint16_t
{
    Unknown,
    Tab,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    PageUp,
    PageDown,
    Home,
    End,
    Insert,
    Delete,
    Backspace,
    Space,
    Enter,
    Escape,
    LeftControl,
    LeftShift,
    LeftAlt,
    LeftSuper,
    RightControl,
    RightShift,
    RightAlt,
    RightSuper,
    Menu,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    Apostrophe,
    Comma,
    Minus,
    Period,
    Slash,
    Semicolon,
    Equal,
    LeftBracket,
    Backslash,
    RightBracket,
    GraveAccent,
    CapsLock,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    KeypadDecimal,
    KeypadDivide,
    KeypadMultiply,
    KeypadSubtract,
    KeypadAdd,
    KeypadEnter,
    KeypadEqual,
    BrowserBack,
    BrowserForward,
    NonUsBackslash
};

enum class NamedKey : std::uint16_t
{
    Unknown,
    Tab,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    PageUp,
    PageDown,
    Home,
    End,
    Insert,
    Delete,
    Backspace,
    Enter,
    Escape,
    LeftControl,
    LeftShift,
    LeftAlt,
    LeftSuper,
    RightControl,
    RightShift,
    RightAlt,
    RightSuper,
    Menu,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    CapsLock,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    KeypadDecimal,
    KeypadDivide,
    KeypadMultiply,
    KeypadSubtract,
    KeypadAdd,
    KeypadEnter,
    KeypadEqual,
    BrowserBack,
    BrowserForward
};

class LogicalKey final
{
public:
    enum class Kind : std::uint8_t
    {
        Unknown,
        Character,
        Named
    };

    constexpr LogicalKey() noexcept = default;

    [[nodiscard]] static constexpr LogicalKey Unknown() noexcept
    {
        return LogicalKey{};
    }

    [[nodiscard]] static constexpr LogicalKey FromCharacter(char32_t character) noexcept
    {
        const auto value = static_cast<std::uint32_t>(character);
        if (value == 0U || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
        {
            return Unknown();
        }

        return LogicalKey{Kind::Character, character, NamedKey::Unknown};
    }

    [[nodiscard]] static constexpr LogicalKey FromNamed(NamedKey namedKey) noexcept
    {
        const auto value = static_cast<std::uint16_t>(namedKey);
        if (value == 0U || value > static_cast<std::uint16_t>(NamedKey::BrowserForward))
        {
            return Unknown();
        }

        return LogicalKey{Kind::Named, U'\0', namedKey};
    }

    [[nodiscard]] constexpr Kind GetKind() const noexcept
    {
        return m_kind;
    }

    [[nodiscard]] constexpr std::optional<char32_t> GetCharacter() const noexcept
    {
        if (m_kind != Kind::Character)
        {
            return std::nullopt;
        }
        return m_character;
    }

    [[nodiscard]] constexpr std::optional<NamedKey> GetNamedKey() const noexcept
    {
        if (m_kind != Kind::Named)
        {
            return std::nullopt;
        }
        return m_namedKey;
    }

    [[nodiscard]] friend constexpr bool operator==(const LogicalKey& lhs,
                                                   const LogicalKey& rhs) noexcept = default;

private:
    constexpr LogicalKey(Kind kind, char32_t character, NamedKey namedKey) noexcept
        : m_kind(kind), m_character(character), m_namedKey(namedKey)
    {
    }

    Kind m_kind{Kind::Unknown};
    char32_t m_character{};
    NamedKey m_namedKey{NamedKey::Unknown};
};

enum class KeyModifiers : std::uint16_t
{
    None = 0,
    LeftShift = 1U << 0U,
    RightShift = 1U << 1U,
    Level5Shift = 1U << 2U,
    LeftControl = 1U << 3U,
    RightControl = 1U << 4U,
    LeftAlt = 1U << 5U,
    RightAlt = 1U << 6U,
    LeftSuper = 1U << 7U,
    RightSuper = 1U << 8U,
    NumLock = 1U << 9U,
    CapsLock = 1U << 10U,
    AltGraph = 1U << 11U,
    ScrollLock = 1U << 12U,
    Shift = (1U << 0U) | (1U << 1U),
    Control = (1U << 3U) | (1U << 4U),
    Alt = (1U << 5U) | (1U << 6U),
    Super = (1U << 7U) | (1U << 8U)
};

[[nodiscard]] constexpr KeyModifiers operator|(KeyModifiers lhs, KeyModifiers rhs) noexcept
{
    return static_cast<KeyModifiers>(static_cast<std::uint16_t>(lhs) |
                                     static_cast<std::uint16_t>(rhs));
}

[[nodiscard]] constexpr KeyModifiers operator&(KeyModifiers lhs, KeyModifiers rhs) noexcept
{
    return static_cast<KeyModifiers>(static_cast<std::uint16_t>(lhs) &
                                     static_cast<std::uint16_t>(rhs));
}

constexpr KeyModifiers& operator|=(KeyModifiers& lhs, KeyModifiers rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr KeyModifiers& operator&=(KeyModifiers& lhs, KeyModifiers rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

[[nodiscard]] constexpr bool HasAnyKeyModifier(KeyModifiers modifiers, KeyModifiers mask) noexcept
{
    return (modifiers & mask) != KeyModifiers::None;
}

[[nodiscard]] constexpr bool HasAllKeyModifiers(KeyModifiers modifiers, KeyModifiers mask) noexcept
{
    return (modifiers & mask) == mask;
}
} // namespace pond::platform
