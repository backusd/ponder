#include <ponder/core/String.hpp>

#include <gtest/gtest.h>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>

namespace
{
constexpr std::string_view kRocketUtf8{"\xF0\x9F\x9A\x80", 4};

static_assert(pond::core::EqualsCaseInsensitive("Ponder", "PONDER"));
static_assert(!pond::core::EqualsCaseInsensitive("Ponder", "Pond"));

constexpr bool Utf8ToWideStringAsciiIsConstexpr()
{
    const auto result = pond::core::Utf8ToWideString("Ponder");
    return result.HasValue() && result.GetValue() == L"Ponder";
}

constexpr bool Utf8ToWideStringRocketIsConstexpr()
{
    const auto result = pond::core::Utf8ToWideString(kRocketUtf8);
    if (!result.HasValue())
    {
        return false;
    }

    const std::wstring& value = result.GetValue();
    if constexpr (sizeof(wchar_t) == 2)
    {
        return value.size() == 2 && value[0] == static_cast<wchar_t>(0xD83D) &&
               value[1] == static_cast<wchar_t>(0xDE80);
    }
    else
    {
        return value.size() == 1 && value[0] == static_cast<wchar_t>(0x0001F680);
    }
}

constexpr bool WideStringToUtf8AsciiIsConstexpr()
{
    const auto result = pond::core::WideStringToUtf8(L"Ponder");
    return result.HasValue() && result.GetValue() == "Ponder";
}

constexpr bool WideStringToUtf8RocketIsConstexpr()
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

    const auto result = pond::core::WideStringToUtf8(text);
    if (!result.HasValue())
    {
        return false;
    }

    const std::string& value = result.GetValue();
    return value.size() == kRocketUtf8.size() && value[0] == kRocketUtf8[0] &&
           value[1] == kRocketUtf8[1] && value[2] == kRocketUtf8[2] && value[3] == kRocketUtf8[3];
}

static_assert(Utf8ToWideStringAsciiIsConstexpr());
static_assert(Utf8ToWideStringRocketIsConstexpr());
static_assert(WideStringToUtf8AsciiIsConstexpr());
static_assert(WideStringToUtf8RocketIsConstexpr());
constexpr bool Utf8ValidationIsConstexpr()
{
    return pond::core::IsValidUtf8("Ponder") && pond::core::IsValidUtf8(kRocketUtf8) &&
           !pond::core::IsValidUtf8("\xF0\x9F") &&
           pond::core::IsValidUtf8WithoutEmbeddedNull("Ponder") &&
           !pond::core::IsValidUtf8WithoutEmbeddedNull(std::string_view{"P\0nder", 6}) &&
           !pond::core::IsValidUtf8WithoutEmbeddedNull("\xF0\x9F");
}

static_assert(Utf8ValidationIsConstexpr());
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

TEST(StringComparisonTests, ComparesAsciiCaseInsensitively)
{
    EXPECT_TRUE(pond::core::EqualsCaseInsensitive("", ""));
    EXPECT_TRUE(pond::core::EqualsCaseInsensitive("SDL_FALSE", "sdl_false"));
    EXPECT_FALSE(pond::core::EqualsCaseInsensitive("SDL_TRUE", "SDL_FALSE"));
    EXPECT_FALSE(pond::core::EqualsCaseInsensitive("Ponder", "Ponder "));
}

TEST(Utf8ValidationTests, ValidatesUtf8WithoutAllocatingAResult)
{
    EXPECT_TRUE(pond::core::IsValidUtf8(""));
    EXPECT_TRUE(pond::core::IsValidUtf8("Ponder"));
    EXPECT_TRUE(pond::core::IsValidUtf8(kRocketUtf8));

    EXPECT_FALSE(pond::core::IsValidUtf8(MakeBytes({0xC0U, 0xAFU})));
    EXPECT_FALSE(pond::core::IsValidUtf8(MakeBytes({0xF0U, 0x9FU})));
    EXPECT_FALSE(pond::core::IsValidUtf8(MakeBytes({0xEDU, 0xA0U, 0x80U})));
}

TEST(Utf8ValidationTests, RejectsEmbeddedNulls)
{
    EXPECT_TRUE(pond::core::IsValidUtf8WithoutEmbeddedNull(""));
    EXPECT_TRUE(pond::core::IsValidUtf8WithoutEmbeddedNull("Ponder"));
    EXPECT_TRUE(pond::core::IsValidUtf8WithoutEmbeddedNull(kRocketUtf8));

    EXPECT_FALSE(pond::core::IsValidUtf8WithoutEmbeddedNull(std::string_view{"\0", 1}));
    EXPECT_FALSE(pond::core::IsValidUtf8WithoutEmbeddedNull(std::string_view{"P\0nder", 6}));
    EXPECT_FALSE(pond::core::IsValidUtf8WithoutEmbeddedNull(MakeBytes({0xF0U, 0x9FU})));
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
TEST(NumberParsingTests, ParsesCompleteIntegralAndFloatingPointValues)
{
    const auto signedValue = pond::core::ParseNumber<int>("-42");
    ASSERT_TRUE(signedValue.has_value());
    EXPECT_EQ(*signedValue, -42);

    const auto unsignedValue = pond::core::ParseNumber<std::uint32_t>("4294967295");
    ASSERT_TRUE(unsignedValue.has_value());
    EXPECT_EQ(*unsignedValue, std::numeric_limits<std::uint32_t>::max());

    const auto floatingValue = pond::core::ParseNumber<float>("1.25");
    ASSERT_TRUE(floatingValue.has_value());
    EXPECT_FLOAT_EQ(*floatingValue, 1.25F);
}

TEST(NumberParsingTests, RejectsEmptyPartialWhitespaceAndOutOfRangeValues)
{
    EXPECT_FALSE(pond::core::ParseNumber<int>("").has_value());
    EXPECT_FALSE(pond::core::ParseNumber<int>("12ms").has_value());
    EXPECT_FALSE(pond::core::ParseNumber<int>(" 12").has_value());
    EXPECT_FALSE(pond::core::ParseNumber<unsigned int>("-1").has_value());
    EXPECT_FALSE(pond::core::ParseNumber<int>("999999999999999999999999").has_value());
}
} // namespace
