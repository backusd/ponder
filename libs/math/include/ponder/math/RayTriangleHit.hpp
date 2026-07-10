#pragma once

#include <ponder/math/Ray.hpp>
#include <ponder/math/Triangle.hpp>

#include <limits>
#include <optional>

namespace pond::math
{
class RayTriangleHit;

[[nodiscard]] inline std::optional<RayTriangleHit> Intersect(const Ray& ray,
                                                             const Triangle& triangle) noexcept;

namespace detail
{
inline constexpr double kRayTriangleUncertaintyMultiplier{32.0};

struct RayTriangleVector final
{
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

[[nodiscard]] constexpr double AbsRayTriangleValue(double value) noexcept
{
    return value < 0.0 ? -value : value;
}

[[nodiscard]] constexpr double MaxRayTriangleValue(double lhs, double rhs) noexcept
{
    return lhs < rhs ? rhs : lhs;
}

[[nodiscard]] constexpr RayTriangleVector ToRayTriangleVector(Vector3 value) noexcept
{
    return RayTriangleVector{static_cast<double>(value.x), static_cast<double>(value.y),
                             static_cast<double>(value.z)};
}

[[nodiscard]] constexpr RayTriangleVector operator-(RayTriangleVector lhs,
                                                    RayTriangleVector rhs) noexcept
{
    return RayTriangleVector{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] constexpr RayTriangleVector Cross(RayTriangleVector lhs,
                                                RayTriangleVector rhs) noexcept
{
    return RayTriangleVector{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                             lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] constexpr double Dot(RayTriangleVector lhs, RayTriangleVector rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] constexpr double DotMagnitude(RayTriangleVector lhs, RayTriangleVector rhs) noexcept
{
    return AbsRayTriangleValue(lhs.x * rhs.x) + AbsRayTriangleValue(lhs.y * rhs.y) +
           AbsRayTriangleValue(lhs.z * rhs.z);
}

[[nodiscard]] constexpr double MaxComponentMagnitude(RayTriangleVector value) noexcept
{
    return MaxRayTriangleValue(
        MaxRayTriangleValue(AbsRayTriangleValue(value.x), AbsRayTriangleValue(value.y)),
        AbsRayTriangleValue(value.z));
}

[[nodiscard]] constexpr double DeterminantErrorBound(RayTriangleVector edge0,
                                                     RayTriangleVector pVector) noexcept
{
    return kRayTriangleUncertaintyMultiplier * std::numeric_limits<float>::epsilon() *
           DotMagnitude(edge0, pVector);
}

[[nodiscard]] constexpr double BarycentricErrorBound(double determinant, double numerator0,
                                                     double numerator1) noexcept
{
    const double magnitude = AbsRayTriangleValue(determinant) + AbsRayTriangleValue(numerator0) +
                             AbsRayTriangleValue(numerator1);
    return kRayTriangleUncertaintyMultiplier * std::numeric_limits<float>::epsilon() * magnitude /
           AbsRayTriangleValue(determinant);
}

[[nodiscard]] constexpr double DistanceErrorBound(RayTriangleVector origin,
                                                  RayTriangleVector vertex0,
                                                  RayTriangleVector vertex1,
                                                  RayTriangleVector vertex2,
                                                  double distance) noexcept
{
    const double worldMagnitude = MaxRayTriangleValue(
        MaxRayTriangleValue(MaxComponentMagnitude(origin), MaxComponentMagnitude(vertex0)),
        MaxRayTriangleValue(MaxComponentMagnitude(vertex1), MaxComponentMagnitude(vertex2)));
    const double distanceMagnitude =
        MaxRayTriangleValue(worldMagnitude, AbsRayTriangleValue(distance));
    return kRayTriangleUncertaintyMultiplier * std::numeric_limits<float>::epsilon() *
           distanceMagnitude;
}

[[nodiscard]] constexpr double ClampRayTriangleBarycentric(double value) noexcept
{
    if (value < 0.0)
    {
        return 0.0;
    }
    if (value > 1.0)
    {
        return 1.0;
    }

    return value;
}
} // namespace detail

class RayTriangleHit final
{
public:
    [[nodiscard]] constexpr float GetDistance() const noexcept
    {
        return m_distance;
    }

    [[nodiscard]] constexpr float GetBarycentric0() const noexcept
    {
        return m_barycentric0;
    }

    [[nodiscard]] constexpr float GetBarycentric1() const noexcept
    {
        return m_barycentric1;
    }

    [[nodiscard]] constexpr float GetBarycentric2() const noexcept
    {
        return m_barycentric2;
    }

    [[nodiscard]] friend constexpr bool operator==(const RayTriangleHit& lhs,
                                                   const RayTriangleHit& rhs) noexcept = default;

private:
    friend std::optional<RayTriangleHit> Intersect(const Ray& ray,
                                                   const Triangle& triangle) noexcept;

    constexpr RayTriangleHit(float distance, float barycentric0, float barycentric1,
                             float barycentric2) noexcept
        : m_distance(distance), m_barycentric0(barycentric0), m_barycentric1(barycentric1),
          m_barycentric2(barycentric2)
    {
    }

    float m_distance;
    float m_barycentric0;
    float m_barycentric1;
    float m_barycentric2;
};

[[nodiscard]] inline std::optional<RayTriangleHit> Intersect(const Ray& ray,
                                                             const Triangle& triangle) noexcept
{
    const detail::RayTriangleVector origin = detail::ToRayTriangleVector(ray.GetOrigin());
    const detail::RayTriangleVector direction = detail::ToRayTriangleVector(ray.GetDirection());
    const detail::RayTriangleVector vertex0 = detail::ToRayTriangleVector(triangle.GetVertex0());
    const detail::RayTriangleVector vertex1 = detail::ToRayTriangleVector(triangle.GetVertex1());
    const detail::RayTriangleVector vertex2 = detail::ToRayTriangleVector(triangle.GetVertex2());
    const detail::RayTriangleVector edge0 = vertex1 - vertex0;
    const detail::RayTriangleVector edge1 = vertex2 - vertex0;
    const detail::RayTriangleVector pVector = detail::Cross(direction, edge1);
    const double determinant = detail::Dot(edge0, pVector);
    const double determinantError = detail::DeterminantErrorBound(edge0, pVector);

    if (detail::AbsRayTriangleValue(determinant) <= determinantError)
    {
        return std::nullopt;
    }

    const detail::RayTriangleVector originToVertex0 = origin - vertex0;
    const double barycentric1Numerator = detail::Dot(originToVertex0, pVector);
    const detail::RayTriangleVector qVector = detail::Cross(originToVertex0, edge0);
    const double barycentric2Numerator = detail::Dot(direction, qVector);
    const double barycentric1 = barycentric1Numerator / determinant;
    const double barycentric2 = barycentric2Numerator / determinant;
    const double barycentric0 = 1.0 - barycentric1 - barycentric2;
    const double barycentricTolerance =
        detail::BarycentricErrorBound(determinant, barycentric1Numerator, barycentric2Numerator);

    if (barycentric0 < -barycentricTolerance || barycentric1 < -barycentricTolerance ||
        barycentric2 < -barycentricTolerance)
    {
        return std::nullopt;
    }

    double adjustedBarycentric0 = detail::ClampRayTriangleBarycentric(barycentric0);
    double adjustedBarycentric1 = detail::ClampRayTriangleBarycentric(barycentric1);
    double adjustedBarycentric2 = detail::ClampRayTriangleBarycentric(barycentric2);
    const double barycentricSum =
        adjustedBarycentric0 + adjustedBarycentric1 + adjustedBarycentric2;
    if (barycentricSum == 0.0)
    {
        return std::nullopt;
    }
    adjustedBarycentric0 /= barycentricSum;
    adjustedBarycentric1 /= barycentricSum;
    adjustedBarycentric2 /= barycentricSum;

    const double distanceNumerator = detail::Dot(edge1, qVector);
    double distance = distanceNumerator / determinant;
    const double distanceTolerance =
        detail::DistanceErrorBound(origin, vertex0, vertex1, vertex2, distance);
    if (distance < -distanceTolerance)
    {
        return std::nullopt;
    }
    if (distance < 0.0)
    {
        distance = 0.0;
    }

    const float resultDistance = static_cast<float>(distance);
    const float resultBarycentric0 = static_cast<float>(adjustedBarycentric0);
    const float resultBarycentric1 = static_cast<float>(adjustedBarycentric1);
    const float resultBarycentric2 = static_cast<float>(adjustedBarycentric2);
    if (!IsFinite(resultDistance) || !IsFinite(resultBarycentric0) ||
        !IsFinite(resultBarycentric1) || !IsFinite(resultBarycentric2))
    {
        return std::nullopt;
    }

    return RayTriangleHit{resultDistance, resultBarycentric0, resultBarycentric1,
                          resultBarycentric2};
}
} // namespace pond::math
