#include <ponder/platform/Dialogs.hpp>
#include <ponder/platform/PlatformError.hpp>
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

static_assert(std::variant_size_v<ponder::platform::PlatformEvent> == 33U);

TEST(PlatformEventTests, ConstructsVisitsAndCopiesEveryAlternative)
{
    const ponder::core::Timestamp timestamp{123ns};
    const ponder::platform::WindowId windowId{17};
    const ponder::platform::DisplayId displayId{29};

    const std::array<ponder::platform::PlatformEvent, 33> events{
        ponder::platform::QuitRequestedEvent{timestamp},
        ponder::platform::WindowCloseRequestedEvent{timestamp, windowId},
        ponder::platform::WindowMovedEvent{timestamp, windowId, {-120, 45}},
        ponder::platform::WindowLogicalSizeChangedEvent{timestamp, windowId, {1280, 800}},
        ponder::platform::WindowPixelSizeChangedEvent{timestamp, windowId, {2560, 1600}},
        ponder::platform::WindowFocusChangedEvent{timestamp, windowId, true},
        ponder::platform::WindowVisibilityChangedEvent{timestamp, windowId, false},
        ponder::platform::WindowStateChangedEvent{timestamp, windowId, ponder::platform::WindowState::Maximized},
        ponder::platform::WindowPresentationChangedEvent{timestamp, windowId, ponder::platform::WindowPresentation::DesktopFullscreen},
        ponder::platform::WindowDisplayChangedEvent{timestamp, windowId, std::optional{displayId}},
        ponder::platform::WindowDisplayScaleChangedEvent{timestamp, windowId},
        ponder::platform::WindowPointerEnteredEvent{timestamp, windowId},
        ponder::platform::WindowPointerLeftEvent{timestamp, windowId},
        ponder::platform::DisplayAddedEvent{timestamp, displayId},
        ponder::platform::DisplayRemovedEvent{timestamp, displayId},
        ponder::platform::DisplayMovedEvent{timestamp, displayId},
        ponder::platform::DisplayDesktopModeChangedEvent{timestamp, displayId, std::optional{ponder::platform::ScreenExtent{1920, 1080}}},
        ponder::platform::DisplayCurrentModeChangedEvent{timestamp, displayId, std::nullopt},
        ponder::platform::DisplayOrientationChangedEvent{timestamp, displayId, ponder::platform::DisplayOrientation::Unknown},
        ponder::platform::DisplayContentScaleChangedEvent{timestamp, displayId},
        ponder::platform::DisplayUsableBoundsChangedEvent{timestamp, displayId},
        ponder::platform::KeyboardKeyEvent{timestamp, windowId, ponder::platform::PhysicalKey::A, ponder::platform::LogicalKey::FromCharacter(U'a'),
                                           ponder::platform::KeyModifiers::LeftControl, true, false},
        ponder::platform::TextInputEvent{timestamp, windowId, std::string{"molecule"}},
        ponder::platform::TextCompositionEvent{timestamp, std::nullopt, std::string{"composition"}, ponder::platform::TextCompositionRange{2, 4}},
        ponder::platform::MouseMotionEvent{timestamp, windowId, {42.5F, -3.25F}, {1.5F, -2.0F}},
        ponder::platform::MouseButtonEvent{timestamp, std::nullopt, {12.25F, 24.5F}, ponder::platform::MouseButton::X1, true},
        ponder::platform::MouseWheelEvent{timestamp, windowId, {7.0F, 9.0F}, 1.25F, -0.5F},
        ponder::platform::DropBeginEvent{timestamp, windowId, std::optional<std::string>{"source-app"}},
        ponder::platform::DroppedFileEvent{timestamp,
                                           windowId,
                                           std::filesystem::path{"C:/tmp/molecule.sdf"},
                                           {2.0F, 4.0F},
                                           std::optional<std::string>{"source-app"}},
        ponder::platform::DroppedTextEvent{timestamp, std::nullopt, std::string{"SMILES"}, {6.0F, 8.0F}, std::nullopt},
        ponder::platform::DropPositionEvent{timestamp, windowId, {10.0F, 12.0F}, std::optional<std::string>{"source-app"}},
        ponder::platform::DropCompleteEvent{timestamp, std::nullopt, {14.0F, 16.0F}, std::nullopt},
        ponder::platform::DialogCompletedEvent{
            timestamp,
            ponder::platform::DialogRequestInfo{.id = ponder::platform::dialogs::DialogRequestId{37},
                                                .kind = ponder::platform::dialogs::DialogKind::OpenFile,
                                                .requestedAt = timestamp,
                                                .parentWindowId = windowId,
                                                .filterCount = 1,
                                                .allowMultipleSelection = false},
            ponder::platform::DialogSelection{.paths = {std::filesystem::path{"C:/tmp/selected.sdf"}}, .selectedFilterIndex = 0U}}};

    std::array<bool, 33> visited{};
    for (std::size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex)
    {
        const ponder::platform::PlatformEvent& event = events[eventIndex];
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
    const ponder::core::Timestamp timestamp{456ns};
    const ponder::platform::WindowId windowId{31};
    const ponder::platform::DisplayId displayId{47};

    const ponder::platform::WindowMovedEvent moved{timestamp, windowId, {-1920, 100}};
    EXPECT_EQ(moved.position, (ponder::platform::ScreenPosition{-1920, 100}));

    const ponder::platform::WindowLogicalSizeChangedEvent logical{timestamp, windowId, {640, 480}};
    EXPECT_EQ(logical.logicalSize, (ponder::platform::LogicalSize{640, 480}));

    const ponder::platform::WindowPixelSizeChangedEvent pixel{timestamp, windowId, {1280, 960}};
    EXPECT_EQ(pixel.pixelSize, (ponder::platform::PixelSize{1280, 960}));

    const ponder::platform::WindowDisplayChangedEvent withoutDisplay{timestamp, windowId, std::nullopt};
    EXPECT_FALSE(withoutDisplay.displayId.has_value());

    const ponder::platform::WindowDisplayChangedEvent withDisplay{timestamp, windowId, displayId};
    ASSERT_TRUE(withDisplay.displayId.has_value());
    EXPECT_EQ(*withDisplay.displayId, displayId);

    const ponder::platform::DisplayDesktopModeChangedEvent withoutExtent{timestamp, displayId, std::nullopt};
    EXPECT_FALSE(withoutExtent.extent.has_value());

    const ponder::platform::DisplayCurrentModeChangedEvent withExtent{timestamp, displayId, ponder::platform::ScreenExtent{3840, 2160}};
    ASSERT_TRUE(withExtent.extent.has_value());
    EXPECT_EQ(*withExtent.extent, (ponder::platform::ScreenExtent{3840, 2160}));

    const ponder::platform::DisplayOrientationChangedEvent unknownOrientation{timestamp, displayId, ponder::platform::DisplayOrientation::Unknown};
    EXPECT_EQ(unknownOrientation.orientation, ponder::platform::DisplayOrientation::Unknown);

    const ponder::platform::KeyboardKeyEvent targetlessKey{timestamp,
                                                           std::nullopt,
                                                           ponder::platform::PhysicalKey::Unknown,
                                                           ponder::platform::LogicalKey::Unknown(),
                                                           ponder::platform::KeyModifiers::None,
                                                           false,
                                                           false};
    EXPECT_FALSE(targetlessKey.windowId.has_value());

    const ponder::platform::TextCompositionEvent withoutSelection{timestamp, windowId, std::string{}, std::nullopt};
    EXPECT_FALSE(withoutSelection.selection.has_value());

    const ponder::platform::TextCompositionEvent withInsertion{timestamp, windowId, std::string{}, ponder::platform::TextCompositionRange{0, 0}};
    ASSERT_TRUE(withInsertion.selection.has_value());
    EXPECT_EQ(*withInsertion.selection, (ponder::platform::TextCompositionRange{0, 0}));

    const ponder::platform::MouseMotionEvent targetlessMotion{timestamp, std::nullopt, {15.5F, 20.25F}, {-1.0F, 2.5F}};
    EXPECT_FALSE(targetlessMotion.windowId.has_value());
    EXPECT_EQ(targetlessMotion.position, (ponder::platform::LogicalPoint{15.5F, 20.25F}));
    EXPECT_EQ(targetlessMotion.relativeMovement, (ponder::platform::LogicalPoint{-1.0F, 2.5F}));

    const ponder::platform::MouseButtonEvent button{timestamp, windowId, {4.0F, 8.0F}, ponder::platform::MouseButton::Right, true};
    EXPECT_EQ(button.position, (ponder::platform::LogicalPoint{4.0F, 8.0F}));
    EXPECT_EQ(button.button, ponder::platform::MouseButton::Right);
    EXPECT_TRUE(button.pressed);

    const ponder::platform::MouseWheelEvent wheel{timestamp, windowId, {3.5F, 6.5F}, 2.0F, -1.25F};
    EXPECT_EQ(wheel.position, (ponder::platform::LogicalPoint{3.5F, 6.5F}));
    EXPECT_FLOAT_EQ(wheel.horizontal, 2.0F);
    EXPECT_FLOAT_EQ(wheel.vertical, -1.25F);

    const ponder::platform::DropBeginEvent beginDrop{timestamp, std::nullopt, std::nullopt};
    EXPECT_FALSE(beginDrop.windowId.has_value());
    EXPECT_FALSE(beginDrop.sourceApplication.has_value());

    const ponder::platform::DroppedFileEvent droppedFile{timestamp,
                                                         windowId,
                                                         std::filesystem::path{"C:/tmp/molecule.sdf"},
                                                         {11.5F, 22.25F},
                                                         std::optional<std::string>{"source-app"}};
    EXPECT_EQ(droppedFile.path, std::filesystem::path{"C:/tmp/molecule.sdf"});
    EXPECT_EQ(droppedFile.position, (ponder::platform::LogicalPoint{11.5F, 22.25F}));
    ASSERT_TRUE(droppedFile.sourceApplication.has_value());
    EXPECT_EQ(*droppedFile.sourceApplication, "source-app");

    const ponder::platform::DroppedTextEvent droppedText{timestamp, std::nullopt, std::string{"C=O"}, {1.0F, 2.0F}, std::nullopt};
    EXPECT_FALSE(droppedText.windowId.has_value());
    EXPECT_EQ(droppedText.text, "C=O");

    const ponder::platform::DropPositionEvent dropPosition{timestamp, windowId, {3.0F, 4.0F}, std::nullopt};
    EXPECT_EQ(dropPosition.position, (ponder::platform::LogicalPoint{3.0F, 4.0F}));

    const ponder::platform::DropCompleteEvent dropComplete{timestamp, windowId, {5.0F, 6.0F}, std::optional<std::string>{"source-app"}};
    ASSERT_TRUE(dropComplete.sourceApplication.has_value());
    EXPECT_EQ(*dropComplete.sourceApplication, "source-app");

    const ponder::platform::DialogCompletedEvent selectedDialog{
        timestamp,
        ponder::platform::DialogRequestInfo{.id = ponder::platform::dialogs::DialogRequestId{3},
                                            .kind = ponder::platform::dialogs::DialogKind::OpenFile,
                                            .requestedAt = ponder::core::Timestamp{123ns},
                                            .parentWindowId = windowId,
                                            .filterCount = 2,
                                            .allowMultipleSelection = true},
        ponder::platform::DialogSelection{.paths = {std::filesystem::path{"C:/tmp/dialog.sdf"}}, .selectedFilterIndex = 1U}};
    EXPECT_EQ(selectedDialog.request.id, ponder::platform::dialogs::DialogRequestId{3});
    EXPECT_EQ(selectedDialog.request.kind, ponder::platform::dialogs::DialogKind::OpenFile);
    EXPECT_EQ(selectedDialog.request.requestedAt, ponder::core::Timestamp{123ns});
    EXPECT_EQ(selectedDialog.request.parentWindowId, windowId);
    EXPECT_EQ(selectedDialog.request.filterCount, 2U);
    EXPECT_TRUE(selectedDialog.request.allowMultipleSelection);
    const auto& selection = std::get<ponder::platform::DialogSelection>(selectedDialog.outcome);
    ASSERT_EQ(selection.paths.size(), 1U);
    EXPECT_EQ(selection.paths.front(), std::filesystem::path{"C:/tmp/dialog.sdf"});
    ASSERT_TRUE(selection.selectedFilterIndex.has_value());
    EXPECT_EQ(*selection.selectedFilterIndex, 1U);

    const ponder::platform::DialogCompletedEvent cancelledDialog{
        timestamp,
        ponder::platform::DialogRequestInfo{.id = ponder::platform::dialogs::DialogRequestId{4},
                                            .kind = ponder::platform::dialogs::DialogKind::OpenFolder},
        ponder::platform::DialogCancellation{}};
    EXPECT_TRUE(std::holds_alternative<ponder::platform::DialogCancellation>(cancelledDialog.outcome));

    const ponder::platform::DialogCompletedEvent failedDialog{
        timestamp,
        ponder::platform::DialogRequestInfo{.id = ponder::platform::dialogs::DialogRequestId{5},
                                            .kind = ponder::platform::dialogs::DialogKind::SaveFile},
        ponder::platform::DialogFailure{
            ponder::core::Error{ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::BackendFailure), "asynchronous dialog failure"}}};
    const auto* failure = std::get_if<ponder::platform::DialogFailure>(&failedDialog.outcome);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->error.GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::BackendFailure));
    EXPECT_EQ(failure->error.GetMessage(), "asynchronous dialog failure");
}

TEST(PlatformEventTests, DefaultedEqualityIncludesPayloadFields)
{
    const ponder::platform::WindowFocusChangedEvent focused{ponder::core::Timestamp{789ns}, ponder::platform::WindowId{53}, true};
    const ponder::platform::WindowFocusChangedEvent same = focused;
    const ponder::platform::WindowFocusChangedEvent different{focused.timestamp, focused.windowId, false};

    EXPECT_EQ(same, focused);
    EXPECT_NE(different, focused);
}
} // namespace
