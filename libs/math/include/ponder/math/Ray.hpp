#pragma once

#include <ponder/math/MathError.hpp>
#include <ponder/math/Vector3.hpp>

namespace pond::math
{
namespace detail
{
[[nodiscard]] constexpr bool IsFiniteRayVector(Vector3 value) noexcept
{
    return ::pond::core::IsFinite(value.x) && ::pond::core::IsFinite(value.y) &&
           ::pond::core::IsFinite(value.z);
}
} // namespace detail

class Ray final
{
public:
    [[nodiscard]] static inline core::Result<Ray> Create(Vector3 origin, Vector3 direction)
    {
        if (!detail::IsFiniteRayVector(origin) || !detail::IsFiniteRayVector(direction))
            [[unlikely]]
        {
            return core::Result<Ray>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "Ray construction requires finite origin and direction."});
        }

        auto normalizedDirection = Normalize(direction);
        if (!normalizedDirection.HasValue()) [[unlikely]]
        {
            return core::Result<Ray>::FromError(normalizedDirection.GetError());
        }

        return Ray{origin, normalizedDirection.GetValue()};
    }

    [[nodiscard]] constexpr Vector3 GetOrigin() const noexcept
    {
        return m_origin;
    }

    [[nodiscard]] constexpr Vector3 GetDirection() const noexcept
    {
        return m_direction;
    }

    [[nodiscard]] friend constexpr bool operator==(const Ray& lhs,
                                                   const Ray& rhs) noexcept = default;

private:
    constexpr Ray(Vector3 origin, Vector3 direction) noexcept
        : m_origin(origin), m_direction(direction)
    {
    }

    Vector3 m_origin;
    Vector3 m_direction;
};
} // namespace pond::math