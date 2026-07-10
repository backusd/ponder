#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/Frustum.hpp>
#include <ponder/math/RayBoxHit.hpp>
#include <ponder/math/RayTriangleHit.hpp>
#include <ponder/math/Sphere.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <random>
#include <type_traits>

namespace
{
using Classification = pond::math::CollisionClassification;

template <typename Shape>
concept FrustumClassifies = requires(const pond::math::Frustum& frustum, const Shape& shape) {
    { frustum.Classify(shape) } -> std::same_as<Classification>;
};

template <typename Left, typename Right>
concept HasCollisionIntersect =
    requires(const Left& left, const Right& right) { pond::math::Intersect(left, right); };

struct DoubleRayBoxHit final
{
    double entryDistance{0.0};
    double exitDistance{0.0};
};

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

[[nodiscard]] pond::math::Frustum MakeOrthographicFrustum(float scale)
{
    auto projection = pond::math::Matrix4x4::Orthographic(
        -scale, scale, -scale, scale, scale, 5.0F * scale, pond::math::ProjectionDepth::ForwardZ);
    EXPECT_TRUE(projection.HasValue());

    auto frustum = pond::math::Frustum::FromWorldToClip(projection.GetValue());
    EXPECT_TRUE(frustum.HasValue());
    return frustum.GetValue();
}

[[nodiscard]] pond::math::Ray MakeRay(pond::math::Vector3 origin, pond::math::Vector3 direction)
{
    auto ray = pond::math::Ray::Create(origin, direction);
    EXPECT_TRUE(ray.HasValue());
    return ray.GetValue();
}

[[nodiscard]] pond::math::AxisAlignedBox MakeBox(pond::math::Vector3 minimum,
                                                 pond::math::Vector3 maximum)
{
    auto box = pond::math::AxisAlignedBox::Create(minimum, maximum);
    EXPECT_TRUE(box.HasValue());
    return box.GetValue();
}

[[nodiscard]] pond::math::Sphere MakeSphere(pond::math::Vector3 center, float radius)
{
    auto sphere = pond::math::Sphere::Create(center, radius);
    EXPECT_TRUE(sphere.HasValue());
    return sphere.GetValue();
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

[[nodiscard]] double EvaluatePlane(const pond::math::Plane& plane, pond::math::Vector3 point)
{
    const pond::math::Vector3 normal = plane.GetNormal();
    return static_cast<double>(normal.x) * static_cast<double>(point.x) +
           static_cast<double>(normal.y) * static_cast<double>(point.y) +
           static_cast<double>(normal.z) * static_cast<double>(point.z) +
           static_cast<double>(plane.GetOffset());
}

[[nodiscard]] Classification MergeClassification(Classification current,
                                                 Classification next) noexcept
{
    if (next == Classification::Disjoint)
    {
        return Classification::Disjoint;
    }
    if (next == Classification::Intersects)
    {
        return Classification::Intersects;
    }

    return current;
}

[[nodiscard]] Classification ClassifyBoxAgainstPlane(const pond::math::Plane& plane,
                                                     const pond::math::AxisAlignedBox& box)
{
    const pond::math::Vector3 normal = plane.GetNormal();
    const pond::math::Vector3 minimum = box.GetMinimum();
    const pond::math::Vector3 maximum = box.GetMaximum();
    const pond::math::Vector3 positive{normal.x >= 0.0F ? maximum.x : minimum.x,
                                       normal.y >= 0.0F ? maximum.y : minimum.y,
                                       normal.z >= 0.0F ? maximum.z : minimum.z};
    const pond::math::Vector3 negative{normal.x >= 0.0F ? minimum.x : maximum.x,
                                       normal.y >= 0.0F ? minimum.y : maximum.y,
                                       normal.z >= 0.0F ? minimum.z : maximum.z};

    if (EvaluatePlane(plane, positive) < 0.0)
    {
        return Classification::Disjoint;
    }
    if (EvaluatePlane(plane, negative) <= 0.0)
    {
        return Classification::Intersects;
    }

    return Classification::Contains;
}

[[nodiscard]] Classification ClassifySphereAgainstPlane(const pond::math::Plane& plane,
                                                        const pond::math::Sphere& sphere)
{
    const double distance = EvaluatePlane(plane, sphere.GetCenter());
    const double radius = static_cast<double>(sphere.GetRadius());
    if (distance + radius < 0.0)
    {
        return Classification::Disjoint;
    }
    if (distance - radius <= 0.0)
    {
        return Classification::Intersects;
    }

    return Classification::Contains;
}

template <typename Shape, typename PlaneClassifier>
[[nodiscard]] Classification ClassifyFrustumOracle(const pond::math::Frustum& frustum,
                                                   const Shape& shape, PlaneClassifier classifier)
{
    Classification result = Classification::Contains;
    result = MergeClassification(result, classifier(frustum.GetLeftPlane(), shape));
    result = MergeClassification(result, classifier(frustum.GetRightPlane(), shape));
    result = MergeClassification(result, classifier(frustum.GetBottomPlane(), shape));
    result = MergeClassification(result, classifier(frustum.GetTopPlane(), shape));
    if (frustum.GetMinimumDepthPlane().has_value())
    {
        result = MergeClassification(result, classifier(*frustum.GetMinimumDepthPlane(), shape));
    }
    if (frustum.GetMaximumDepthPlane().has_value())
    {
        result = MergeClassification(result, classifier(*frustum.GetMaximumDepthPlane(), shape));
    }

    return result;
}

[[nodiscard]] bool UpdateRayBoxOracleInterval(float origin, float direction, float minimum,
                                              float maximum, double& entry, double& exit)
{
    if (direction == 0.0F)
    {
        return origin >= minimum && origin <= maximum;
    }

    double nearDistance = (static_cast<double>(minimum) - static_cast<double>(origin)) /
                          static_cast<double>(direction);
    double farDistance = (static_cast<double>(maximum) - static_cast<double>(origin)) /
                         static_cast<double>(direction);
    if (farDistance < nearDistance)
    {
        std::swap(nearDistance, farDistance);
    }

    entry = std::max(entry, nearDistance);
    exit = std::min(exit, farDistance);
    return entry <= exit;
}

[[nodiscard]] std::optional<DoubleRayBoxHit> IntersectRayBoxOracle(
    const pond::math::Ray& ray, const pond::math::AxisAlignedBox& box)
{
    const pond::math::Vector3 origin = ray.GetOrigin();
    const pond::math::Vector3 direction = ray.GetDirection();
    const pond::math::Vector3 minimum = box.GetMinimum();
    const pond::math::Vector3 maximum = box.GetMaximum();
    double entry = -std::numeric_limits<double>::infinity();
    double exit = std::numeric_limits<double>::infinity();

    if (!UpdateRayBoxOracleInterval(origin.x, direction.x, minimum.x, maximum.x, entry, exit) ||
        !UpdateRayBoxOracleInterval(origin.y, direction.y, minimum.y, maximum.y, entry, exit) ||
        !UpdateRayBoxOracleInterval(origin.z, direction.z, minimum.z, maximum.z, entry, exit) ||
        exit < 0.0)
    {
        return std::nullopt;
    }

    return DoubleRayBoxHit{std::max(0.0, entry), exit};
}

[[nodiscard]] std::optional<DoubleTriangleHit> IntersectRayTriangleOracle(
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

TEST(CollisionMilestoneTests, PublicQuerySurfaceContainsOnlyInitialCollisionMatrix)
{
    using pond::math::AxisAlignedBox;
    using pond::math::Frustum;
    using pond::math::Ray;
    using pond::math::RayBoxHit;
    using pond::math::RayTriangleHit;
    using pond::math::Sphere;
    using pond::math::Triangle;

    static_assert(FrustumClassifies<AxisAlignedBox>);
    static_assert(FrustumClassifies<Sphere>);
    static_assert(!FrustumClassifies<Ray>);
    static_assert(!FrustumClassifies<Triangle>);

    static_assert(HasCollisionIntersect<Ray, AxisAlignedBox>);
    static_assert(HasCollisionIntersect<Ray, Triangle>);
    static_assert(!HasCollisionIntersect<AxisAlignedBox, Ray>);
    static_assert(!HasCollisionIntersect<Triangle, Ray>);
    static_assert(!HasCollisionIntersect<Ray, Sphere>);
    static_assert(!HasCollisionIntersect<Ray, Frustum>);
    static_assert(!HasCollisionIntersect<AxisAlignedBox, Sphere>);
    static_assert(!HasCollisionIntersect<AxisAlignedBox, Triangle>);
    static_assert(!HasCollisionIntersect<Sphere, Triangle>);

    static_assert(
        std::same_as<decltype(pond::math::Intersect(std::declval<const Ray&>(),
                                                    std::declval<const AxisAlignedBox&>())),
                     std::optional<RayBoxHit>>);
    static_assert(std::same_as<decltype(pond::math::Intersect(std::declval<const Ray&>(),
                                                              std::declval<const Triangle&>())),
                               std::optional<RayTriangleHit>>);
}

TEST(CollisionMilestoneTests, SeededFrustumQueriesDoNotFalseRejectOracleVisibleObjects)
{
    constexpr std::array<float, 3> kScales{1.0e-3F, 1.0F, 1.0e6F};
    std::mt19937 generator{0x43323046U};

    for (const float scale : kScales)
    {
        const pond::math::Frustum frustum = MakeOrthographicFrustum(scale);
        std::uniform_real_distribution<float> centerDistribution{-1.25F * scale, 1.25F * scale};
        std::uniform_real_distribution<float> depthDistribution{-5.5F * scale, -0.5F * scale};
        std::uniform_real_distribution<float> extentDistribution{0.0F, 0.2F * scale};

        for (int index = 0; index < 96; ++index)
        {
            const pond::math::Vector3 center{centerDistribution(generator),
                                             centerDistribution(generator),
                                             depthDistribution(generator)};
            const pond::math::Vector3 extent{extentDistribution(generator),
                                             extentDistribution(generator),
                                             extentDistribution(generator)};
            const pond::math::AxisAlignedBox box = MakeBox(center - extent, center + extent);
            const pond::math::Sphere sphere = MakeSphere(center, extentDistribution(generator));
            const Classification boxOracle =
                ClassifyFrustumOracle(frustum, box, ClassifyBoxAgainstPlane);
            const Classification sphereOracle =
                ClassifyFrustumOracle(frustum, sphere, ClassifySphereAgainstPlane);

            if (boxOracle != Classification::Disjoint)
            {
                EXPECT_NE(frustum.Classify(box), Classification::Disjoint);
            }
            if (sphereOracle != Classification::Disjoint)
            {
                EXPECT_NE(frustum.Classify(sphere), Classification::Disjoint);
            }
        }
    }
}

TEST(CollisionMilestoneTests, SeededRayBoxQueriesMatchHigherPrecisionOracle)
{
    constexpr std::array<float, 3> kScales{1.0e-3F, 1.0F, 1.0e6F};
    std::mt19937 generator{0x43323042U};

    for (const float scale : kScales)
    {
        std::uniform_real_distribution<float> coordinateDistribution{-2.0F * scale, 2.0F * scale};
        std::uniform_real_distribution<float> extentDistribution{0.05F * scale, 0.4F * scale};
        std::uniform_real_distribution<float> distanceDistribution{2.0F * scale, 8.0F * scale};

        for (int index = 0; index < 64; ++index)
        {
            const pond::math::Vector3 center{coordinateDistribution(generator),
                                             coordinateDistribution(generator),
                                             coordinateDistribution(generator)};
            const pond::math::Vector3 extent{extentDistribution(generator),
                                             extentDistribution(generator),
                                             extentDistribution(generator)};
            const pond::math::AxisAlignedBox box = MakeBox(center - extent, center + extent);
            const pond::math::Vector3 target{center.x, center.y, center.z};
            const float distance = distanceDistribution(generator);
            const pond::math::Vector3 direction{0.0F, 0.0F, -1.0F};
            const pond::math::Ray ray = MakeRay(target - direction * distance, direction);
            const auto actual = pond::math::Intersect(ray, box);
            const auto expected = IntersectRayBoxOracle(ray, box);

            ASSERT_EQ(actual.has_value(), expected.has_value());
            ASSERT_TRUE(actual.has_value());
            const double entryMagnitude = std::max(1.0, Abs(expected->entryDistance));
            const double exitMagnitude = std::max(1.0, Abs(expected->exitDistance));
            EXPECT_NEAR(actual->GetEntryDistance(), static_cast<float>(expected->entryDistance),
                        static_cast<float>(1.0e-5 * entryMagnitude));
            EXPECT_NEAR(actual->GetExitDistance(), static_cast<float>(expected->exitDistance),
                        static_cast<float>(1.0e-5 * exitMagnitude));
        }
    }
}

TEST(CollisionMilestoneTests, SeededRayTriangleQueriesMatchHigherPrecisionOracle)
{
    constexpr std::array<float, 3> kScales{1.0e-3F, 1.0F, 1.0e6F};
    std::mt19937 generator{0x43323054U};
    std::uniform_real_distribution<float> barycentricDistribution{0.15F, 0.7F};

    for (const float scale : kScales)
    {
        std::uniform_real_distribution<float> coordinateDistribution{-2.0F * scale, 2.0F * scale};
        std::uniform_real_distribution<float> sizeDistribution{0.5F * scale, 2.0F * scale};
        std::uniform_real_distribution<float> distanceDistribution{2.0F * scale, 8.0F * scale};

        for (int index = 0; index < 64; ++index)
        {
            const pond::math::Vector3 vertex0{coordinateDistribution(generator),
                                              coordinateDistribution(generator), -5.0F * scale};
            const float width = sizeDistribution(generator);
            const float height = sizeDistribution(generator);
            const pond::math::Triangle triangle =
                MakeTriangle(vertex0, vertex0 + pond::math::Vector3{width, 0.0F, 0.0F},
                             vertex0 + pond::math::Vector3{0.0F, height, 0.0F});
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
            const pond::math::Ray ray =
                MakeRay(hitPoint + pond::math::Vector3{0.0F, 0.0F, distance},
                        pond::math::Vector3{0.0F, 0.0F, -1.0F});
            const auto actual = pond::math::Intersect(ray, triangle);
            const auto expected = IntersectRayTriangleOracle(ray, triangle);

            ASSERT_EQ(actual.has_value(), expected.has_value());
            ASSERT_TRUE(actual.has_value());
            const double distanceMagnitude = std::max(1.0, Abs(expected->distance));
            EXPECT_NEAR(actual->GetDistance(), static_cast<float>(expected->distance),
                        static_cast<float>(1.0e-5 * distanceMagnitude));
            EXPECT_NEAR(actual->GetBarycentric0(), static_cast<float>(expected->barycentric0),
                        1.0e-4F);
            EXPECT_NEAR(actual->GetBarycentric1(), static_cast<float>(expected->barycentric1),
                        1.0e-4F);
            EXPECT_NEAR(actual->GetBarycentric2(), static_cast<float>(expected->barycentric2),
                        1.0e-4F);
        }
    }
}
} // namespace
