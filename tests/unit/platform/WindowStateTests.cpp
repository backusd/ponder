#include <ponder/platform/WindowState.hpp>

#include <cstdint>
#include <gtest/gtest.h>
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
} // namespace
