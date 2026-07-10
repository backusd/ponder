#pragma once

#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/CollisionClassification.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Plane.hpp>
#include <ponder/math/Sphere.hpp>

#include <limits>
#include <optional>

namespace pond::math
{
namespace detail
{
inline constexpr double kFrustumPlaneTestErrorMultiplier{8.0};

[[nodiscard]] constexpr double AbsFrustumTestValue(double value) noexcept
{
    return value < 0.0 ? -value : value;
}

[[nodiscard]] constexpr Vector4 FrustumRow0(Matrix4x4 matrix) noexcept
{
    return Vector4{matrix.row0Column0, matrix.row0Column1, matrix.row0Column2, matrix.row0Column3};
}

[[nodiscard]] constexpr Vector4 FrustumRow1(Matrix4x4 matrix) noexcept
{
    return Vector4{matrix.row1Column0, matrix.row1Column1, matrix.row1Column2, matrix.row1Column3};
}

[[nodiscard]] constexpr Vector4 FrustumRow2(Matrix4x4 matrix) noexcept
{
    return Vector4{matrix.row2Column0, matrix.row2Column1, matrix.row2Column2, matrix.row2Column3};
}

[[nodiscard]] constexpr Vector4 FrustumRow3(Matrix4x4 matrix) noexcept
{
    return Vector4{matrix.row3Column0, matrix.row3Column1, matrix.row3Column2, matrix.row3Column3};
}

[[nodiscard]] constexpr Vector3 FrustumNormal(Vector4 coefficients) noexcept
{
    return Vector3{coefficients.x, coefficients.y, coefficients.z};
}

[[nodiscard]] constexpr bool HasZeroFrustumNormal(Vector4 coefficients) noexcept
{
    return coefficients.x == 0.0F && coefficients.y == 0.0F && coefficients.z == 0.0F;
}

struct FrustumPlaneTest final
{
    double distance{0.0};
    double errorBound{0.0};
};

[[nodiscard]] constexpr FrustumPlaneTest TestPointAgainstFrustumPlane(
    const Plane& plane, Vector3 point, double extraMagnitude = 0.0) noexcept
{
    const Vector3 normal = plane.GetNormal();
    const double xTerm = static_cast<double>(normal.x) * static_cast<double>(point.x);
    const double yTerm = static_cast<double>(normal.y) * static_cast<double>(point.y);
    const double zTerm = static_cast<double>(normal.z) * static_cast<double>(point.z);
    const double offset = static_cast<double>(plane.GetOffset());
    const double magnitude = AbsFrustumTestValue(xTerm) + AbsFrustumTestValue(yTerm) +
                             AbsFrustumTestValue(zTerm) + AbsFrustumTestValue(offset) +
                             extraMagnitude;
    const double errorBound =
        kFrustumPlaneTestErrorMultiplier * std::numeric_limits<float>::epsilon() * magnitude;

    return FrustumPlaneTest{xTerm + yTerm + zTerm + offset, errorBound};
}

[[nodiscard]] constexpr Vector3 SelectPositiveFrustumBoxVertex(const Plane& plane,
                                                               const AxisAlignedBox& box) noexcept
{
    const Vector3 normal = plane.GetNormal();
    const Vector3 minimum = box.GetMinimum();
    const Vector3 maximum = box.GetMaximum();

    return Vector3{normal.x >= 0.0F ? maximum.x : minimum.x,
                   normal.y >= 0.0F ? maximum.y : minimum.y,
                   normal.z >= 0.0F ? maximum.z : minimum.z};
}

[[nodiscard]] constexpr Vector3 SelectNegativeFrustumBoxVertex(const Plane& plane,
                                                               const AxisAlignedBox& box) noexcept
{
    const Vector3 normal = plane.GetNormal();
    const Vector3 minimum = box.GetMinimum();
    const Vector3 maximum = box.GetMaximum();

    return Vector3{normal.x >= 0.0F ? minimum.x : maximum.x,
                   normal.y >= 0.0F ? minimum.y : maximum.y,
                   normal.z >= 0.0F ? minimum.z : maximum.z};
}

[[nodiscard]] constexpr CollisionClassification ClassifyBoxAgainstFrustumPlane(
    const Plane& plane, const AxisAlignedBox& box) noexcept
{
    const FrustumPlaneTest positive =
        TestPointAgainstFrustumPlane(plane, SelectPositiveFrustumBoxVertex(plane, box));
    if (positive.distance < -positive.errorBound)
    {
        return CollisionClassification::Disjoint;
    }

    const FrustumPlaneTest negative =
        TestPointAgainstFrustumPlane(plane, SelectNegativeFrustumBoxVertex(plane, box));
    if (negative.distance <= negative.errorBound)
    {
        return CollisionClassification::Intersects;
    }

    return CollisionClassification::Contains;
}

[[nodiscard]] constexpr CollisionClassification ClassifySphereAgainstFrustumPlane(
    const Plane& plane, const Sphere& sphere) noexcept
{
    const double radius = static_cast<double>(sphere.GetRadius());
    const FrustumPlaneTest center = TestPointAgainstFrustumPlane(plane, sphere.GetCenter(), radius);

    if (center.distance + radius < -center.errorBound)
    {
        return CollisionClassification::Disjoint;
    }

    if (center.distance - radius <= center.errorBound)
    {
        return CollisionClassification::Intersects;
    }

    return CollisionClassification::Contains;
}

[[nodiscard]] constexpr CollisionClassification MergeFrustumPlaneClassification(
    CollisionClassification current, CollisionClassification plane) noexcept
{
    if (plane == CollisionClassification::Disjoint)
    {
        return CollisionClassification::Disjoint;
    }

    if (plane == CollisionClassification::Intersects)
    {
        return CollisionClassification::Intersects;
    }

    return current;
}

[[nodiscard]] inline core::Result<Plane> CreateRequiredFrustumPlane(Vector4 coefficients)
{
    return Plane::Create(FrustumNormal(coefficients), coefficients.w);
}

[[nodiscard]] inline core::Error MakeFrustumPlaneConstructionError(core::ErrorCode code)
{
    return core::Error{code, "Frustum construction requires finite, non-degenerate clip planes."};
}

[[nodiscard]] inline core::Result<std::optional<Plane>> CreateOptionalFrustumPlane(
    Vector4 coefficients)
{
    if (HasZeroFrustumNormal(coefficients))
    {
        if (coefficients.w < 0.0F) [[unlikely]]
        {
            return core::Result<std::optional<Plane>>::FromError(
                core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                            "Frustum depth inequality is unsatisfiable."});
        }

        return std::optional<Plane>{};
    }

