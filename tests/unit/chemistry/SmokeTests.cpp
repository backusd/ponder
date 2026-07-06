#include <ponder/chemistry/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(ChemistrySmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::chemistry::GetLibraryName(), std::string_view{"chemistry"});
}