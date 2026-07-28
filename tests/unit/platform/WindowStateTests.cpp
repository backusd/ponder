#include <ponder/platform/WindowState.hpp>

#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <sstream>
#include <type_traits>

namespace
{
static_assert(std::is_same_v<std::underlying_type_t<ponder::platform::WindowPresentation>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<ponder::platform::WindowDecoration>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<ponder::platform::WindowState>, std::uint8_t>);
static_assert(ponder::platform::GetWindowPresentationName(ponder::platform::WindowPresentation::DesktopFullscreen) == "desktop_fullscreen");
static_assert(ponder::platform::GetWindowDecorationName(ponder::platform::WindowDecoration::Borderless) == "borderless");
static_assert(ponder::platform::GetWindowStateName(ponder::platform::WindowState::Maximized) == "maximized");
static_assert(noexcept(ponder::platform::GetWindowPresentationName(ponder::platform::WindowPresentation::Windowed)));
static_assert(noexcept(ponder::platform::GetWindowDecorationName(ponder::platform::WindowDecoration::System)));
static_assert(noexcept(ponder::platform::GetWindowStateName(ponder::platform::WindowState::Normal)));

TEST(PlatformWindowStateTests, DefinesStablePresentationAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(ponder::platform::WindowPresentation::Windowed), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(ponder::platform::WindowPresentation::DesktopFullscreen), 1U);
}

TEST(PlatformWindowStateTests, DefinesStableDecorationAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(ponder::platform::WindowDecoration::System), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(ponder::platform::WindowDecoration::Borderless), 1U);
}

TEST(PlatformWindowStateTests, DefinesStableStateAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(ponder::platform::WindowState::Normal), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(ponder::platform::WindowState::Minimized), 1U);
    EXPECT_EQ(static_cast<std::uint8_t>(ponder::platform::WindowState::Maximized), 2U);
}

TEST(PlatformWindowStateTests, FormatsAndStreamsStateEnums)
{
    std::ostringstream stream;
    stream << ponder::platform::WindowPresentation::DesktopFullscreen << ' ' << ponder::platform::WindowDecoration::Borderless << ' '
           << ponder::platform::WindowState::Maximized;

    EXPECT_EQ(std::format("{}", ponder::platform::WindowPresentation::Windowed), "windowed");
    EXPECT_EQ(std::format("{}", ponder::platform::WindowPresentation::DesktopFullscreen), "desktop_fullscreen");
    EXPECT_EQ(std::format("{}", ponder::platform::WindowDecoration::System), "system");
    EXPECT_EQ(std::format("{}", ponder::platform::WindowDecoration::Borderless), "borderless");
    EXPECT_EQ(std::format("{}", ponder::platform::WindowState::Normal), "normal");
    EXPECT_EQ(std::format("{}", ponder::platform::WindowState::Minimized), "minimized");
    EXPECT_EQ(std::format("{}", ponder::platform::WindowState::Maximized), "maximized");
    EXPECT_EQ(stream.str(), "desktop_fullscreen borderless maximized");
}

TEST(PlatformWindowStateTests, FormatsForgedStateEnumsAsUnrecognized)
{
    constexpr auto forgedPresentation = static_cast<ponder::platform::WindowPresentation>(std::uint8_t{0xFF});
    constexpr auto forgedDecoration = static_cast<ponder::platform::WindowDecoration>(std::uint8_t{0xFF});
    constexpr auto forgedState = static_cast<ponder::platform::WindowState>(std::uint8_t{0xFF});

    EXPECT_EQ(ponder::platform::GetWindowPresentationName(forgedPresentation), "unrecognized");
    EXPECT_EQ(ponder::platform::GetWindowDecorationName(forgedDecoration), "unrecognized");
    EXPECT_EQ(ponder::platform::GetWindowStateName(forgedState), "unrecognized");
    EXPECT_EQ(std::format("{} {} {}", forgedPresentation, forgedDecoration, forgedState), "unrecognized unrecognized unrecognized");
}
} // namespace
