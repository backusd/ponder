#include <ponder/platform/WindowGraphics.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace
{
static_assert(std::is_same_v<std::underlying_type_t<
                                pond::platform::WindowGraphicsCompatibility>,
                            std::uint8_t>);
static_assert(static_cast<std::uint8_t>(
                  pond::platform::WindowGraphicsCompatibility::Default) == 0U);
static_assert(static_cast<std::uint8_t>(
                  pond::platform::WindowGraphicsCompatibility::Vulkan) == 1U);

TEST(PlatformWindowGraphicsTests, DefinesOnlyTheFrozenInitialAlternatives)
{
    EXPECT_EQ(static_cast<std::uint8_t>(
                  pond::platform::WindowGraphicsCompatibility::Default),
              0U);
    EXPECT_EQ(static_cast<std::uint8_t>(
                  pond::platform::WindowGraphicsCompatibility::Vulkan),
              1U);
}
} // namespace
