#include <ponder/core/Exception.hpp>
#include <ponder/core/Numbers.hpp>

#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

namespace
{
template <typename T>
concept HasIsFinite = requires(T value) { ponder::core::IsFinite(value); };

template <typename Target, typename Source>
concept HasRoundToInteger = requires(Source value) { ponder::core::RoundToInteger<Target>(value); };

[[nodiscard]] ponder::core::Tolerance RequireTolerance(float absoluteTolerance, float relativeTolerance)
{
    return ponder::core::Tolerance::Create(absoluteTolerance, relativeTolerance);
}

void ExpectToleranceException(float absoluteTolerance, float relativeTolerance, std::string_view expectedMessage)
{
    try
    {
        (void)ponder::core::Tolerance::Create(absoluteTolerance, relativeTolerance);
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), expectedMessage);
        return;
    }

    FAIL() << "Tolerance::Create should throw";
}

static_assert(ponder::core::kPi == 0x1.921fb6p+1F);
static_assert(ponder::core::kTwoPi == 0x1.921fb6p+2F);
static_assert(ponder::core::kHalfPi == 0x1.921fb6p+0F);

static_assert(ponder::core::IsFinite(0));
static_assert(ponder::core::IsFinite(-42));
static_assert(ponder::core::IsFinite(0.0F));
static_assert(ponder::core::IsFinite(std::numeric_limits<float>::max()));
static_assert(!ponder::core::IsFinite(std::numeric_limits<float>::infinity()));
static_assert(!ponder::core::IsFinite(-std::numeric_limits<float>::infinity()));
static_assert(!ponder::core::IsFinite(std::numeric_limits<float>::quiet_NaN()));
static_assert(HasIsFinite<int>);
static_assert(HasIsFinite<unsigned int>);
static_assert(HasIsFinite<float>);
static_assert(HasIsFinite<double>);
static_assert(!HasIsFinite<bool>);
static_assert(!HasIsFinite<void*>);
static_assert(std::is_same_v<decltype(ponder::core::IsFinite(1.0F)), bool>);
static_assert(HasRoundToInteger<int, float>);
static_assert(HasRoundToInteger<std::uint16_t, double>);
static_assert(!HasRoundToInteger<bool, float>);
static_assert(!HasRoundToInteger<int, int>);
static_assert(std::is_same_v<decltype(ponder::core::RoundToInteger<int>(1.0F)), std::optional<int>>);
static_assert(noexcept(ponder::core::RoundToInteger<int>(1.0F)));

constexpr auto kCompileTimeTolerance = ponder::core::Tolerance::Create(0.25F, 0.5F);
static_assert(std::is_same_v<decltype(ponder::core::Tolerance::Create(0.0F, 0.0F)), ponder::core::Tolerance>);
static_assert(kCompileTimeTolerance.GetAbsolute() == 0.25F);
static_assert(kCompileTimeTolerance.GetRelative() == 0.5F);
constexpr auto kZeroTolerance = ponder::core::Tolerance::Create(0.0F, 0.0F);
constexpr auto kMidRelativeTolerance = ponder::core::Tolerance::Create(0.0F, 1.5F);
constexpr auto kFullRelativeTolerance = ponder::core::Tolerance::Create(0.0F, 2.0F);
static_assert(ponder::core::Clamp(2.0F, 0.0F, 1.0F) == 1.0F);
static_assert(ponder::core::Clamp(-1.0F, 0.0F, 1.0F) == 0.0F);
static_assert(ponder::core::Clamp(0.5F, 0.0F, 1.0F) == 0.5F);
static_assert(ponder::core::Lerp(2.0F, 6.0F, 0.25F) == 3.0F);
static_assert(ponder::core::Lerp(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.5F) == 0.0F);
static_assert(ponder::core::Lerp(0.0F, std::numeric_limits<float>::denorm_min() * 2.0F, 0.5F) == std::numeric_limits<float>::denorm_min());
static_assert(ponder::core::IsNear(1.0F, 1.0F, kZeroTolerance));
static_assert(ponder::core::IsNear(-0.0F, 0.0F, kZeroTolerance));
static_assert(!ponder::core::IsNear(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), kMidRelativeTolerance));
static_assert(ponder::core::IsNear(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), kFullRelativeTolerance));

