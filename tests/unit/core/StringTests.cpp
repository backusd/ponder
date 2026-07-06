#include <ponder/core/String.hpp>

#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>

namespace
{
[[nodiscard]] std::string MakeBytes(std::initializer_list<unsigned int> bytes)
{
    std::string text;
    text.reserve(bytes.size());

    for (unsigned int byte : bytes)
    {
        text.push_back(static_cast<char>(byte));
    }

    return text;
}

[[nodiscard]] std::wstring MakeWideRocket()
{
    std::wstring text;

    if constexpr (sizeof(wchar_t) == 2)
    {
        text.push_back(static_cast<wchar_t>(0xD83D));
        text.push_back(static_cast<wchar_t>(0xDE80));
    }
    else
    {
        text.push_back(static_cast<wchar_t>(0x0001F680));
    }

    return text;
}

void ExpectParseFailure(const pond::core::Result<std::wstring>& result)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode().GetCategory(), pond::core::ErrorCategory::Parse);
}

void ExpectParseFailure(const pond::core::Result<std::string>& result)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode().GetCategory(), pond::core::ErrorCategory::Parse);
}

TEST(StringConversionTests, ConvertsEmptyStrings)
{
    pond::core::Result<std::wstring> wideResult = pond::core::Utf8ToWideString("");
    ASSERT_TRUE(wideResult.HasValue());
    EXPECT_TRUE(wideResult.GetValue().empty());

    pond::core::Result<std::string> utf8Result = pond::core::WideStringToUtf8(L"");
    ASSERT_TRUE(utf8Result.HasValue());
    EXPECT_TRUE(utf8Result.GetValue().empty());
}

TEST(StringConversionTests, ConvertsAsciiBothWays)
{
    pond::core::Result<std::wstring> wideResult = pond::core::Utf8ToWideString("Ponder");
    ASSERT_TRUE(wideResult.HasValue());
    EXPECT_EQ(wideResult.GetValue(), L"Ponder");

    pond::core::Result<std::string> utf8Result = pond::core::WideStringToUtf8(L"Ponder");
    ASSERT_TRUE(utf8Result.HasValue());
    EXPECT_EQ(utf8Result.GetValue(), "Ponder");
}

TEST(StringConversionTests, ConvertsRepresentativeUnicodeRoundTrip)
{
    std::string utf8{"alpha "};
    utf8 += MakeBytes({0xCEU, 0xB1U});
    utf8 += " micro ";
    utf8 += MakeBytes({0xC2U, 0xB5U});
    utf8 += " rocket ";
    utf8 += MakeBytes({0xF0U, 0x9FU, 0x9AU, 0x80U});

    pond::core::Result<std::wstring> wideResult = pond::core::Utf8ToWideString(utf8);
    ASSERT_TRUE(wideResult.HasValue());

    pond::core::Result<std::string> roundTripResult =
        pond::core::WideStringToUtf8(wideResult.GetValue());
    ASSERT_TRUE(roundTripResult.HasValue());
    EXPECT_EQ(roundTripResult.GetValue(), utf8);
}

TEST(StringConversionTests, EncodesSupplementaryWideCharacters)
{
    pond::core::Result<std::string> utf8Result = pond::core::WideStringToUtf8(MakeWideRocket());

    ASSERT_TRUE(utf8Result.HasValue());
    EXPECT_EQ(utf8Result.GetValue(), MakeBytes({0xF0U, 0x9FU, 0x9AU, 0x80U}));
}

TEST(StringConversionTests, DecodesSupplementaryUtf8Characters)
{
    pond::core::Result<std::wstring> wideResult =
        pond::core::Utf8ToWideString(MakeBytes({0xF0U, 0x9FU, 0x9AU, 0x80U}));

    ASSERT_TRUE(wideResult.HasValue());
    EXPECT_EQ(wideResult.GetValue(), MakeWideRocket());
}

TEST(StringConversionTests, RejectsInvalidUtf8)
{
    ExpectParseFailure(pond::core::Utf8ToWideString(MakeBytes({0xC0U, 0xAFU})));
    ExpectParseFailure(pond::core::Utf8ToWideString(MakeBytes({0xF0U, 0x9FU})));
    ExpectParseFailure(pond::core::Utf8ToWideString(MakeBytes({0xEDU, 0xA0U, 0x80U})));
}

TEST(StringConversionTests, RejectsInvalidWideStrings)
{
    std::wstring highSurrogateOnly{static_cast<wchar_t>(0xD800)};
    ExpectParseFailure(pond::core::WideStringToUtf8(highSurrogateOnly));

    std::wstring lowSurrogateOnly{static_cast<wchar_t>(0xDC00)};
    ExpectParseFailure(pond::core::WideStringToUtf8(lowSurrogateOnly));

    if constexpr (sizeof(wchar_t) == 4)
    {
        std::wstring tooLarge{static_cast<wchar_t>(0x00110000)};
        ExpectParseFailure(pond::core::WideStringToUtf8(tooLarge));
    }
}
} // namespace