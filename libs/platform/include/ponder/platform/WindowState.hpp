#pragma once

#include <cstdint>

namespace pond::platform
{
enum class WindowPresentation : std::uint8_t
{
    Windowed = 0,
    DesktopFullscreen = 1
};

enum class WindowDecoration : std::uint8_t
{
    System = 0,
    Borderless = 1
};

enum class WindowState : std::uint8_t
{
    Normal = 0,
    Minimized = 1,
    Maximized = 2
};
} // namespace pond::platform
