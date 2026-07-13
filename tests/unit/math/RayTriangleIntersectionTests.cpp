#include <ponder/math/RayTriangleHit.hpp>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <random>

namespace
{
struct DoubleTriangleHit final
{
    double distance{0.0};
    double barycentric0{0.0};
    double barycentric1{0.0};
    double barycentric2{0.0};
};

struct DoubleVector3 final
{
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

[[nodiscard]] pond::math::Ray MakeRay(pond::math::Vector3 origin, pond::math::Vector3 direction)
{
    auto ray = pond::math::Ray::Create(origin, direction);
    EXPECT_TRUE(ray.HasValue());
    return ray.GetValue();
}

[[nodiscard]] pond::math::Triangle MakeTriangle(pond::math::Vector3 vertex0,
                                                pond::math::Vector3 vertex1,
                                                pond::math::Vector3 vertex2)
{
    auto triangle = pond::math::Triangle::Create(vertex0, vertex1, vertex2);
    EXPECT_TRUE(triangle.HasValue());
    return triangle.GetValue();
}

[[nodiscard]] constexpr DoubleVector3 ToDouble(pond::math::Vector3 value) noexcept
{
    return DoubleVector3{static_cast<double>(value.x), static_cast<double>(value.y),
                         static_cast<double>(value.z)};
}

[[nodiscard]] constexpr DoubleVector3 operator-(DoubleVector3 lhs, DoubleVector3 rhs) noexcept
{
    return DoubleVector3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] constexpr DoubleVector3 Cross(DoubleVector3 lhs, DoubleVector3 rhs) noexcept
{
    return DoubleVector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                         lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] constexpr double Dot(DoubleVector3 lhs, DoubleVector3 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] constexpr double Abs(double value) noexcept
{
    return value < 0.0 ? -value : value;
}

[[nodiscard]] std::optional<DoubleTriangleHit> IntersectDoubleOracle(
    const pond::math::Ray& ray, const pond::math::Triangle& triangle)
{
    constexpr double kOracleEpsilon{1.0e-12};
    const DoubleVector3 origin = ToDouble(ray.GetOrigin());
    const DoubleVector3 direction = ToDouble(ray.GetDirection());
    const DoubleVector3 vertex0 = ToDouble(triangle.GetVertex0());
    const DoubleVector3 vertex1 = ToDouble(triangle.GetVertex1());
    const DoubleVector3 vertex2 = ToDouble(triangle.GetVertex2());
    const DoubleVector3 edge0 = vertex1 - vertex0;
    const DoubleVector3 edge1 = vertex2 - vertex0;
    const DoubleVector3 pVector = Cross(direction, edge1);
    const double determinant = Dot(edge0, pVector);

    if (Abs(determinant) <= kOracleEpsilon)
    {
        return std::nullopt;
    }

    const DoubleVector3 originToVertex0 = origin - vertex0;
    const double barycentric1 = Dot(originToVertex0, pVector) / determinant;
    const DoubleVector3 qVector = Cross(originToVertex0, edge0);
    const double barycentric2 = Dot(direction, qVector) / determinant;
    const double barycentric0 = 1.0 - barycentric1 - barycentric2;
    const double distance = Dot(edge1, qVector) / determinant;

    if (barycentric0 < 0.0 || barycentric1 < 0.0 || barycentric2 < 0.0 || distance < 0.0)
    {
        return std::nullopt;
    }

    return DoubleTriangleHit{distance, barycentric0, barycentric1, barycentric2};
}

void ExpectVectorNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectTriangleHitNear(std::optional<pond::math::RayTriangleHit> hit, float expectedDistance,
                           float expectedBarycentric0, float expectedBarycentric1,
                           float expectedBarycentric2, float tolerance = 1.0e-5F)
{
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->GetDistance(), expectedDistance, tolerance);
    EXPECT_NEAR(hit->GetBarycentric0(), expectedBarycentric0, tolerance);
    EXPECT_NEAR(hit->GetBarycentric1(), expectedBarycentric1, tolerance);
    EXPECT_NEAR(hit->GetBarycentric2(), expectedBarycentric2, tolerance);
    EXPECT_NEAR(hit->GetBarycentric0() + hit->GetBarycentric1() + hit->GetBarycentric2(), 1.0F,
                tolerance);
}

[[nodiscard]] pond::math::Vector3 ReconstructPoint(const pond::math::Triangle& triangle,
                                                   const pond::math::RayTriangleHit& hit)
{
    return triangle.GetVertex0() * hit.GetBarycentric0() +
           triangle.GetVertex1() * hit.GetBarycentric1() +
           triangle.GetVertex2() * hit.GetBarycentric2();
}

[[nodiscard]] bool IsOutsideUncertaintyRegion(const DoubleTriangleHit& hit) noexcept
{
    constexpr double kMinimumBoundarySeparation{1.0e-4};
    return hit.distance > kMinimumBoundarySeparation &&
           hit.barycentric0 > kMinimumBoundarySeparation &&
           hit.barycentric1 > kMinimumBoundarySeparation &&
           hit.barycentric2 > kMinimumBoundarySeparation;
}

TEST(RayTriangleIntersectionTests, ReturnsDistanceAndOrderedBarycentricsForFaceHits)
{
    const auto triangle = MakeTriangle(pond::math::Vector3{-1.0F, 0.0F, -5.0F},
                                       pond::math::Vector3{1.0F, 0.0F, -5.0F},
                                       pond::math::Vector3{0.0F, 1.0F, -5.0F});
    const auto ray =
        MakeRay(pond::math::Vector3{0.0F, 0.25F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F});

    const auto hit = pond::math::Intersect(ray, triangle);

    ExpectTriangleHitNear(hit, 5.0F, 0.375F, 0.375F, 0.25F);
    ExpectVectorNear(ReconstructPoint(triangle, hit.value()),
                     ray.GetOrigin() + ray.GetDirection() * hit->GetDistance());
}

TEST(RayTriangleIntersectionTests, AcceptsBothTriangleWindings)
{
    const auto triangle = MakeTriangle(pond::math::Vector3{-1.0F, 0.0F, -5.0F},
                                       pond::math::Vector3{0.0F, 1.0F, -5.0F},
                                       pond::math::Vector3{1.0F, 0.0F, -5.0F});
    const auto ray =
        MakeRay(pond::math::Vector3{0.0F, 0.25F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F});

    ExpectTriangleHitNear(pond::math::Intersect(ray, triangle), 5.0F, 0.375F, 0.25F, 0.375F);
}

TEST(RayTriangleIntersectionTests, AcceptsEdgeVertexAndDistanceZeroHits)
{
    const auto triangle = MakeTriangle(pond::math::Vector3{-1.0F, 0.0F, -5.0F},
                                       pond::math::Vector3{1.0F, 0.0F, -5.0F},
                                       pond::math::Vector3{0.0F, 1.0F, -5.0F});

    ExpectTriangleHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                        pond::math::Vector3{0.0F, 0.0F, -1.0F}),
                                                triangle),
                          5.0F, 0.5F, 0.5F, 0.0F);
    ExpectTriangleHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{-1.0F, 0.0F, 0.0F},
                                                        pond::math::Vector3{0.0F, 0.0F, -1.0F}),
                                                triangle),
                          5.0F, 1.0F, 0.0F, 0.0F);
    ExpectTriangleHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{0.0F, 0.25F, -5.0F},
                                                        pond::math::Vector3{0.0F, 0.0F, -1.0F}),
                                                triangle),
                          0.0F, 0.375F, 0.375F, 0.25F);
}

