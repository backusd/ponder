#include <ponder/platform/WindowGraphics.hpp>

#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_video.h>
#include <concepts>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <ostream>
#include <sstream>
#include <string>

#include "PlatformRuntimeBackend.hpp"
#include "ProcessBackend.hpp"

namespace
{
template <typename Type>
concept FormattableAndStreamable =
    std::formattable<Type, char> && requires(std::ostream& output, const Type& value) {
        { output << value } -> std::same_as<std::ostream&>;
    };

template <typename... Types>
consteval bool AreFormattableAndStreamable()
{
    return (FormattableAndStreamable<Types> && ...);
}

using namespace pond::platform::detail;

static_assert(AreFormattableAndStreamable<
              ApplicationMetadataProperty, CursorHandle,
              BackendDialogKind, BackendDialogFileFilter, BackendDialogSelection,
              BackendDialogCancellation, BackendDialogFailure, BackendDialogRequestDesc,
              BackendWindowHandle, BackendWindowPosition, BackendWindowLogicalSize,
              BackendWindowPixelSize, BackendWindowCreateDesc, BackendWindowProperties,
              BackendTextInputArea, BackendNativeWindowDriver, BackendDisplayOrientation,
              BackendScreenRectangle, BackendProcessExitKind, BackendProcessExitStatus,
              BackendProcessKillResult>());

TEST(WindowBackendFormattingTests, FormatsAndStreamsBackendWindowValues)
{
    const BackendWindowHandle window{42};
    const BackendWindowPosition position{12, -4};
    const BackendWindowLogicalSize size{1280, 800};

    EXPECT_EQ(std::format("{}", window), "0x2A");
    EXPECT_EQ(std::format("{}", position), "(12, -4)");
    EXPECT_EQ(std::format("{}", size), "1280x800");
    EXPECT_EQ(std::format("{}", BackendNativeWindowDriver::Wayland), "wayland");

    std::ostringstream output;
    output << BackendTextInputArea{1, 2, 300, 40, 12};
    EXPECT_EQ(output.str(), "(1, 2) / 300x40, cursorOffset=12");
}

TEST(WindowBackendHandleTests, DistinguishesInvalidAndValidValues)
{
    using pond::platform::detail::BackendWindowHandle;

    const BackendWindowHandle invalid;
    const BackendWindowHandle valid{42};

    EXPECT_FALSE(invalid.IsValid());
    EXPECT_TRUE(valid.IsValid());
    EXPECT_EQ(valid.GetValue(), 42U);
    EXPECT_NE(valid, invalid);
}
TEST(WindowBackendFlagTests, StagesEveryWindowHiddenAndKeepsPropertiesOrthogonal)
{
    const pond::platform::detail::BackendWindowCreateDesc defaultDesc{
        .title = "ponder",
        .logicalSize = {1280, 800},
        .resizable = true,
        .highPixelDensity = true,
        .graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Default};

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

TEST(WindowBackendFlagTests, SupportsOnlyApprovedGraphicsCompatibilityForTheCurrentHost)
{
    using pond::platform::WindowGraphicsCompatibility;
    using pond::platform::detail::IsWindowGraphicsCompatibilitySupported;

    EXPECT_TRUE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Default));
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    EXPECT_TRUE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Vulkan));
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Metal));
#elif defined(SDL_PLATFORM_MACOS)
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Vulkan));
    EXPECT_TRUE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Metal));
#else
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Vulkan));
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Metal));
#endif
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(
        static_cast<WindowGraphicsCompatibility>(0xFF)));
}

TEST(WindowBackendFlagTests, MapsRendererCompatibilityOnlyOnSupportedHosts)
{
    using pond::platform::WindowGraphicsCompatibility;
    using pond::platform::detail::BackendWindowCreateDesc;
    using pond::platform::detail::BuildSdlWindowFlags;

    const BackendWindowCreateDesc vulkanDesc{
        .title = "ponder",
        .logicalSize = {1280, 800},
        .resizable = true,
        .highPixelDensity = true,
        .graphicsCompatibility = WindowGraphicsCompatibility::Vulkan};
    const BackendWindowCreateDesc metalDesc{
        .title = "ponder",
        .logicalSize = {1280, 800},
        .resizable = true,
        .highPixelDensity = true,
        .graphicsCompatibility = WindowGraphicsCompatibility::Metal};

    const std::uint64_t vulkanFlags = BuildSdlWindowFlags(vulkanDesc);
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    EXPECT_NE(vulkanFlags & SDL_WINDOW_VULKAN, 0U);
#else
    EXPECT_EQ(vulkanFlags & SDL_WINDOW_VULKAN, 0U);
#endif
    EXPECT_EQ(vulkanFlags & SDL_WINDOW_METAL, 0U);

    const std::uint64_t metalFlags = BuildSdlWindowFlags(metalDesc);
#if defined(SDL_PLATFORM_MACOS)
    EXPECT_NE(metalFlags & SDL_WINDOW_METAL, 0U);
#else
    EXPECT_EQ(metalFlags & SDL_WINDOW_METAL, 0U);
#endif
    EXPECT_EQ(metalFlags & SDL_WINDOW_VULKAN, 0U);
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
              BackendNativeWindowDriver::Unsupported);
    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver("dummy"),
              BackendNativeWindowDriver::Unsupported);
    EXPECT_EQ(pond::platform::detail::GetNativeWindowDriver(""),
              BackendNativeWindowDriver::Unsupported);
}
} // namespace
