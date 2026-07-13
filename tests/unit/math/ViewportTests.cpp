#include <ponder/core/Numbers.hpp>
#include <ponder/math/Viewport.hpp>

#include <gtest/gtest.h>
#include <limits>

namespace
{
[[nodiscard]] pond::math::Viewport RequireViewport(float originX, float originY, float width,
                                                   float height, float minimumDepth = 0.0F,
                                                   float maximumDepth = 1.0F)
{
    auto viewport =
        pond::math::Viewport::Create(originX, originY, width, height, minimumDepth, maximumDepth);
    EXPECT_TRUE(viewport.HasValue());
    return viewport.GetValue();
}

void ExpectViewportFailure(pond::core::Result<pond::math::Viewport> result,
                           pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectVectorFailure(pond::core::Result<pond::math::Vector3> result,
                         pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectVectorNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectVectorResultNear(pond::core::Result<pond::math::Vector3> actual,
                            pond::math::Vector3 expected, float tolerance = 1.0e-5F)
{
    ASSERT_TRUE(actual.HasValue());
    ExpectVectorNear(actual.GetValue(), expected, tolerance);
}

TEST(ViewportTests, CreatesValidatedViewportValues)
{
    const pond::math::Viewport viewport = RequireViewport(10.0F, 20.0F, 640.0F, 480.0F);

    EXPECT_EQ(viewport.GetOriginX(), 10.0F);
    EXPECT_EQ(viewport.GetOriginY(), 20.0F);
    EXPECT_EQ(viewport.GetWidth(), 640.0F);
    EXPECT_EQ(viewport.GetHeight(), 480.0F);
    EXPECT_EQ(viewport.GetMinimumDepth(), 0.0F);
    EXPECT_EQ(viewport.GetMaximumDepth(), 1.0F);
    EXPECT_EQ(viewport, RequireViewport(10.0F, 20.0F, 640.0F, 480.0F));
}

TEST(ViewportTests, RejectsInvalidViewportDescriptors)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();

    ExpectViewportFailure(pond::math::Viewport::Create(infinity, 0.0F, 1.0F, 1.0F),
                          pond::math::MathErrorCode::NonFiniteInput);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, quietNaN, 1.0F, 1.0F),
                          pond::math::MathErrorCode::NonFiniteInput);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, infinity, 1.0F),
                          pond::math::MathErrorCode::NonFiniteInput);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, quietNaN),
                          pond::math::MathErrorCode::NonFiniteInput);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, 1.0F, infinity, 1.0F),
                          pond::math::MathErrorCode::NonFiniteInput);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, 1.0F, 0.0F, quietNaN),
                          pond::math::MathErrorCode::NonFiniteInput);

    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 0.0F, 1.0F),
                          pond::math::MathErrorCode::InvalidArgument);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, -1.0F, 1.0F),
                          pond::math::MathErrorCode::InvalidArgument);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, 0.0F),
                          pond::math::MathErrorCode::InvalidArgument);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, -1.0F),
                          pond::math::MathErrorCode::InvalidArgument);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, 1.0F, 0.5F, 0.5F),
                          pond::math::MathErrorCode::InvalidArgument);
    ExpectViewportFailure(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F),
                          pond::math::MathErrorCode::InvalidArgument);
}

TEST(ViewportTests, MapsNdcEdgesCenterAndDepthToContinuousViewportCoordinates)
{
    const pond::math::Viewport viewport = RequireViewport(10.0F, 20.0F, 640.0F, 480.0F);

    ExpectVectorResultNear(
        pond::math::NdcToViewport(viewport, pond::math::Vector3{-1.0F, 1.0F, 0.0F}),
        pond::math::Vector3{10.0F, 20.0F, 0.0F});
    ExpectVectorResultNear(
        pond::math::NdcToViewport(viewport, pond::math::Vector3{1.0F, -1.0F, 1.0F}),
        pond::math::Vector3{650.0F, 500.0F, 1.0F});
    ExpectVectorResultNear(
        pond::math::NdcToViewport(viewport, pond::math::Vector3{0.0F, 0.0F, 0.5F}),
        pond::math::Vector3{330.0F, 260.0F, 0.5F});
}

