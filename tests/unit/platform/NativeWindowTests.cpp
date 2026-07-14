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
static_assert(std::is_same_v<decltype(pond::platform::NativeWin32Window::instance), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeWin32Window::window), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeX11Window::display), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeX11Window::window), std::uintptr_t>);
static_assert(std::is_same_v<decltype(pond::platform::NativeWaylandWindow::display), void*>);
static_assert(std::is_same_v<decltype(pond::platform::NativeWaylandWindow::surface), void*>);
static_assert(std::variant_size_v<pond::platform::NativeWindowHandle> == 3U);
static_assert(std::is_same_v<std::variant_alternative_t<0, pond::platform::NativeWindowHandle>,
                             pond::platform::NativeWin32Window>);
static_assert(std::is_same_v<std::variant_alternative_t<1, pond::platform::NativeWindowHandle>,
                             pond::platform::NativeX11Window>);
static_assert(std::is_same_v<std::variant_alternative_t<2, pond::platform::NativeWindowHandle>,
                             pond::platform::NativeWaylandWindow>);

TEST(NativeWindowHandleTests, IsAClosedTaggedVariant)
{
    EXPECT_EQ(std::variant_size_v<pond::platform::NativeWindowHandle>, 3U);

    const pond::platform::NativeWindowHandle x11 =
        pond::platform::NativeX11Window{reinterpret_cast<void*>(0x3000), std::uintptr_t{0x4000}};
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeX11Window>(x11));
    EXPECT_EQ(std::get<pond::platform::NativeX11Window>(x11).window, std::uintptr_t{0x4000});
}
} // namespace
