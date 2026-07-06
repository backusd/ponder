#include <ponder/workflow/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(WorkflowSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::workflow::GetLibraryName(), std::string_view{"workflow"});
}