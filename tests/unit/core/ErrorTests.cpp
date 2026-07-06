#include <ponder/core/Result.hpp>

#include <gtest/gtest.h>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace
{
TEST(ErrorCodeTests, DefaultsToGeneralZero)
{
    constexpr pond::core::ErrorCode code;

    static_assert(code.GetCategory() == pond::core::ErrorCategory::General);
    static_assert(code.GetValue() == 0);
}

TEST(ErrorCodeTests, StoresCategoryAndValue)
{
    constexpr pond::core::ErrorCode code{pond::core::ErrorCategory::InvalidArgument, 42};

    static_assert(code.GetCategory() == pond::core::ErrorCategory::InvalidArgument);
    static_assert(code.GetValue() == 42);
}

TEST(ErrorTests, ConstructsWithDefaultCodeMessageLocationAndStackTraceFallback)
{
    const auto location = std::source_location::current();
    const pond::core::Error error{"failed", location};

    EXPECT_TRUE(error.GetCode() == pond::core::ErrorCode{});
    EXPECT_EQ(error.GetMessage(), std::string_view{"failed"});
    EXPECT_STREQ(error.GetLocation().file_name(), location.file_name());
    EXPECT_EQ(error.GetLocation().line(), location.line());
    EXPECT_TRUE(error.GetStackTrace().IsEmpty());
    EXPECT_TRUE(error.GetStackTrace().GetFrames().empty());
}

TEST(ErrorTests, ConstructsWithExplicitCode)
{
    constexpr pond::core::ErrorCode code{pond::core::ErrorCategory::NotFound, 7};
    const pond::core::Error error{code, "missing"};

    EXPECT_TRUE(error.GetCode() == code);
    EXPECT_EQ(error.GetMessage(), std::string_view{"missing"});
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

    EXPECT_EQ(stackTrace.Format(), "0: first\n1: second");
}

TEST(StackTraceTests, EmptyFallbackIsWellFormed)
{
    const pond::core::StackTrace stackTrace = pond::core::CaptureStackTrace();

    EXPECT_EQ(stackTrace.IsEmpty(), stackTrace.GetFrames().empty());
}

TEST(ErrorFormattingTests, FormatsCategoriesAndErrors)
{
    constexpr pond::core::ErrorCode code{pond::core::ErrorCategory::Parse, 12};
    const auto location = std::source_location::current();
    const pond::core::Error error{code, "bad input", pond::core::StackTrace{}, location};

    EXPECT_EQ(pond::core::GetErrorCategoryName(pond::core::ErrorCategory::Parse),
              std::string_view{"parse"});
    EXPECT_EQ(pond::core::FormatErrorCode(code), "parse:12");

    const std::string formatted = pond::core::FormatError(error);
    EXPECT_NE(formatted.find("[parse:12] bad input"), std::string::npos);
    EXPECT_NE(formatted.find(location.file_name()), std::string::npos);
}

TEST(ErrorPropagationTests, MakeUnexpectedBuildsExplicitError)
{
    constexpr pond::core::ErrorCode code{pond::core::ErrorCategory::Unsupported, 3};

    pond::core::Result<int> result = pond::core::MakeUnexpected(code, "unsupported");

    ASSERT_FALSE(result.HasValue());
    EXPECT_TRUE(result.GetError().GetCode() == code);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"unsupported"});
}
} // namespace
