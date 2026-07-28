#include <ponder/platform/WindowGraphics.hpp>

static_assert(static_cast<std::uint8_t>(ponder::platform::WindowGraphicsCompatibility::Default) == 0U);
static_assert(static_cast<std::uint8_t>(ponder::platform::WindowGraphicsCompatibility::Vulkan) == 1U);
static_assert(static_cast<std::uint8_t>(ponder::platform::WindowGraphicsCompatibility::Metal) == 2U);
