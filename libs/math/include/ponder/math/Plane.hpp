#pragma once

#include <ponder/math/MathError.hpp>
#include <ponder/math/Vector3.hpp>

#include <cmath>

namespace pond::math
{
namespace detail
{
[[nodiscard]] constexpr bool IsFinitePlaneVector(Vector3 value) noexcept
{
    return ::pond::core::IsFinite(value.x) && ::pond::core::IsFinite(value.y) &&
           ::pond::core::IsFinite(value.z);
}
} // namespace detail

class Plane final
{
public:
    [[nodiscard]] static inline core::Result<Plane> Create(Vector3 normal, float offset)
    {
        if (!detail::IsFinitePlaneVector(normal) || !core::IsFinite(offset)) [[unlikely]]
        {
            return core::Result<Plane>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "Plane construction requires finite normal and offset."});
        }

        const double x = static_cast<double>(normal.x);
        const double y = static_cast<double>(normal.y);
        const double z = static_cast<double>(normal.z);
        const double length = std::sqrt(x * x + y * y + z * z);
        if (length == 0.0) [[unlikely]]
        {
            return core::Result<Plane>::FromError(
                core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                            "Plane construction requires a non-zero normal."});
        }

        const Vector3 normalized{static_cast<float>(x / length), static_cast<float>(y / length),
                                 static_cast<float>(z / length)};
        const float adjustedOffset = static_cast<float>(static_cast<double>(offset) / length);
        if (!detail::IsFinitePlaneVector(normalized) || !core::IsFinite(adjustedOffset))
            [[unlikely]]
        {
            return core::Result<Plane>::FromError(core::Error{
                ToErrorCode(MathErrorCode::DegenerateInput),
                "Plane construction result is not representable as finite coefficients."});
        }

        return Plane{normalized, adjustedOffset};
    }

    [[nodiscard]] constexpr Vector3 GetNormal() const noexcept
    {
        return m_normal;
    }

    [[nodiscard]] constexpr float GetOffset() const noexcept
    {
        return m_offset;
    }

    [[nodiscard]] friend constexpr bool operator==(const Plane& lhs,
                                                   const Plane& rhs) noexcept = default;

private:
    constexpr Plane(Vector3 normal, float offset) noexcept : m_normal(normal), m_offset(offset) {}

    Vector3 m_normal;
    float m_offset;
};
} // namespace pond::math