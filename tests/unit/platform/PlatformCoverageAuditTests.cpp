#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#ifndef PONDER_PLATFORM_PUBLIC_INCLUDE_DIR
#error "PONDER_PLATFORM_PUBLIC_INCLUDE_DIR must name the platform public include directory."
#endif

#ifndef PONDER_PLATFORM_HEADER_SELF_CONTAINMENT_DIR
#error "PONDER_PLATFORM_HEADER_SELF_CONTAINMENT_DIR must name the platform header test directory."
#endif

namespace
{
[[nodiscard]] std::string ToNarrowPath(const std::filesystem::path& path)
{
    const std::u8string utf8Path = path.u8string();
    std::string result;
    result.reserve(utf8Path.size());
    for (const char8_t character : utf8Path)
    {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

[[nodiscard]] std::vector<std::string> CollectPublicHeaderStems()
{
    std::vector<std::string> stems;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator{PONDER_PLATFORM_PUBLIC_INCLUDE_DIR})
    {
        if (entry.is_regular_file() && entry.path().extension() == ".hpp")
        {
            stems.push_back(ToNarrowPath(entry.path().stem()));
        }
    }
    std::ranges::sort(stems);
    return stems;
}

[[nodiscard]] std::vector<std::string> CollectHeaderSelfContainmentStems()
{
    std::vector<std::string> stems;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator{PONDER_PLATFORM_HEADER_SELF_CONTAINMENT_DIR})
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp")
        {
            continue;
        }

        std::string stem = ToNarrowPath(entry.path().stem());
        constexpr std::string_view kSuffix{"Header"};
        if (!stem.ends_with(kSuffix))
        {
            ADD_FAILURE() << "Unexpected platform header self-containment file: "
                          << ToNarrowPath(entry.path());
            continue;
        }

        stem.erase(stem.size() - kSuffix.size());
        stems.push_back(std::move(stem));
    }
    std::ranges::sort(stems);
    return stems;
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

TEST(PlatformCoverageAuditTests, EveryPublicHeaderHasMatchingSelfContainmentTranslationUnit)
{
    const std::vector<std::string> publicHeaders = CollectPublicHeaderStems();
    const std::vector<std::string> headerTests = CollectHeaderSelfContainmentStems();

    EXPECT_EQ(headerTests, publicHeaders);

    for (const std::string& stem : publicHeaders)
    {
        const std::filesystem::path testPath =
            std::filesystem::path{PONDER_PLATFORM_HEADER_SELF_CONTAINMENT_DIR} /
            (stem + "Header.cpp");
        const std::string expectedInclude = "#include <ponder/platform/" + stem + ".hpp>";
        EXPECT_NE(ReadFile(testPath).find(expectedInclude), std::string::npos)
            << testPath << " should include its matching public header directly.";
    }
}
} // namespace
