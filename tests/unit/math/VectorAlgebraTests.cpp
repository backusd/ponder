#include <ponder/core/Numbers.hpp>
#include <ponder/math/Vector2.hpp>
#include <ponder/math/Vector3.hpp>
#include <ponder/math/Vector4.hpp>

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <random>

namespace
{
[[nodiscard]] pond::core::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::core::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

void ExpectVectorNear(pond::math::Vector2 actual, pond::math::Vector2 expected,
                      float tolerance = 1.0e-6F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
}

void ExpectVectorNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                      float tolerance = 1.0e-6F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectVectorNear(pond::math::Vector4 actual, pond::math::Vector4 expected,
                      float tolerance = 1.0e-6F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
    EXPECT_NEAR(actual.w, expected.w, tolerance);
}

void ExpectNormalizationFailure(auto result, pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

TEST(VectorAlgebraTests, AppliesArithmeticAcrossAllVectorDimensions)
{
    EXPECT_EQ((pond::math::Vector2{1.0F, 2.0F} + pond::math::Vector2{3.0F, 4.0F}),
              (pond::math::Vector2{4.0F, 6.0F}));
    EXPECT_EQ((pond::math::Vector2{3.0F, 4.0F} - pond::math::Vector2{1.0F, 2.0F}),
              (pond::math::Vector2{2.0F, 2.0F}));
    EXPECT_EQ((-pond::math::Vector2{1.0F, -2.0F}), (pond::math::Vector2{-1.0F, 2.0F}));
    EXPECT_EQ((pond::math::Vector2{2.0F, 4.0F} / 2.0F), (pond::math::Vector2{1.0F, 2.0F}));

    EXPECT_EQ((pond::math::Vector3{1.0F, 2.0F, 3.0F} * 2.0F),
              (pond::math::Vector3{2.0F, 4.0F, 6.0F}));
    EXPECT_EQ((2.0F * pond::math::Vector3{1.0F, 2.0F, 3.0F}),
              (pond::math::Vector3{2.0F, 4.0F, 6.0F}));
    EXPECT_EQ(pond::math::ComponentDivide(pond::math::Vector3{8.0F, 9.0F, 10.0F},
                                          pond::math::Vector3{2.0F, 3.0F, 5.0F}),
              (pond::math::Vector3{4.0F, 3.0F, 2.0F}));

    EXPECT_EQ(pond::math::ComponentMultiply(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                                            pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F}),
              (pond::math::Vector4{5.0F, 12.0F, 21.0F, 32.0F}));
    EXPECT_EQ(
        (pond::math::Vector4{8.0F, 6.0F, 4.0F, 2.0F} - pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F}),
        (pond::math::Vector4{7.0F, 4.0F, 1.0F, -2.0F}));
}

TEST(VectorAlgebraTests, ComputesProductsLengthsDistancesMinMaxAndInterpolation)
{
    EXPECT_EQ(pond::math::Dot(pond::math::Vector2{1.0F, 2.0F}, pond::math::Vector2{3.0F, 4.0F}),
              11.0F);
    EXPECT_EQ(pond::math::SquaredLength(pond::math::Vector2{3.0F, 4.0F}), 25.0F);
    EXPECT_FLOAT_EQ(pond::math::Length(pond::math::Vector2{3.0F, 4.0F}), 5.0F);
    EXPECT_EQ(pond::math::SquaredDistance(pond::math::Vector2{-1.0F, 2.0F},
                                          pond::math::Vector2{2.0F, 6.0F}),
              25.0F);
    EXPECT_FLOAT_EQ(
        pond::math::Distance(pond::math::Vector2{-1.0F, 2.0F}, pond::math::Vector2{2.0F, 6.0F}),
        5.0F);

    EXPECT_EQ(pond::math::ComponentMin(pond::math::Vector3{3.0F, -2.0F, 5.0F},
                                       pond::math::Vector3{1.0F, 4.0F, 5.0F}),
              (pond::math::Vector3{1.0F, -2.0F, 5.0F}));
    EXPECT_EQ(pond::math::ComponentMax(pond::math::Vector3{3.0F, -2.0F, 5.0F},
                                       pond::math::Vector3{1.0F, 4.0F, 5.0F}),
              (pond::math::Vector3{3.0F, 4.0F, 5.0F}));
    EXPECT_EQ(pond::math::Lerp(pond::math::Vector3{0.0F, 2.0F, 4.0F},
                               pond::math::Vector3{10.0F, 12.0F, 14.0F}, 0.25F),
              (pond::math::Vector3{2.5F, 4.5F, 6.5F}));

    EXPECT_EQ(pond::math::Dot(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                              pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F}),
              70.0F);
    EXPECT_EQ(pond::math::SquaredDistance(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                                          pond::math::Vector4{2.0F, 4.0F, 6.0F, 8.0F}),
              30.0F);
}

