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
concept HasIsFinite = requires(T value) { pond::core::IsFinite(value); };

template <typename Target, typename Source>
concept HasRoundToInteger = requires(Source value) { pond::core::RoundToInteger<Target>(value); };

[[nodiscard]] pond::core::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::core::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

void ExpectToleranceFailure(pond::core::Result<pond::core::Tolerance> result,
                            pond::core::ErrorCode expectedCode, std::string_view expectedMessage)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), expectedCode);
    EXPECT_EQ(result.GetError().GetMessage(), expectedMessage);
}

static_assert(pond::core::kPi == 0x1.921fb6p+1F);
static_assert(pond::core::kTwoPi == 0x1.921fb6p+2F);
static_assert(pond::core::kHalfPi == 0x1.921fb6p+0F);

static_assert(pond::core::IsFinite(0));
static_assert(pond::core::IsFinite(-42));
static_assert(pond::core::IsFinite(0.0F));
static_assert(pond::core::IsFinite(std::numeric_limits<float>::max()));
static_assert(!pond::core::IsFinite(std::numeric_limits<float>::infinity()));
static_assert(!pond::core::IsFinite(-std::numeric_limits<float>::infinity()));
static_assert(!pond::core::IsFinite(std::numeric_limits<float>::quiet_NaN()));
static_assert(HasIsFinite<int>);
static_assert(HasIsFinite<unsigned int>);
static_assert(HasIsFinite<float>);
static_assert(HasIsFinite<double>);
static_assert(!HasIsFinite<bool>);
static_assert(!HasIsFinite<void*>);
static_assert(std::is_same_v<decltype(pond::core::IsFinite(1.0F)), bool>);
static_assert(HasRoundToInteger<int, float>);
static_assert(HasRoundToInteger<std::uint16_t, double>);
static_assert(!HasRoundToInteger<bool, float>);
static_assert(!HasRoundToInteger<int, int>);
static_assert(std::is_same_v<decltype(pond::core::RoundToInteger<int>(1.0F)), std::optional<int>>);
static_assert(noexcept(pond::core::RoundToInteger<int>(1.0F)));

constexpr auto kCompileTimeTolerance = pond::core::Tolerance::Create(0.25F, 0.5F);
static_assert(kCompileTimeTolerance.HasValue());
static_assert(kCompileTimeTolerance.GetValue().GetAbsolute() == 0.25F);
static_assert(kCompileTimeTolerance.GetValue().GetRelative() == 0.5F);
constexpr auto kZeroTolerance = pond::core::Tolerance::Create(0.0F, 0.0F);
constexpr auto kMidRelativeTolerance = pond::core::Tolerance::Create(0.0F, 1.5F);
constexpr auto kFullRelativeTolerance = pond::core::Tolerance::Create(0.0F, 2.0F);
static_assert(kZeroTolerance.HasValue());
static_assert(kMidRelativeTolerance.HasValue());
static_assert(kFullRelativeTolerance.HasValue());
static_assert(pond::core::Clamp(2.0F, 0.0F, 1.0F) == 1.0F);
static_assert(pond::core::Clamp(-1.0F, 0.0F, 1.0F) == 0.0F);
static_assert(pond::core::Clamp(0.5F, 0.0F, 1.0F) == 0.5F);
static_assert(pond::core::Lerp(2.0F, 6.0F, 0.25F) == 3.0F);
static_assert(pond::core::Lerp(-std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max(), 0.5F) == 0.0F);
static_assert(pond::core::Lerp(0.0F, std::numeric_limits<float>::denorm_min() * 2.0F, 0.5F) ==
              std::numeric_limits<float>::denorm_min());
static_assert(pond::core::IsNear(1.0F, 1.0F, kZeroTolerance.GetValue()));
static_assert(pond::core::IsNear(-0.0F, 0.0F, kZeroTolerance.GetValue()));
static_assert(!pond::core::IsNear(-std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::max(),
                                  kMidRelativeTolerance.GetValue()));
static_assert(pond::core::IsNear(-std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max(),
                                 kFullRelativeTolerance.GetValue()));

