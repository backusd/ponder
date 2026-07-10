#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/Frustum.hpp>
#include <ponder/math/Ray.hpp>
#include <ponder/math/Sphere.hpp>
#include <ponder/math/Triangle.hpp>

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace
{
void ExpectVectorNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectPlaneNear(const pond::math::Plane& actual, pond::math::Vector3 expectedNormal,
                     float expectedOffset, float tolerance = 1.0e-5F)
{
    ExpectVectorNear(actual.GetNormal(), expectedNormal, tolerance);
    EXPECT_NEAR(actual.GetOffset(), expectedOffset, tolerance);
}

void ExpectRayFailure(pond::core::Result<pond::math::Ray> result,
                      pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectPlaneFailure(pond::core::Result<pond::math::Plane> result,
                        pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectSphereFailure(pond::core::Result<pond::math::Sphere> result,
                         pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectBoxFailure(pond::core::Result<pond::math::AxisAlignedBox> result,
                      pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectTriangleFailure(pond::core::Result<pond::math::Triangle> result,
                           pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectFrustumFailure(pond::core::Result<pond::math::Frustum> result,
                          pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

[[nodiscard]] float EvaluatePlane(const pond::math::Plane& plane, pond::math::Vector3 point)
{
    return pond::math::Dot(plane.GetNormal(), point) + plane.GetOffset();
}

TEST(CollisionValueTests, CreatesRaysWithNormalizedDirections)
{
    auto ray = pond::math::Ray::Create(pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                       pond::math::Vector3{0.0F, 3.0F, 4.0F});

    ASSERT_TRUE(ray.HasValue());
    ExpectVectorNear(ray->GetOrigin(), pond::math::Vector3{1.0F, 2.0F, 3.0F});
    ExpectVectorNear(ray->GetDirection(), pond::math::Vector3{0.0F, 0.6F, 0.8F});
    EXPECT_NEAR(pond::math::Length(ray->GetDirection()), 1.0F, 1.0e-6F);
}

TEST(CollisionValueTests, RejectsInvalidRays)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();

    ExpectRayFailure(pond::math::Ray::Create(pond::math::Vector3{infinity, 0.0F, 0.0F},
                                             pond::math::Vector3{1.0F, 0.0F, 0.0F}),
                     pond::math::MathErrorCode::NonFiniteInput);
    ExpectRayFailure(pond::math::Ray::Create(pond::math::Vector3::Zero(),
                                             pond::math::Vector3{0.0F, quietNaN, 0.0F}),
                     pond::math::MathErrorCode::NonFiniteInput);
    ExpectRayFailure(
        pond::math::Ray::Create(pond::math::Vector3::Zero(), pond::math::Vector3::Zero()),
        pond::math::MathErrorCode::DegenerateInput);
}

TEST(CollisionValueTests, CreatesPlanesWithNormalizedCoefficients)
{
    auto plane = pond::math::Plane::Create(pond::math::Vector3{0.0F, 0.0F, 2.0F}, -4.0F);
    auto negative = pond::math::Plane::Create(pond::math::Vector3{0.0F, 0.0F, -2.0F}, 6.0F);

    ASSERT_TRUE(plane.HasValue());
    ASSERT_TRUE(negative.HasValue());
    ExpectPlaneNear(plane.GetValue(), pond::math::Vector3{0.0F, 0.0F, 1.0F}, -2.0F);
    ExpectPlaneNear(negative.GetValue(), pond::math::Vector3{0.0F, 0.0F, -1.0F}, 3.0F);
    EXPECT_NEAR(EvaluatePlane(plane.GetValue(), pond::math::Vector3{0.0F, 0.0F, 2.0F}), 0.0F,
                1.0e-6F);
    EXPECT_NEAR(EvaluatePlane(negative.GetValue(), pond::math::Vector3{0.0F, 0.0F, 3.0F}), 0.0F,
                1.0e-6F);
}

TEST(CollisionValueTests, RejectsInvalidPlanes)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();

    ExpectPlaneFailure(pond::math::Plane::Create(pond::math::Vector3{infinity, 0.0F, 0.0F}, 0.0F),
                       pond::math::MathErrorCode::NonFiniteInput);
    ExpectPlaneFailure(pond::math::Plane::Create(pond::math::Vector3{1.0F, 0.0F, 0.0F}, quietNaN),
                       pond::math::MathErrorCode::NonFiniteInput);
    ExpectPlaneFailure(pond::math::Plane::Create(pond::math::Vector3::Zero(), 1.0F),
                       pond::math::MathErrorCode::DegenerateInput);
}
TEST(CollisionValueTests, CreatesSpheresIncludingPointSpheres)
{
    auto sphere = pond::math::Sphere::Create(pond::math::Vector3{1.0F, 2.0F, 3.0F}, 4.0F);
    auto point = pond::math::Sphere::Create(pond::math::Vector3{-1.0F, -2.0F, -3.0F}, 0.0F);

    ASSERT_TRUE(sphere.HasValue());
    ASSERT_TRUE(point.HasValue());
    ExpectVectorNear(sphere->GetCenter(), pond::math::Vector3{1.0F, 2.0F, 3.0F});
    EXPECT_EQ(sphere->GetRadius(), 4.0F);
    ExpectVectorNear(point->GetCenter(), pond::math::Vector3{-1.0F, -2.0F, -3.0F});
    EXPECT_EQ(point->GetRadius(), 0.0F);
}

TEST(CollisionValueTests, RejectsInvalidSpheres)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();

    ExpectSphereFailure(pond::math::Sphere::Create(pond::math::Vector3{0.0F, infinity, 0.0F}, 1.0F),
                        pond::math::MathErrorCode::NonFiniteInput);
    ExpectSphereFailure(pond::math::Sphere::Create(pond::math::Vector3::Zero(), quietNaN),
                        pond::math::MathErrorCode::NonFiniteInput);
    ExpectSphereFailure(pond::math::Sphere::Create(pond::math::Vector3::Zero(), -1.0F),
                        pond::math::MathErrorCode::InvalidArgument);
}

TEST(CollisionValueTests, CreatesAxisAlignedBoxesIncludingPointBoxes)
{
    auto box = pond::math::AxisAlignedBox::Create(pond::math::Vector3{-1.0F, -2.0F, -3.0F},
                                                  pond::math::Vector3{1.0F, 2.0F, 3.0F});
    auto point = pond::math::AxisAlignedBox::Create(pond::math::Vector3{2.0F, 3.0F, 4.0F},
                                                    pond::math::Vector3{2.0F, 3.0F, 4.0F});

    ASSERT_TRUE(box.HasValue());
    ASSERT_TRUE(point.HasValue());
    ExpectVectorNear(box->GetMinimum(), pond::math::Vector3{-1.0F, -2.0F, -3.0F});
    ExpectVectorNear(box->GetMaximum(), pond::math::Vector3{1.0F, 2.0F, 3.0F});
    ExpectVectorNear(point->GetMinimum(), pond::math::Vector3{2.0F, 3.0F, 4.0F});
    ExpectVectorNear(point->GetMaximum(), pond::math::Vector3{2.0F, 3.0F, 4.0F});
}

TEST(CollisionValueTests, RejectsInvalidAxisAlignedBoxes)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();

    ExpectBoxFailure(pond::math::AxisAlignedBox::Create(pond::math::Vector3{0.0F, infinity, 0.0F},
                                                        pond::math::Vector3{1.0F, 1.0F, 1.0F}),
                     pond::math::MathErrorCode::NonFiniteInput);
    ExpectBoxFailure(pond::math::AxisAlignedBox::Create(pond::math::Vector3::Zero(),
                                                        pond::math::Vector3{1.0F, quietNaN, 1.0F}),
                     pond::math::MathErrorCode::NonFiniteInput);
    ExpectBoxFailure(pond::math::AxisAlignedBox::Create(pond::math::Vector3{2.0F, 0.0F, 0.0F},
                                                        pond::math::Vector3{1.0F, 1.0F, 1.0F}),
                     pond::math::MathErrorCode::InvalidArgument);
    ExpectBoxFailure(pond::math::AxisAlignedBox::Create(pond::math::Vector3{0.0F, 2.0F, 0.0F},
                                                        pond::math::Vector3{1.0F, 1.0F, 1.0F}),
                     pond::math::MathErrorCode::InvalidArgument);
    ExpectBoxFailure(pond::math::AxisAlignedBox::Create(pond::math::Vector3{0.0F, 0.0F, 2.0F},
                                                        pond::math::Vector3{1.0F, 1.0F, 1.0F}),
                     pond::math::MathErrorCode::InvalidArgument);
}

TEST(CollisionValueTests, CreatesTrianglesIncludingZeroAreaTriangles)
{
    auto triangle = pond::math::Triangle::Create(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                 pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                                 pond::math::Vector3{0.0F, 1.0F, 0.0F});
    auto zeroArea = pond::math::Triangle::Create(pond::math::Vector3{1.0F, 1.0F, 1.0F},
                                                 pond::math::Vector3{1.0F, 1.0F, 1.0F},
                                                 pond::math::Vector3{1.0F, 1.0F, 1.0F});

    ASSERT_TRUE(triangle.HasValue());
    ASSERT_TRUE(zeroArea.HasValue());
    ExpectVectorNear(triangle->GetVertex0(), pond::math::Vector3{0.0F, 0.0F, 0.0F});
    ExpectVectorNear(triangle->GetVertex1(), pond::math::Vector3{1.0F, 0.0F, 0.0F});
    ExpectVectorNear(triangle->GetVertex2(), pond::math::Vector3{0.0F, 1.0F, 0.0F});
    ExpectVectorNear(zeroArea->GetVertex0(), pond::math::Vector3{1.0F, 1.0F, 1.0F});
}

TEST(CollisionValueTests, RejectsInvalidTriangles)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();

    ExpectTriangleFailure(pond::math::Triangle::Create(pond::math::Vector3{infinity, 0.0F, 0.0F},
                                                       pond::math::Vector3::Zero(),
                                                       pond::math::Vector3::Zero()),
                          pond::math::MathErrorCode::NonFiniteInput);
    ExpectTriangleFailure(pond::math::Triangle::Create(pond::math::Vector3::Zero(),
                                                       pond::math::Vector3{0.0F, quietNaN, 0.0F},
                                                       pond::math::Vector3::Zero()),
                          pond::math::MathErrorCode::NonFiniteInput);
    ExpectTriangleFailure(pond::math::Triangle::Create(pond::math::Vector3::Zero(),
                                                       pond::math::Vector3::Zero(),
                                                       pond::math::Vector3{0.0F, 0.0F, infinity}),
                          pond::math::MathErrorCode::NonFiniteInput);
}
TEST(CollisionValueTests, BuildsFrustumFromForwardPerspectiveMatrix)
{
    auto projection =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kHalfPi}, 1.0F, 1.0F,
                                           10.0F, pond::math::ProjectionDepth::ForwardZ);
    ASSERT_TRUE(projection.HasValue());

    auto frustum = pond::math::Frustum::FromWorldToClip(projection.GetValue());
    ASSERT_TRUE(frustum.HasValue());

    const float diagonal = std::sqrt(0.5F);
    ExpectPlaneNear(frustum->GetLeftPlane(), pond::math::Vector3{diagonal, 0.0F, -diagonal}, 0.0F);
    ExpectPlaneNear(frustum->GetRightPlane(), pond::math::Vector3{-diagonal, 0.0F, -diagonal},
                    0.0F);
    ASSERT_TRUE(frustum->GetMinimumDepthPlane().has_value());
    ASSERT_TRUE(frustum->GetMaximumDepthPlane().has_value());
    ExpectPlaneNear(*frustum->GetMinimumDepthPlane(), pond::math::Vector3{0.0F, 0.0F, -1.0F},
                    -1.0F);
    ExpectPlaneNear(*frustum->GetMaximumDepthPlane(), pond::math::Vector3{0.0F, 0.0F, 1.0F}, 10.0F);
    EXPECT_NEAR(
        EvaluatePlane(*frustum->GetMinimumDepthPlane(), pond::math::Vector3{0.0F, 0.0F, -1.0F}),
        0.0F, 1.0e-5F);
}

