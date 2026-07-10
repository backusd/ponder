#pragma once

#include <ponder/math/Vector3.hpp>

namespace pond::math
{
namespace detail
{
[[nodiscard]] constexpr bool IsFiniteTriangleVector(Vector3 value) noexcept
{
    return ::pond::math::IsFinite(value.x) && ::pond::math::IsFinite(value.y) &&
           ::pond::math::IsFinite(value.z);
}
} // namespace detail

class Triangle final
{
public:
    [[nodiscard]] static inline core::Result<Triangle> Create(Vector3 vertex0, Vector3 vertex1,
                                                              Vector3 vertex2)
    {
        if (!detail::IsFiniteTriangleVector(vertex0) || !detail::IsFiniteTriangleVector(vertex1) ||
            !detail::IsFiniteTriangleVector(vertex2)) [[unlikely]]
        {
            return core::Result<Triangle>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "Triangle construction requires finite vertices."});
        }

        return Triangle{vertex0, vertex1, vertex2};
    }

    [[nodiscard]] constexpr Vector3 GetVertex0() const noexcept
    {
        return m_vertex0;
    }

    [[nodiscard]] constexpr Vector3 GetVertex1() const noexcept
    {
        return m_vertex1;
    }

    [[nodiscard]] constexpr Vector3 GetVertex2() const noexcept
    {
        return m_vertex2;
    }

    [[nodiscard]] friend constexpr bool operator==(const Triangle& lhs,
                                                   const Triangle& rhs) noexcept = default;

private:
    constexpr Triangle(Vector3 vertex0, Vector3 vertex1, Vector3 vertex2) noexcept
        : m_vertex0(vertex0), m_vertex1(vertex1), m_vertex2(vertex2)
    {
    }

    Vector3 m_vertex0;
    Vector3 m_vertex1;
    Vector3 m_vertex2;
};
} // namespace pond::math