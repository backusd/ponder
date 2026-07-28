#include <ponder/core/Timing.hpp>

#include <chrono>
#include <format>
#include <gtest/gtest.h>
#include <sstream>
#include <type_traits>

namespace
{
using namespace std::chrono_literals;

static_assert(noexcept(ponder::core::Timestamp::Now()));
static_assert(std::is_same_v<ponder::core::Timestamp::Duration, ponder::core::Duration>);

TEST(TimingTests, RepresentsAndFormatsNanosecondTimingValues)
{
    constexpr ponder::core::Timestamp first{125ns};
    constexpr ponder::core::Timestamp second{2us};

    static_assert(second - first == ponder::core::Duration{1875ns});
    EXPECT_EQ(std::format("{}", second), "2000 ns");

    std::ostringstream output;
    output << second - first;
    EXPECT_EQ(output.str(), "1875 ns");
}

TEST(TimingTests, CapturesMonotonicCurrentTimestamps)
{
    const ponder::core::Timestamp first = ponder::core::Timestamp::Now();
    const ponder::core::Timestamp second = ponder::core::Timestamp::Now();

    EXPECT_GE(second, first);
}
} // namespace
