#include <algorithm>
#include <cstddef>
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
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{PONDER_PLATFORM_PUBLIC_INCLUDE_DIR})
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
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{PONDER_PLATFORM_HEADER_SELF_CONTAINMENT_DIR})
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp")
        {
            continue;
        }

        std::string stem = ToNarrowPath(entry.path().stem());
        constexpr std::string_view kSuffix{"Header"};
        if (!stem.ends_with(kSuffix))
        {
            ADD_FAILURE() << "Unexpected platform header self-containment file: " << ToNarrowPath(entry.path());
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

[[nodiscard]] std::size_t CountOccurrences(std::string_view text, std::string_view value)
{
    std::size_t count{};
    for (std::size_t position = text.find(value); position != std::string_view::npos; position = text.find(value, position + value.size()))
    {
        ++count;
    }
    return count;
}

struct ExpectedResultSurface final
{
    std::size_t resultCount{};
    std::size_t voidResultCount{};
};

[[nodiscard]] constexpr ExpectedResultSurface GetExpectedResultSurface(std::string_view stem)
{
    if (stem == "Runtime")
    {
        return {.resultCount = 6U, .voidResultCount = 4U};
    }
    if (stem == "Process")
    {
        return {.resultCount = 3U, .voidResultCount = 1U};
    }
    if (stem == "Window")
    {
        return {.resultCount = 2U};
    }
    return {};
}

TEST(PlatformCoverageAuditTests, EveryPublicHeaderHasMatchingSelfContainmentTranslationUnit)
{
    const std::vector<std::string> publicHeaders = CollectPublicHeaderStems();
    const std::vector<std::string> headerTests = CollectHeaderSelfContainmentStems();

    EXPECT_EQ(headerTests, publicHeaders);

    for (const std::string& stem : publicHeaders)
    {
        const std::filesystem::path testPath = std::filesystem::path{PONDER_PLATFORM_HEADER_SELF_CONTAINMENT_DIR} / (stem + "Header.cpp");
        const std::string expectedInclude = "#include <ponder/platform/" + stem + ".hpp>";
        EXPECT_NE(ReadFile(testPath).find(expectedInclude), std::string::npos) << testPath << " should include its matching public header directly.";
    }
}

TEST(PlatformCoverageAuditTests, PublicHeadersExposeExactlyTheApprovedResultSurface)
{
    constexpr std::string_view kResultType{"ponder::core::Result<"};
    constexpr std::string_view kVoidResultType{"ponder::core::VoidResult"};
    std::size_t resultCount{};
    std::size_t voidResultCount{};

    for (const std::string& stem : CollectPublicHeaderStems())
    {
        const std::filesystem::path headerPath = std::filesystem::path{PONDER_PLATFORM_PUBLIC_INCLUDE_DIR} / (stem + ".hpp");
        const std::string contents = ReadFile(headerPath);
        const ExpectedResultSurface expected = GetExpectedResultSurface(stem);
        const std::size_t headerResultCount = CountOccurrences(contents, kResultType);
        const std::size_t headerVoidResultCount = CountOccurrences(contents, kVoidResultType);

        EXPECT_EQ(headerResultCount, expected.resultCount) << headerPath << " exposes an unapproved generic Result contract.";
        EXPECT_EQ(headerVoidResultCount, expected.voidResultCount) << headerPath << " exposes an unapproved VoidResult contract.";
        resultCount += headerResultCount;
        voidResultCount += headerVoidResultCount;
    }

    EXPECT_EQ(resultCount, 11U);
    EXPECT_EQ(voidResultCount, 5U);
    EXPECT_EQ(resultCount + voidResultCount, 16U);
}
} // namespace
