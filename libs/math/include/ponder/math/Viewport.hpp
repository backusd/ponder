#pragma once

#include <ponder/math/MathError.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Vector3.hpp>

namespace pond::math
{
namespace detail
{
[[nodiscard]] constexpr bool IsFiniteViewportVector(Vector3 value) noexcept
{
    return ::pond::core::IsFinite(value.x) && ::pond::core::IsFinite(value.y) &&
           ::pond::core::IsFinite(value.z);
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
    [[nodiscard]] static constexpr core::Result<Viewport> Create(float originX, float originY,
                                                                 float width, float height,
                                                                 float minimumDepth = 0.0F,
                                                                 float maximumDepth = 1.0F)
    {
        if (!core::IsFinite(originX) || !core::IsFinite(originY) || !core::IsFinite(width) ||
            !core::IsFinite(height) || !core::IsFinite(minimumDepth) ||
            !core::IsFinite(maximumDepth)) [[unlikely]]
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

[[nodiscard]] constexpr core::Result<Vector3> NdcToViewport(Viewport viewport, Vector3 ndc)
{
    if (!detail::IsFiniteViewportVector(ndc)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportNonFiniteInputError());
    }

    const double depthSpan = static_cast<double>(viewport.GetMaximumDepth()) -
                             static_cast<double>(viewport.GetMinimumDepth());
    const double mappedX =
        static_cast<double>(viewport.GetOriginX()) +
        (static_cast<double>(ndc.x) + 1.0) * static_cast<double>(viewport.GetWidth()) * 0.5;
    const double mappedY =
        static_cast<double>(viewport.GetOriginY()) +
        (1.0 - static_cast<double>(ndc.y)) * static_cast<double>(viewport.GetHeight()) * 0.5;
    const double mappedZ =
        static_cast<double>(viewport.GetMinimumDepth()) + static_cast<double>(ndc.z) * depthSpan;

    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    if (!detail::TryConvertFiniteFloat(mappedX, x) || !detail::TryConvertFiniteFloat(mappedY, y) ||
        !detail::TryConvertFiniteFloat(mappedZ, z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportUnrepresentableResultError());
    }

    return Vector3{x, y, z};
}

[[nodiscard]] constexpr core::Result<Vector3> ViewportToNdc(Viewport viewport, Vector3 point)
{
    if (!detail::IsFiniteViewportVector(point)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportNonFiniteInputError());
    }

    const double depthSpan = static_cast<double>(viewport.GetMaximumDepth()) -
                             static_cast<double>(viewport.GetMinimumDepth());
    const double ndcX =
        ((static_cast<double>(point.x) - static_cast<double>(viewport.GetOriginX())) /
         static_cast<double>(viewport.GetWidth())) *
            2.0 -
        1.0;
    const double ndcY =
        1.0 - ((static_cast<double>(point.y) - static_cast<double>(viewport.GetOriginY())) /
               static_cast<double>(viewport.GetHeight())) *
                  2.0;
    const double ndcZ =
        (static_cast<double>(point.z) - static_cast<double>(viewport.GetMinimumDepth())) /
        depthSpan;

    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    if (!detail::TryConvertFiniteFloat(ndcX, x) || !detail::TryConvertFiniteFloat(ndcY, y) ||
        !detail::TryConvertFiniteFloat(ndcZ, z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportUnrepresentableResultError());
    }

    return Vector3{x, y, z};
}

[[nodiscard]] constexpr core::Result<Vector3> Project(Matrix4x4 worldToClip, Viewport viewport,
                                                      Vector3 point)
{
    auto ndc = TransformPointToNdc(worldToClip, point);
    if (!ndc.HasValue()) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(ndc.GetError());
    }

    return NdcToViewport(viewport, ndc.GetValue());
}

[[nodiscard]] constexpr core::Result<Vector3> UnprojectFromClipToWorld(Matrix4x4 clipToWorld,
                                                                       Viewport viewport,
                                                                       Vector3 point)
{
    auto ndc = ViewportToNdc(viewport, point);
    if (!ndc.HasValue()) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(ndc.GetError());
    }

    if (!IsFinite(clipToWorld)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(detail::MakeViewportNonFiniteInputError());
    }

    return TransformPointToNdc(clipToWorld, ndc.GetValue());
}

[[nodiscard]] constexpr core::Result<Vector3> Unproject(Matrix4x4 worldToClip, Viewport viewport,
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

    return TransformPointToNdc(clipToWorld.GetValue(), ndc.GetValue());
}
} // namespace pond::math
