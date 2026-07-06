#include <ponder/core/BuildInfo.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>

namespace
{
[[nodiscard]] bool IsHexDigit(char character) noexcept
{
    return std::isxdigit(static_cast<unsigned char>(character)) != 0;
}

[[nodiscard]] bool IsKnownBuildType(std::string_view buildType) noexcept
{
    constexpr std::array<std::string_view, 6> kKnownBuildTypes{
        "", "Debug", "Release", "RelWithDebInfo", "MinSizeRel", "multi-config"};

    return std::ranges::find(kKnownBuildTypes, buildType) != kKnownBuildTypes.end();
}

TEST(BuildInfoTests, ExposesStableProjectMetadata)
{
    const pond::core::BuildInfo kBuildInfo = pond::core::GetBuildInfo();

    EXPECT_EQ(kBuildInfo.GetProjectName(), std::string_view{"ponder"});
    EXPECT_EQ(kBuildInfo.GetProjectVersion(), std::string_view{"0.1.0"});
}

TEST(BuildInfoTests, ExposesOptionalGitCommitWhenAvailable)
{
    const pond::core::BuildInfo kBuildInfo = pond::core::GetBuildInfo();

    EXPECT_EQ(kBuildInfo.HasGitCommit(), !kBuildInfo.GetGitCommit().empty());

    if (kBuildInfo.HasGitCommit())
    {
        EXPECT_GE(kBuildInfo.GetGitCommit().size(), 7U);
        EXPECT_TRUE(std::ranges::all_of(kBuildInfo.GetGitCommit(), IsHexDigit));
    }
}

TEST(BuildInfoTests, ExposesBuildAndCompilerMetadata)
{
    const pond::core::BuildInfo kBuildInfo = pond::core::GetBuildInfo();

    EXPECT_TRUE(IsKnownBuildType(kBuildInfo.GetBuildType()));
    EXPECT_FALSE(kBuildInfo.GetCompilerName().empty());
}

TEST(BuildInfoTests, ExposesPlatformAndGeneratorMetadata)
{
    const pond::core::BuildInfo kBuildInfo = pond::core::GetBuildInfo();

    EXPECT_FALSE(kBuildInfo.GetTargetSystemName().empty());
    EXPECT_TRUE(kBuildInfo.GetPointerWidthBits() == 0U || kBuildInfo.GetPointerWidthBits() >= 32U);
    EXPECT_TRUE(kBuildInfo.GetPointerWidthBits() == 0U ||
                kBuildInfo.GetPointerWidthBits() % std::uint32_t{8} == 0U);
    EXPECT_FALSE(kBuildInfo.GetCMakeGenerator().empty());
    EXPECT_FALSE(kBuildInfo.GetCMakeVersion().empty());
}
} // namespace