#include <ponder/math/Angle.hpp>
#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/Frustum.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Ray.hpp>
#include <ponder/math/RayBoxHit.hpp>
#include <ponder/math/RayTriangleHit.hpp>
#include <ponder/math/Sphere.hpp>
#include <ponder/math/Triangle.hpp>
#include <ponder/math/Viewport.hpp>

#include <optional>

namespace pond::math::examples
{
struct CameraMatrices final
{
    Matrix4x4 view;
    Matrix4x4 reverseZProjection;
    Matrix4x4 reverseZInfiniteProjection;
    Matrix4x4 viewProjection;
};

struct ProjectRoundTrip final
{
    Vector3 screenPoint;
    Vector3 worldPoint;
};

struct CullingResult final
{
    CollisionClassification bounds;
    CollisionClassification sphere;
};

struct PickResult final
{
    Ray ray;
    RayBoxHit boundsHit;
    RayTriangleHit triangleHit;
    Vector3 triangleHitPoint;
    Vector3 rayHitPoint;
};

[[nodiscard]] core::Result<CameraMatrices> BuildCameraMatrices()
{
    auto view = Matrix4x4::LookAt(Vector3{0.0F, 1.5F, 6.0F}, Vector3{0.0F, 0.0F, -2.0F},
                                  Vector3{0.0F, 1.0F, 0.0F});
    if (!view.HasValue())
    {
        return core::Result<CameraMatrices>::FromError(view.GetError());
    }

    auto projection = Matrix4x4::Perspective(ToRadians(Degrees{60.0F}), 16.0F / 9.0F, 0.1F, 1000.0F,
                                             ProjectionDepth::ReverseZ);
    if (!projection.HasValue())
    {
        return core::Result<CameraMatrices>::FromError(projection.GetError());
    }

    auto infiniteProjection = Matrix4x4::InfinitePerspective(
        ToRadians(Degrees{60.0F}), 16.0F / 9.0F, 0.1F, ProjectionDepth::ReverseZ);
    if (!infiniteProjection.HasValue())
    {
        return core::Result<CameraMatrices>::FromError(infiniteProjection.GetError());
    }

    return CameraMatrices{view.GetValue(), projection.GetValue(), infiniteProjection.GetValue(),
                          projection.GetValue() * view.GetValue()};
}

[[nodiscard]] core::Result<ProjectRoundTrip> ProjectAndUnprojectPoint()
{
    auto camera = BuildCameraMatrices();
    if (!camera.HasValue())
    {
        return core::Result<ProjectRoundTrip>::FromError(camera.GetError());
    }

    auto viewport = Viewport::Create(0.0F, 0.0F, 1280.0F, 720.0F);
    if (!viewport.HasValue())
    {
        return core::Result<ProjectRoundTrip>::FromError(viewport.GetError());
    }

    constexpr Vector3 kWorldPoint{0.25F, -0.1F, -2.0F};
    auto screenPoint = Project(camera->viewProjection, viewport.GetValue(), kWorldPoint);
    if (!screenPoint.HasValue())
    {
        return core::Result<ProjectRoundTrip>::FromError(screenPoint.GetError());
    }

    auto worldPoint =
        Unproject(camera->viewProjection, viewport.GetValue(), screenPoint.GetValue());
    if (!worldPoint.HasValue())
    {
        return core::Result<ProjectRoundTrip>::FromError(worldPoint.GetError());
    }

    return ProjectRoundTrip{screenPoint.GetValue(), worldPoint.GetValue()};
}

[[nodiscard]] core::Result<CullingResult> ClassifyRenderableBounds()
{
    auto camera = BuildCameraMatrices();
    if (!camera.HasValue())
    {
        return core::Result<CullingResult>::FromError(camera.GetError());
    }

    auto frustum = Frustum::FromWorldToClip(camera->viewProjection);
    if (!frustum.HasValue())
    {
        return core::Result<CullingResult>::FromError(frustum.GetError());
    }

    auto bounds = AxisAlignedBox::Create(Vector3{-0.5F, -0.5F, -2.5F}, Vector3{0.5F, 0.5F, -1.5F});
    if (!bounds.HasValue())
    {
        return core::Result<CullingResult>::FromError(bounds.GetError());
    }

    auto sphere = Sphere::Create(Vector3{0.0F, 0.0F, -2.0F}, 0.75F);
    if (!sphere.HasValue())
    {
        return core::Result<CullingResult>::FromError(sphere.GetError());
    }

    return CullingResult{frustum->Classify(bounds.GetValue()),
                         frustum->Classify(sphere.GetValue())};
}

[[nodiscard]] core::Result<std::optional<PickResult>> PickTriangleCenter()
{
    auto camera = BuildCameraMatrices();
    if (!camera.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(camera.GetError());
    }

    auto viewport = Viewport::Create(0.0F, 0.0F, 1280.0F, 720.0F);
    if (!viewport.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(viewport.GetError());
    }

    auto triangle = Triangle::Create(Vector3{-0.5F, -0.5F, -2.0F}, Vector3{0.5F, -0.5F, -2.0F},
                                     Vector3{0.0F, 0.5F, -2.0F});
    if (!triangle.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(triangle.GetError());
    }

    auto bounds = AxisAlignedBox::Create(Vector3{-0.5F, -0.5F, -2.0F}, Vector3{0.5F, 0.5F, -2.0F});
    if (!bounds.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(bounds.GetError());
    }

    constexpr Vector3 kWorldTarget{0.0F, 0.0F, -2.0F};
    auto screenTarget = Project(camera->viewProjection, viewport.GetValue(), kWorldTarget);
    if (!screenTarget.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(screenTarget.GetError());
    }

    auto nearPoint = Unproject(camera->viewProjection, viewport.GetValue(),
                               Vector3{screenTarget->x, screenTarget->y, 1.0F});
    if (!nearPoint.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(nearPoint.GetError());
    }

    auto farPoint = Unproject(camera->viewProjection, viewport.GetValue(),
                              Vector3{screenTarget->x, screenTarget->y, 0.0F});
    if (!farPoint.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(farPoint.GetError());
    }

    auto ray = Ray::Create(nearPoint.GetValue(), farPoint.GetValue() - nearPoint.GetValue());
    if (!ray.HasValue())
    {
        return core::Result<std::optional<PickResult>>::FromError(ray.GetError());
    }

    const std::optional<RayBoxHit> boundsHit = Intersect(ray.GetValue(), bounds.GetValue());
    const std::optional<RayTriangleHit> triangleHit =
        Intersect(ray.GetValue(), triangle.GetValue());
    if (!boundsHit.has_value() || !triangleHit.has_value())
    {
        return std::optional<PickResult>{};
    }

    const Vector3 triangleHitPoint = triangle->GetVertex0() * triangleHit->GetBarycentric0() +
                                     triangle->GetVertex1() * triangleHit->GetBarycentric1() +
                                     triangle->GetVertex2() * triangleHit->GetBarycentric2();
    const Vector3 rayHitPoint = ray->GetOrigin() + ray->GetDirection() * triangleHit->GetDistance();

    return std::optional<PickResult>{
        PickResult{ray.GetValue(), *boundsHit, *triangleHit, triangleHitPoint, rayHitPoint}};
}
} // namespace pond::math::examples
