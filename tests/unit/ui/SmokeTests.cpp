#include <ponder/ui/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(UiSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::ui::GetLibraryName(), std::string_view{"ui"});
}