TEST(VectorAlgebraTests, ComputesScaleSafeVector2LengthsAndDistances)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();
    constexpr float kMinimumSubnormal = std::numeric_limits<float>::denorm_min();

    EXPECT_FLOAT_EQ(pond::math::Length(pond::math::Vector2{3.0F, 4.0F}), 5.0F);
    EXPECT_EQ(pond::math::Length(pond::math::Vector2{kMaximum, 0.0F}), kMaximum);
    EXPECT_EQ(pond::math::Length(pond::math::Vector2{kMinimumSubnormal, 0.0F}), kMinimumSubnormal);
    EXPECT_EQ(pond::math::Length(pond::math::Vector2{kMaximum, kMinimumSubnormal}), kMaximum);
    EXPECT_FALSE(std::signbit(pond::math::Length(pond::math::Vector2{-0.0F, 0.0F})));

    EXPECT_EQ(pond::math::Distance(pond::math::Vector2{}, pond::math::Vector2{kMaximum, 0.0F}),
              kMaximum);
    EXPECT_EQ(
        pond::math::Distance(pond::math::Vector2{}, pond::math::Vector2{kMinimumSubnormal, 0.0F}),
        kMinimumSubnormal);
    EXPECT_TRUE(std::isinf(pond::math::Distance(pond::math::Vector2{-kMaximum, 0.0F},
                                                pond::math::Vector2{kMaximum, 0.0F})));
}

TEST(VectorAlgebraTests, ComputesScaleSafeVector3LengthsAndDistances)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();
    constexpr float kMinimumSubnormal = std::numeric_limits<float>::denorm_min();

    EXPECT_FLOAT_EQ(pond::math::Length(pond::math::Vector3{2.0F, -3.0F, 6.0F}), 7.0F);
    EXPECT_EQ(pond::math::Length(pond::math::Vector3{0.0F, kMaximum, 0.0F}), kMaximum);
    EXPECT_EQ(pond::math::Length(pond::math::Vector3{0.0F, 0.0F, kMinimumSubnormal}),
              kMinimumSubnormal);
    EXPECT_EQ(pond::math::Length(pond::math::Vector3{kMinimumSubnormal, kMaximum, 0.0F}), kMaximum);
    EXPECT_FALSE(std::signbit(pond::math::Length(pond::math::Vector3{-0.0F, 0.0F, -0.0F})));

    EXPECT_EQ(
        pond::math::Distance(pond::math::Vector3{}, pond::math::Vector3{0.0F, kMaximum, 0.0F}),
        kMaximum);
    EXPECT_EQ(pond::math::Distance(pond::math::Vector3{},
                                   pond::math::Vector3{0.0F, 0.0F, kMinimumSubnormal}),
              kMinimumSubnormal);
    EXPECT_TRUE(std::isinf(pond::math::Distance(pond::math::Vector3{-kMaximum, 0.0F, 0.0F},
                                                pond::math::Vector3{kMaximum, 0.0F, 0.0F})));
}

TEST(VectorAlgebraTests, ComputesScaleSafeVector4LengthsAndDistances)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();
    constexpr float kMinimumSubnormal = std::numeric_limits<float>::denorm_min();

    EXPECT_FLOAT_EQ(pond::math::Length(pond::math::Vector4{1.0F, 2.0F, 2.0F, 4.0F}), 5.0F);
    EXPECT_EQ(pond::math::Length(pond::math::Vector4{0.0F, 0.0F, kMaximum, 0.0F}), kMaximum);
    EXPECT_EQ(pond::math::Length(pond::math::Vector4{0.0F, kMinimumSubnormal, 0.0F, 0.0F}),
              kMinimumSubnormal);
    EXPECT_EQ(pond::math::Length(pond::math::Vector4{kMaximum, 0.0F, kMinimumSubnormal, 0.0F}),
              kMaximum);
    EXPECT_FALSE(std::signbit(pond::math::Length(pond::math::Vector4{-0.0F, 0.0F, -0.0F, 0.0F})));

    EXPECT_EQ(pond::math::Distance(pond::math::Vector4{},
                                   pond::math::Vector4{0.0F, 0.0F, kMaximum, 0.0F}),
              kMaximum);
    EXPECT_EQ(pond::math::Distance(pond::math::Vector4{},
                                   pond::math::Vector4{0.0F, kMinimumSubnormal, 0.0F, 0.0F}),
              kMinimumSubnormal);
    EXPECT_TRUE(std::isinf(pond::math::Distance(pond::math::Vector4{-kMaximum, 0.0F, 0.0F, 0.0F},
                                                pond::math::Vector4{kMaximum, 0.0F, 0.0F, 0.0F})));
}

