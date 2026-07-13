#include <ponder/math/Angle.hpp>
#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/Frustum.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Plane.hpp>
#include <ponder/math/Quaternion.hpp>
#include <ponder/math/Ray.hpp>
#include <ponder/math/RayBoxHit.hpp>
#include <ponder/math/RayTriangleHit.hpp>
#include <ponder/math/Sphere.hpp>
#include <ponder/math/Triangle.hpp>
#include <ponder/math/Viewport.hpp>

#include <array>
#include <gtest/gtest.h>

namespace
{
constexpr float kRendererTolerance{1.0e-3F};

void ExpectVectorNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                      float tolerance = kRendererTolerance)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

[[nodiscard]] pond::math::Viewport RequireViewport()
{
    auto viewport = pond::math::Viewport::Create(32.0F, 48.0F, 1280.0F, 720.0F);
    EXPECT_TRUE(viewport.HasValue());
    return viewport.GetValue();
}

[[nodiscard]] pond::math::Matrix4x4 RequireView(pond::math::Vector3 target)
{
    auto view = pond::math::Matrix4x4::LookAt(pond::math::Vector3{0.75F, 1.25F, 6.0F}, target,
                                              pond::math::Vector3{0.0F, 1.0F, 0.0F});
    EXPECT_TRUE(view.HasValue());
    return view.GetValue();
}

[[nodiscard]] pond::math::Matrix4x4 RequirePerspective(pond::math::ProjectionDepth depth)
{
    auto projection = pond::math::Matrix4x4::Perspective(
        pond::math::ToRadians(pond::math::Degrees{60.0F}), 16.0F / 9.0F, 0.25F, 80.0F, depth);
    EXPECT_TRUE(projection.HasValue());
    return projection.GetValue();
}

[[nodiscard]] pond::math::Matrix4x4 RequireReverseInfinitePerspective()
{
    auto projection = pond::math::Matrix4x4::InfinitePerspective(
        pond::math::ToRadians(pond::math::Degrees{60.0F}), 16.0F / 9.0F, 0.25F,
        pond::math::ProjectionDepth::ReverseZ);
    EXPECT_TRUE(projection.HasValue());
    return projection.GetValue();
}

[[nodiscard]] pond::math::Triangle RequireTriangle(pond::math::Vector3 vertex0,
                                                   pond::math::Vector3 vertex1,
                                                   pond::math::Vector3 vertex2)
{
    auto triangle = pond::math::Triangle::Create(vertex0, vertex1, vertex2);
    EXPECT_TRUE(triangle.HasValue());
    return triangle.GetValue();
}

[[nodiscard]] pond::math::AxisAlignedBox RequireBounds(
    const std::array<pond::math::Vector3, 3>& points)
{
    pond::math::Vector3 minimum =
        pond::math::ComponentMin(pond::math::ComponentMin(points[0], points[1]), points[2]);
    pond::math::Vector3 maximum =
        pond::math::ComponentMax(pond::math::ComponentMax(points[0], points[1]), points[2]);
    constexpr pond::math::Vector3 kPadding{0.05F, 0.05F, 0.05F};

    auto bounds = pond::math::AxisAlignedBox::Create(minimum - kPadding, maximum + kPadding);
    EXPECT_TRUE(bounds.HasValue());
    return bounds.GetValue();
}

[[nodiscard]] pond::math::Sphere RequireSphere(pond::math::Vector3 center, float radius)
{
    auto sphere = pond::math::Sphere::Create(center, radius);
    EXPECT_TRUE(sphere.HasValue());
    return sphere.GetValue();
}

[[nodiscard]] pond::math::Ray RequirePickingRay(pond::math::Matrix4x4 viewProjection,
                                                pond::math::Viewport viewport,
                                                pond::math::Vector3 screenPoint, float nearDepth,
                                                float farDepth)
{
    auto clipToWorld = pond::math::Inverse(viewProjection);
    EXPECT_TRUE(clipToWorld.HasValue());
    auto nearPoint = pond::math::UnprojectFromClipToWorld(
        clipToWorld.GetValue(), viewport,
        pond::math::Vector3{screenPoint.x, screenPoint.y, nearDepth});
    auto farPoint = pond::math::UnprojectFromClipToWorld(
        clipToWorld.GetValue(), viewport,
        pond::math::Vector3{screenPoint.x, screenPoint.y, farDepth});
    EXPECT_TRUE(nearPoint.HasValue());
    EXPECT_TRUE(farPoint.HasValue());

    auto ray =
        pond::math::Ray::Create(nearPoint.GetValue(), farPoint.GetValue() - nearPoint.GetValue());
    EXPECT_TRUE(ray.HasValue());
    return ray.GetValue();
}

[[nodiscard]] pond::math::Vector3 ReconstructHitPoint(
    const pond::math::Triangle& triangle, const pond::math::RayTriangleHit& hit) noexcept
{
    return triangle.GetVertex0() * hit.GetBarycentric0() +
           triangle.GetVertex1() * hit.GetBarycentric1() +
           triangle.GetVertex2() * hit.GetBarycentric2();
}

