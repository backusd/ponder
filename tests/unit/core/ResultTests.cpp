#include <ponder/core/Result.hpp>

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{
TEST(ResultTests, StoresValue)
{
    pond::core::Result<int> result = 42;

    ASSERT_TRUE(result);
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue(), 42);
    EXPECT_EQ(*result, 42);
}

TEST(ResultTests, ConstructsValueInPlace)
{
    const auto result = pond::core::Result<std::string>::FromValue(3, 'x');

    ASSERT_TRUE(result);
    EXPECT_EQ(result.GetValue(), "xxx");
    EXPECT_EQ(result->size(), 3U);
}

TEST(ResultTests, StoresError)
{
    constexpr pond::core::ErrorCode code{pond::core::ErrorCategory::InvalidArgument, 12};

    pond::core::Result<int> result =
        pond::core::Result<int>::FromError(pond::core::Error{code, "bad value"});

    ASSERT_FALSE(result);
    EXPECT_TRUE(result.GetError().GetCode() == code);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"bad value"});
}

TEST(ResultTests, AcceptsUnexpectedHelper)
{
    pond::core::Result<std::string> result = pond::core::MakeUnexpected(
        pond::core::ErrorCode{pond::core::ErrorCategory::NotFound, 7}, "missing");

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"missing"});
}

TEST(ResultTests, SupportsMoveOnlyValues)
{
    auto value = std::make_unique<int>(5);

    pond::core::Result<std::unique_ptr<int>> result = std::move(value);

    ASSERT_TRUE(result);
    ASSERT_NE(result.GetValue(), nullptr);
    EXPECT_EQ(*result.GetValue(), 5);
}

TEST(VoidResultTests, DefaultsToSuccess)
{
    pond::core::VoidResult result;

    EXPECT_TRUE(result);
    EXPECT_TRUE(result.HasValue());
}

TEST(VoidResultTests, StoresError)
{
    pond::core::VoidResult result = pond::core::MakeUnexpected("void failed");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"void failed"});
}
} // namespace
