#pragma once

#include <cstdint>

namespace pond::platform
{
enum class WindowGraphicsCompatibility : std::uint8_t
{
    Default = 0,
    Vulkan = 1,
    Metal = 2
};
} // namespace pond::platform
