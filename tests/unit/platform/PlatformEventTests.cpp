#include <ponder/platform/PlatformEvent.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <variant>

namespace
{
using namespace std::chrono_literals;

static_assert(std::variant_size_v<pond::platform::PlatformEvent> == 33U);

TEST(PlatformEventTests, ConstructsVisitsAndCopiesEveryAlternative)
{
    const pond::platform::PlatformTimestamp timestamp{123ns};
    const pond::platform::WindowId windowId{17};
    const pond::platform::DisplayId displayId{29};

    const std::array<pond::platform::PlatformEvent, 33> events{
        pond::platform::QuitRequestedEvent{timestamp},
        pond::platform::WindowCloseRequestedEvent{timestamp, windowId},
        pond::platform::WindowMovedEvent{timestamp, windowId, {-120, 45}},
        pond::platform::WindowLogicalSizeChangedEvent{timestamp, windowId, {1280, 800}},
        pond::platform::WindowPixelSizeChangedEvent{timestamp, windowId, {2560, 1600}},
        pond::platform::WindowFocusChangedEvent{timestamp, windowId, true},
        pond::platform::WindowVisibilityChangedEvent{timestamp, windowId, false},
        pond::platform::WindowStateChangedEvent{timestamp, windowId,
                                                pond::platform::WindowState::Maximized},
        pond::platform::WindowPresentationChangedEvent{
            timestamp, windowId, pond::platform::WindowPresentation::DesktopFullscreen},
        pond::platform::WindowDisplayChangedEvent{timestamp, windowId, std::optional{displayId}},
        pond::platform::WindowDisplayScaleChangedEvent{timestamp, windowId},
        pond::platform::WindowPointerEnteredEvent{timestamp, windowId},
        pond::platform::WindowPointerLeftEvent{timestamp, windowId},
        pond::platform::DisplayAddedEvent{timestamp, displayId},
        pond::platform::DisplayRemovedEvent{timestamp, displayId},
        pond::platform::DisplayMovedEvent{timestamp, displayId},
        pond::platform::DisplayDesktopModeChangedEvent{
            timestamp, displayId, std::optional{pond::platform::ScreenExtent{1920, 1080}}},
        pond::platform::DisplayCurrentModeChangedEvent{timestamp, displayId, std::nullopt},
        pond::platform::DisplayOrientationChangedEvent{timestamp, displayId,
                                                       pond::platform::DisplayOrientation::Unknown},
        pond::platform::DisplayContentScaleChangedEvent{timestamp, displayId},
        pond::platform::DisplayUsableBoundsChangedEvent{timestamp, displayId},
        pond::platform::KeyboardKeyEvent{timestamp, windowId, pond::platform::PhysicalKey::A,
                                         pond::platform::LogicalKey::FromCharacter(U'a'),
                                         pond::platform::KeyModifiers::LeftControl, true, false},
        pond::platform::TextInputEvent{timestamp, windowId, std::string{"molecule"}},
        pond::platform::TextCompositionEvent{timestamp, std::nullopt, std::string{"composition"},
                                             pond::platform::TextCompositionRange{2, 4}},
        pond::platform::MouseMotionEvent{timestamp, windowId, {42.5F, -3.25F}, {1.5F, -2.0F}},
        pond::platform::MouseButtonEvent{
            timestamp, std::nullopt, {12.25F, 24.5F}, pond::platform::MouseButton::X1, true},
        pond::platform::MouseWheelEvent{timestamp, windowId, {7.0F, 9.0F}, 1.25F, -0.5F},
        pond::platform::DropBeginEvent{timestamp, windowId,
                                       std::optional<std::string>{"source-app"}},
        pond::platform::DroppedFileEvent{timestamp,
                                         windowId,
                                         std::filesystem::path{"C:/tmp/molecule.sdf"},
                                         {2.0F, 4.0F},
                                         std::optional<std::string>{"source-app"}},
        pond::platform::DroppedTextEvent{
            timestamp, std::nullopt, std::string{"SMILES"}, {6.0F, 8.0F}, std::nullopt},
        pond::platform::DropPositionEvent{
            timestamp, windowId, {10.0F, 12.0F}, std::optional<std::string>{"source-app"}},
        pond::platform::DropCompleteEvent{timestamp, std::nullopt, {14.0F, 16.0F}, std::nullopt},
        pond::platform::DialogCompletedEvent{
            timestamp, pond::platform::DialogRequestId{37},
            pond::platform::DialogSelection{.paths = {std::filesystem::path{"C:/tmp/selected.sdf"}},
                                            .selectedFilterIndex = 0U}}};

    std::array<bool, 33> visited{};
    for (std::size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex)
    {
        const pond::platform::PlatformEvent& event = events[eventIndex];
        ASSERT_EQ(event.index(), eventIndex);
        std::visit(
            [&visited, eventIndex, timestamp](const auto& payload)
            {
                EXPECT_EQ(payload.timestamp, timestamp);
                visited[eventIndex] = true;
            },
            event);
    }

    for (const bool wasVisited : visited)
    {
        EXPECT_TRUE(wasVisited);
    }

    const auto copiedEvents = events;
    EXPECT_EQ(copiedEvents, events);
}

TEST(PlatformEventTests, PreservesTypedUnitsAndOptionalValues)
{
    const pond::platform::PlatformTimestamp timestamp{456ns};
    const pond::platform::WindowId windowId{31};
    const pond::platform::DisplayId displayId{47};

    const pond::platform::WindowMovedEvent moved{timestamp, windowId, {-1920, 100}};
    EXPECT_EQ(moved.position, (pond::platform::ScreenPosition{-1920, 100}));

    const pond::platform::WindowLogicalSizeChangedEvent logical{timestamp, windowId, {640, 480}};
    EXPECT_EQ(logical.logicalSize, (pond::platform::LogicalSize{640, 480}));

    const pond::platform::WindowPixelSizeChangedEvent pixel{timestamp, windowId, {1280, 960}};
    EXPECT_EQ(pixel.pixelSize, (pond::platform::PixelSize{1280, 960}));

    const pond::platform::WindowDisplayChangedEvent withoutDisplay{timestamp, windowId,
                                                                   std::nullopt};
    EXPECT_FALSE(withoutDisplay.displayId.has_value());

    const pond::platform::WindowDisplayChangedEvent withDisplay{timestamp, windowId, displayId};
    ASSERT_TRUE(withDisplay.displayId.has_value());
    EXPECT_EQ(*withDisplay.displayId, displayId);

    const pond::platform::DisplayDesktopModeChangedEvent withoutExtent{timestamp, displayId,
                                                                       std::nullopt};
    EXPECT_FALSE(withoutExtent.extent.has_value());

    const pond::platform::DisplayCurrentModeChangedEvent withExtent{
        timestamp, displayId, pond::platform::ScreenExtent{3840, 2160}};
    ASSERT_TRUE(withExtent.extent.has_value());
    EXPECT_EQ(*withExtent.extent, (pond::platform::ScreenExtent{3840, 2160}));

    const pond::platform::DisplayOrientationChangedEvent unknownOrientation{
        timestamp, displayId, pond::platform::DisplayOrientation::Unknown};
    EXPECT_EQ(unknownOrientation.orientation, pond::platform::DisplayOrientation::Unknown);

    const pond::platform::KeyboardKeyEvent targetlessKey{timestamp,
                                                         std::nullopt,
                                                         pond::platform::PhysicalKey::Unknown,
                                                         pond::platform::LogicalKey::Unknown(),
                                                         pond::platform::KeyModifiers::None,
                                                         false,
                                                         false};
    EXPECT_FALSE(targetlessKey.windowId.has_value());

    const pond::platform::TextCompositionEvent withoutSelection{timestamp, windowId, std::string{},
                                                                std::nullopt};
    EXPECT_FALSE(withoutSelection.selection.has_value());

    const pond::platform::TextCompositionEvent withInsertion{
        timestamp, windowId, std::string{}, pond::platform::TextCompositionRange{0, 0}};
    ASSERT_TRUE(withInsertion.selection.has_value());
    EXPECT_EQ(*withInsertion.selection, (pond::platform::TextCompositionRange{0, 0}));

    const pond::platform::MouseMotionEvent targetlessMotion{
        timestamp, std::nullopt, {15.5F, 20.25F}, {-1.0F, 2.5F}};
    EXPECT_FALSE(targetlessMotion.windowId.has_value());
    EXPECT_EQ(targetlessMotion.position, (pond::platform::LogicalPoint{15.5F, 20.25F}));
    EXPECT_EQ(targetlessMotion.relativeMovement, (pond::platform::LogicalPoint{-1.0F, 2.5F}));

    const pond::platform::MouseButtonEvent button{
        timestamp, windowId, {4.0F, 8.0F}, pond::platform::MouseButton::Right, true};
    EXPECT_EQ(button.position, (pond::platform::LogicalPoint{4.0F, 8.0F}));
    EXPECT_EQ(button.button, pond::platform::MouseButton::Right);
    EXPECT_TRUE(button.pressed);

    const pond::platform::MouseWheelEvent wheel{timestamp, windowId, {3.5F, 6.5F}, 2.0F, -1.25F};
    EXPECT_EQ(wheel.position, (pond::platform::LogicalPoint{3.5F, 6.5F}));
    EXPECT_FLOAT_EQ(wheel.horizontal, 2.0F);
    EXPECT_FLOAT_EQ(wheel.vertical, -1.25F);

    const pond::platform::DropBeginEvent beginDrop{timestamp, std::nullopt, std::nullopt};
    EXPECT_FALSE(beginDrop.windowId.has_value());
    EXPECT_FALSE(beginDrop.sourceApplication.has_value());

    const pond::platform::DroppedFileEvent droppedFile{timestamp,
                                                       windowId,
                                                       std::filesystem::path{"C:/tmp/molecule.sdf"},
                                                       {11.5F, 22.25F},
                                                       std::optional<std::string>{"source-app"}};
    EXPECT_EQ(droppedFile.path, std::filesystem::path{"C:/tmp/molecule.sdf"});
    EXPECT_EQ(droppedFile.position, (pond::platform::LogicalPoint{11.5F, 22.25F}));
    ASSERT_TRUE(droppedFile.sourceApplication.has_value());
    EXPECT_EQ(*droppedFile.sourceApplication, "source-app");

    const pond::platform::DroppedTextEvent droppedText{
        timestamp, std::nullopt, std::string{"C=O"}, {1.0F, 2.0F}, std::nullopt};
    EXPECT_FALSE(droppedText.windowId.has_value());
    EXPECT_EQ(droppedText.text, "C=O");

    const pond::platform::DropPositionEvent dropPosition{
        timestamp, windowId, {3.0F, 4.0F}, std::nullopt};
    EXPECT_EQ(dropPosition.position, (pond::platform::LogicalPoint{3.0F, 4.0F}));

    const pond::platform::DropCompleteEvent dropComplete{
        timestamp, windowId, {5.0F, 6.0F}, std::optional<std::string>{"source-app"}};
    ASSERT_TRUE(dropComplete.sourceApplication.has_value());
    EXPECT_EQ(*dropComplete.sourceApplication, "source-app");

    const pond::platform::DialogCompletedEvent selectedDialog{
        timestamp, pond::platform::DialogRequestId{3},
        pond::platform::DialogSelection{.paths = {std::filesystem::path{"C:/tmp/dialog.sdf"}},
                                        .selectedFilterIndex = 1U}};
    EXPECT_EQ(selectedDialog.requestId, pond::platform::DialogRequestId{3});
    const auto& selection = std::get<pond::platform::DialogSelection>(selectedDialog.outcome);
    ASSERT_EQ(selection.paths.size(), 1U);
    EXPECT_EQ(selection.paths.front(), std::filesystem::path{"C:/tmp/dialog.sdf"});
    ASSERT_TRUE(selection.selectedFilterIndex.has_value());
    EXPECT_EQ(*selection.selectedFilterIndex, 1U);

    const pond::platform::DialogCompletedEvent cancelledDialog{
        timestamp, pond::platform::DialogRequestId{4}, pond::platform::DialogCancellation{}};
    EXPECT_TRUE(
        std::holds_alternative<pond::platform::DialogCancellation>(cancelledDialog.outcome));
}

TEST(PlatformEventTests, DefaultedEqualityIncludesPayloadFields)
{
    const pond::platform::WindowFocusChangedEvent focused{pond::platform::PlatformTimestamp{789ns},
                                                          pond::platform::WindowId{53}, true};
    const pond::platform::WindowFocusChangedEvent same = focused;
    const pond::platform::WindowFocusChangedEvent different{focused.timestamp, focused.windowId,
                                                            false};

    EXPECT_EQ(same, focused);
    EXPECT_NE(different, focused);
}
} // namespace
