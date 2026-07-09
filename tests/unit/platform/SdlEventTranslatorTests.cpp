#include "SdlEventTranslator.hpp"

#include <ponder/platform/PlatformEvent.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace
{
constexpr std::uint32_t kBackendWindowId{41};
constexpr std::uint32_t kBackendDisplayId{73};
constexpr pond::platform::WindowId kWindowId{7};
constexpr pond::platform::DisplayId kDisplayId{11};
constexpr std::uint64_t kTimestampNanoseconds{123'456'789};

[[nodiscard]] constexpr pond::platform::PlatformTimestamp MakeTimestamp(
    std::int64_t nanoseconds = static_cast<std::int64_t>(kTimestampNanoseconds))
{
    return pond::platform::PlatformTimestamp{std::chrono::nanoseconds{nanoseconds}};
}

struct ResolverState final
{
    std::unordered_map<std::uint32_t, pond::platform::WindowId> windows;
    std::unordered_map<std::uint32_t, pond::platform::DisplayId> displays;
    std::vector<std::uint32_t> windowRequests;
    std::vector<std::uint32_t> displayRequests;
};

[[nodiscard]] std::optional<pond::platform::WindowId> ResolveWindowId(
    void* context, std::uint32_t backendWindowId)
{
    ResolverState& state = *static_cast<ResolverState*>(context);
    state.windowRequests.push_back(backendWindowId);
    const auto iterator = state.windows.find(backendWindowId);
    return iterator == state.windows.end()
             ? std::nullopt
             : std::optional<pond::platform::WindowId>{iterator->second};
}

[[nodiscard]] std::optional<pond::platform::DisplayId> ResolveDisplayId(
    void* context, std::uint32_t backendDisplayId)
{
    ResolverState& state = *static_cast<ResolverState*>(context);
    state.displayRequests.push_back(backendDisplayId);
    const auto iterator = state.displays.find(backendDisplayId);
    return iterator == state.displays.end()
             ? std::nullopt
             : std::optional<pond::platform::DisplayId>{iterator->second};
}

[[nodiscard]] SDL_Event MakeQuitEvent(
    std::uint64_t timestamp = kTimestampNanoseconds)
{
    SDL_Event event{};
    event.quit.type = SDL_EVENT_QUIT;
    event.quit.timestamp = timestamp;
    return event;
}

[[nodiscard]] SDL_Event MakeWindowEvent(
    SDL_EventType type, std::uint64_t timestamp = kTimestampNanoseconds,
    std::uint32_t backendWindowId = kBackendWindowId)
{
    SDL_Event event{};
    event.window.type = type;
    event.window.timestamp = timestamp;
    event.window.windowID = backendWindowId;
    return event;
}

[[nodiscard]] SDL_Event MakeDisplayEvent(
    SDL_EventType type, std::uint64_t timestamp = kTimestampNanoseconds,
    std::uint32_t backendDisplayId = kBackendDisplayId)
{
    SDL_Event event{};
    event.display.type = type;
    event.display.timestamp = timestamp;
    event.display.displayID = backendDisplayId;
    return event;
}

template <typename Event>
void ExpectTranslatedEvent(
    const std::optional<pond::platform::PlatformEvent>& translated,
    const Event& expected)
{
    ASSERT_TRUE(translated.has_value());
    ASSERT_TRUE(std::holds_alternative<Event>(*translated));
    EXPECT_EQ(std::get<Event>(*translated), expected);
}

class SdlEventTranslatorTests : public testing::Test
{
protected:
    void SetUp() override
    {
        m_resolvers.windows.emplace(kBackendWindowId, kWindowId);
        m_resolvers.displays.emplace(kBackendDisplayId, kDisplayId);
        m_context = pond::platform::detail::EventTranslationContext{
            &m_resolvers, ResolveWindowId, ResolveDisplayId};
    }

    [[nodiscard]] std::optional<pond::platform::PlatformEvent> Translate(
        const SDL_Event& event)
    {
        return pond::platform::detail::TranslateSdlEvent(event, m_context);
    }

    ResolverState m_resolvers;
    pond::platform::detail::EventTranslationContext m_context;
};

TEST_F(SdlEventTranslatorTests, TranslatesQuitAndEveryWindowLifecycleSignal)
{
    const pond::platform::PlatformTimestamp timestamp = MakeTimestamp();

    ExpectTranslatedEvent(
        Translate(MakeQuitEvent()),
        pond::platform::QuitRequestedEvent{.timestamp = timestamp});
    EXPECT_TRUE(m_resolvers.windowRequests.empty());
    EXPECT_TRUE(m_resolvers.displayRequests.empty());

    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED)),
        pond::platform::WindowCloseRequestedEvent{
            .timestamp = timestamp, .windowId = kWindowId});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED)),
        pond::platform::WindowFocusChangedEvent{
            .timestamp = timestamp, .windowId = kWindowId, .focused = true});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST)),
        pond::platform::WindowFocusChangedEvent{
            .timestamp = timestamp, .windowId = kWindowId, .focused = false});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_SHOWN)),
        pond::platform::WindowVisibilityChangedEvent{
            .timestamp = timestamp, .windowId = kWindowId, .visible = true});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_HIDDEN)),
        pond::platform::WindowVisibilityChangedEvent{
            .timestamp = timestamp, .windowId = kWindowId, .visible = false});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MINIMIZED)),
        pond::platform::WindowStateChangedEvent{
            .timestamp = timestamp,
            .windowId = kWindowId,
            .state = pond::platform::WindowState::Minimized});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MAXIMIZED)),
        pond::platform::WindowStateChangedEvent{
            .timestamp = timestamp,
            .windowId = kWindowId,
            .state = pond::platform::WindowState::Maximized});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_RESTORED)),
        pond::platform::WindowStateChangedEvent{
            .timestamp = timestamp,
            .windowId = kWindowId,
            .state = pond::platform::WindowState::Normal});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_ENTER_FULLSCREEN)),
        pond::platform::WindowPresentationChangedEvent{
            .timestamp = timestamp,
            .windowId = kWindowId,
            .presentation =
                pond::platform::WindowPresentation::DesktopFullscreen});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)),
        pond::platform::WindowPresentationChangedEvent{
            .timestamp = timestamp,
            .windowId = kWindowId,
            .presentation = pond::platform::WindowPresentation::Windowed});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)),
        pond::platform::WindowDisplayScaleChangedEvent{
            .timestamp = timestamp, .windowId = kWindowId});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MOUSE_ENTER)),
        pond::platform::WindowPointerEnteredEvent{
            .timestamp = timestamp, .windowId = kWindowId});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MOUSE_LEAVE)),
        pond::platform::WindowPointerLeftEvent{
            .timestamp = timestamp, .windowId = kWindowId});
}