TEST(CollisionValueTests, BuildsFrustumFromReversePerspectiveMatrix)
{
    auto projection =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kHalfPi}, 1.0F, 1.0F,
                                           10.0F, pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(projection.HasValue());

    auto frustum = pond::math::Frustum::FromWorldToClip(projection.GetValue());
    ASSERT_TRUE(frustum.HasValue());

    ASSERT_TRUE(frustum->GetMinimumDepthPlane().has_value());
    ASSERT_TRUE(frustum->GetMaximumDepthPlane().has_value());
    ExpectPlaneNear(*frustum->GetMinimumDepthPlane(), pond::math::Vector3{0.0F, 0.0F, 1.0F}, 10.0F);
    ExpectPlaneNear(*frustum->GetMaximumDepthPlane(), pond::math::Vector3{0.0F, 0.0F, -1.0F},
                    -1.0F);
}

TEST(CollisionValueTests, BuildsFrustumFromInfiniteFarPerspectiveMatrices)
{
    auto forward =
        pond::math::Matrix4x4::InfinitePerspective(pond::math::Radians{pond::math::kHalfPi}, 1.0F,
                                                   1.0F, pond::math::ProjectionDepth::ForwardZ);
    auto reverse =
        pond::math::Matrix4x4::InfinitePerspective(pond::math::Radians{pond::math::kHalfPi}, 1.0F,
                                                   1.0F, pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(forward.HasValue());
    ASSERT_TRUE(reverse.HasValue());

    auto forwardFrustum = pond::math::Frustum::FromWorldToClip(forward.GetValue());
    auto reverseFrustum = pond::math::Frustum::FromWorldToClip(reverse.GetValue());
    ASSERT_TRUE(forwardFrustum.HasValue());
    ASSERT_TRUE(reverseFrustum.HasValue());

    EXPECT_TRUE(forwardFrustum->GetMinimumDepthPlane().has_value());
    EXPECT_FALSE(forwardFrustum->GetMaximumDepthPlane().has_value());
    EXPECT_FALSE(reverseFrustum->GetMinimumDepthPlane().has_value());
    EXPECT_TRUE(reverseFrustum->GetMaximumDepthPlane().has_value());
}

TEST(CollisionValueTests, BuildsFrustumFromOrthographicMatrix)
{
    auto projection = pond::math::Matrix4x4::Orthographic(-2.0F, 6.0F, -3.0F, 5.0F, 1.0F, 11.0F,
                                                          pond::math::ProjectionDepth::ForwardZ);
    ASSERT_TRUE(projection.HasValue());

    auto frustum = pond::math::Frustum::FromWorldToClip(projection.GetValue());
    ASSERT_TRUE(frustum.HasValue());

    ExpectPlaneNear(frustum->GetLeftPlane(), pond::math::Vector3{1.0F, 0.0F, 0.0F}, 2.0F);
    ExpectPlaneNear(frustum->GetRightPlane(), pond::math::Vector3{-1.0F, 0.0F, 0.0F}, 6.0F);
    ExpectPlaneNear(frustum->GetBottomPlane(), pond::math::Vector3{0.0F, 1.0F, 0.0F}, 3.0F);
    ExpectPlaneNear(frustum->GetTopPlane(), pond::math::Vector3{0.0F, -1.0F, 0.0F}, 5.0F);
    ASSERT_TRUE(frustum->GetMinimumDepthPlane().has_value());
    ASSERT_TRUE(frustum->GetMaximumDepthPlane().has_value());
    ExpectPlaneNear(*frustum->GetMinimumDepthPlane(), pond::math::Vector3{0.0F, 0.0F, -1.0F},
                    -1.0F);
    ExpectPlaneNear(*frustum->GetMaximumDepthPlane(), pond::math::Vector3{0.0F, 0.0F, 1.0F}, 11.0F);
}

TEST(CollisionValueTests, RejectsInvalidFrustumMatrices)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    const pond::math::Matrix4x4 nonFinite{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, infinity, 0.0F, 0.0F,
                                          0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F,     0.0F, 1.0F};
    const pond::math::Matrix4x4 unsatisfiableDepth{1.0F, 0.0F, 0.0F,  0.0F, 0.0F, 1.0F,
                                                   0.0F, 0.0F, 0.0F,  0.0F, 0.0F, -1.0F,
                                                   0.0F, 0.0F, -1.0F, 0.0F};

    ExpectFrustumFailure(pond::math::Frustum::FromWorldToClip(nonFinite),
                         pond::math::MathErrorCode::NonFiniteInput);
    ExpectFrustumFailure(pond::math::Frustum::FromWorldToClip(pond::math::Matrix4x4::Zero()),
                         pond::math::MathErrorCode::DegenerateInput);
    ExpectFrustumFailure(pond::math::Frustum::FromWorldToClip(unsatisfiableDepth),
                         pond::math::MathErrorCode::DegenerateInput);
}
} // namespace