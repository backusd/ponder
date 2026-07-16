#include <ponder/platform/WindowState.hpp>

#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <sstream>
#include <type_traits>

namespace
{
static_assert(
    std::is_same_v<std::underlying_type_t<pond::platform::WindowPresentation>, std::uint8_t>);
static_assert(
    std::is_same_v<std::underlying_type_t<pond::platform::WindowDecoration>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<pond::platform::WindowState>, std::uint8_t>);

TEST(PlatformWindowStateTests, DefinesStablePresentationAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowPresentation::Windowed), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowPresentation::DesktopFullscreen), 1U);
}

TEST(PlatformWindowStateTests, DefinesStableDecorationAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowDecoration::System), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowDecoration::Borderless), 1U);
}

TEST(PlatformWindowStateTests, DefinesStableStateAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowState::Normal), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowState::Minimized), 1U);
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowState::Maximized), 2U);
}

TEST(PlatformWindowStateTests, FormatsAndStreamsStateEnums)
{
    std::ostringstream stream;
    stream << pond::platform::WindowPresentation::DesktopFullscreen << ' '
           << pond::platform::WindowDecoration::Borderless << ' '
           << pond::platform::WindowState::Maximized;

    EXPECT_EQ(std::format("{}", pond::platform::WindowPresentation::Windowed), "windowed");
    EXPECT_EQ(std::format("{}", pond::platform::WindowPresentation::DesktopFullscreen),
              "desktop_fullscreen");
    EXPECT_EQ(std::format("{}", pond::platform::WindowDecoration::System), "system");
    EXPECT_EQ(std::format("{}", pond::platform::WindowDecoration::Borderless), "borderless");
    EXPECT_EQ(std::format("{}", pond::platform::WindowState::Normal), "normal");
    EXPECT_EQ(std::format("{}", pond::platform::WindowState::Minimized), "minimized");
    EXPECT_EQ(std::format("{}", pond::platform::WindowState::Maximized), "maximized");
    EXPECT_EQ(stream.str(), "desktop_fullscreen borderless maximized");
}
} // namespace