TEST_F(SdlEventTranslatorTests, PreservesSignedMovementAndDistinctSizeUnits)
{
    SDL_Event moved = MakeWindowEvent(SDL_EVENT_WINDOW_MOVED);
    moved.window.data1 = std::numeric_limits<Sint32>::min();
    moved.window.data2 = std::numeric_limits<Sint32>::max();
    ExpectTranslatedEvent(
        Translate(moved),
        pond::platform::WindowMovedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .position = {std::numeric_limits<std::int32_t>::min(),
                         std::numeric_limits<std::int32_t>::max()}});

    SDL_Event logical = MakeWindowEvent(SDL_EVENT_WINDOW_RESIZED);
    logical.window.data1 = 640;
    logical.window.data2 = 480;
    ExpectTranslatedEvent(
        Translate(logical),
        pond::platform::WindowLogicalSizeChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .logicalSize = {640, 480}});

    SDL_Event pixel = MakeWindowEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
    pixel.window.data1 = 1280;
    pixel.window.data2 = 960;
    ExpectTranslatedEvent(
        Translate(pixel),
        pond::platform::WindowPixelSizeChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .pixelSize = {1280, 960}});

    logical.window.data1 = 0;
    logical.window.data2 = 0;
    ExpectTranslatedEvent(
        Translate(logical),
        pond::platform::WindowLogicalSizeChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .logicalSize = {0, 0}});
    pixel.window.data1 = 0;
    pixel.window.data2 = 0;
    ExpectTranslatedEvent(
        Translate(pixel),
        pond::platform::WindowPixelSizeChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .pixelSize = {0, 0}});

    logical.window.data1 = -1;
    logical.window.data2 = 480;
    EXPECT_FALSE(Translate(logical).has_value());
    logical.window.data1 = 640;
    logical.window.data2 = -1;
    EXPECT_FALSE(Translate(logical).has_value());
    pixel.window.data1 = -1;
    pixel.window.data2 = 960;
    EXPECT_FALSE(Translate(pixel).has_value());
    pixel.window.data1 = 1280;
    pixel.window.data2 = -1;
    EXPECT_FALSE(Translate(pixel).has_value());
}

