#pragma once

#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Vector3.hpp>

namespace pond::math
{
namespace detail
{
[[nodiscard]] constexpr bool IsFiniteViewportVector(Vector3 value) noexcept
{
    return ::pond::math::IsFinite(value.x) && ::pond::math::IsFinite(value.y) &&
           ::pond::math::IsFinite(value.z);
}

[[nodiscard]] inline core::Error MakeViewportNonFiniteInputError()
{
    return core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                       "Viewport mapping requires finite input values."};
}

[[nodiscard]] inline core::Error MakeViewportInvalidArgumentError()
{
    return core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                       "Viewport dimensions and depth interval are outside the valid domain."};
}

[[nodiscard]] inline core::Error MakeViewportUnrepresentableResultError()
{
    return core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                       "Viewport mapping result is not representable as finite coordinates."};
}
} // namespace detail

class Viewport final
{
public:
    [[nodiscard]] static core::Result<Viewport> Create(float originX, float originY, float width,
                                                       float height, float minimumDepth = 0.0F,
                                                       float maximumDepth = 1.0F)
    {
        if (!IsFinite(originX) || !IsFinite(originY) || !IsFinite(width) || !IsFinite(height) ||
            !IsFinite(minimumDepth) || !IsFinite(maximumDepth)) [[unlikely]]
        {
            return core::Result<Viewport>::FromError(detail::MakeViewportNonFiniteInputError());
        }

        if (width <= 0.0F || height <= 0.0F || maximumDepth <= minimumDepth) [[unlikely]]
        {
            return core::Result<Viewport>::FromError(detail::MakeViewportInvalidArgumentError());
        }

        return Viewport{originX, originY, width, height, minimumDepth, maximumDepth};
    }

    [[nodiscard]] constexpr float GetOriginX() const noexcept
    {
        return m_originX;
    }

    [[nodiscard]] constexpr float GetOriginY() const noexcept
    {
        return m_originY;
    }

    [[nodiscard]] constexpr float GetWidth() const noexcept
    {
        return m_width;
    }

    [[nodiscard]] constexpr float GetHeight() const noexcept
    {
        return m_height;
    }

    [[nodiscard]] constexpr float GetMinimumDepth() const noexcept
    {
        return m_minimumDepth;
    }

    [[nodiscard]] constexpr float GetMaximumDepth() const noexcept
    {
        return m_maximumDepth;
    }

    [[nodiscard]] friend constexpr bool operator==(const Viewport& lhs,
                                                   const Viewport& rhs) noexcept = default;

private:
    constexpr Viewport(float originX, float originY, float width, float height, float minimumDepth,
                       float maximumDepth) noexcept
        : m_originX(originX), m_originY(originY), m_width(width), m_height(height),
          m_minimumDepth(minimumDepth), m_maximumDepth(maximumDepth)
    {
    }

    float m_originX;
    float m_originY;
    float m_width;
    float m_height;
    float m_minimumDepth;
    float m_maximumDepth;
};

[[nodiscard]] inline core::Result<Vector3> NdcToViewport(Viewport viewport, Vector3 ndc)
{
    if (!detail::IsFiniteViewportVector(ndc)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportNonFiniteInputError());
    }

    const float x = viewport.GetOriginX() + (ndc.x + 1.0F) * viewport.GetWidth() * 0.5F;
    const float y = viewport.GetOriginY() + (1.0F - ndc.y) * viewport.GetHeight() * 0.5F;
    const float depthSpan = viewport.GetMaximumDepth() - viewport.GetMinimumDepth();
    const float z = viewport.GetMinimumDepth() + ndc.z * depthSpan;
    const Vector3 mapped{x, y, z};
    if (!detail::IsFiniteViewportVector(mapped)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportUnrepresentableResultError());
    }

    return mapped;
}

[[nodiscard]] inline core::Result<Vector3> ViewportToNdc(Viewport viewport, Vector3 point)
{
    if (!detail::IsFiniteViewportVector(point)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportNonFiniteInputError());
    }

    const float x = ((point.x - viewport.GetOriginX()) / viewport.GetWidth()) * 2.0F - 1.0F;
    const float y = 1.0F - ((point.y - viewport.GetOriginY()) / viewport.GetHeight()) * 2.0F;
    const float depthSpan = viewport.GetMaximumDepth() - viewport.GetMinimumDepth();
    const float z = (point.z - viewport.GetMinimumDepth()) / depthSpan;
    const Vector3 ndc{x, y, z};
    if (!detail::IsFiniteViewportVector(ndc)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportUnrepresentableResultError());
    }

    return ndc;
}

[[nodiscard]] inline core::Result<Vector3> Project(Matrix4x4 worldToClip, Viewport viewport,
                                                   Vector3 point)
{
    auto ndc = TransformPointToNdc(worldToClip, point);
    if (!ndc.HasValue()) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(ndc.GetError());
    }

    return NdcToViewport(viewport, ndc.GetValue());
}

[[nodiscard]] inline core::Result<Vector3> Unproject(Matrix4x4 worldToClip, Viewport viewport,
                                                     Vector3 point)
{
    auto ndc = ViewportToNdc(viewport, point);
    if (!ndc.HasValue()) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(ndc.GetError());
    }

    auto clipToWorld = Inverse(worldToClip);
    if (!clipToWorld.HasValue()) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(clipToWorld.GetError());
    }

    return PerspectiveDivide(clipToWorld.GetValue() * Vector4{ndc->x, ndc->y, ndc->z, 1.0F});
}
} // namespace pond::math