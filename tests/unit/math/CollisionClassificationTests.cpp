#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/Frustum.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Sphere.hpp>

#include <gtest/gtest.h>

namespace
{
using Classification = pond::math::CollisionClassification;

[[nodiscard]] pond::core::Result<pond::math::Frustum> MakeOrthographicFrustum(
    float left = -2.0F, float right = 2.0F, float bottom = -2.0F, float top = 2.0F,
    float nearDistance = 1.0F, float farDistance = 9.0F)
{
    auto projection = pond::math::Matrix4x4::Orthographic(
        left, right, bottom, top, nearDistance, farDistance, pond::math::ProjectionDepth::ForwardZ);
    if (!projection.HasValue())
    {
        return pond::core::Result<pond::math::Frustum>::FromError(projection.GetError());
    }

    return pond::math::Frustum::FromWorldToClip(projection.GetValue());
}

[[nodiscard]] pond::core::Result<pond::math::Frustum> MakeInfinitePerspectiveFrustum()
{
    auto projection =
        pond::math::Matrix4x4::InfinitePerspective(pond::math::Radians{pond::math::kHalfPi}, 1.0F,
                                                   1.0F, pond::math::ProjectionDepth::ForwardZ);
    if (!projection.HasValue())
    {
        return pond::core::Result<pond::math::Frustum>::FromError(projection.GetError());
    }

    return pond::math::Frustum::FromWorldToClip(projection.GetValue());
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

TEST(CollisionClassificationTests, ClassifiesFiniteFrustumAxisAlignedBoxes)
{
    auto frustum = MakeOrthographicFrustum();
    ASSERT_TRUE(frustum.HasValue());

    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{-1.0F, -1.0F, -5.0F},
                                        pond::math::Vector3{1.0F, 1.0F, -2.0F})),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{-2.5F, -1.0F, -5.0F},
                                        pond::math::Vector3{-1.5F, 1.0F, -2.0F})),
              Classification::Intersects);
    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{-4.0F, -1.0F, -5.0F},
                                        pond::math::Vector3{-3.0F, 1.0F, -2.0F})),
              Classification::Disjoint);
    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{-1.0F, -1.0F, -2.0F},
                                        pond::math::Vector3{1.0F, 1.0F, -1.0F})),
              Classification::Intersects);
    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{-2.0F, -2.0F, -1.0F},
                                        pond::math::Vector3{-2.0F, -2.0F, -1.0F})),
              Classification::Intersects);
}

TEST(CollisionClassificationTests, ClassifiesFiniteFrustumSpheres)
{
    auto frustum = MakeOrthographicFrustum();
    ASSERT_TRUE(frustum.HasValue());

    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{0.0F, 0.0F, -5.0F}, 1.0F)),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{-2.0F, 0.0F, -5.0F}, 0.5F)),
              Classification::Intersects);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{-1.5F, 0.0F, -5.0F}, 0.5F)),
              Classification::Intersects);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{-3.0F, 0.0F, -5.0F}, 0.25F)),
              Classification::Disjoint);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{-2.0F, -2.0F, -1.0F}, 0.0F)),
              Classification::Intersects);
}

TEST(CollisionClassificationTests, ClassifiesZeroSizeAndVerySmallFiniteObjects)
{
    auto frustum = MakeOrthographicFrustum(-1.0e-4F, 1.0e-4F, -1.0e-4F, 1.0e-4F, 1.0e-4F, 2.0e-4F);
    ASSERT_TRUE(frustum.HasValue());

    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{0.0F, 0.0F, -1.5e-4F},
                                        pond::math::Vector3{0.0F, 0.0F, -1.5e-4F})),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{0.0F, 0.0F, -1.5e-4F}, 1.0e-5F)),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{1.5e-4F, 0.0F, -1.5e-4F},
                                        pond::math::Vector3{1.6e-4F, 0.0F, -1.5e-4F})),
              Classification::Disjoint);
}

TEST(CollisionClassificationTests, ClassifiesVeryLargeFiniteObjects)
{
    auto frustum = MakeOrthographicFrustum(-1.0e16F, 1.0e16F, -1.0e16F, 1.0e16F, 1.0e16F, 2.0e16F);
    ASSERT_TRUE(frustum.HasValue());

    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{-1.0e15F, -1.0e15F, -1.5e16F},
                                        pond::math::Vector3{1.0e15F, 1.0e15F, -1.25e16F})),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{0.0F, 0.0F, -1.5e16F}, 1.0e15F)),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{1.25e16F, 0.0F, -1.5e16F},
                                        pond::math::Vector3{1.5e16F, 1.0F, -1.25e16F})),
              Classification::Disjoint);
}

TEST(CollisionClassificationTests, SkipsAbsentFarPlaneForInfiniteFarFrustums)
{
    auto frustum = MakeInfinitePerspectiveFrustum();
    ASSERT_TRUE(frustum.HasValue());
    EXPECT_FALSE(frustum->GetMaximumDepthPlane().has_value());

    EXPECT_EQ(frustum->Classify(MakeBox(pond::math::Vector3{-1.0F, -1.0F, -1.0e6F},
                                        pond::math::Vector3{1.0F, 1.0F, -9.0e5F})),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{0.0F, 0.0F, -1.0e6F}, 10.0F)),
              Classification::Contains);
    EXPECT_EQ(frustum->Classify(MakeSphere(pond::math::Vector3{0.0F, 0.0F, -0.5F}, 0.1F)),
              Classification::Disjoint);
}

TEST(CollisionClassificationTests, RetainsObjectsInsideFloatingPointUncertainty)
{
    auto frustum = MakeOrthographicFrustum(-1.0e8F, 1.0e8F, -1.0e8F, 1.0e8F, 1.0F, 1.0e8F);
    ASSERT_TRUE(frustum.HasValue());

    const auto barelyOutsideBox = MakeBox(pond::math::Vector3{-100000040.0F, 0.0F, -10.0F},
                                          pond::math::Vector3{-100000032.0F, 1.0F, -9.0F});
    const double boxOracleMaximumDistance =
        static_cast<double>(barelyOutsideBox.GetMaximum().x) + 1.0e8;
    ASSERT_LT(boxOracleMaximumDistance, 0.0);

    EXPECT_EQ(frustum->Classify(barelyOutsideBox), Classification::Intersects);

    const auto barelyInsideBox = MakeBox(pond::math::Vector3{-99999968.0F, 0.0F, -10.0F},
                                         pond::math::Vector3{-99999960.0F, 1.0F, -9.0F});
    const double boxOracleMinimumDistance =
        static_cast<double>(barelyInsideBox.GetMinimum().x) + 1.0e8;
    ASSERT_GT(boxOracleMinimumDistance, 0.0);

    EXPECT_EQ(frustum->Classify(barelyInsideBox), Classification::Intersects);

    const auto barelyOutsideSphere =
        MakeSphere(pond::math::Vector3{-100000040.0F, 0.0F, -10.0F}, 0.0F);
    const double sphereOracleDistance =
        static_cast<double>(barelyOutsideSphere.GetCenter().x) + 1.0e8;
    ASSERT_LT(sphereOracleDistance, 0.0);

    EXPECT_EQ(frustum->Classify(barelyOutsideSphere), Classification::Intersects);
}
} // namespace