void ExerciseRendererPipeline(pond::math::Matrix4x4 projection, float nearDepth, float farDepth)
{
    const pond::math::Matrix4x4 world =
        pond::math::Matrix4x4::Translation(pond::math::Vector3{0.75F, -0.25F, -2.0F}) *
        pond::math::Matrix4x4::RotationY(pond::math::ToRadians(pond::math::Degrees{20.0F})) *
        pond::math::Matrix4x4::Scale(pond::math::Vector3{1.25F, 0.75F, 1.5F});
    const pond::math::Vector3 localVertex0{-0.5F, -0.4F, 0.0F};
    const pond::math::Vector3 localVertex1{0.6F, -0.3F, 0.0F};
    const pond::math::Vector3 localVertex2{-0.1F, 0.7F, 0.0F};
    const pond::math::Vector3 localHitPoint =
        localVertex0 * 0.25F + localVertex1 * 0.35F + localVertex2 * 0.4F;
    const pond::math::Vector3 worldHitPoint = pond::math::TransformPoint(world, localHitPoint);
    const pond::math::Vector3 worldVertex0 = pond::math::TransformPoint(world, localVertex0);
    const pond::math::Vector3 worldVertex1 = pond::math::TransformPoint(world, localVertex1);
    const pond::math::Vector3 worldVertex2 = pond::math::TransformPoint(world, localVertex2);
    const pond::math::Triangle worldTriangle =
        RequireTriangle(worldVertex0, worldVertex1, worldVertex2);
    const pond::math::AxisAlignedBox worldBounds =
        RequireBounds(std::array{worldVertex0, worldVertex1, worldVertex2});
    const pond::math::Sphere worldSphere = RequireSphere(worldHitPoint, 0.35F);
    const pond::math::Viewport viewport = RequireViewport();
    const pond::math::Matrix4x4 view = RequireView(worldHitPoint);
    const pond::math::Matrix4x4 viewProjection = projection * view;
    const pond::math::Matrix4x4 worldToClip = viewProjection * world;

    const pond::math::Vector3 worldTangent0 =
        pond::math::TransformVector(world, localVertex1 - localVertex0);
    const pond::math::Vector3 worldTangent1 =
        pond::math::TransformVector(world, localVertex2 - localVertex0);
    auto worldNormal = pond::math::TransformNormal(world, pond::math::Vector3{0.0F, 0.0F, 1.0F});
    ASSERT_TRUE(worldNormal.HasValue());
    EXPECT_NEAR(pond::math::Dot(worldNormal.GetValue(), worldTangent0), 0.0F, 1.0e-5F);
    EXPECT_NEAR(pond::math::Dot(worldNormal.GetValue(), worldTangent1), 0.0F, 1.0e-5F);

    auto localProjected = pond::math::Project(worldToClip, viewport, localHitPoint);
    ASSERT_TRUE(localProjected.HasValue());
    auto localRoundTrip = pond::math::Unproject(worldToClip, viewport, localProjected.GetValue());
    ASSERT_TRUE(localRoundTrip.HasValue());
    ExpectVectorNear(localRoundTrip.GetValue(), localHitPoint);

    auto worldProjected = pond::math::Project(viewProjection, viewport, worldHitPoint);
    ASSERT_TRUE(worldProjected.HasValue());
    auto worldRoundTrip =
        pond::math::Unproject(viewProjection, viewport, worldProjected.GetValue());
    ASSERT_TRUE(worldRoundTrip.HasValue());
    ExpectVectorNear(worldRoundTrip.GetValue(), worldHitPoint);
    ExpectVectorNear(worldProjected.GetValue(), localProjected.GetValue());

    auto frustum = pond::math::Frustum::FromWorldToClip(viewProjection);
    ASSERT_TRUE(frustum.HasValue());
    EXPECT_NE(frustum->Classify(worldBounds), pond::math::CollisionClassification::Disjoint);
    EXPECT_NE(frustum->Classify(worldSphere), pond::math::CollisionClassification::Disjoint);

    auto outsideBounds = pond::math::AxisAlignedBox::Create(
        worldHitPoint + pond::math::Vector3{1000.0F, 0.0F, 0.0F},
        worldHitPoint + pond::math::Vector3{1001.0F, 1.0F, 1.0F});
    ASSERT_TRUE(outsideBounds.HasValue());
    EXPECT_EQ(frustum->Classify(outsideBounds.GetValue()),
              pond::math::CollisionClassification::Disjoint);

    const pond::math::Ray pickingRay =
        RequirePickingRay(viewProjection, viewport, worldProjected.GetValue(), nearDepth, farDepth);
    const std::optional<pond::math::RayBoxHit> boxHit =
        pond::math::Intersect(pickingRay, worldBounds);
    const std::optional<pond::math::RayTriangleHit> triangleHit =
        pond::math::Intersect(pickingRay, worldTriangle);
    ASSERT_TRUE(boxHit.has_value());
    ASSERT_TRUE(triangleHit.has_value());
    EXPECT_LE(boxHit->GetEntryDistance(), triangleHit->GetDistance() + kRendererTolerance);
    EXPECT_GE(boxHit->GetExitDistance(), triangleHit->GetDistance() - kRendererTolerance);

    const pond::math::Vector3 reconstructedTrianglePoint =
        ReconstructHitPoint(worldTriangle, *triangleHit);
    const pond::math::Vector3 reconstructedRayPoint =
        pickingRay.GetOrigin() + pickingRay.GetDirection() * triangleHit->GetDistance();
    ExpectVectorNear(reconstructedTrianglePoint, worldHitPoint);
    ExpectVectorNear(reconstructedRayPoint, worldHitPoint);
}

TEST(RendererReadyTests, ExercisesForwardAndReverseFiniteRendererPipeline)
{
    ExerciseRendererPipeline(RequirePerspective(pond::math::ProjectionDepth::ForwardZ), 0.0F, 1.0F);
    ExerciseRendererPipeline(RequirePerspective(pond::math::ProjectionDepth::ReverseZ), 1.0F, 0.0F);
}

TEST(RendererReadyTests, ExercisesReverseZInfiniteFarRendererPipeline)
{
    ExerciseRendererPipeline(RequireReverseInfinitePerspective(), 1.0F, 0.001F);
}
} // namespace
