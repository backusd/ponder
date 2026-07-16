#include <ponder/platform/Display.hpp>

#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>

namespace
{
static_assert(std::is_copy_constructible_v<pond::platform::DisplayInfo>);
static_assert(std::is_copy_assignable_v<pond::platform::DisplayInfo>);
static_assert(std::is_move_constructible_v<pond::platform::DisplayInfo>);
static_assert(std::is_move_assignable_v<pond::platform::DisplayInfo>);
static_assert(sizeof(pond::platform::DisplayOrientation) == sizeof(std::uint8_t));

TEST(DisplayInfoTests, ProvidesExplicitEmptyDefaults)
{
    const pond::platform::DisplayInfo display;

    EXPECT_EQ(display.id, pond::platform::DisplayId::Invalid());
    EXPECT_TRUE(display.name.empty());
    EXPECT_EQ(display.bounds, pond::platform::ScreenRectangle{});
    EXPECT_EQ(display.usableBounds, pond::platform::ScreenRectangle{});
    EXPECT_EQ(display.refreshRateHertz, std::nullopt);
    EXPECT_EQ(display.orientation, pond::platform::DisplayOrientation::Unknown);
    EXPECT_FLOAT_EQ(display.contentScale, 0.0F);
}

TEST(DisplayInfoTests, OwnsAndComparesConfiguredSnapshots)
{
    std::string sourceName{"Primary display"};
    const pond::platform::DisplayInfo expected{
        .id = pond::platform::DisplayId{17},
        .name = sourceName,
        .bounds = {{-1920, 40}, {1920, 1080}},
        .usableBounds = {{-1920, 80}, {1920, 1040}},
        .refreshRateHertz = 59.94F,
        .orientation = pond::platform::DisplayOrientation::LandscapeFlipped,
        .contentScale = 1.5F};
    const pond::platform::DisplayInfo copy = expected;

    sourceName.assign("Changed backend storage");

    EXPECT_EQ(copy, expected);
    EXPECT_EQ(copy.name, "Primary display");
    EXPECT_NE(copy, pond::platform::DisplayInfo{});
}

TEST(DisplayOrientationTests, ExposesEveryPortableAlternative)
{
    EXPECT_NE(pond::platform::DisplayOrientation::Landscape,
              pond::platform::DisplayOrientation::LandscapeFlipped);
    EXPECT_NE(pond::platform::DisplayOrientation::Portrait,
              pond::platform::DisplayOrientation::PortraitFlipped);
    EXPECT_NE(pond::platform::DisplayOrientation::Unknown,
              pond::platform::DisplayOrientation::Landscape);
}

TEST(DisplayOrientationTests, FormatsAndStreamsPortableAlternatives)
{
    std::ostringstream stream;
    stream << pond::platform::DisplayOrientation::LandscapeFlipped;

    EXPECT_EQ(std::format("{}", pond::platform::DisplayOrientation::Unknown), "unknown");
    EXPECT_EQ(std::format("{}", pond::platform::DisplayOrientation::LandscapeFlipped),
              "landscape_flipped");
    EXPECT_EQ(stream.str(), "landscape_flipped");
}
} // namespace
