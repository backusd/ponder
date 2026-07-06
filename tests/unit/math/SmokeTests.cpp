#include <ponder/math/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(MathSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::math::GetLibraryName(), std::string_view{"math"});
}