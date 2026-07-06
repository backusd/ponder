#include <ponder/io/Library.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(IoSmokeTests, ExposesLibraryName)
{
    EXPECT_EQ(pond::io::GetLibraryName(), std::string_view{"io"});
}