#include <ponder/platform/WindowGraphics.hpp>

static_assert(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Default) ==
              0U);
static_assert(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Vulkan) == 1U);
static_assert(static_cast<std::uint8_t>(pond::platform::WindowGraphicsCompatibility::Metal) == 2U);
