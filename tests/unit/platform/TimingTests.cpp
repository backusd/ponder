#include <ponder/platform/Timing.hpp>

#include <chrono>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
using namespace std::chrono_literals;

constexpr bool TimestampSemanticsAreConstexpr()
{
    constexpr pond::platform::PlatformTimestamp kEpoch;
    constexpr pond::platform::PlatformTimestamp kEarlier{12ns};
    constexpr pond::platform::PlatformTimestamp kLater{47ns};

    return kEpoch.GetTimeSinceEpoch() == 0ns && kEarlier.GetTimeSinceEpoch() == 12ns &&
           kEarlier < kLater && kLater - kEarlier == 35ns;
}

static_assert(TimestampSemanticsAreConstexpr());
static_assert(
    std::is_nothrow_constructible_v<pond::platform::PlatformTimestamp, std::chrono::nanoseconds>);
static_assert(std::is_trivially_copyable_v<pond::platform::PlatformTimestamp>);

TEST(PlatformTimingTests, DefaultsToTheMonotonicTickEpoch)
{
    const pond::platform::PlatformTimestamp timestamp;

    EXPECT_EQ(timestamp.GetTimeSinceEpoch(), 0ns);
}

TEST(PlatformTimingTests, PreservesNanosecondUnits)
{
    const pond::platform::PlatformTimestamp earlier{1'000'000'001ns};
    const pond::platform::PlatformTimestamp later{1'000'000'019ns};

    EXPECT_EQ(earlier.GetTimeSinceEpoch(), 1'000'000'001ns);
    EXPECT_EQ(later - earlier, 18ns);
    EXPECT_GT(later, earlier);
}
} // namespace