TEST(ViewportTests, UsesArbitraryOriginAndDepthIntervalWithoutClamping)
{
    const pond::math::Viewport viewport =
        RequireViewport(-100.0F, 50.0F, 200.0F, 100.0F, 0.25F, 0.75F);

    ExpectVectorResultNear(
        pond::math::NdcToViewport(viewport, pond::math::Vector3{0.0F, 0.0F, 0.5F}),
        pond::math::Vector3{0.0F, 100.0F, 0.5F});
    ExpectVectorResultNear(
        pond::math::NdcToViewport(viewport, pond::math::Vector3{2.0F, -3.0F, 1.5F}),
        pond::math::Vector3{200.0F, 250.0F, 1.0F});
    ExpectVectorResultNear(
        pond::math::ViewportToNdc(viewport, pond::math::Vector3{200.0F, 250.0F, 1.0F}),
        pond::math::Vector3{2.0F, -3.0F, 1.5F});
}

TEST(ViewportTests, MapsViewportCoordinatesBackToNdc)
{
    const pond::math::Viewport viewport =
        RequireViewport(10.0F, 20.0F, 640.0F, 480.0F, 0.25F, 0.75F);

    ExpectVectorResultNear(
        pond::math::ViewportToNdc(viewport, pond::math::Vector3{10.0F, 20.0F, 0.25F}),
        pond::math::Vector3{-1.0F, 1.0F, 0.0F});
    ExpectVectorResultNear(
        pond::math::ViewportToNdc(viewport, pond::math::Vector3{650.0F, 500.0F, 0.75F}),
        pond::math::Vector3{1.0F, -1.0F, 1.0F});
    ExpectVectorResultNear(
        pond::math::ViewportToNdc(viewport, pond::math::Vector3{330.0F, 260.0F, 0.5F}),
        pond::math::Vector3{0.0F, 0.0F, 0.5F});
}

TEST(ViewportTests, MapsRepresentableMaximumRangeCoordinatesWithoutIntermediateOverflow)
{
    constexpr float kFiniteMaximum = std::numeric_limits<float>::max();
    const pond::math::Viewport viewport = RequireViewport(
        0.0F, 0.0F, kFiniteMaximum, kFiniteMaximum, -kFiniteMaximum, kFiniteMaximum);

    auto maximumEdge = pond::math::NdcToViewport(viewport, pond::math::Vector3{1.0F, -1.0F, 0.5F});
    ASSERT_TRUE(maximumEdge.HasValue());
    EXPECT_FLOAT_EQ(maximumEdge->x, kFiniteMaximum);
    EXPECT_FLOAT_EQ(maximumEdge->y, kFiniteMaximum);
    EXPECT_FLOAT_EQ(maximumEdge->z, 0.0F);

    auto minimumEdge = pond::math::NdcToViewport(viewport, pond::math::Vector3{-1.0F, 1.0F, 0.0F});
    ASSERT_TRUE(minimumEdge.HasValue());
    EXPECT_FLOAT_EQ(minimumEdge->x, 0.0F);
    EXPECT_FLOAT_EQ(minimumEdge->y, 0.0F);
    EXPECT_FLOAT_EQ(minimumEdge->z, -kFiniteMaximum);

    auto roundTrip = pond::math::ViewportToNdc(
        viewport, pond::math::Vector3{kFiniteMaximum, kFiniteMaximum, 0.0F});
    ASSERT_TRUE(roundTrip.HasValue());
    EXPECT_EQ(roundTrip.GetValue(), (pond::math::Vector3{1.0F, -1.0F, 0.5F}));

    const pond::math::Viewport offsetViewport =
        RequireViewport(kFiniteMaximum * 0.5F, -kFiniteMaximum * 0.5F, kFiniteMaximum * 0.5F,
                        kFiniteMaximum * 0.5F);
    auto offsetEdge =
        pond::math::NdcToViewport(offsetViewport, pond::math::Vector3{1.0F, -1.0F, 0.5F});
    ASSERT_TRUE(offsetEdge.HasValue());
    EXPECT_FLOAT_EQ(offsetEdge->x, kFiniteMaximum);
    EXPECT_FLOAT_EQ(offsetEdge->y, 0.0F);
    EXPECT_FLOAT_EQ(offsetEdge->z, 0.5F);
    ExpectVectorResultNear(pond::math::ViewportToNdc(offsetViewport, offsetEdge.GetValue()),
                           pond::math::Vector3{1.0F, -1.0F, 0.5F});
}

