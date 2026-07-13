#pragma once

#include <ponder/math/MathError.hpp>
#include <ponder/math/Vector3.hpp>

namespace pond::math
{
namespace detail
{
[[nodiscard]] constexpr bool IsFiniteAxisAlignedBoxVector(Vector3 value) noexcept
{
    return ::pond::core::IsFinite(value.x) && ::pond::core::IsFinite(value.y) &&
           ::pond::core::IsFinite(value.z);
}
} // namespace detail

class AxisAlignedBox final
{
public:
    [[nodiscard]] static constexpr core::Result<AxisAlignedBox> Create(Vector3 minimum,
                                                                       Vector3 maximum)
    {
        if (!detail::IsFiniteAxisAlignedBoxVector(minimum) ||
            !detail::IsFiniteAxisAlignedBoxVector(maximum)) [[unlikely]]
        {
            return core::Result<AxisAlignedBox>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "AxisAlignedBox construction requires finite bounds."});
        }

        if (maximum.x < minimum.x || maximum.y < minimum.y || maximum.z < minimum.z) [[unlikely]]
        {
            return core::Result<AxisAlignedBox>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument),
                "AxisAlignedBox construction requires ordered minimum and maximum bounds."});
        }

        return AxisAlignedBox{minimum, maximum};
    }

    [[nodiscard]] constexpr Vector3 GetMinimum() const noexcept
    {
        return m_minimum;
    }

    [[nodiscard]] constexpr Vector3 GetMaximum() const noexcept
    {
        return m_maximum;
    }

    [[nodiscard]] friend constexpr bool operator==(const AxisAlignedBox& lhs,
                                                   const AxisAlignedBox& rhs) noexcept = default;

private:
    constexpr AxisAlignedBox(Vector3 minimum, Vector3 maximum) noexcept
        : m_minimum(minimum), m_maximum(maximum)
    {
    }

    Vector3 m_minimum;
    Vector3 m_maximum;
};
} // namespace pond::math