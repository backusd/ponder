#include <ponder/platform/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(PlatformSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::platform::GetLibraryName(), std::string_view{"platform"});
}