TEST_F(SdlEventTranslatorTests, MapsOptionalWindowDestinationDisplays)
{
    SDL_Event event = MakeWindowEvent(SDL_EVENT_WINDOW_DISPLAY_CHANGED);
    event.window.data1 = static_cast<Sint32>(kBackendDisplayId);
    ExpectTranslatedEvent(
        Translate(event),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .displayId = kDisplayId});
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kBackendDisplayId);

    m_resolvers.displayRequests.clear();
    event.window.data1 = 0;
    ExpectTranslatedEvent(
        Translate(event),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .displayId = std::nullopt});
    EXPECT_TRUE(m_resolvers.displayRequests.empty());

    constexpr std::uint32_t kUnresolvedDisplayId{82};
    event.window.data1 = static_cast<Sint32>(kUnresolvedDisplayId);
    ExpectTranslatedEvent(
        Translate(event),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .displayId = std::nullopt});
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kUnresolvedDisplayId);

    constexpr std::uint32_t kHighBitBackendDisplayId{0xF123'4567U};
    constexpr pond::platform::DisplayId kHighBitDisplayId{99};
    m_resolvers.displays.emplace(kHighBitBackendDisplayId, kHighBitDisplayId);
    m_resolvers.displayRequests.clear();
    event.window.data1 =
        std::bit_cast<std::int32_t>(kHighBitBackendDisplayId);
    ExpectTranslatedEvent(
        Translate(event),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .displayId = kHighBitDisplayId});
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kHighBitBackendDisplayId);

    constexpr std::uint32_t kInvalidMappedDisplayBackendId{83};
    m_resolvers.displays.emplace(kInvalidMappedDisplayBackendId,
                                 pond::platform::DisplayId::Invalid());
    event.window.data1 = static_cast<Sint32>(kInvalidMappedDisplayBackendId);
    ExpectTranslatedEvent(
        Translate(event),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .displayId = std::nullopt});
}

TEST_F(SdlEventTranslatorTests, TranslatesEveryDisplaySignalAndOptionalModeExtent)
{
    const pond::platform::PlatformTimestamp timestamp = MakeTimestamp();

    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_ADDED)),
        pond::platform::DisplayAddedEvent{
            .timestamp = timestamp, .displayId = kDisplayId});
    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_REMOVED)),
        pond::platform::DisplayRemovedEvent{
            .timestamp = timestamp, .displayId = kDisplayId});
    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED)),
        pond::platform::DisplayMovedEvent{
            .timestamp = timestamp, .displayId = kDisplayId});
    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED)),
        pond::platform::DisplayContentScaleChangedEvent{
            .timestamp = timestamp, .displayId = kDisplayId});
    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED)),
        pond::platform::DisplayUsableBoundsChangedEvent{
            .timestamp = timestamp, .displayId = kDisplayId});

    SDL_Event desktopMode =
        MakeDisplayEvent(SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED);
    desktopMode.display.data1 = 1920;
    desktopMode.display.data2 = 1080;
    ExpectTranslatedEvent(
        Translate(desktopMode),
        pond::platform::DisplayDesktopModeChangedEvent{
            .timestamp = timestamp,
            .displayId = kDisplayId,
            .extent = pond::platform::ScreenExtent{1920, 1080}});

    SDL_Event currentMode =
        MakeDisplayEvent(SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED);
    currentMode.display.data1 = 3840;
    currentMode.display.data2 = 2160;
    ExpectTranslatedEvent(
        Translate(currentMode),
        pond::platform::DisplayCurrentModeChangedEvent{
            .timestamp = timestamp,
            .displayId = kDisplayId,
            .extent = pond::platform::ScreenExtent{3840, 2160}});

    desktopMode.display.data1 = 0;
    desktopMode.display.data2 = 1080;
    ExpectTranslatedEvent(
        Translate(desktopMode),
        pond::platform::DisplayDesktopModeChangedEvent{
            .timestamp = timestamp,
            .displayId = kDisplayId,
            .extent = std::nullopt});
    desktopMode.display.data1 = 1920;
    desktopMode.display.data2 = 0;
    ExpectTranslatedEvent(
        Translate(desktopMode),
        pond::platform::DisplayDesktopModeChangedEvent{
            .timestamp = timestamp,
            .displayId = kDisplayId,
            .extent = std::nullopt});

    currentMode.display.data1 = -1;
    currentMode.display.data2 = 2160;
    ExpectTranslatedEvent(
        Translate(currentMode),
        pond::platform::DisplayCurrentModeChangedEvent{
            .timestamp = timestamp,
            .displayId = kDisplayId,
            .extent = std::nullopt});
    currentMode.display.data1 = 3840;
    currentMode.display.data2 = -1;
    ExpectTranslatedEvent(
        Translate(currentMode),
        pond::platform::DisplayCurrentModeChangedEvent{
            .timestamp = timestamp,
            .displayId = kDisplayId,
            .extent = std::nullopt});
}