TEST(VectorAlgebraTests, InterpolatesExtremeVectorsWithoutIntermediateOverflow)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();

    EXPECT_EQ(pond::math::Lerp(pond::math::Vector2{-kMaximum, kMaximum},
                               pond::math::Vector2{kMaximum, -kMaximum}, 0.5F),
              pond::math::Vector2{});
    EXPECT_EQ(pond::math::Lerp(pond::math::Vector3{-kMaximum, kMaximum, -kMaximum},
                               pond::math::Vector3{kMaximum, -kMaximum, kMaximum}, 0.5F),
              pond::math::Vector3{});
    EXPECT_EQ(pond::math::Lerp(pond::math::Vector4{-kMaximum, kMaximum, -kMaximum, kMaximum},
                               pond::math::Vector4{kMaximum, -kMaximum, kMaximum, -kMaximum}, 0.5F),
              pond::math::Vector4{});
}

TEST(VectorAlgebraTests, PreservesBasisVectorOrientation)
{
    constexpr pond::math::Vector3 xAxis{1.0F, 0.0F, 0.0F};
    constexpr pond::math::Vector3 yAxis{0.0F, 1.0F, 0.0F};
    constexpr pond::math::Vector3 zAxis{0.0F, 0.0F, 1.0F};

    EXPECT_EQ(pond::math::Dot(xAxis, yAxis), 0.0F);
    EXPECT_EQ(pond::math::Dot(xAxis, xAxis), 1.0F);
    EXPECT_EQ(pond::math::Cross(xAxis, yAxis), zAxis);
    EXPECT_EQ(pond::math::Cross(yAxis, xAxis), -zAxis);
}

TEST(VectorAlgebraTests, ComparesVectorsWithCallerTolerance)
{
    const pond::core::Tolerance tolerance = RequireTolerance(0.01F, 0.0F);

    EXPECT_TRUE(pond::math::IsNear(pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                   pond::math::Vector3{1.005F, 1.995F, 3.0F}, tolerance));
    EXPECT_FALSE(pond::math::IsNear(pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                    pond::math::Vector3{1.005F, 1.995F, 3.02F}, tolerance));
    EXPECT_FALSE(
        pond::math::IsNear(pond::math::Vector2{std::numeric_limits<float>::quiet_NaN(), 0.0F},
                           pond::math::Vector2{0.0F, 0.0F}, tolerance));
}

TEST(VectorAlgebraTests, NormalizesFiniteVectors)
{
    auto normalized2 = pond::math::Normalize(pond::math::Vector2{3.0F, 4.0F});
    ASSERT_TRUE(normalized2.HasValue());
    ExpectVectorNear(normalized2.GetValue(), pond::math::Vector2{0.6F, 0.8F});
    EXPECT_NEAR(pond::math::Length(normalized2.GetValue()), 1.0F, 1.0e-6F);

    auto normalized3 = pond::math::Normalize(pond::math::Vector3{0.0F, -3.0F, 4.0F});
    ASSERT_TRUE(normalized3.HasValue());
    ExpectVectorNear(normalized3.GetValue(), pond::math::Vector3{0.0F, -0.6F, 0.8F});
    EXPECT_NEAR(pond::math::Length(normalized3.GetValue()), 1.0F, 1.0e-6F);

    auto normalized4 = pond::math::Normalize(pond::math::Vector4{1.0F, 2.0F, 2.0F, 4.0F});
    ASSERT_TRUE(normalized4.HasValue());
    ExpectVectorNear(normalized4.GetValue(), pond::math::Vector4{0.2F, 0.4F, 0.4F, 0.8F});
    EXPECT_NEAR(pond::math::Length(normalized4.GetValue()), 1.0F, 1.0e-6F);
}

