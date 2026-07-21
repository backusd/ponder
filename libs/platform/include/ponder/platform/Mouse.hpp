#pragma once

#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <string_view>

namespace pond::platform
{
struct MousePosition final
{
    float x{};
    float y{};

    friend constexpr bool operator==(const MousePosition&, const MousePosition&) noexcept = default;
};

enum class MouseButton : std::uint8_t
{
    Unknown,
    Left,
    Right,
    Middle,
    X1,
    X2
};

enum class SystemCursorShape : std::uint8_t
{
    Default,
    TextInput,
    Move,
    ResizeNorthSouth,
    ResizeEastWest,
    ResizeNortheastSouthwest,
    ResizeNorthwestSoutheast,
    Pointer,
    Wait,
    Progress,
    NotAllowed
};
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::MousePosition> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::MousePosition position, FormatContext& context) const
    {
        return formatter<string>::format(std::format("({}, {})", position.x, position.y), context);
    }
};

template <>
struct formatter<pond::platform::MouseButton> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::MouseButton button, FormatContext& context) const
    {
        using pond::platform::MouseButton;

        string_view name{"unknown"};
        switch (button)
        {
        case MouseButton::Unknown:
            break;
        case MouseButton::Left:
            name = "left";
            break;
        case MouseButton::Right:
            name = "right";
            break;
        case MouseButton::Middle:
            name = "middle";
            break;
        case MouseButton::X1:
            name = "x1";
            break;
        case MouseButton::X2:
            name = "x2";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};

template <>
struct formatter<pond::platform::SystemCursorShape> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::SystemCursorShape shape, FormatContext& context) const
    {
        using pond::platform::SystemCursorShape;

        string_view name{"unknown"};
        switch (shape)
        {
        case SystemCursorShape::Default:
            name = "default";
            break;
        case SystemCursorShape::TextInput:
            name = "text_input";
            break;
        case SystemCursorShape::Move:
            name = "move";
            break;
        case SystemCursorShape::ResizeNorthSouth:
            name = "resize_north_south";
            break;
        case SystemCursorShape::ResizeEastWest:
            name = "resize_east_west";
            break;
        case SystemCursorShape::ResizeNortheastSouthwest:
            name = "resize_northeast_southwest";
            break;
        case SystemCursorShape::ResizeNorthwestSoutheast:
            name = "resize_northwest_southeast";
            break;
        case SystemCursorShape::Pointer:
            name = "pointer";
            break;
        case SystemCursorShape::Wait:
            name = "wait";
            break;
        case SystemCursorShape::Progress:
            name = "progress";
            break;
        case SystemCursorShape::NotAllowed:
            name = "not_allowed";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};
} // namespace std

namespace pond::platform
{
inline std::ostream& operator<<(std::ostream& output, MousePosition position)
{
    return output << std::format("{}", position);
}

inline std::ostream& operator<<(std::ostream& output, MouseButton button)
{
    return output << std::format("{}", button);
}

inline std::ostream& operator<<(std::ostream& output, SystemCursorShape shape)
{
    return output << std::format("{}", shape);
}
} // namespace pond::platform