#include <ponder/plugin_sdk/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(PluginSdkSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::plugin_sdk::GetLibraryName(), std::string_view{"plugin_sdk"});
}