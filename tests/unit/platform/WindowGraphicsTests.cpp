#include <ponder/platform/WindowGraphics.hpp>

#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <sstream>
#include <type_traits>

namespace
{
static_assert(std::is_same_v<std::underlying_type_t<pond::platform::WindowGraphicsCompatibility>,
                             std::uint8_t>);
static_assert(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Default) ==
              0U);
static_assert(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Vulkan) == 1U);
static_assert(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Metal) == 2U);

TEST(PlatformWindowGraphicsTests, DefinesTheFrozenRendererCompatibilityAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Default), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Vulkan), 1U);
    EXPECT_EQ(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Metal), 2U);
}

TEST(PlatformWindowGraphicsTests, FormatsAndStreamsCompatibilityValues)
{
    std::ostringstream stream;
    stream << pond::platform::WindowGraphicsCompatibility::Vulkan;

    EXPECT_EQ(std::format("{}", pond::platform::WindowGraphicsCompatibility::Default), "default");
    EXPECT_EQ(std::format("{}", pond::platform::WindowGraphicsCompatibility::Vulkan), "vulkan");
    EXPECT_EQ(std::format("{}", pond::platform::WindowGraphicsCompatibility::Metal), "metal");
    EXPECT_EQ(stream.str(), "vulkan");
}
} // namespace