TEST(RayTriangleIntersectionTests, RejectsMissesBehindOriginParallelRaysAndZeroAreaTriangles)
{
    const auto triangle = MakeTriangle(pond::math::Vector3{-1.0F, 0.0F, -5.0F},
                                       pond::math::Vector3{1.0F, 0.0F, -5.0F},
                                       pond::math::Vector3{0.0F, 1.0F, -5.0F});
    const auto zeroArea =
        MakeTriangle(pond::math::Vector3{0.0F, 0.0F, -5.0F}, pond::math::Vector3{1.0F, 0.0F, -5.0F},
                     pond::math::Vector3{2.0F, 0.0F, -5.0F});

    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{2.0F, 2.0F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F}),
        triangle));
    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{0.0F, 0.25F, -6.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F}),
        triangle));
    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{0.0F, 0.25F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F}),
        triangle));
    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{0.5F, 0.0F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F}),
        zeroArea));
}

TEST(RayTriangleIntersectionTests, RejectsNearParallelAndNearDegenerateUncertainHits)
{
    const auto nearParallelTriangle = MakeTriangle(pond::math::Vector3{0.0F, 0.0F, -5.0F},
                                                   pond::math::Vector3{1.0F, -1.0F, -4.0F},
                                                   pond::math::Vector3{1.0F, -1.0F, -7.0F});
    const pond::math::Vector3 nearParallelHitPoint{0.5F, -0.5F, -5.25F};
    const pond::math::Vector3 nearParallelDirection{1.0F, -0.999999F, 0.0F};
    const auto nearParallelRay =
        MakeRay(nearParallelHitPoint - nearParallelDirection * 2.0F, nearParallelDirection);

    ASSERT_TRUE(IntersectDoubleOracle(nearParallelRay, nearParallelTriangle).has_value());
    EXPECT_FALSE(pond::math::Intersect(nearParallelRay, nearParallelTriangle).has_value());

    const auto nearDegenerateTriangle = MakeTriangle(pond::math::Vector3{0.0F, 0.0F, -5.0F},
                                                     pond::math::Vector3{1.0F, -1.0F, -5.0F},
                                                     pond::math::Vector3{1.0F, -0.999999F, -5.0F});
    const pond::math::Vector3 nearDegenerateHitPoint = nearDegenerateTriangle.GetVertex0() * 0.5F +
                                                       nearDegenerateTriangle.GetVertex1() * 0.25F +
                                                       nearDegenerateTriangle.GetVertex2() * 0.25F;
    const auto nearDegenerateRay =
        MakeRay(nearDegenerateHitPoint + pond::math::Vector3{0.0F, 0.0F, 2.0F},
                pond::math::Vector3{0.0F, 0.0F, -1.0F});

    ASSERT_TRUE(IntersectDoubleOracle(nearDegenerateRay, nearDegenerateTriangle).has_value());
    EXPECT_FALSE(pond::math::Intersect(nearDegenerateRay, nearDegenerateTriangle).has_value());
}

