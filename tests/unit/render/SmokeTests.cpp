#include <ponder/render/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(RenderSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::render::GetLibraryName(), std::string_view{"render"});
}