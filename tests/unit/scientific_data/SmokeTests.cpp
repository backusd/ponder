#include <ponder/scientific_data/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(ScientificDataSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::scientific_data::GetLibraryName(), std::string_view{"scientific_data"});
}