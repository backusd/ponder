#include <ponder/compute/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(ComputeSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::compute::GetLibraryName(), std::string_view{"compute"});
}