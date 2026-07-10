#pragma once

#include <ponder/math/Scalar.hpp>

namespace pond::math
{
class Radians final
{
public:
    constexpr Radians() noexcept = default;

    explicit constexpr Radians(float value) noexcept : m_value(value) {}

    [[nodiscard]] constexpr float GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr bool operator==(const Radians& lhs,
                                                   const Radians& rhs) noexcept = default;

private:
    float m_value{0.0F};
};

class Degrees final
{
public:
    constexpr Degrees() noexcept = default;

    explicit constexpr Degrees(float value) noexcept : m_value(value) {}

    [[nodiscard]] constexpr float GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr bool operator==(const Degrees& lhs,
                                                   const Degrees& rhs) noexcept = default;

private:
    float m_value{0.0F};
};

[[nodiscard]] constexpr Radians ToRadians(Degrees degrees) noexcept
{
    return Radians{degrees.GetValue() * (kPi / 180.0F)};
}

[[nodiscard]] constexpr Degrees ToDegrees(Radians radians) noexcept
{
    return Degrees{radians.GetValue() * (180.0F / kPi)};
}
} // namespace pond::math
