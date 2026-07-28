#include <ponder/core/Exception.hpp>
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
void ThrowingUnreachableHandler(const ponder::core::AssertionFailure&)
{
    throw PONDER_EXCEPTION("unreachable captured");
}

TEST(ErrorCodeTests, DefaultsToGeneralZero)
{
    constexpr ponder::core::ErrorCode kCode;

    static_assert(kCode.GetCategory() == ponder::core::ErrorCategory::General);
    static_assert(kCode.GetValue() == 0);
}

TEST(ErrorCodeTests, StoresCategoryAndValue)
{
    constexpr ponder::core::ErrorCode kCode{ponder::core::ErrorCategory::InvalidArgument, 42};

    static_assert(kCode.GetCategory() == ponder::core::ErrorCategory::InvalidArgument);
    static_assert(kCode.GetValue() == 42);
}

TEST(ErrorCategoryTests, UsesCompactUnderlyingTypeAndRejectsUnknownValues)
{
    static_assert(std::is_same_v<std::underlying_type_t<ponder::core::ErrorCategory>, std::uint8_t>);
    static_assert(ponder::core::GetErrorCategoryName(ponder::core::ErrorCategory::Parse) == std::string_view{"parse"});
    static_assert(ponder::core::GetErrorCategoryName(ponder::core::ErrorCategory::Unsupported) == std::string_view{"unsupported"});
    static_assert(!noexcept(ponder::core::GetErrorCategoryName(ponder::core::ErrorCategory::General)));

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto unknownCategory = static_cast<ponder::core::ErrorCategory>(255);
    const ponder::core::ScopedAssertionFailureHandler handler{ThrowingUnreachableHandler};

    EXPECT_THROW((void)ponder::core::GetErrorCategoryName(unknownCategory), ponder::core::Exception);
}

TEST(ErrorTests, ConstructsWithDefaultCodeMessageLocationAndStackTraceFallback)
{
    const auto location = std::source_location::current();
    const ponder::core::Error error{"failed", location};

    EXPECT_TRUE(error.GetCode() == ponder::core::ErrorCode{});
    EXPECT_EQ(error.GetMessage(), std::string_view{"failed"});
    EXPECT_STREQ(error.GetLocation().file_name(), location.file_name());
    EXPECT_EQ(error.GetLocation().line(), location.line());
    EXPECT_EQ(error.GetStackTrace().IsEmpty(), error.GetStackTrace().GetFrames().empty());

    if (!ponder::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(error.GetStackTrace().IsEmpty());
    }
}

TEST(ErrorTests, ConstructsWithExplicitCode)
{
    constexpr ponder::core::ErrorCode kCode{ponder::core::ErrorCategory::NotFound, 7};
    const ponder::core::Error error{kCode, "missing"};

    EXPECT_TRUE(error.GetCode() == kCode);
    EXPECT_EQ(error.GetMessage(), std::string_view{"missing"});
}

TEST(ErrorTests, ComparesDirectlyWithErrorCode)
{
    constexpr ponder::core::ErrorCode kCode{ponder::core::ErrorCategory::NotFound, 7};
    constexpr ponder::core::ErrorCode kOtherCode{ponder::core::ErrorCategory::Unsupported, 8};
    const ponder::core::Error error{kCode, "missing"};

    static_assert(ponder::core::ConvertToErrorCode<ponder::core::ErrorCode>);

    EXPECT_TRUE(error == kCode);
    EXPECT_TRUE(kCode == error);
    EXPECT_FALSE(error == kOtherCode);
    EXPECT_TRUE(error != kOtherCode);
}

TEST(ErrorTests, AcceptsExplicitStackTrace)
{
    ponder::core::StackTrace stackTrace{{"frame A", "frame B"}};
    const ponder::core::Error error{ponder::core::ErrorCode{ponder::core::ErrorCategory::Internal, 99}, "broken", std::move(stackTrace)};

    ASSERT_EQ(error.GetStackTrace().GetFrames().size(), 2U);
    EXPECT_EQ(error.GetStackTrace().GetFrames()[0], "frame A");
    EXPECT_EQ(error.GetStackTrace().GetFrames()[1], "frame B");
}