TEST_F(SdlEventTranslatorTests, MapsEveryDisplayOrientationIncludingUnknownValues)
{
    struct OrientationCase final
    {
        std::int32_t backend;
        pond::platform::DisplayOrientation expected;
    };
    constexpr std::array<OrientationCase, 6> kCases{{
        {SDL_ORIENTATION_UNKNOWN, pond::platform::DisplayOrientation::Unknown},
        {SDL_ORIENTATION_LANDSCAPE,
         pond::platform::DisplayOrientation::Landscape},
        {SDL_ORIENTATION_LANDSCAPE_FLIPPED,
         pond::platform::DisplayOrientation::LandscapeFlipped},
        {SDL_ORIENTATION_PORTRAIT,
         pond::platform::DisplayOrientation::Portrait},
        {SDL_ORIENTATION_PORTRAIT_FLIPPED,
         pond::platform::DisplayOrientation::PortraitFlipped},
        {0x7FFF, pond::platform::DisplayOrientation::Unknown},
    }};

    for (const OrientationCase& orientationCase : kCases)
    {
        SDL_Event event = MakeDisplayEvent(SDL_EVENT_DISPLAY_ORIENTATION);
        event.display.data1 = orientationCase.backend;
        ExpectTranslatedEvent(
            Translate(event),
            pond::platform::DisplayOrientationChangedEvent{
                .timestamp = MakeTimestamp(),
                .displayId = kDisplayId,
                .orientation = orientationCase.expected});
    }
}

TEST_F(SdlEventTranslatorTests, EnforcesTimestampRepresentationBounds)
{
    constexpr std::int64_t kMaximumTimestamp =
        std::numeric_limits<std::int64_t>::max();
    ExpectTranslatedEvent(
        Translate(MakeQuitEvent(static_cast<std::uint64_t>(kMaximumTimestamp))),
        pond::platform::QuitRequestedEvent{
            .timestamp = MakeTimestamp(kMaximumTimestamp)});
    ExpectTranslatedEvent(
        Translate(MakeQuitEvent(0)),
        pond::platform::QuitRequestedEvent{.timestamp = MakeTimestamp(0)});

    EXPECT_FALSE(
        Translate(MakeQuitEvent(static_cast<std::uint64_t>(kMaximumTimestamp) +
                                1U))
            .has_value());

    m_resolvers.windowRequests.clear();
    SDL_Event overflowedWindow = MakeWindowEvent(
        SDL_EVENT_WINDOW_CLOSE_REQUESTED,
        std::numeric_limits<std::uint64_t>::max());
    EXPECT_FALSE(Translate(overflowedWindow).has_value());
    EXPECT_TRUE(m_resolvers.windowRequests.empty());
}

