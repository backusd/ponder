#include <ponder/platform/Display.hpp>
#include <ponder/platform/Runtime.hpp>
#include <ponder/platform/Window.hpp>

#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
static_assert(std::is_copy_constructible_v<ponder::platform::DisplayInfo>);
static_assert(std::is_copy_assignable_v<ponder::platform::DisplayInfo>);
static_assert(std::is_move_constructible_v<ponder::platform::DisplayInfo>);
static_assert(std::is_move_assignable_v<ponder::platform::DisplayInfo>);
static_assert(sizeof(ponder::platform::DisplayOrientation) == sizeof(std::uint8_t));
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Runtime&>().DisplayEnumerate()), std::vector<ponder::platform::DisplayInfo>>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Runtime&>().DisplayGetInfo(ponder::platform::DisplayId{1})),
                             ponder::core::Result<ponder::platform::DisplayInfo>>);
static_assert(
    std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetDisplayId()), ponder::core::Result<ponder::platform::DisplayId>>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetPixelDensity()), float>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetDisplayScale()), float>);

TEST(DisplayInfoTests, ProvidesExplicitEmptyDefaults)
{
    const ponder::platform::DisplayInfo display;

    EXPECT_EQ(display.id, ponder::platform::DisplayId::Invalid());
    EXPECT_TRUE(display.name.empty());
    EXPECT_EQ(display.bounds, ponder::platform::ScreenRectangle{});
    EXPECT_EQ(display.usableBounds, ponder::platform::ScreenRectangle{});
    EXPECT_EQ(display.refreshRateHertz, std::nullopt);
    EXPECT_EQ(display.orientation, ponder::platform::DisplayOrientation::Unknown);
    EXPECT_FLOAT_EQ(display.contentScale, 0.0F);
}

TEST(DisplayInfoTests, OwnsAndComparesConfiguredSnapshots)
{
    std::string sourceName{"Primary display"};
    const ponder::platform::DisplayInfo expected{.id = ponder::platform::DisplayId{17},
                                                 .name = sourceName,
                                                 .bounds = {{-1920, 40}, {1920, 1080}},
                                                 .usableBounds = {{-1920, 80}, {1920, 1040}},
                                                 .refreshRateHertz = 59.94F,
                                                 .orientation = ponder::platform::DisplayOrientation::LandscapeFlipped,
                                                 .contentScale = 1.5F};
    const ponder::platform::DisplayInfo copy = expected;

    sourceName.assign("Changed backend storage");

    EXPECT_EQ(copy, expected);
    EXPECT_EQ(copy.name, "Primary display");
    EXPECT_NE(copy, ponder::platform::DisplayInfo{});
}

TEST(DisplayOrientationTests, ExposesEveryPortableAlternative)
{
    EXPECT_NE(ponder::platform::DisplayOrientation::Landscape, ponder::platform::DisplayOrientation::LandscapeFlipped);
    EXPECT_NE(ponder::platform::DisplayOrientation::Portrait, ponder::platform::DisplayOrientation::PortraitFlipped);
    EXPECT_NE(ponder::platform::DisplayOrientation::Unknown, ponder::platform::DisplayOrientation::Landscape);
}

TEST(DisplayOrientationTests, FormatsAndStreamsPortableAlternatives)
{
    std::ostringstream stream;
    stream << ponder::platform::DisplayOrientation::LandscapeFlipped;

    EXPECT_EQ(std::format("{}", ponder::platform::DisplayOrientation::Unknown), "unknown");
    EXPECT_EQ(std::format("{}", ponder::platform::DisplayOrientation::LandscapeFlipped), "landscape_flipped");
    EXPECT_EQ(stream.str(), "landscape_flipped");
}
} // namespace
