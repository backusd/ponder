#include <ponder/math/Scalar.hpp>

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace
{
[[nodiscard]] pond::math::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::math::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

void ExpectToleranceFailure(pond::core::Result<pond::math::Tolerance> result,
                            pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

TEST(ScalarTests, DefinesExactFloatConstants)
{
    static_assert(pond::math::kPi == 0x1.921fb6p+1F);
    static_assert(pond::math::kTwoPi == 0x1.921fb6p+2F);
    static_assert(pond::math::kHalfPi == 0x1.921fb6p+0F);

    EXPECT_EQ(pond::math::kPi, 0x1.921fb6p+1F);
    EXPECT_EQ(pond::math::kTwoPi, 0x1.921fb6p+2F);
    EXPECT_EQ(pond::math::kHalfPi, 0x1.921fb6p+0F);
}

TEST(ScalarTests, DetectsFiniteFloatValues)
{
    EXPECT_TRUE(pond::math::IsFinite(0.0F));
    EXPECT_TRUE(pond::math::IsFinite(-0.0F));
    EXPECT_TRUE(pond::math::IsFinite(std::numeric_limits<float>::denorm_min()));
    EXPECT_TRUE(pond::math::IsFinite(std::numeric_limits<float>::max()));

    EXPECT_FALSE(pond::math::IsFinite(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(pond::math::IsFinite(-std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(pond::math::IsFinite(std::numeric_limits<float>::quiet_NaN()));
}

TEST(ScalarTests, CreatesValidatedTolerances)
{
    const pond::math::Tolerance tolerance = RequireTolerance(0.25F, 0.5F);

    EXPECT_EQ(tolerance.GetAbsolute(), 0.25F);
    EXPECT_EQ(tolerance.GetRelative(), 0.5F);
}

TEST(ScalarTests, RejectsInvalidTolerances)
{
    ExpectToleranceFailure(pond::math::Tolerance::Create(-1.0F, 0.0F),
                           pond::math::MathErrorCode::InvalidArgument);
    ExpectToleranceFailure(pond::math::Tolerance::Create(0.0F, -1.0F),
                           pond::math::MathErrorCode::InvalidArgument);
    ExpectToleranceFailure(
        pond::math::Tolerance::Create(std::numeric_limits<float>::infinity(), 0.0F),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectToleranceFailure(
        pond::math::Tolerance::Create(0.0F, std::numeric_limits<float>::quiet_NaN()),
        pond::math::MathErrorCode::NonFiniteInput);
}

TEST(ScalarTests, ComparesWithExactAndAbsoluteTolerance)
{
    const pond::math::Tolerance exact = RequireTolerance(0.0F, 0.0F);
    const pond::math::Tolerance absolute = RequireTolerance(0.001F, 0.0F);

    EXPECT_TRUE(pond::math::IsNear(0.0F, -0.0F, exact));
    EXPECT_TRUE(pond::math::IsNear(1.0F, 1.0005F, absolute));
    EXPECT_FALSE(pond::math::IsNear(1.0F, 1.002F, absolute));
}

TEST(ScalarTests, ComparesSubnormalAndLargeValues)
{
    const float tiny = std::numeric_limits<float>::denorm_min();
    const pond::math::Tolerance exact = RequireTolerance(0.0F, 0.0F);
    const pond::math::Tolerance tinyAbsolute = RequireTolerance(tiny, 0.0F);

    EXPECT_FALSE(pond::math::IsNear(0.0F, tiny, exact));
    EXPECT_TRUE(pond::math::IsNear(0.0F, tiny, tinyAbsolute));

    const float large = 1.0e20F;
    const float nearbyLarge = large + 1.0e15F;
    EXPECT_TRUE(pond::math::IsNear(large, nearbyLarge, RequireTolerance(0.0F, 1.0e-4F)));
    EXPECT_FALSE(pond::math::IsNear(large, nearbyLarge, RequireTolerance(0.0F, 1.0e-6F)));
}

TEST(ScalarTests, RejectsNonFiniteNearComparisonInputs)
{
    const pond::math::Tolerance tolerance = RequireTolerance(1.0F, 1.0F);

    EXPECT_FALSE(pond::math::IsNear(std::numeric_limits<float>::infinity(), 1.0F, tolerance));
    EXPECT_FALSE(pond::math::IsNear(1.0F, -std::numeric_limits<float>::infinity(), tolerance));
    EXPECT_FALSE(pond::math::IsNear(std::numeric_limits<float>::quiet_NaN(), 1.0F, tolerance));
}

TEST(ScalarTests, ClampsAndInterpolatesValues)
{
    EXPECT_EQ(pond::math::Clamp(-1.0F, 0.0F, 2.0F), 0.0F);
    EXPECT_EQ(pond::math::Clamp(3.0F, 0.0F, 2.0F), 2.0F);
    EXPECT_EQ(pond::math::Clamp(1.0F, 0.0F, 2.0F), 1.0F);
    EXPECT_TRUE(std::signbit(pond::math::Clamp(-0.0F, -1.0F, 1.0F)));

    EXPECT_EQ(pond::math::Lerp(2.0F, 6.0F, 0.25F), 3.0F);
    EXPECT_EQ(pond::math::Lerp(2.0F, 6.0F, -1.0F), -2.0F);
}
} // namespace