TEST(NumberTests, TreatsIntegralValuesAsFinite)
{
    EXPECT_TRUE(pond::core::IsFinite(0));
    EXPECT_TRUE(pond::core::IsFinite(std::numeric_limits<int>::lowest()));
    EXPECT_TRUE(pond::core::IsFinite(std::numeric_limits<unsigned int>::max()));
}

TEST(NumberTests, RejectsFloatingPointInfinityAndNan)
{
    EXPECT_TRUE(pond::core::IsFinite(0.0F));
    EXPECT_TRUE(pond::core::IsFinite(-0.0));
    EXPECT_TRUE(pond::core::IsFinite(std::numeric_limits<long double>::max()));

    EXPECT_FALSE(pond::core::IsFinite(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(pond::core::IsFinite(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(pond::core::IsFinite(std::numeric_limits<long double>::quiet_NaN()));
}

TEST(NumberTests, RoundsFiniteFloatingPointValuesToIntegralTargets)
{
    const std::optional<int> roundedDown = pond::core::RoundToInteger<int>(1.49F);
    const std::optional<int> roundedUp = pond::core::RoundToInteger<int>(1.5F);
    const std::optional<int> roundedNegative = pond::core::RoundToInteger<int>(-1.5F);
    const std::optional<std::uint16_t> roundedUnsigned =
        pond::core::RoundToInteger<std::uint16_t>(65535.0);

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
    const std::optional<int> infinity =
        pond::core::RoundToInteger<int>(std::numeric_limits<float>::infinity());
    const std::optional<int> nan =
        pond::core::RoundToInteger<int>(std::numeric_limits<float>::quiet_NaN());
    const std::optional<int> outOfRange =
        pond::core::RoundToInteger<int>(static_cast<double>(std::numeric_limits<int>::max()) + 1.0);
    const std::optional<std::uint16_t> negativeUnsigned =
        pond::core::RoundToInteger<std::uint16_t>(-1.0);

    EXPECT_FALSE(infinity.has_value());
    EXPECT_FALSE(nan.has_value());
    EXPECT_FALSE(outOfRange.has_value());
    EXPECT_FALSE(negativeUnsigned.has_value());
}

TEST(NumberTests, CreatesValidatedTolerances)
{
    const pond::core::Tolerance tolerance = RequireTolerance(0.125F, 0.25F);

    EXPECT_FLOAT_EQ(tolerance.GetAbsolute(), 0.125F);
    EXPECT_FLOAT_EQ(tolerance.GetRelative(), 0.25F);
}

TEST(NumberTests, RejectsInvalidTolerances)
{
    constexpr pond::core::ErrorCode kNonFinite{pond::core::ErrorCategory::InvalidArgument,
                                               0x0001'0001};
    constexpr pond::core::ErrorCode kNegative{pond::core::ErrorCategory::InvalidArgument,
                                              0x0001'0002};

    ExpectToleranceFailure(
        pond::core::Tolerance::Create(std::numeric_limits<float>::infinity(), 0.0F), kNonFinite,
        "Tolerance values must be finite.");
    ExpectToleranceFailure(pond::core::Tolerance::Create(0.0F, -1.0F), kNegative,
                           "Tolerance values must be non-negative.");
}

TEST(NumberTests, ClampsAndInterpolatesScalars)
{
    EXPECT_FLOAT_EQ(pond::core::Clamp(1.5F, 0.0F, 1.0F), 1.0F);
    EXPECT_FLOAT_EQ(pond::core::Clamp(-0.5F, 0.0F, 1.0F), 0.0F);
    EXPECT_FLOAT_EQ(pond::core::Clamp(0.25F, 0.0F, 1.0F), 0.25F);
    EXPECT_FLOAT_EQ(pond::core::Lerp(2.0F, 6.0F, 0.25F), 3.0F);
}

TEST(NumberTests, InterpolatesWithoutAvoidableOverflowOrUnderflow)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();
    constexpr float kMinimumSubnormal = std::numeric_limits<float>::denorm_min();

    EXPECT_EQ(pond::core::Lerp(-kMaximum, kMaximum, 0.0F), -kMaximum);
    EXPECT_EQ(pond::core::Lerp(-kMaximum, kMaximum, 0.5F), 0.0F);
    EXPECT_EQ(pond::core::Lerp(-kMaximum, kMaximum, 1.0F), kMaximum);
    EXPECT_EQ(pond::core::Lerp(0.0F, kMinimumSubnormal * 2.0F, 0.5F), kMinimumSubnormal);

    const float negativeZero = pond::core::Lerp(-0.0F, 1.0F, 0.0F);
    const float zeroMidpoint = pond::core::Lerp(-0.0F, 0.0F, 0.5F);
    EXPECT_TRUE(std::signbit(negativeZero));
    EXPECT_FALSE(std::signbit(zeroMidpoint));
}

TEST(NumberTests, DefinesInterpolationAtNonFiniteEdges)
{
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_EQ(pond::core::Lerp(2.0F, kInfinity, 0.0F), 2.0F);
    EXPECT_EQ(pond::core::Lerp(kInfinity, 2.0F, 1.0F), 2.0F);
    EXPECT_TRUE(std::isinf(pond::core::Lerp(2.0F, kInfinity, 0.5F)));
    EXPECT_TRUE(std::isinf(pond::core::Lerp(kInfinity, 2.0F, 0.5F)));
    EXPECT_EQ(pond::core::Lerp(2.0F, nan, 0.0F), 2.0F);
    EXPECT_TRUE(std::isnan(pond::core::Lerp(2.0F, nan, 0.5F)));
    EXPECT_TRUE(std::isnan(pond::core::Lerp(2.0F, 4.0F, nan)));
}

TEST(NumberTests, ComparesNearScalarsWithAbsoluteAndRelativeTolerance)
{
    const pond::core::Tolerance absoluteTolerance = RequireTolerance(0.01F, 0.0F);
    const pond::core::Tolerance relativeTolerance = RequireTolerance(0.0F, 0.01F);

    EXPECT_TRUE(pond::core::IsNear(1.0F, 1.005F, absoluteTolerance));
    EXPECT_FALSE(pond::core::IsNear(1.0F, 1.02F, absoluteTolerance));
    EXPECT_TRUE(pond::core::IsNear(1000.0F, 1005.0F, relativeTolerance));
    EXPECT_FALSE(pond::core::IsNear(1000.0F, 1020.0F, relativeTolerance));
    EXPECT_FALSE(
        pond::core::IsNear(std::numeric_limits<float>::quiet_NaN(), 0.0F, absoluteTolerance));
}

TEST(NumberTests, ComparesExtremeAndSpecialScalarsWithoutOverflow)
{
    constexpr float kMaximum = std::numeric_limits<float>::max();
    constexpr float kMinimumSubnormal = std::numeric_limits<float>::denorm_min();
    const pond::core::Tolerance zeroTolerance = RequireTolerance(0.0F, 0.0F);
    const pond::core::Tolerance subnormalTolerance = RequireTolerance(kMinimumSubnormal, 0.0F);
    const pond::core::Tolerance midRelativeTolerance = RequireTolerance(0.0F, 1.5F);
    const pond::core::Tolerance fullRelativeTolerance = RequireTolerance(0.0F, 2.0F);

    EXPECT_TRUE(pond::core::IsNear(-0.0F, 0.0F, zeroTolerance));
    EXPECT_TRUE(pond::core::IsNear(0.0F, kMinimumSubnormal, subnormalTolerance));
    EXPECT_FALSE(pond::core::IsNear(-kMaximum, kMaximum, midRelativeTolerance));
    EXPECT_TRUE(pond::core::IsNear(-kMaximum, kMaximum, fullRelativeTolerance));
    EXPECT_FALSE(pond::core::IsNear(std::numeric_limits<float>::infinity(),
                                    std::numeric_limits<float>::infinity(), fullRelativeTolerance));
    EXPECT_FALSE(pond::core::IsNear(std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::quiet_NaN(),
                                    fullRelativeTolerance));
}
} // namespace
