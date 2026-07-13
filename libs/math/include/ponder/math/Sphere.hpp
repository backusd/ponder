#pragma once

#include <ponder/math/MathError.hpp>
#include <ponder/math/Vector3.hpp>

namespace pond::math
{
namespace detail
{
[[nodiscard]] constexpr bool IsFiniteSphereVector(Vector3 value) noexcept
{
    return ::pond::core::IsFinite(value.x) && ::pond::core::IsFinite(value.y) &&
           ::pond::core::IsFinite(value.z);
}
} // namespace detail

class Sphere final
{
public:
    [[nodiscard]] static constexpr core::Result<Sphere> Create(Vector3 center, float radius)
    {
        if (!detail::IsFiniteSphereVector(center) || !core::IsFinite(radius)) [[unlikely]]
        {
            return core::Result<Sphere>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "Sphere construction requires finite center and radius."});
        }

        if (radius < 0.0F) [[unlikely]]
        {
            return core::Result<Sphere>::FromError(
                core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                            "Sphere construction requires a non-negative radius."});
        }

        return Sphere{center, radius};
    }

    [[nodiscard]] constexpr Vector3 GetCenter() const noexcept
    {
        return m_center;
    }

    [[nodiscard]] constexpr float GetRadius() const noexcept
    {
        return m_radius;
    }

    [[nodiscard]] friend constexpr bool operator==(const Sphere& lhs,
                                                   const Sphere& rhs) noexcept = default;

private:
    constexpr Sphere(Vector3 center, float radius) noexcept : m_center(center), m_radius(radius) {}

    Vector3 m_center;
    float m_radius;
};
} // namespace pond::math