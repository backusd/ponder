#include <ponder/platform/Runtime.hpp>
#include <ponder/platform/Window.hpp>

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
static_assert(!std::is_default_constructible_v<ponder::platform::Window>);
static_assert(!std::is_copy_constructible_v<ponder::platform::Window>);
static_assert(!std::is_copy_assignable_v<ponder::platform::Window>);
static_assert(std::is_nothrow_move_constructible_v<ponder::platform::Window>);
static_assert(std::is_nothrow_move_assignable_v<ponder::platform::Window>);
static_assert(std::is_nothrow_destructible_v<ponder::platform::Window>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Runtime&>().WindowCreate(std::declval<const ponder::platform::WindowDesc&>())),
                             ponder::platform::Window>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetTitle()), std::string>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetTitle(std::declval<std::string_view>())), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetPosition()), ponder::platform::ScreenPosition>);
static_assert(
    std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetPosition(std::declval<ponder::platform::ScreenPosition>())), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetLogicalSize()), ponder::platform::LogicalSize>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetPixelSize()), ponder::platform::PixelSize>);
static_assert(
    std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetLogicalSize(std::declval<ponder::platform::LogicalSize>())), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().Show()), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().Hide()), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().IsVisible()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetPresentation()), ponder::platform::WindowPresentation>);
static_assert(
    std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetPresentation(std::declval<ponder::platform::WindowPresentation>())), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetDecoration()), ponder::platform::WindowDecoration>);
static_assert(
    std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetDecoration(std::declval<ponder::platform::WindowDecoration>())), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetState()), ponder::platform::WindowState>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().Minimize()), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().Maximize()), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().Restore()), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().IsResizable()), bool>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetResizable(true)), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().IsFocused()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().IsAlwaysOnTop()), bool>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetAlwaysOnTop(true)), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().StartTextInput()), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().StopTextInput()), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().ClearTextComposition()), void>);
static_assert(
    std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetTextInputArea(std::declval<ponder::platform::TextInputArea>())), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().ClearTextInputArea()), void>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetMouseGrab(true)), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().IsMouseGrabbed()), bool>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Window&>().SetRelativeMouseMode(true)), void>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().IsRelativeMouseModeEnabled()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetNativeHandle()),
                             ponder::core::Result<ponder::platform::NativeWindowHandle>>);
static_assert(
    std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetDisplayId()), ponder::core::Result<ponder::platform::DisplayId>>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetPixelDensity()), float>);
static_assert(std::is_same_v<decltype(std::declval<const ponder::platform::Window&>().GetDisplayScale()), float>);

TEST(WindowDescTests, ProvidesStableDefaults)
{
    const ponder::platform::WindowDesc desc;

    EXPECT_EQ(desc.title, "ponder");
    EXPECT_EQ(desc.logicalSize, (ponder::platform::LogicalSize{1280, 800}));
    EXPECT_TRUE(desc.visible);
    EXPECT_TRUE(desc.resizable);
    EXPECT_TRUE(desc.highPixelDensity);
    EXPECT_FALSE(desc.minimumLogicalSize.has_value());
    EXPECT_EQ(desc.graphicsCompatibility, ponder::platform::WindowGraphicsCompatibility::Default);
}

TEST(WindowDescTests, OwnsConfiguredValues)
{
    const ponder::platform::WindowDesc desc{.title = std::string{"Molecular View"},
                                            .logicalSize = {900, 700},
                                            .visible = false,
                                            .resizable = false,
                                            .highPixelDensity = false,
                                            .minimumLogicalSize = ponder::platform::LogicalSize{300, 200},
                                            .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Vulkan};

    EXPECT_EQ(desc.title, "Molecular View");
    EXPECT_EQ(desc.logicalSize, (ponder::platform::LogicalSize{900, 700}));
    EXPECT_FALSE(desc.visible);
    EXPECT_FALSE(desc.resizable);
    EXPECT_FALSE(desc.highPixelDensity);
    EXPECT_EQ(desc.minimumLogicalSize, (ponder::platform::LogicalSize{300, 200}));
    EXPECT_EQ(desc.graphicsCompatibility, ponder::platform::WindowGraphicsCompatibility::Vulkan);
}
} // namespace
