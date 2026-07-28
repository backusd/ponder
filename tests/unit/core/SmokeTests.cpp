#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/Result.hpp>

#include <gtest/gtest.h>
#include <string_view>

TEST(CoreSmokeTests, ExercisesCoreConventions)
{
    ponder::core::Result<int> value = 42;
    ASSERT_TRUE(value.HasValue());
    EXPECT_EQ(value.GetValue(), 42);

    ponder::core::Result<int> failure = ponder::core::MakeUnexpected("failure");
    ASSERT_FALSE(failure.HasValue());
    EXPECT_EQ(failure.GetError().GetMessage(), std::string_view{"failure"});

    const ponder::core::Exception exception{"boom"};
    EXPECT_EQ(exception.GetMessage(), std::string_view{"boom"});

    PONDER_ASSERT(true);
    LOG_INFO("core smoke test completed");
}