TEST(ViewportTests, RejectsNonFiniteMappingInputs)
{
    const pond::math::Viewport viewport = RequireViewport(0.0F, 0.0F, 100.0F, 100.0F);
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();

    ExpectVectorFailure(
        pond::math::NdcToViewport(viewport, pond::math::Vector3{infinity, 0.0F, 0.0F}),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectVectorFailure(
        pond::math::ViewportToNdc(viewport, pond::math::Vector3{0.0F, quietNaN, 0.0F}),
        pond::math::MathErrorCode::NonFiniteInput);

    constexpr float kFiniteMaximum = std::numeric_limits<float>::max();
    const pond::math::Viewport overflowing =
        RequireViewport(kFiniteMaximum, 0.0F, kFiniteMaximum, 1.0F);
    ExpectVectorFailure(
        pond::math::NdcToViewport(overflowing, pond::math::Vector3{1.0F, 0.0F, 0.0F}),
        pond::math::MathErrorCode::InvalidArgument);
}

TEST(ViewportTests, ProjectsForwardAndReverseDepthDirections)
{
    const pond::math::Viewport viewport = RequireViewport(0.0F, 0.0F, 800.0F, 600.0F);
    auto forward =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::core::kHalfPi}, 4.0F / 3.0F,
                                           1.0F, 10.0F, pond::math::ProjectionDepth::ForwardZ);
    auto reverse =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::core::kHalfPi}, 4.0F / 3.0F,
                                           1.0F, 10.0F, pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(forward.HasValue());
    ASSERT_TRUE(reverse.HasValue());

    ExpectVectorResultNear(
        pond::math::Project(forward.GetValue(), viewport, pond::math::Vector3{0.0F, 0.0F, -1.0F}),
        pond::math::Vector3{400.0F, 300.0F, 0.0F}, 2.0e-5F);
    ExpectVectorResultNear(pond::math::Project(forward.GetValue(), viewport,
                                               pond::math::Vector3{10.0F, -7.5F, -10.0F}),
                           pond::math::Vector3{700.0F, 525.0F, 1.0F}, 2.0e-5F);
    ExpectVectorResultNear(
        pond::math::Project(reverse.GetValue(), viewport, pond::math::Vector3{0.0F, 0.0F, -1.0F}),
        pond::math::Vector3{400.0F, 300.0F, 1.0F}, 2.0e-5F);
    ExpectVectorResultNear(
        pond::math::Project(reverse.GetValue(), viewport, pond::math::Vector3{0.0F, 0.0F, -10.0F}),
        pond::math::Vector3{400.0F, 300.0F, 0.0F}, 2.0e-5F);
}

TEST(ViewportTests, CheckedProjectionAndUnprojectionUseWideHomogeneousProducts)
{
    constexpr float kFiniteMaximum = std::numeric_limits<float>::max();
    const pond::math::Viewport viewport = RequireViewport(0.0F, 0.0F, 100.0F, 100.0F);
    const pond::math::Matrix4x4 clipToWorld{kFiniteMaximum, kFiniteMaximum, 0.0F, 0.0F, 0.0F, 1.0F,
                                            0.0F,           0.0F,           0.0F, 0.0F, 1.0F, 0.0F,
                                            0.0F,           0.0F,           0.0F, 1.0F};

    auto projected =
        pond::math::Project(clipToWorld, viewport, pond::math::Vector3{2.0F, -2.0F, 0.0F});
    ASSERT_TRUE(projected.HasValue());
    EXPECT_EQ(projected.GetValue(), (pond::math::Vector3{50.0F, 150.0F, 0.0F}));

    const pond::math::Vector3 cancellationPoint{150.0F, 150.0F, 0.0F};
    auto cached = pond::math::UnprojectFromClipToWorld(clipToWorld, viewport, cancellationPoint);
    ASSERT_TRUE(cached.HasValue());
    EXPECT_EQ(cached.GetValue(), (pond::math::Vector3{0.0F, -2.0F, 0.0F}));

    constexpr float kExactlyInvertibleLargeValue = 0x1.0p+126F;
    const pond::math::Matrix4x4 regularClipToWorld{kExactlyInvertibleLargeValue,
                                                   kExactlyInvertibleLargeValue,
                                                   0.0F,
                                                   0.0F,
                                                   0.0F,
                                                   1.0F,
                                                   0.0F,
                                                   0.0F,
                                                   0.0F,
                                                   0.0F,
                                                   1.0F,
                                                   0.0F,
                                                   0.0F,
                                                   0.0F,
                                                   0.0F,
                                                   1.0F};
    auto worldToClip = pond::math::Inverse(regularClipToWorld);
    ASSERT_TRUE(worldToClip.HasValue());
    auto regular = pond::math::Unproject(worldToClip.GetValue(), viewport,
                                         pond::math::Vector3{450.0F, 450.0F, 0.0F});
    ASSERT_TRUE(regular.HasValue());
    EXPECT_EQ(regular.GetValue(), (pond::math::Vector3{0.0F, -8.0F, 0.0F}));
}