TEST_F(SdlEventTranslatorTests, DropsRequiredZeroUnresolvedAndInvalidIds)
{
    m_resolvers.windowRequests.clear();
    EXPECT_FALSE(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                                  kTimestampNanoseconds, 0))
            .has_value());
    EXPECT_TRUE(m_resolvers.windowRequests.empty());

    constexpr std::uint32_t kUnknownBackendWindowId{42};
    EXPECT_FALSE(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                                  kTimestampNanoseconds,
                                  kUnknownBackendWindowId))
            .has_value());
    ASSERT_EQ(m_resolvers.windowRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.windowRequests.back(), kUnknownBackendWindowId);

    constexpr std::uint32_t kInvalidMappedBackendWindowId{43};
    m_resolvers.windows.emplace(kInvalidMappedBackendWindowId,
                                pond::platform::WindowId::Invalid());
    EXPECT_FALSE(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                                  kTimestampNanoseconds,
                                  kInvalidMappedBackendWindowId))
            .has_value());

    m_resolvers.displayRequests.clear();
    EXPECT_FALSE(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED,
                                   kTimestampNanoseconds, 0))
            .has_value());
    EXPECT_TRUE(m_resolvers.displayRequests.empty());

    constexpr std::uint32_t kUnknownBackendDisplayId{74};
    EXPECT_FALSE(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED,
                                   kTimestampNanoseconds,
                                   kUnknownBackendDisplayId))
            .has_value());
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kUnknownBackendDisplayId);

    constexpr std::uint32_t kInvalidMappedBackendDisplayId{75};
    m_resolvers.displays.emplace(kInvalidMappedBackendDisplayId,
                                 pond::platform::DisplayId::Invalid());
    EXPECT_FALSE(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED,
                                   kTimestampNanoseconds,
                                   kInvalidMappedBackendDisplayId))
            .has_value());

    const pond::platform::detail::EventTranslationContext missingResolvers{
        .context = &m_resolvers,
        .resolveWindowId = nullptr,
        .resolveDisplayId = nullptr};
    EXPECT_FALSE(pond::platform::detail::TranslateSdlEvent(
                     MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED),
                     missingResolvers)
                     .has_value());
    EXPECT_FALSE(pond::platform::detail::TranslateSdlEvent(
                     MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED),
                     missingResolvers)
                     .has_value());
}

TEST_F(SdlEventTranslatorTests,
       DropsIgnoredAndUnknownKindsWithoutResolvingIds)
{
    constexpr std::array<Uint32, 9> kIgnoredTypes{
        SDL_EVENT_WINDOW_EXPOSED,
        SDL_EVENT_WINDOW_METAL_VIEW_RESIZED,
        SDL_EVENT_WINDOW_HIT_TEST,
        SDL_EVENT_WINDOW_DESTROYED,
        SDL_EVENT_KEY_DOWN,
        SDL_EVENT_MOUSE_MOTION,
        SDL_EVENT_DROP_FILE,
        SDL_EVENT_USER,
        0xFFFF'FFFEU,
    };

    for (const Uint32 type : kIgnoredTypes)
    {
        SDL_Event event{};
        event.type = type;
        event.common.timestamp = kTimestampNanoseconds;
        event.window.windowID = kBackendWindowId;
        EXPECT_FALSE(Translate(event).has_value()) << "SDL event type " << type;
    }

    EXPECT_TRUE(m_resolvers.windowRequests.empty());
    EXPECT_TRUE(m_resolvers.displayRequests.empty());
}

TEST_F(SdlEventTranslatorTests, OwnsTranslatedDataAfterTheSdlSourceIsOverwritten)
{
    SDL_Event source = MakeWindowEvent(SDL_EVENT_WINDOW_MOVED);
    source.window.data1 = -500;
    source.window.data2 = 275;
    std::optional<pond::platform::PlatformEvent> translated = Translate(source);
    ASSERT_TRUE(translated.has_value());

    source = SDL_Event{};
    m_resolvers.windows.clear();
    m_resolvers.displays.clear();

    ExpectTranslatedEvent(
        translated,
        pond::platform::WindowMovedEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .position = {-500, 275}});
}
} // namespace