TEST(StackTraceTests, FormatsFrames)
{
    const ponder::core::StackTrace stackTrace{{"first", "second"}};
    std::ostringstream stream;
    stream << stackTrace;

    EXPECT_EQ(stackTrace.Format(), "0: first\n1: second");
    EXPECT_EQ(std::format("{}", stackTrace), "0: first\n1: second");
    EXPECT_EQ(stream.str(), "0: first\n1: second");
}

TEST(StackTraceTests, EmptyFallbackIsWellFormed)
{
    const ponder::core::StackTrace stackTrace = ponder::core::CaptureStackTrace();

    EXPECT_EQ(stackTrace.IsEmpty(), stackTrace.GetFrames().empty());

    if (!ponder::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(stackTrace.IsEmpty());
    }
}

TEST(StackTraceTests, CaptureOptionsCanDisableCapture)
{
    const ponder::core::StackTrace stackTrace = ponder::core::CaptureStackTrace(ponder::core::StackTraceCaptureOptions{0, 0});

    EXPECT_TRUE(stackTrace.IsEmpty());
    EXPECT_TRUE(stackTrace.GetFrames().empty());
}

TEST(StackTraceTests, CaptureOptionsLimitCapturedFramesWhenSupported)
{
    constexpr std::size_t kMaxFrames{4};
    const ponder::core::StackTrace stackTrace = ponder::core::CaptureStackTrace(ponder::core::StackTraceCaptureOptions{0, kMaxFrames});

    EXPECT_LE(stackTrace.GetFrames().size(), kMaxFrames);

    if (!ponder::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(stackTrace.IsEmpty());
    }
}

TEST(SourceLocationTests, FormatsFileLineAndColumn)
{
    const auto location = std::source_location::current();

    EXPECT_EQ(ponder::core::FormatSourceLocation(location), std::format("{}:{}:{}", location.file_name(), location.line(), location.column()));
}

TEST(SourceLocationTests, FormatsFunctionWhenRequested)
{
    const auto location = std::source_location::current();
    const std::string formatted = ponder::core::FormatSourceLocationWithFunction(location);

    EXPECT_NE(formatted.find(ponder::core::FormatSourceLocation(location)), std::string::npos);
    EXPECT_NE(formatted.find(location.function_name()), std::string::npos);
}

TEST(ErrorFormattingTests, FormatsCategoriesAndErrors)
{
    constexpr ponder::core::ErrorCode kCode{ponder::core::ErrorCategory::Parse, 12};
    const auto location = std::source_location::current();
    const ponder::core::Error error{kCode, "bad input", ponder::core::StackTrace{}, location};
    std::ostringstream categoryStream;
    std::ostringstream codeStream;
    std::ostringstream errorStream;

    categoryStream << ponder::core::ErrorCategory::Parse;
    codeStream << kCode;
    errorStream << error;

    EXPECT_EQ(ponder::core::GetErrorCategoryName(ponder::core::ErrorCategory::Parse), std::string_view{"parse"});
    EXPECT_EQ(std::format("{}", ponder::core::ErrorCategory::Parse), "parse");
    EXPECT_EQ(categoryStream.str(), "parse");
    EXPECT_EQ(std::format("{}", kCode), "parse:12");
    EXPECT_EQ(codeStream.str(), "parse:12");

    const std::string formatted = std::format("{}", error);
    EXPECT_NE(formatted.find("[parse:12] bad input"), std::string::npos);
    EXPECT_NE(formatted.find(ponder::core::FormatSourceLocation(location)), std::string::npos);
    EXPECT_EQ(errorStream.str(), formatted);
}

TEST(ErrorPropagationTests, MakeUnexpectedBuildsExplicitError)
{
    constexpr ponder::core::ErrorCode kCode{ponder::core::ErrorCategory::Unsupported, 3};

    ponder::core::Result<int> result = ponder::core::MakeUnexpected(kCode, "unsupported");

    ASSERT_FALSE(result.HasValue());
    EXPECT_TRUE(result.GetError().GetCode() == kCode);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"unsupported"});
}
} // namespace
