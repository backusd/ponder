#include <ponder/core/Result.hpp>
#include <ponder/core/StackTrace.hpp>

#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
TEST(ErrorCodeTests, DefaultsToGeneralZero)
{
    constexpr pond::core::ErrorCode kCode;

    static_assert(kCode.GetCategory() == pond::core::ErrorCategory::General);
    static_assert(kCode.GetValue() == 0);
}

TEST(ErrorCodeTests, StoresCategoryAndValue)
{
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::InvalidArgument, 42};

    static_assert(kCode.GetCategory() == pond::core::ErrorCategory::InvalidArgument);
    static_assert(kCode.GetValue() == 42);
}

TEST(ErrorCategoryTests, UsesCompactUnderlyingTypeAndUnknownFallback)
{
    static_assert(std::is_same_v<std::underlying_type_t<pond::core::ErrorCategory>, std::uint8_t>);
    static_assert(pond::core::GetErrorCategoryName(pond::core::ErrorCategory::Parse) ==
                  std::string_view{"parse"});
    static_assert(pond::core::GetErrorCategoryName(pond::core::ErrorCategory::Unsupported) ==
                  std::string_view{"unsupported"});

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto unknownCategory = static_cast<pond::core::ErrorCategory>(255);

    EXPECT_EQ(pond::core::GetErrorCategoryName(unknownCategory), std::string_view{"unknown"});
}

TEST(ErrorTests, ConstructsWithDefaultCodeMessageLocationAndStackTraceFallback)
{
    const auto location = std::source_location::current();
    const pond::core::Error error{"failed", location};

    EXPECT_TRUE(error.GetCode() == pond::core::ErrorCode{});
    EXPECT_EQ(error.GetMessage(), std::string_view{"failed"});
    EXPECT_STREQ(error.GetLocation().file_name(), location.file_name());
    EXPECT_EQ(error.GetLocation().line(), location.line());
    EXPECT_EQ(error.GetStackTrace().IsEmpty(), error.GetStackTrace().GetFrames().empty());

    if (!pond::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(error.GetStackTrace().IsEmpty());
    }
}

TEST(ErrorTests, ConstructsWithExplicitCode)
{
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::NotFound, 7};
    const pond::core::Error error{kCode, "missing"};

    EXPECT_TRUE(error.GetCode() == kCode);
    EXPECT_EQ(error.GetMessage(), std::string_view{"missing"});
}

TEST(ErrorTests, ComparesDirectlyWithErrorCode)
{
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::NotFound, 7};
    constexpr pond::core::ErrorCode kOtherCode{pond::core::ErrorCategory::Unsupported, 8};
    const pond::core::Error error{kCode, "missing"};

    static_assert(pond::core::ConvertToErrorCode<pond::core::ErrorCode>);

    EXPECT_TRUE(error == kCode);
    EXPECT_TRUE(kCode == error);
    EXPECT_FALSE(error == kOtherCode);
    EXPECT_TRUE(error != kOtherCode);
}

TEST(ErrorTests, AcceptsExplicitStackTrace)
{
    pond::core::StackTrace stackTrace{{"frame A", "frame B"}};
    const pond::core::Error error{pond::core::ErrorCode{pond::core::ErrorCategory::Internal, 99},
                                  "broken", std::move(stackTrace)};

    ASSERT_EQ(error.GetStackTrace().GetFrames().size(), 2U);
    EXPECT_EQ(error.GetStackTrace().GetFrames()[0], "frame A");
    EXPECT_EQ(error.GetStackTrace().GetFrames()[1], "frame B");
}

TEST(StackTraceTests, FormatsFrames)
{
    const pond::core::StackTrace stackTrace{{"first", "second"}};
    std::ostringstream stream;
    stream << stackTrace;

    EXPECT_EQ(stackTrace.Format(), "0: first\n1: second");
    EXPECT_EQ(std::format("{}", stackTrace), "0: first\n1: second");
    EXPECT_EQ(stream.str(), "0: first\n1: second");
}

TEST(StackTraceTests, EmptyFallbackIsWellFormed)
{
    const pond::core::StackTrace stackTrace = pond::core::CaptureStackTrace();

    EXPECT_EQ(stackTrace.IsEmpty(), stackTrace.GetFrames().empty());

    if (!pond::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(stackTrace.IsEmpty());
    }
}

TEST(StackTraceTests, CaptureOptionsCanDisableCapture)
{
    const pond::core::StackTrace stackTrace =
        pond::core::CaptureStackTrace(pond::core::StackTraceCaptureOptions{0, 0});

    EXPECT_TRUE(stackTrace.IsEmpty());
    EXPECT_TRUE(stackTrace.GetFrames().empty());
}

TEST(StackTraceTests, CaptureOptionsLimitCapturedFramesWhenSupported)
{
    constexpr std::size_t kMaxFrames{4};
    const pond::core::StackTrace stackTrace =
        pond::core::CaptureStackTrace(pond::core::StackTraceCaptureOptions{0, kMaxFrames});

    EXPECT_LE(stackTrace.GetFrames().size(), kMaxFrames);

    if (!pond::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(stackTrace.IsEmpty());
    }
}

TEST(SourceLocationTests, FormatsFileLineAndColumn)
{
    const auto location = std::source_location::current();

    EXPECT_EQ(pond::core::FormatSourceLocation(location),
              std::format("{}:{}:{}", location.file_name(), location.line(), location.column()));
}

TEST(SourceLocationTests, FormatsFunctionWhenRequested)
{
    const auto location = std::source_location::current();
    const std::string formatted = pond::core::FormatSourceLocationWithFunction(location);

    EXPECT_NE(formatted.find(pond::core::FormatSourceLocation(location)), std::string::npos);
    EXPECT_NE(formatted.find(location.function_name()), std::string::npos);
}

TEST(ErrorFormattingTests, FormatsCategoriesAndErrors)
{
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::Parse, 12};
    const auto location = std::source_location::current();
    const pond::core::Error error{kCode, "bad input", pond::core::StackTrace{}, location};
    std::ostringstream categoryStream;
    std::ostringstream codeStream;
    std::ostringstream errorStream;

    categoryStream << pond::core::ErrorCategory::Parse;
    codeStream << kCode;
    errorStream << error;

    EXPECT_EQ(pond::core::GetErrorCategoryName(pond::core::ErrorCategory::Parse),
              std::string_view{"parse"});
    EXPECT_EQ(std::format("{}", pond::core::ErrorCategory::Parse), "parse");
    EXPECT_EQ(categoryStream.str(), "parse");
    EXPECT_EQ(std::format("{}", kCode), "parse:12");
    EXPECT_EQ(codeStream.str(), "parse:12");

    const std::string formatted = std::format("{}", error);
    EXPECT_NE(formatted.find("[parse:12] bad input"), std::string::npos);
    EXPECT_NE(formatted.find(pond::core::FormatSourceLocation(location)), std::string::npos);
    EXPECT_EQ(errorStream.str(), formatted);
}

TEST(ErrorPropagationTests, MakeUnexpectedBuildsExplicitError)
{
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::Unsupported, 3};

    pond::core::Result<int> result = pond::core::MakeUnexpected(kCode, "unsupported");

    ASSERT_FALSE(result.HasValue());
    EXPECT_TRUE(result.GetError().GetCode() == kCode);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"unsupported"});
}
} // namespace
