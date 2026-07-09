#include <ponder/platform/PlatformRuntime.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
static_assert(!std::is_copy_constructible_v<pond::platform::PlatformRuntime>);
static_assert(!std::is_copy_assignable_v<pond::platform::PlatformRuntime>);
static_assert(std::is_nothrow_move_constructible_v<pond::platform::PlatformRuntime>);
static_assert(std::is_nothrow_move_assignable_v<pond::platform::PlatformRuntime>);
static_assert(std::is_nothrow_destructible_v<pond::platform::PlatformRuntime>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>().PollEvent()),
              std::optional<pond::platform::PlatformEvent>>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>().SetMouseCapture(
                  true)),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<const pond::platform::PlatformRuntime&>()
                           .GetGlobalMousePosition()),
              pond::core::Result<pond::platform::LogicalPoint>>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>().SetSystemCursor(
                  pond::platform::SystemCursorShape::Default)),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>().ShowCursor()),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>().HideCursor()),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<const pond::platform::PlatformRuntime&>()
                           .IsCursorVisible()),
              bool>);
static_assert(std::is_same_v<
              decltype(std::declval<const pond::platform::PlatformRuntime&>()
                           .GetClipboardText()),
              pond::core::Result<std::string>>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>()
                           .SetClipboardText(std::string_view{})),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>()
                           .OpenExternalUri(std::string_view{})),
              pond::core::VoidResult>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>()
                           .ShowOpenFileDialog(
                               std::declval<const pond::platform::OpenFileDialogDesc&>())),
              pond::core::Result<pond::platform::DialogRequestId>>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>()
                           .ShowSaveFileDialog(
                               std::declval<const pond::platform::SaveFileDialogDesc&>())),
              pond::core::Result<pond::platform::DialogRequestId>>);
static_assert(std::is_same_v<
              decltype(std::declval<pond::platform::PlatformRuntime&>()
                           .ShowOpenFolderDialog(
                               std::declval<const pond::platform::OpenFolderDialogDesc&>())),
              pond::core::Result<pond::platform::DialogRequestId>>);

TEST(PlatformRuntimeDescTests, ProvidesStableApplicationMetadataDefaults)
{
    const pond::platform::PlatformRuntimeDesc desc;

    EXPECT_EQ(desc.applicationName, "ponder");
    EXPECT_FALSE(desc.applicationVersion.has_value());
    EXPECT_FALSE(desc.applicationIdentifier.has_value());
}

TEST(PlatformRuntimeDescTests, OwnsConfiguredApplicationMetadata)
{
    const pond::platform::PlatformRuntimeDesc desc{
        .applicationName = "Molecular Workbench",
        .applicationVersion = std::string{"1.2.3"},
        .applicationIdentifier = std::string{"org.ponder.workbench"}};

    EXPECT_EQ(desc.applicationName, "Molecular Workbench");
    EXPECT_EQ(desc.applicationVersion, "1.2.3");
    EXPECT_EQ(desc.applicationIdentifier, "org.ponder.workbench");
}
} // namespace
