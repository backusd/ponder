#include <ponder/project/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(ProjectSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::project::GetLibraryName(), std::string_view{"project"});
}