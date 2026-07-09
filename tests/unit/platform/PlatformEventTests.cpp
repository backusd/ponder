#include <ponder/platform/PlatformEvent.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>

namespace
{
using namespace std::chrono_literals;

static_assert(std::variant_size_v<pond::platform::PlatformEvent> == 24U);

TEST(PlatformEventTests, ConstructsVisitsAndCopiesEveryAlternative)
{
    const pond::platform::PlatformTimestamp timestamp{123ns};
    const pond::platform::WindowId windowId{17};
    const pond::platform::DisplayId displayId{29};

    const std::array<pond::platform::PlatformEvent, 24> events{
        pond::platform::QuitRequestedEvent{timestamp},
        pond::platform::WindowCloseRequestedEvent{timestamp, windowId},
        pond::platform::WindowMovedEvent{timestamp, windowId, {-120, 45}},
        pond::platform::WindowLogicalSizeChangedEvent{timestamp, windowId,
                                                       {1280, 800}},
        pond::platform::WindowPixelSizeChangedEvent{timestamp, windowId,
                                                     {2560, 1600}},
        pond::platform::WindowFocusChangedEvent{timestamp, windowId, true},
        pond::platform::WindowVisibilityChangedEvent{timestamp, windowId, false},
        pond::platform::WindowStateChangedEvent{
            timestamp, windowId, pond::platform::WindowState::Maximized},
        pond::platform::WindowPresentationChangedEvent{
            timestamp, windowId,
            pond::platform::WindowPresentation::DesktopFullscreen},
        pond::platform::WindowDisplayChangedEvent{
            timestamp, windowId, std::optional{displayId}},
        pond::platform::WindowDisplayScaleChangedEvent{timestamp, windowId},
        pond::platform::WindowPointerEnteredEvent{timestamp, windowId},
        pond::platform::WindowPointerLeftEvent{timestamp, windowId},
        pond::platform::DisplayAddedEvent{timestamp, displayId},
        pond::platform::DisplayRemovedEvent{timestamp, displayId},
        pond::platform::DisplayMovedEvent{timestamp, displayId},
        pond::platform::DisplayDesktopModeChangedEvent{
            timestamp, displayId,
            std::optional{pond::platform::ScreenExtent{1920, 1080}}},
        pond::platform::DisplayCurrentModeChangedEvent{
            timestamp, displayId, std::nullopt},
        pond::platform::DisplayOrientationChangedEvent{
            timestamp, displayId, pond::platform::DisplayOrientation::Unknown},
        pond::platform::DisplayContentScaleChangedEvent{timestamp, displayId},
        pond::platform::DisplayUsableBoundsChangedEvent{timestamp, displayId},
        pond::platform::KeyboardKeyEvent{
            timestamp,
            windowId,
            pond::platform::PhysicalKey::A,
            pond::platform::LogicalKey::FromCharacter(U'a'),
            pond::platform::KeyModifiers::LeftControl,
            true,
            false},
        pond::platform::TextInputEvent{
            timestamp, windowId, std::string{"molecule"}},
        pond::platform::TextCompositionEvent{
            timestamp,
            std::nullopt,
            std::string{"composition"},
            pond::platform::TextCompositionRange{2, 4}}};

    std::array<bool, 24> visited{};
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

    const pond::platform::WindowMovedEvent moved{
        timestamp, windowId, {-1920, 100}};
    EXPECT_EQ(moved.position, (pond::platform::ScreenPosition{-1920, 100}));

    const pond::platform::WindowLogicalSizeChangedEvent logical{
        timestamp, windowId, {640, 480}};
    EXPECT_EQ(logical.logicalSize, (pond::platform::LogicalSize{640, 480}));

    const pond::platform::WindowPixelSizeChangedEvent pixel{
        timestamp, windowId, {1280, 960}};
    EXPECT_EQ(pixel.pixelSize, (pond::platform::PixelSize{1280, 960}));

    const pond::platform::WindowDisplayChangedEvent withoutDisplay{
        timestamp, windowId, std::nullopt};
    EXPECT_FALSE(withoutDisplay.displayId.has_value());

    const pond::platform::WindowDisplayChangedEvent withDisplay{
        timestamp, windowId, displayId};
    ASSERT_TRUE(withDisplay.displayId.has_value());
    EXPECT_EQ(*withDisplay.displayId, displayId);

    const pond::platform::DisplayDesktopModeChangedEvent withoutExtent{
        timestamp, displayId, std::nullopt};
    EXPECT_FALSE(withoutExtent.extent.has_value());

    const pond::platform::DisplayCurrentModeChangedEvent withExtent{
        timestamp, displayId, pond::platform::ScreenExtent{3840, 2160}};
    ASSERT_TRUE(withExtent.extent.has_value());
    EXPECT_EQ(*withExtent.extent,
              (pond::platform::ScreenExtent{3840, 2160}));

    const pond::platform::DisplayOrientationChangedEvent unknownOrientation{
        timestamp, displayId, pond::platform::DisplayOrientation::Unknown};
    EXPECT_EQ(unknownOrientation.orientation,
              pond::platform::DisplayOrientation::Unknown);

    const pond::platform::KeyboardKeyEvent targetlessKey{
        timestamp,
        std::nullopt,
        pond::platform::PhysicalKey::Unknown,
        pond::platform::LogicalKey::Unknown(),
        pond::platform::KeyModifiers::None,
        false,
        false};
    EXPECT_FALSE(targetlessKey.windowId.has_value());

    const pond::platform::TextCompositionEvent withoutSelection{
        timestamp, windowId, std::string{}, std::nullopt};
    EXPECT_FALSE(withoutSelection.selection.has_value());

    const pond::platform::TextCompositionEvent withInsertion{
        timestamp, windowId, std::string{},
        pond::platform::TextCompositionRange{0, 0}};
    ASSERT_TRUE(withInsertion.selection.has_value());
    EXPECT_EQ(*withInsertion.selection,
              (pond::platform::TextCompositionRange{0, 0}));
}

TEST(PlatformEventTests, DefaultedEqualityIncludesPayloadFields)
{
    const pond::platform::WindowFocusChangedEvent focused{
        pond::platform::PlatformTimestamp{789ns},
        pond::platform::WindowId{53},
        true};
    const pond::platform::WindowFocusChangedEvent same = focused;
    const pond::platform::WindowFocusChangedEvent different{
        focused.timestamp, focused.windowId, false};

    EXPECT_EQ(same, focused);
    EXPECT_NE(different, focused);
}
} // namespace
