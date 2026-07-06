#include <ponder/core/Assert.hpp>
#include <ponder/core/Library.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/PonderException.hpp>
#include <ponder/core/Result.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(CoreSmokeTests, ExposesLibraryNameAndConventions)
{
    EXPECT_EQ(pond::core::GetLibraryName(), std::string_view{"core"});

    pond::core::Result<int> value = 42;
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 42);

    pond::core::Result<int> failure = pond::core::MakeUnexpected("failure");
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(failure.error().GetMessage(), std::string_view{"failure"});

    const pond::core::PonderException exception{"boom"};
    EXPECT_STREQ(exception.what(), "boom");

    PONDER_ASSERT(true);
    LOG_INFO("core smoke test completed");
}