TEST(NumberTests, TreatsIntegralValuesAsFinite)
{
    EXPECT_TRUE(ponder::core::IsFinite(0));
    EXPECT_TRUE(ponder::core::IsFinite(std::numeric_limits<int>::lowest()));
    EXPECT_TRUE(ponder::core::IsFinite(std::numeric_limits<unsigned int>::max()));
}

TEST(NumberTests, RejectsFloatingPointInfinityAndNan)
{
    EXPECT_TRUE(ponder::core::IsFinite(0.0F));
    EXPECT_TRUE(ponder::core::IsFinite(-0.0));
    EXPECT_TRUE(ponder::core::IsFinite(std::numeric_limits<long double>::max()));

    EXPECT_FALSE(ponder::core::IsFinite(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(ponder::core::IsFinite(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(ponder::core::IsFinite(std::numeric_limits<long double>::quiet_NaN()));
}

TEST(NumberTests, RoundsFiniteFloatingPointValuesToIntegralTargets)
{
    const std::optional<int> roundedDown = ponder::core::RoundToInteger<int>(1.49F);
    const std::optional<int> roundedUp = ponder::core::RoundToInteger<int>(1.5F);
    const std::optional<int> roundedNegative = ponder::core::RoundToInteger<int>(-1.5F);
    const std::optional<std::uint16_t> roundedUnsigned = ponder::core::RoundToInteger<std::uint16_t>(65535.0);

    ASSERT_TRUE(roundedDown.has_value());
    ASSERT_TRUE(roundedUp.has_value());
    ASSERT_TRUE(roundedNegative.has_value());
    ASSERT_TRUE(roundedUnsigned.has_value());
    EXPECT_EQ(*roundedDown, 1);
    EXPECT_EQ(*roundedUp, 2);
    EXPECT_EQ(*roundedNegative, -2);
    EXPECT_EQ(*roundedUnsigned, 65535U);
}

TEST(NumberTests, RejectsNonFiniteAndOutOfRangeRoundedIntegralTargets)
{
    const std::optional<int> infinity = ponder::core::RoundToInteger<int>(std::numeric_limits<float>::infinity());
    const std::optional<int> nan = ponder::core::RoundToInteger<int>(std::numeric_limits<float>::quiet_NaN());
    const std::optional<int> outOfRange = ponder::core::RoundToInteger<int>(static_cast<double>(std::numeric_limits<int>::max()) + 1.0);
    const std::optional<std::uint16_t> negativeUnsigned = ponder::core::RoundToInteger<std::uint16_t>(-1.0);

    EXPECT_FALSE(infinity.has_value());
    EXPECT_FALSE(nan.has_value());
    EXPECT_FALSE(outOfRange.has_value());
    EXPECT_FALSE(negativeUnsigned.has_value());
}

TEST(NumberTests, CreatesValidatedTolerances)
{
    const ponder::core::Tolerance tolerance = RequireTolerance(0.125F, 0.25F);

    EXPECT_FLOAT_EQ(tolerance.GetAbsolute(), 0.125F);
    EXPECT_FLOAT_EQ(tolerance.GetRelative(), 0.25F);
}

TEST(NumberTests, ThrowsForInvalidTolerances)
{
    ExpectToleranceException(std::numeric_limits<float>::infinity(), 0.0F, "Tolerance values must be finite.");
    ExpectToleranceException(0.0F, -1.0F, "Tolerance values must be non-negative.");
}

TEST(NumberTests, ClampsAndInterpolatesScalars)
{
    EXPECT_FLOAT_EQ(ponder::core::Clamp(1.5F, 0.0F, 1.0F), 1.0F);
    EXPECT_FLOAT_EQ(ponder::core::Clamp(-0.5F, 0.0F, 1.0F), 0.0F);
    EXPECT_FLOAT_EQ(ponder::core::Clamp(0.25F, 0.0F, 1.0F), 0.25F);
    EXPECT_FLOAT_EQ(ponder::core::Lerp(2.0F, 6.0F, 0.25F), 3.0F);
}

TEST(NumberTests, InterpolatesWithoutAvoidableOverflowOrUnderflow)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();
    constexpr float kMinimumSubnormal = std::numeric_limits<float>::denorm_min();

    EXPECT_EQ(ponder::core::Lerp(-kMaximum, kMaximum, 0.0F), -kMaximum);
    EXPECT_EQ(ponder::core::Lerp(-kMaximum, kMaximum, 0.5F), 0.0F);
    EXPECT_EQ(ponder::core::Lerp(-kMaximum, kMaximum, 1.0F), kMaximum);
    EXPECT_EQ(ponder::core::Lerp(0.0F, kMinimumSubnormal * 2.0F, 0.5F), kMinimumSubnormal);

    const float negativeZero = ponder::core::Lerp(-0.0F, 1.0F, 0.0F);
    const float zeroMidpoint = ponder::core::Lerp(-0.0F, 0.0F, 0.5F);
    EXPECT_TRUE(std::signbit(negativeZero));
    EXPECT_FALSE(std::signbit(zeroMidpoint));
}

TEST(NumberTests, DefinesInterpolationAtNonFiniteEdges)
{
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_EQ(ponder::core::Lerp(2.0F, kInfinity, 0.0F), 2.0F);
    EXPECT_EQ(ponder::core::Lerp(kInfinity, 2.0F, 1.0F), 2.0F);
    EXPECT_TRUE(std::isinf(ponder::core::Lerp(2.0F, kInfinity, 0.5F)));
    EXPECT_TRUE(std::isinf(ponder::core::Lerp(kInfinity, 2.0F, 0.5F)));
    EXPECT_EQ(ponder::core::Lerp(2.0F, nan, 0.0F), 2.0F);
    EXPECT_TRUE(std::isnan(ponder::core::Lerp(2.0F, nan, 0.5F)));
    EXPECT_TRUE(std::isnan(ponder::core::Lerp(2.0F, 4.0F, nan)));
}

TEST(NumberTests, ComparesNearScalarsWithAbsoluteAndRelativeTolerance)
{
    const ponder::core::Tolerance absoluteTolerance = RequireTolerance(0.01F, 0.0F);
    const ponder::core::Tolerance relativeTolerance = RequireTolerance(0.0F, 0.01F);

    EXPECT_TRUE(ponder::core::IsNear(1.0F, 1.005F, absoluteTolerance));
    EXPECT_FALSE(ponder::core::IsNear(1.0F, 1.02F, absoluteTolerance));
    EXPECT_TRUE(ponder::core::IsNear(1000.0F, 1005.0F, relativeTolerance));
    EXPECT_FALSE(ponder::core::IsNear(1000.0F, 1020.0F, relativeTolerance));
    EXPECT_FALSE(ponder::core::IsNear(std::numeric_limits<float>::quiet_NaN(), 0.0F, absoluteTolerance));
}

TEST(NumberTests, ComparesExtremeAndSpecialScalarsWithoutOverflow)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();
    constexpr float kMinimumSubnormal = std::numeric_limits<float>::denorm_min();
    const ponder::core::Tolerance zeroTolerance = RequireTolerance(0.0F, 0.0F);
    const ponder::core::Tolerance subnormalTolerance = RequireTolerance(kMinimumSubnormal, 0.0F);
    const ponder::core::Tolerance midRelativeTolerance = RequireTolerance(0.0F, 1.5F);
    const ponder::core::Tolerance fullRelativeTolerance = RequireTolerance(0.0F, 2.0F);

    EXPECT_TRUE(ponder::core::IsNear(-0.0F, 0.0F, zeroTolerance));
    EXPECT_TRUE(ponder::core::IsNear(0.0F, kMinimumSubnormal, subnormalTolerance));
    EXPECT_FALSE(ponder::core::IsNear(-kMaximum, kMaximum, midRelativeTolerance));
    EXPECT_TRUE(ponder::core::IsNear(-kMaximum, kMaximum, fullRelativeTolerance));
    EXPECT_FALSE(ponder::core::IsNear(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), fullRelativeTolerance));
    EXPECT_FALSE(ponder::core::IsNear(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), fullRelativeTolerance));
}
} // namespace