TEST(RayTriangleIntersectionTests, HandlesVerySmallAndVeryLargeFiniteCoordinates)
{
    ExpectTriangleHitNear(
        pond::math::Intersect(MakeRay(pond::math::Vector3{0.25e-6F, 0.25e-6F, 0.0F},
                                      pond::math::Vector3{0.0F, 0.0F, -1.0F}),
                              MakeTriangle(pond::math::Vector3{0.0F, 0.0F, -1.0e-6F},
                                           pond::math::Vector3{1.0e-6F, 0.0F, -1.0e-6F},
                                           pond::math::Vector3{0.0F, 1.0e-6F, -1.0e-6F})),
        1.0e-6F, 0.5F, 0.25F, 0.25F, 1.0e-9F);

    ExpectTriangleHitNear(
        pond::math::Intersect(MakeRay(pond::math::Vector3{2.5e15F, 2.5e15F, 0.0F},
                                      pond::math::Vector3{0.0F, 0.0F, -1.0F}),
                              MakeTriangle(pond::math::Vector3{0.0F, 0.0F, -1.0e16F},
                                           pond::math::Vector3{1.0e16F, 0.0F, -1.0e16F},
                                           pond::math::Vector3{0.0F, 1.0e16F, -1.0e16F})),
        1.0e16F, 0.5F, 0.25F, 0.25F, 1.0e10F);
}

TEST(RayTriangleIntersectionTests, SeededCasesAgreeWithDoublePrecisionOracle)
{
    std::mt19937 generator{0x52415452U};
    std::uniform_real_distribution<float> coordinateDistribution{-10.0F, 10.0F};
    std::uniform_real_distribution<float> barycentricDistribution{0.1F, 0.8F};
    std::uniform_real_distribution<float> distanceDistribution{0.25F, 20.0F};

    for (int index = 0; index < 128; ++index)
    {
        const auto triangle =
            MakeTriangle(pond::math::Vector3{coordinateDistribution(generator),
                                             coordinateDistribution(generator),
                                             coordinateDistribution(generator) - 30.0F},
                         pond::math::Vector3{coordinateDistribution(generator),
                                             coordinateDistribution(generator),
                                             coordinateDistribution(generator) - 30.0F},
                         pond::math::Vector3{coordinateDistribution(generator),
                                             coordinateDistribution(generator),
                                             coordinateDistribution(generator) - 30.0F});
        const float rawBarycentric1 = barycentricDistribution(generator);
        const float rawBarycentric2 =
            std::min(barycentricDistribution(generator), 0.9F - rawBarycentric1);
        const float barycentric0 = 1.0F - rawBarycentric1 - rawBarycentric2;
        if (barycentric0 <= 0.1F)
        {
            continue;
        }

        const pond::math::Vector3 hitPoint = triangle.GetVertex0() * barycentric0 +
                                             triangle.GetVertex1() * rawBarycentric1 +
                                             triangle.GetVertex2() * rawBarycentric2;
        const float distance = distanceDistribution(generator);
        const auto ray = MakeRay(hitPoint + pond::math::Vector3{0.0F, 0.0F, distance},
                                 pond::math::Vector3{0.0F, 0.0F, -1.0F});
        const auto expected = IntersectDoubleOracle(ray, triangle);
        if (!expected.has_value() || !IsOutsideUncertaintyRegion(expected.value()))
        {
            continue;
        }

        const auto actual = pond::math::Intersect(ray, triangle);
        ASSERT_TRUE(actual.has_value());
        EXPECT_NEAR(actual->GetDistance(), static_cast<float>(expected->distance), 1.0e-4F);
        EXPECT_NEAR(actual->GetBarycentric0(), static_cast<float>(expected->barycentric0), 1.0e-4F);
        EXPECT_NEAR(actual->GetBarycentric1(), static_cast<float>(expected->barycentric1), 1.0e-4F);
        EXPECT_NEAR(actual->GetBarycentric2(), static_cast<float>(expected->barycentric2), 1.0e-4F);
    }
}
} // namespace
