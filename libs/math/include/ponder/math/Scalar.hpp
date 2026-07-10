#pragma once

#include <ponder/math/MathError.hpp>

#include <limits>

namespace pond::math
{
inline constexpr float kPi{0x1.921fb6p+1F};
inline constexpr float kTwoPi{0x1.921fb6p+2F};
inline constexpr float kHalfPi{0x1.921fb6p+0F};

[[nodiscard]] constexpr bool IsFinite(float value) noexcept
{
    return value >= -std::numeric_limits<float>::max() &&
           value <= std::numeric_limits<float>::max();
}

class Tolerance final
{
public:
    [[nodiscard]] static core::Result<Tolerance> Create(float absoluteTolerance,
                                                        float relativeTolerance)
    {
        if (!IsFinite(absoluteTolerance) || !IsFinite(relativeTolerance)) [[unlikely]]
        {
            return core::Result<Tolerance>::FromError(core::Error{
                ToErrorCode(MathErrorCode::NonFiniteInput), "Tolerance values must be finite."});
        }

        if (absoluteTolerance < 0.0F || relativeTolerance < 0.0F) [[unlikely]]
        {
            return core::Result<Tolerance>::FromError(
                core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                            "Tolerance values must be non-negative."});
        }

        return Tolerance{absoluteTolerance, relativeTolerance};
    }

    [[nodiscard]] constexpr float GetAbsolute() const noexcept
    {
        return m_absolute;
    }

    [[nodiscard]] constexpr float GetRelative() const noexcept
    {
        return m_relative;
    }

    [[nodiscard]] friend constexpr bool operator==(const Tolerance& lhs,
                                                   const Tolerance& rhs) noexcept = default;

private:
    constexpr Tolerance(float absoluteTolerance, float relativeTolerance) noexcept
        : m_absolute(absoluteTolerance), m_relative(relativeTolerance)
    {
    }

    float m_absolute;
    float m_relative;
};

[[nodiscard]] constexpr float Clamp(float value, float minimum, float maximum) noexcept
{
    if (value < minimum)
    {
        return minimum;
    }

    if (maximum < value)
    {
        return maximum;
    }

    return value;
}

[[nodiscard]] constexpr float Lerp(float start, float end, float amount) noexcept
{
    return start + (end - start) * amount;
}

[[nodiscard]] constexpr bool IsNear(float lhs, float rhs, Tolerance tolerance) noexcept
{
    if (!IsFinite(lhs) || !IsFinite(rhs))
    {
        return false;
    }

    const auto abs = [](float value) constexpr noexcept
    {
        return value < 0.0F ? -value : value;
    };

    const auto max = [](float left, float right) constexpr noexcept
    {
        return left < right ? right : left;
    };

    const float difference = abs(lhs - rhs);
    const float magnitude = max(abs(lhs), abs(rhs));
    const float allowed = max(tolerance.GetAbsolute(), tolerance.GetRelative() * magnitude);
    return difference <= allowed;
}
} // namespace pond::math