    auto plane = Plane::Create(FrustumNormal(coefficients), coefficients.w);
    if (!plane.HasValue()) [[unlikely]]
    {
        return core::Result<std::optional<Plane>>::FromError(
            MakeFrustumPlaneConstructionError(plane.GetError().GetCode()));
    }

    return std::optional<Plane>{plane.GetValue()};
}
} // namespace detail

class Frustum final
{
public:
    [[nodiscard]] static inline core::Result<Frustum> FromWorldToClip(Matrix4x4 worldToClip)
    {
        if (!IsFinite(worldToClip)) [[unlikely]]
        {
            return core::Result<Frustum>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "Frustum construction requires a finite world-to-clip matrix."});
        }

        const Vector4 row0 = detail::FrustumRow0(worldToClip);
        const Vector4 row1 = detail::FrustumRow1(worldToClip);
        const Vector4 row2 = detail::FrustumRow2(worldToClip);
        const Vector4 row3 = detail::FrustumRow3(worldToClip);

        auto left = detail::CreateRequiredFrustumPlane(row3 + row0);
        auto right = detail::CreateRequiredFrustumPlane(row3 - row0);
        auto bottom = detail::CreateRequiredFrustumPlane(row3 + row1);
        auto top = detail::CreateRequiredFrustumPlane(row3 - row1);
        auto minimumDepth = detail::CreateOptionalFrustumPlane(row2);
        auto maximumDepth = detail::CreateOptionalFrustumPlane(row3 - row2);

        if (!left.HasValue()) [[unlikely]]
        {
            return core::Result<Frustum>::FromError(
                detail::MakeFrustumPlaneConstructionError(left.GetError().GetCode()));
        }
        if (!right.HasValue()) [[unlikely]]
        {
            return core::Result<Frustum>::FromError(
                detail::MakeFrustumPlaneConstructionError(right.GetError().GetCode()));
        }
        if (!bottom.HasValue()) [[unlikely]]
        {
            return core::Result<Frustum>::FromError(
                detail::MakeFrustumPlaneConstructionError(bottom.GetError().GetCode()));
        }
        if (!top.HasValue()) [[unlikely]]
        {
            return core::Result<Frustum>::FromError(
                detail::MakeFrustumPlaneConstructionError(top.GetError().GetCode()));
        }
        if (!minimumDepth.HasValue()) [[unlikely]]
        {
            return core::Result<Frustum>::FromError(minimumDepth.GetError());
        }
        if (!maximumDepth.HasValue()) [[unlikely]]
        {
            return core::Result<Frustum>::FromError(maximumDepth.GetError());
        }

        return Frustum{left.GetValue(), right.GetValue(),        bottom.GetValue(),
                       top.GetValue(),  minimumDepth.GetValue(), maximumDepth.GetValue()};
    }

    [[nodiscard]] constexpr CollisionClassification Classify(
        const AxisAlignedBox& box) const noexcept
    {
        CollisionClassification result = CollisionClassification::Contains;

        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifyBoxAgainstFrustumPlane(m_left, box));
        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifyBoxAgainstFrustumPlane(m_right, box));
        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifyBoxAgainstFrustumPlane(m_bottom, box));
        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifyBoxAgainstFrustumPlane(m_top, box));

        if (m_minimumDepth.has_value())
        {
            result = detail::MergeFrustumPlaneClassification(
                result, detail::ClassifyBoxAgainstFrustumPlane(*m_minimumDepth, box));
        }
        if (m_maximumDepth.has_value())
        {
            result = detail::MergeFrustumPlaneClassification(
                result, detail::ClassifyBoxAgainstFrustumPlane(*m_maximumDepth, box));
        }

        return result;
    }

    [[nodiscard]] constexpr CollisionClassification Classify(const Sphere& sphere) const noexcept
    {
        CollisionClassification result = CollisionClassification::Contains;

        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifySphereAgainstFrustumPlane(m_left, sphere));
        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifySphereAgainstFrustumPlane(m_right, sphere));
        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifySphereAgainstFrustumPlane(m_bottom, sphere));
        result = detail::MergeFrustumPlaneClassification(
            result, detail::ClassifySphereAgainstFrustumPlane(m_top, sphere));

        if (m_minimumDepth.has_value())
        {
            result = detail::MergeFrustumPlaneClassification(
                result, detail::ClassifySphereAgainstFrustumPlane(*m_minimumDepth, sphere));
        }
        if (m_maximumDepth.has_value())
        {
            result = detail::MergeFrustumPlaneClassification(
                result, detail::ClassifySphereAgainstFrustumPlane(*m_maximumDepth, sphere));
        }

        return result;
    }

    [[nodiscard]] constexpr const Plane& GetLeftPlane() const noexcept
    {
        return m_left;
    }

    [[nodiscard]] constexpr const Plane& GetRightPlane() const noexcept
    {
        return m_right;
    }

    [[nodiscard]] constexpr const Plane& GetBottomPlane() const noexcept
    {
        return m_bottom;
    }

    [[nodiscard]] constexpr const Plane& GetTopPlane() const noexcept
    {
        return m_top;
    }

    [[nodiscard]] constexpr const std::optional<Plane>& GetMinimumDepthPlane() const noexcept
    {
        return m_minimumDepth;
    }

    [[nodiscard]] constexpr const std::optional<Plane>& GetMaximumDepthPlane() const noexcept
    {
        return m_maximumDepth;
    }

private:
    constexpr Frustum(Plane left, Plane right, Plane bottom, Plane top,
                      std::optional<Plane> minimumDepth, std::optional<Plane> maximumDepth) noexcept
        : m_left(left), m_right(right), m_bottom(bottom), m_top(top), m_minimumDepth(minimumDepth),
          m_maximumDepth(maximumDepth)
    {
    }

    Plane m_left;
    Plane m_right;
    Plane m_bottom;
    Plane m_top;
    std::optional<Plane> m_minimumDepth;
    std::optional<Plane> m_maximumDepth;
};
} // namespace pond::math