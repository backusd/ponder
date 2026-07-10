#include <ponder/io/Path.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

namespace
{
TEST(IoPathTests, ConvertsEmptyPaths)
{
    EXPECT_TRUE(pond::io::PathToUtf8(std::filesystem::path{}).empty());
    EXPECT_TRUE(pond::io::PathFromUtf8("").empty());
}

TEST(IoPathTests, RoundTripsRepresentativeUtf8PathText)
{
    const std::string pathText{"molecule_\xF0\x9F\xA7\xAA_\xE6\xB0\xB4.sld3"};
    const std::filesystem::path path = pond::io::PathFromUtf8(pathText);

    EXPECT_EQ(pond::io::PathToUtf8(path), pathText);
}
} // namespace