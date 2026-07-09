#include <ponder/platform/Window.hpp>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

namespace
{
static_assert(!std::is_default_constructible_v<pond::platform::Window>);
static_assert(!std::is_copy_constructible_v<pond::platform::Window>);
static_assert(!std::is_copy_assignable_v<pond::platform::Window>);
static_assert(std::is_nothrow_move_constructible_v<pond::platform::Window>);
static_assert(std::is_nothrow_move_assignable_v<pond::platform::Window>);
static_assert(std::is_nothrow_destructible_v<pond::platform::Window>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::Window&>().SetMouseGrab(true)),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<const pond::platform::Window&>().IsMouseGrabbed()),
              bool>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::Window&>().SetRelativeMouseMode(true)),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<const pond::platform::Window&>()
                           .IsRelativeMouseModeEnabled()),
              bool>);
static_assert(std::is_same_v<
              decltype(std::declval<const pond::platform::Window&>()
                           .GetNativeHandle()),
              pond::core::Result<pond::platform::NativeWindowHandle>>);

TEST(WindowDescTests, ProvidesStableDefaults)
{
    const pond::platform::WindowDesc desc;

    EXPECT_EQ(desc.title, "ponder");
    EXPECT_EQ(desc.logicalSize, (pond::platform::LogicalSize{1280, 800}));
    EXPECT_TRUE(desc.visible);
    EXPECT_TRUE(desc.resizable);
    EXPECT_TRUE(desc.highPixelDensity);
    EXPECT_FALSE(desc.minimumLogicalSize.has_value());
    EXPECT_EQ(desc.graphicsCompatibility,
              pond::platform::WindowGraphicsCompatibility::Default);
}

TEST(WindowDescTests, OwnsConfiguredValues)
{
    const pond::platform::WindowDesc desc{
        .title = std::string{"Molecular View"},
        .logicalSize = {900, 700},
        .visible = false,
        .resizable = false,
        .highPixelDensity = false,
        .minimumLogicalSize = pond::platform::LogicalSize{300, 200},
        .graphicsCompatibility =
            pond::platform::WindowGraphicsCompatibility::Vulkan};

    EXPECT_EQ(desc.title, "Molecular View");
    EXPECT_EQ(desc.logicalSize, (pond::platform::LogicalSize{900, 700}));
    EXPECT_FALSE(desc.visible);
    EXPECT_FALSE(desc.resizable);
    EXPECT_FALSE(desc.highPixelDensity);
    EXPECT_EQ(desc.minimumLogicalSize,
              (pond::platform::LogicalSize{300, 200}));
    EXPECT_EQ(desc.graphicsCompatibility,
              pond::platform::WindowGraphicsCompatibility::Vulkan);
}
} // namespace
