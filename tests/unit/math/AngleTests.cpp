#include <ponder/math/Angle.hpp>

#include <gtest/gtest.h>

#include <array>
#include <type_traits>

namespace
{
static_assert(pond::math::Radians{}.GetValue() == 0.0F);
static_assert(pond::math::Degrees{}.GetValue() == 0.0F);
static_assert(pond::math::Radians{1.0F} == pond::math::Radians{1.0F});
static_assert(pond::math::Degrees{1.0F} == pond::math::Degrees{1.0F});
static_assert(!std::is_convertible_v<float, pond::math::Radians>);
static_assert(!std::is_convertible_v<float, pond::math::Degrees>);
static_assert(!std::is_convertible_v<pond::math::Radians, float>);
static_assert(!std::is_convertible_v<pond::math::Degrees, float>);
static_assert(!std::is_convertible_v<pond::math::Degrees, pond::math::Radians>);
static_assert(!std::is_convertible_v<pond::math::Radians, pond::math::Degrees>);

[[nodiscard]] pond::math::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::math::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

TEST(AngleTests, ProvidesExplicitScalarConstructionAndValueAccess)
{
    const pond::math::Radians radians{1.25F};
    const pond::math::Degrees degrees{90.0F};

    EXPECT_EQ(radians.GetValue(), 1.25F);
    EXPECT_EQ(degrees.GetValue(), 90.0F);
    EXPECT_EQ(pond::math::Radians{}, pond::math::Radians{0.0F});
    EXPECT_EQ(pond::math::Degrees{}, pond::math::Degrees{0.0F});
}

TEST(AngleTests, ConvertsBetweenDegreesAndRadians)
{
    const pond::math::Tolerance tolerance = RequireTolerance(1.0e-5F, 1.0e-6F);

    EXPECT_TRUE(pond::math::IsNear(pond::math::ToRadians(pond::math::Degrees{180.0F}).GetValue(),
                                   pond::math::kPi, tolerance));
    EXPECT_TRUE(pond::math::IsNear(pond::math::ToDegrees(pond::math::Radians{pond::math::kPi})
                                       .GetValue(),
                                   180.0F, tolerance));
}

TEST(AngleTests, RoundTripsRepresentativeAngles)
{
    const pond::math::Tolerance tolerance = RequireTolerance(1.0e-4F, 1.0e-6F);
    constexpr std::array kDegreeValues{-720.0F, -90.0F, 0.0F, 45.0F, 180.0F, 360.0F};

    for (const float degreeValue : kDegreeValues)
    {
        const pond::math::Degrees degrees{degreeValue};
        const pond::math::Degrees roundTripped = pond::math::ToDegrees(
            pond::math::ToRadians(degrees));

        EXPECT_TRUE(pond::math::IsNear(roundTripped.GetValue(), degreeValue, tolerance));
    }
}
} // namespace