TEST(ViewportTests, RoundTripsPerspectiveWorldScreenWorld)
{
    const pond::math::Viewport viewport =
        RequireViewport(13.0F, 17.0F, 1280.0F, 720.0F, 0.1F, 0.9F);
    auto projection =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::core::kHalfPi}, 16.0F / 9.0F,
                                           0.5F, 50.0F, pond::math::ProjectionDepth::ForwardZ);
    ASSERT_TRUE(projection.HasValue());
    auto clipToWorld = pond::math::Inverse(projection.GetValue());
    ASSERT_TRUE(clipToWorld.HasValue());

    const pond::math::Vector3 points[]{pond::math::Vector3{0.0F, 0.0F, -1.0F},
                                       pond::math::Vector3{0.25F, -0.5F, -2.0F},
                                       pond::math::Vector3{-4.0F, 2.0F, -10.0F}};

    for (pond::math::Vector3 point : points)
    {
        auto projected = pond::math::Project(projection.GetValue(), viewport, point);
        ASSERT_TRUE(projected.HasValue());
        ExpectVectorResultNear(
            pond::math::Unproject(projection.GetValue(), viewport, projected.GetValue()), point,
            2.0e-4F);
        ExpectVectorResultNear(pond::math::UnprojectFromClipToWorld(clipToWorld.GetValue(),
                                                                    viewport, projected.GetValue()),
                               point, 2.0e-4F);
    }
}

TEST(ViewportTests, RoundTripsOrthographicWorldScreenWorld)
{
    const pond::math::Viewport viewport =
        RequireViewport(-10.0F, 4.0F, 300.0F, 200.0F, 0.25F, 0.75F);
    auto projection = pond::math::Matrix4x4::Orthographic(-5.0F, 7.0F, -3.0F, 5.0F, 1.0F, 21.0F,
                                                          pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(projection.HasValue());

    const pond::math::Vector3 points[]{pond::math::Vector3{-5.0F, 5.0F, -1.0F},
                                       pond::math::Vector3{7.0F, -3.0F, -21.0F},
                                       pond::math::Vector3{1.0F, 2.0F, -11.0F}};

    for (pond::math::Vector3 point : points)
    {
        auto projected = pond::math::Project(projection.GetValue(), viewport, point);
        ASSERT_TRUE(projected.HasValue());
        ExpectVectorResultNear(
            pond::math::Unproject(projection.GetValue(), viewport, projected.GetValue()), point,
            2.0e-5F);
    }
}

TEST(ViewportTests, PropagatesProjectAndUnprojectFailures)
{
    const pond::math::Viewport viewport = RequireViewport(0.0F, 0.0F, 100.0F, 100.0F);
    const pond::math::Matrix4x4 zeroW{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                      0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};

    ExpectVectorFailure(pond::math::Project(zeroW, viewport, pond::math::Vector3{1.0F, 2.0F, 3.0F}),
                        pond::math::MathErrorCode::UndefinedHomogeneousCoordinate);
    ExpectVectorFailure(pond::math::Unproject(pond::math::Matrix4x4::Zero(), viewport,
                                              pond::math::Vector3{50.0F, 50.0F, 0.5F}),
                        pond::math::MathErrorCode::SingularMatrix);

    const pond::math::Matrix4x4 inverseProducesZeroW{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                                     0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                                     0.0F, 0.0F, 2.0F, -2.0F};
    ExpectVectorFailure(pond::math::Unproject(inverseProducesZeroW, viewport,
                                              pond::math::Vector3{50.0F, 50.0F, 0.5F}),
                        pond::math::MathErrorCode::UndefinedHomogeneousCoordinate);

    pond::math::Matrix4x4 nonFiniteClipToWorld = pond::math::Matrix4x4::Identity();
    nonFiniteClipToWorld.row0Column0 = std::numeric_limits<float>::infinity();
    ExpectVectorFailure(
        pond::math::UnprojectFromClipToWorld(nonFiniteClipToWorld, viewport,
                                             pond::math::Vector3{50.0F, 50.0F, 0.5F}),
        pond::math::MathErrorCode::NonFiniteInput);
}
} // namespace
