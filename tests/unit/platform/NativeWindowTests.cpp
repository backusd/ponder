#include <ponder/platform/NativeWindow.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>
#include <variant>

namespace
{
static_assert(std::is_standard_layout_v<pond::platform::NativeWin32Window>);
static_assert(std::is_standard_layout_v<pond::platform::NativeX11Window>);
static_assert(std::is_standard_layout_v<pond::platform::NativeWaylandWindow>);
static_assert(std::is_standard_layout_v<pond::platform::NativeCocoaWindow>);
static_assert(std::is_same_v<decltype(pond::platform::NativeWin32Window::instance), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeWin32Window::window), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeX11Window::display), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeX11Window::window), std::uintptr_t>);
static_assert(std::is_same_v<decltype(pond::platform::NativeWaylandWindow::display), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeWaylandWindow::surface), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeCocoaWindow::metalLayer), void*>);

TEST(NativeWindowHandleTests, IsAClosedTaggedVariant)
{
    EXPECT_EQ(std::variant_size_v<pond::platform::NativeWindowHandle>, 4U);

    const pond::platform::NativeWindowHandle x11 =
        pond::platform::NativeX11Window{reinterpret_cast<void*>(0x3000), std::uintptr_t{0x4000}};
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeX11Window>(x11));
    EXPECT_EQ(std::get<pond::platform::NativeX11Window>(x11).window, std::uintptr_t{0x4000});

    const pond::platform::NativeWindowHandle cocoa =
        pond::platform::NativeCocoaWindow{reinterpret_cast<void*>(0x5000)};
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeCocoaWindow>(cocoa));
    EXPECT_EQ(std::get<pond::platform::NativeCocoaWindow>(cocoa).metalLayer,
              reinterpret_cast<void*>(0x5000));
}
} // namespace
