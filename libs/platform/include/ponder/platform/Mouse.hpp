#pragma once

#include <cstdint>

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