TEST(VectorAlgebraTests, NormalizesLargeAndSmallMagnitudes)
{
    const float max = std::numeric_limits<float>::max();
    auto large = pond::math::Normalize(pond::math::Vector4{max, -max, max, -max});
    ASSERT_TRUE(large.HasValue());
    ExpectVectorNear(large.GetValue(), pond::math::Vector4{0.5F, -0.5F, 0.5F, -0.5F});
    EXPECT_NEAR(pond::math::Length(large.GetValue()), 1.0F, 1.0e-6F);

    const float tiny = std::numeric_limits<float>::denorm_min();
    auto small = pond::math::Normalize(pond::math::Vector3{0.0F, -tiny, 0.0F});
    ASSERT_TRUE(small.HasValue());
    ExpectVectorNear(small.GetValue(), pond::math::Vector3{0.0F, -1.0F, 0.0F});
    EXPECT_NEAR(pond::math::Length(small.GetValue()), 1.0F, 1.0e-6F);
}

TEST(VectorAlgebraTests, RejectsInvalidNormalizationInputsWithStableCodes)
{
    ExpectNormalizationFailure(pond::math::Normalize(pond::math::Vector2{}),
                               pond::math::MathErrorCode::DegenerateInput);
    ExpectNormalizationFailure(pond::math::Normalize(pond::math::Vector3{}),
                               pond::math::MathErrorCode::DegenerateInput);
    ExpectNormalizationFailure(pond::math::Normalize(pond::math::Vector4{}),
                               pond::math::MathErrorCode::DegenerateInput);
    ExpectNormalizationFailure(
        pond::math::Normalize(pond::math::Vector2{std::numeric_limits<float>::infinity(), 0.0F}),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectNormalizationFailure(pond::math::Normalize(pond::math::Vector3{
                                   0.0F, std::numeric_limits<float>::quiet_NaN(), 0.0F}),
                               pond::math::MathErrorCode::NonFiniteInput);
    ExpectNormalizationFailure(pond::math::Normalize(pond::math::Vector4{
                                   0.0F, 0.0F, -std::numeric_limits<float>::infinity(), 0.0F}),
                               pond::math::MathErrorCode::NonFiniteInput);
}

TEST(VectorAlgebraTests, SeededFinitePropertyChecks)
{
    std::mt19937 generator{0x506F6E64U};
    std::uniform_real_distribution<float> distribution{-10.0F, 10.0F};
    const pond::core::Tolerance vectorTolerance = RequireTolerance(1.0e-3F, 1.0e-5F);
    const pond::core::Tolerance scalarTolerance = RequireTolerance(1.0e-3F, 1.0e-5F);
    const pond::core::Tolerance orthogonalityTolerance = RequireTolerance(1.0e-2F, 1.0e-5F);

    for (int iteration = 0; iteration < 64; ++iteration)
    {
        const pond::math::Vector3 a{distribution(generator) + 0.25F, distribution(generator) - 0.5F,
                                    distribution(generator) + 0.75F};
        const pond::math::Vector3 b{distribution(generator) - 0.125F,
                                    distribution(generator) + 0.375F,
                                    distribution(generator) - 0.625F};

        EXPECT_TRUE(pond::math::IsNear((a + b) - b, a, vectorTolerance));
        EXPECT_TRUE(pond::math::IsNear((a * 3.0F) / 3.0F, a, vectorTolerance));
        EXPECT_TRUE(
            pond::core::IsNear(pond::math::Dot(a, b), pond::math::Dot(b, a), scalarTolerance));
        EXPECT_TRUE(pond::core::IsNear(pond::math::SquaredDistance(a, b),
                                       pond::math::SquaredDistance(b, a), scalarTolerance));

        const pond::math::Vector3 cross = pond::math::Cross(a, b);
        EXPECT_TRUE(pond::core::IsNear(pond::math::Dot(cross, a), 0.0F, orthogonalityTolerance));
        EXPECT_TRUE(pond::core::IsNear(pond::math::Dot(cross, b), 0.0F, orthogonalityTolerance));

        auto normalized = pond::math::Normalize(a);
        ASSERT_TRUE(normalized.HasValue());
        EXPECT_TRUE(
            pond::core::IsNear(pond::math::Length(normalized.GetValue()), 1.0F, scalarTolerance));
    }
}
} // namespace
