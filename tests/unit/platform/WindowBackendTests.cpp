#include <ponder/platform/WindowGraphics.hpp>

#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <gtest/gtest.h>

#include "PlatformRuntimeBackend.hpp"

namespace
{
TEST(WindowBackendFlagTests, StagesEveryWindowHiddenAndKeepsPropertiesOrthogonal)
{
    const pond::platform::detail::BackendWindowCreateDesc defaultDesc{
        "ponder", 1280, 800, true, true, pond::platform::WindowGraphicsCompatibility::Default};

    const std::uint64_t defaultFlags = pond::platform::detail::BuildSdlWindowFlags(defaultDesc);
    EXPECT_NE(defaultFlags & SDL_WINDOW_HIDDEN, 0U);
    EXPECT_NE(defaultFlags & SDL_WINDOW_RESIZABLE, 0U);
    EXPECT_NE(defaultFlags & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0U);
    EXPECT_EQ(defaultFlags & SDL_WINDOW_VULKAN, 0U);
    EXPECT_EQ(defaultFlags & SDL_WINDOW_METAL, 0U);

    pond::platform::detail::BackendWindowCreateDesc minimalDesc = defaultDesc;
    minimalDesc.resizable = false;
    minimalDesc.highPixelDensity = false;
    const std::uint64_t minimalFlags = pond::platform::detail::BuildSdlWindowFlags(minimalDesc);
    EXPECT_NE(minimalFlags & SDL_WINDOW_HIDDEN, 0U);
    EXPECT_EQ(minimalFlags & SDL_WINDOW_RESIZABLE, 0U);
    EXPECT_EQ(minimalFlags & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0U);
}

TEST(WindowBackendFlagTests, MapsVulkanCompatibilityForTheCurrentHost)
{
    const pond::platform::detail::BackendWindowCreateDesc desc{
        "ponder", 1280, 800, true, true, pond::platform::WindowGraphicsCompatibility::Vulkan};

    const std::uint64_t flags = pond::platform::detail::BuildSdlWindowFlags(desc);
#if defined(SDL_PLATFORM_MACOS)
    EXPECT_NE(flags & SDL_WINDOW_METAL, 0U);
    EXPECT_EQ(flags & SDL_WINDOW_VULKAN, 0U);
#else
    EXPECT_NE(flags & SDL_WINDOW_VULKAN, 0U);
    EXPECT_EQ(flags & SDL_WINDOW_METAL, 0U);
#endif
}

TEST(WindowBackendFlagTests, IdentifiesBackendReservedPositionEncodings)
{
    EXPECT_TRUE(pond::platform::detail::IsReservedSdlWindowPosition(
        static_cast<std::int32_t>(SDL_WINDOWPOS_UNDEFINED)));
    EXPECT_TRUE(pond::platform::detail::IsReservedSdlWindowPosition(
        static_cast<std::int32_t>(SDL_WINDOWPOS_CENTERED)));
    EXPECT_FALSE(pond::platform::detail::IsReservedSdlWindowPosition(-250));
    EXPECT_FALSE(pond::platform::detail::IsReservedSdlWindowPosition(250));
}

TEST(WindowBackendFlagTests, ClassifiesApprovedNativeWindowDrivers)
{
    using pond::platform::detail::BackendNativeWindowDriver;

    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver("windows"),
              BackendNativeWindowDriver::Win32);
    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver("x11"), BackendNativeWindowDriver::X11);
    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver("wayland"),
              BackendNativeWindowDriver::Wayland);
    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver("cocoa"),
              BackendNativeWindowDriver::Cocoa);
    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver("dummy"),
              BackendNativeWindowDriver::Unsupported);
    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver(""),
              BackendNativeWindowDriver::Unsupported);
}
} // namespace
