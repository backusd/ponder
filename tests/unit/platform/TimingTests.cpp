#include <ponder/platform/Timing.hpp>

#include <chrono>
#include <format>
#include <gtest/gtest.h>
#include <sstream>
#include <type_traits>

namespace
{
using namespace std::chrono_literals;

constexpr bool TimestampSemanticsAreConstexpr()
{
    constexpr pond::platform::Timestamp kEpoch;
    constexpr pond::platform::Timestamp kEarlier{12ns};
    constexpr pond::platform::Timestamp kLater{47ns};

    return kEpoch.GetTimeSinceEpoch() == 0ns && kEarlier.GetTimeSinceEpoch() == 12ns &&
           kEarlier < kLater && kLater - kEarlier == 35ns;
}

static_assert(TimestampSemanticsAreConstexpr());
static_assert(
    std::is_nothrow_constructible_v<pond::platform::Timestamp, std::chrono::nanoseconds>);
static_assert(std::is_trivially_copyable_v<pond::platform::Timestamp>);

TEST(PlatformTimingTests, DefaultsToTheMonotonicTickEpoch)
{
    const pond::platform::Timestamp timestamp;

    EXPECT_EQ(timestamp.GetTimeSinceEpoch(), 0ns);
}

TEST(PlatformTimingTests, PreservesNanosecondUnits)
{
    const pond::platform::Timestamp earlier{1'000'000'001ns};
    const pond::platform::Timestamp later{1'000'000'019ns};

    EXPECT_EQ(earlier.GetTimeSinceEpoch(), 1'000'000'001ns);
    EXPECT_EQ(later - earlier, 18ns);
    EXPECT_GT(later, earlier);
}

TEST(PlatformTimingTests, FormatsAndStreamsTimestampsAndDurations)
{
    const pond::platform::Duration duration{42ns};
    const pond::platform::Timestamp timestamp{123ns};
    std::ostringstream durationStream;
    std::ostringstream timestampStream;

    durationStream << duration;
    timestampStream << timestamp;

    EXPECT_EQ(std::format("{}", duration), "42 ns");
    EXPECT_EQ(durationStream.str(), "42 ns");
    EXPECT_EQ(std::format("{}", timestamp), "123 ns");
    EXPECT_EQ(timestampStream.str(), "123 ns");
}
} // namespace
