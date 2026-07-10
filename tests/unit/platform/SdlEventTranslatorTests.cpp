#include <ponder/platform/PlatformEvent.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pen.h>
#include <SDL3/SDL_touch.h>
#include <SDL3/SDL_video.h>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "SdlEventTranslator.hpp"

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

[[nodiscard]] std::optional<pond::platform::WindowId> ResolveWindowId(void* context,
                                                                      std::uint32_t backendWindowId)
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

[[nodiscard]] SDL_Event MakeQuitEvent(std::uint64_t timestamp = kTimestampNanoseconds)
{
    SDL_Event event{};
    event.quit.type = SDL_EVENT_QUIT;
    event.quit.timestamp = timestamp;
    return event;
}

[[nodiscard]] SDL_Event MakeWindowEvent(SDL_EventType type,
                                        std::uint64_t timestamp = kTimestampNanoseconds,
                                        std::uint32_t backendWindowId = kBackendWindowId)
{
    SDL_Event event{};
    event.window.type = type;
    event.window.timestamp = timestamp;
    event.window.windowID = backendWindowId;
    return event;
}

[[nodiscard]] SDL_Event MakeDisplayEvent(SDL_EventType type,
                                         std::uint64_t timestamp = kTimestampNanoseconds,
                                         std::uint32_t backendDisplayId = kBackendDisplayId)
{
    SDL_Event event{};
    event.display.type = type;
    event.display.timestamp = timestamp;
    event.display.displayID = backendDisplayId;
    return event;
}

[[nodiscard]] SDL_Event MakeKeyboardEvent(SDL_EventType type, SDL_Scancode scancode,
                                          SDL_Keycode key, SDL_Keymod modifiers = SDL_KMOD_NONE,
                                          bool repeat = false,
                                          std::uint32_t backendWindowId = kBackendWindowId,
                                          std::uint64_t timestamp = kTimestampNanoseconds)
{
    SDL_Event event{};
    event.key.type = type;
    event.key.timestamp = timestamp;
    event.key.windowID = backendWindowId;
    event.key.scancode = scancode;
    event.key.key = key;
    event.key.mod = modifiers;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    event.key.repeat = repeat;
    return event;
}

[[nodiscard]] SDL_Event MakeMouseMotionEvent(float x = 12.25F, float y = -8.5F,
                                             float relativeX = 3.75F, float relativeY = -1.5F,
                                             std::uint32_t backendWindowId = kBackendWindowId,
                                             std::uint64_t timestamp = kTimestampNanoseconds,
                                             SDL_MouseID which = 0)
{
    SDL_Event event{};
    event.motion.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.timestamp = timestamp;
    event.motion.windowID = backendWindowId;
    event.motion.which = which;
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = relativeX;
    event.motion.yrel = relativeY;
    return event;
}

[[nodiscard]] SDL_Event MakeMouseButtonEvent(SDL_EventType type,
                                             std::uint8_t button = SDL_BUTTON_LEFT,
                                             float x = 12.25F, float y = -8.5F,
                                             std::uint32_t backendWindowId = kBackendWindowId,
                                             std::uint64_t timestamp = kTimestampNanoseconds,
                                             SDL_MouseID which = 0)
{
    SDL_Event event{};
    event.button.type = type;
    event.button.timestamp = timestamp;
    event.button.windowID = backendWindowId;
    event.button.which = which;
    event.button.button = button;
    event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.x = x;
    event.button.y = y;
    return event;
}

[[nodiscard]] SDL_Event MakeMouseWheelEvent(
    float horizontal = 1.25F, float vertical = -2.5F,
    SDL_MouseWheelDirection direction = SDL_MOUSEWHEEL_NORMAL, float x = 12.25F, float y = -8.5F,
    std::uint32_t backendWindowId = kBackendWindowId,
    std::uint64_t timestamp = kTimestampNanoseconds, SDL_MouseID which = 0)
{
    SDL_Event event{};
    event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.timestamp = timestamp;
    event.wheel.windowID = backendWindowId;
    event.wheel.which = which;
    event.wheel.x = horizontal;
    event.wheel.y = vertical;
    event.wheel.direction = direction;
    event.wheel.mouse_x = x;
    event.wheel.mouse_y = y;
    return event;
}

[[nodiscard]] SDL_Event MakeTextInputEvent(const char* text,
                                           std::uint32_t backendWindowId = kBackendWindowId,
                                           std::uint64_t timestamp = kTimestampNanoseconds)
{
    SDL_Event event{};
    event.text.type = SDL_EVENT_TEXT_INPUT;
    event.text.timestamp = timestamp;
    event.text.windowID = backendWindowId;
    event.text.text = text;
    return event;
}

[[nodiscard]] SDL_Event MakeTextCompositionEvent(const char* text, std::int32_t start,
                                                 std::int32_t length,
                                                 std::uint32_t backendWindowId = kBackendWindowId,
                                                 std::uint64_t timestamp = kTimestampNanoseconds)
{
    SDL_Event event{};
    event.edit.type = SDL_EVENT_TEXT_EDITING;
    event.edit.timestamp = timestamp;
    event.edit.windowID = backendWindowId;
    event.edit.text = text;
    event.edit.start = start;
    event.edit.length = length;
    return event;
}

[[nodiscard]] SDL_Event MakeDropEvent(SDL_EventType type, const char* data = nullptr,
                                      const char* source = nullptr, float x = 12.25F,
                                      float y = -8.5F,
                                      std::uint32_t backendWindowId = kBackendWindowId,
                                      std::uint64_t timestamp = kTimestampNanoseconds)
{
    SDL_Event event{};
    event.drop.type = type;
    event.drop.timestamp = timestamp;
    event.drop.windowID = backendWindowId;
    event.drop.x = x;
    event.drop.y = y;
    event.drop.source = source;
    event.drop.data = data;
    return event;
}

template <typename Event>
void ExpectTranslatedEvent(const std::optional<pond::platform::PlatformEvent>& translated,
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
        m_context = pond::platform::detail::EventTranslationContext{&m_resolvers, ResolveWindowId,
                                                                    ResolveDisplayId};
    }

    [[nodiscard]] std::optional<pond::platform::PlatformEvent> Translate(const SDL_Event& event)
    {
        return pond::platform::detail::TranslateSdlEvent(event, m_context);
    }

    ResolverState m_resolvers;
    pond::platform::detail::EventTranslationContext m_context;
};

TEST_F(SdlEventTranslatorTests, TranslatesQuitAndEveryWindowLifecycleSignal)
{
    const pond::platform::PlatformTimestamp timestamp = MakeTimestamp();

    ExpectTranslatedEvent(Translate(MakeQuitEvent()),
                          pond::platform::QuitRequestedEvent{.timestamp = timestamp});
    EXPECT_TRUE(m_resolvers.windowRequests.empty());
    EXPECT_TRUE(m_resolvers.displayRequests.empty());

    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED)),
        pond::platform::WindowCloseRequestedEvent{.timestamp = timestamp, .windowId = kWindowId});
    ExpectTranslatedEvent(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED)),
                          pond::platform::WindowFocusChangedEvent{
                              .timestamp = timestamp, .windowId = kWindowId, .focused = true});
    ExpectTranslatedEvent(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_FOCUS_LOST)),
                          pond::platform::WindowFocusChangedEvent{
                              .timestamp = timestamp, .windowId = kWindowId, .focused = false});
    ExpectTranslatedEvent(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_SHOWN)),
                          pond::platform::WindowVisibilityChangedEvent{
                              .timestamp = timestamp, .windowId = kWindowId, .visible = true});
    ExpectTranslatedEvent(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_HIDDEN)),
                          pond::platform::WindowVisibilityChangedEvent{
                              .timestamp = timestamp, .windowId = kWindowId, .visible = false});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MINIMIZED)),
        pond::platform::WindowStateChangedEvent{.timestamp = timestamp,
                                                .windowId = kWindowId,
                                                .state = pond::platform::WindowState::Minimized});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MAXIMIZED)),
        pond::platform::WindowStateChangedEvent{.timestamp = timestamp,
                                                .windowId = kWindowId,
                                                .state = pond::platform::WindowState::Maximized});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_RESTORED)),
        pond::platform::WindowStateChangedEvent{.timestamp = timestamp,
                                                .windowId = kWindowId,
                                                .state = pond::platform::WindowState::Normal});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_ENTER_FULLSCREEN)),
        pond::platform::WindowPresentationChangedEvent{
            .timestamp = timestamp,
            .windowId = kWindowId,
            .presentation = pond::platform::WindowPresentation::DesktopFullscreen});
    ExpectTranslatedEvent(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)),
                          pond::platform::WindowPresentationChangedEvent{
                              .timestamp = timestamp,
                              .windowId = kWindowId,
                              .presentation = pond::platform::WindowPresentation::Windowed});
    ExpectTranslatedEvent(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)),
                          pond::platform::WindowDisplayScaleChangedEvent{.timestamp = timestamp,
                                                                         .windowId = kWindowId});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MOUSE_ENTER)),
        pond::platform::WindowPointerEnteredEvent{.timestamp = timestamp, .windowId = kWindowId});
    ExpectTranslatedEvent(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_MOUSE_LEAVE)),
        pond::platform::WindowPointerLeftEvent{.timestamp = timestamp, .windowId = kWindowId});
}

TEST_F(SdlEventTranslatorTests, TranslatesKnownPhysicalUnicodeAndEveryModifierBit)
{
    constexpr SDL_Keymod kAllModifiers = static_cast<SDL_Keymod>(
        SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT | SDL_KMOD_LEVEL5 | SDL_KMOD_LCTRL | SDL_KMOD_RCTRL |
        SDL_KMOD_LALT | SDL_KMOD_RALT | SDL_KMOD_LGUI | SDL_KMOD_RGUI | SDL_KMOD_NUM |
        SDL_KMOD_CAPS | SDL_KMOD_MODE | SDL_KMOD_SCROLL | 0x0008U);
    constexpr pond::platform::KeyModifiers kExpectedModifiers =
        pond::platform::KeyModifiers::LeftShift | pond::platform::KeyModifiers::RightShift |
        pond::platform::KeyModifiers::Level5Shift | pond::platform::KeyModifiers::LeftControl |
        pond::platform::KeyModifiers::RightControl | pond::platform::KeyModifiers::LeftAlt |
        pond::platform::KeyModifiers::RightAlt | pond::platform::KeyModifiers::LeftSuper |
        pond::platform::KeyModifiers::RightSuper | pond::platform::KeyModifiers::NumLock |
        pond::platform::KeyModifiers::CapsLock | pond::platform::KeyModifiers::AltGraph |
        pond::platform::KeyModifiers::ScrollLock;

    SDL_Event event = MakeKeyboardEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_Q,
                                        static_cast<SDL_Keycode>(U'\u00E9'), kAllModifiers, true);
    event.key.raw = 0xFFFFU;

    ExpectTranslatedEvent(Translate(event),
                          pond::platform::KeyboardKeyEvent{
                              .timestamp = MakeTimestamp(),
                              .windowId = kWindowId,
                              .physicalKey = pond::platform::PhysicalKey::Q,
                              .logicalKey = pond::platform::LogicalKey::FromCharacter(U'\u00E9'),
                              .modifiers = kExpectedModifiers,
                              .pressed = true,
                              .repeat = true});
}

TEST_F(SdlEventTranslatorTests, TranslatesNamedAndUnknownKeyValues)
{
    ExpectTranslatedEvent(
        Translate(MakeKeyboardEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_KP_7, SDLK_7)),
        pond::platform::KeyboardKeyEvent{
            .timestamp = MakeTimestamp(),
            .windowId = kWindowId,
            .physicalKey = pond::platform::PhysicalKey::Keypad7,
            .logicalKey = pond::platform::LogicalKey::FromNamed(pond::platform::NamedKey::Keypad7),
            .modifiers = pond::platform::KeyModifiers::None,
            .pressed = true,
            .repeat = false});

    ExpectTranslatedEvent(
        Translate(
            MakeKeyboardEvent(SDL_EVENT_KEY_DOWN, static_cast<SDL_Scancode>(0x7FFF), SDLK_UNKNOWN)),
        pond::platform::KeyboardKeyEvent{.timestamp = MakeTimestamp(),
                                         .windowId = kWindowId,
                                         .physicalKey = pond::platform::PhysicalKey::Unknown,
                                         .logicalKey = pond::platform::LogicalKey::Unknown(),
                                         .modifiers = pond::platform::KeyModifiers::None,
                                         .pressed = true,
                                         .repeat = false});

    ExpectTranslatedEvent(
        Translate(MakeKeyboardEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A,
                                    static_cast<SDL_Keycode>(0x0011'0000U))),
        pond::platform::KeyboardKeyEvent{.timestamp = MakeTimestamp(),
                                         .windowId = kWindowId,
                                         .physicalKey = pond::platform::PhysicalKey::A,
                                         .logicalKey = pond::platform::LogicalKey::Unknown(),
                                         .modifiers = pond::platform::KeyModifiers::None,
                                         .pressed = true,
                                         .repeat = false});
}

TEST_F(SdlEventTranslatorTests, EnforcesKeyTypeAndDownStateConsistency)
{
    ExpectTranslatedEvent(
        Translate(
            MakeKeyboardEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_LEFT, SDLK_LEFT, SDL_KMOD_LCTRL)),
        pond::platform::KeyboardKeyEvent{.timestamp = MakeTimestamp(),
                                         .windowId = kWindowId,
                                         .physicalKey = pond::platform::PhysicalKey::ArrowLeft,
                                         .logicalKey = pond::platform::LogicalKey::FromNamed(
                                             pond::platform::NamedKey::ArrowLeft),
                                         .modifiers = pond::platform::KeyModifiers::LeftControl,
                                         .pressed = false,
                                         .repeat = false});

    SDL_Event inconsistentDown = MakeKeyboardEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A, SDLK_A);
    inconsistentDown.key.down = false;
    EXPECT_FALSE(Translate(inconsistentDown).has_value());

    SDL_Event inconsistentUp = MakeKeyboardEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_A, SDLK_A);
    inconsistentUp.key.down = true;
    EXPECT_FALSE(Translate(inconsistentUp).has_value());
}

TEST_F(SdlEventTranslatorTests, TranslatesFiniteMouseMotionFromMouseTouchAndPenSources)
{
    constexpr std::array<SDL_MouseID, 3> kSources{0, SDL_TOUCH_MOUSEID, SDL_PEN_MOUSEID};
    for (const SDL_MouseID source : kSources)
    {
        ExpectTranslatedEvent(
            Translate(MakeMouseMotionEvent(-27.5F, 14.25F, 3.5F, -6.75F, kBackendWindowId,
                                           kTimestampNanoseconds, source)),
            pond::platform::MouseMotionEvent{.timestamp = MakeTimestamp(),
                                             .windowId = kWindowId,
                                             .position = {-27.5F, 14.25F},
                                             .relativeMovement = {3.5F, -6.75F}});
    }
}

TEST_F(SdlEventTranslatorTests, MapsEveryMouseButtonAndEnforcesPressStateConsistency)
{
    struct ButtonCase final
    {
        std::uint8_t backend;
        pond::platform::MouseButton expected;
    };

    constexpr std::array<ButtonCase, 6> kButtonCases{{
        {SDL_BUTTON_LEFT, pond::platform::MouseButton::Left},
        {SDL_BUTTON_RIGHT, pond::platform::MouseButton::Right},
        {SDL_BUTTON_MIDDLE, pond::platform::MouseButton::Middle},
        {SDL_BUTTON_X1, pond::platform::MouseButton::X1},
        {SDL_BUTTON_X2, pond::platform::MouseButton::X2},
        {0xFFU, pond::platform::MouseButton::Unknown},
    }};

    for (const ButtonCase& buttonCase : kButtonCases)
    {
        ExpectTranslatedEvent(Translate(MakeMouseButtonEvent(
                                  SDL_EVENT_MOUSE_BUTTON_DOWN, buttonCase.backend, -9.5F, 4.25F,
                                  kBackendWindowId, kTimestampNanoseconds, SDL_PEN_MOUSEID)),
                              pond::platform::MouseButtonEvent{.timestamp = MakeTimestamp(),
                                                               .windowId = kWindowId,
                                                               .position = {-9.5F, 4.25F},
                                                               .button = buttonCase.expected,
                                                               .pressed = true});
    }

    ExpectTranslatedEvent(
        Translate(MakeMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_RIGHT, 5.5F, -2.25F,
                                       kBackendWindowId, kTimestampNanoseconds, SDL_TOUCH_MOUSEID)),
        pond::platform::MouseButtonEvent{.timestamp = MakeTimestamp(),
                                         .windowId = kWindowId,
                                         .position = {5.5F, -2.25F},
                                         .button = pond::platform::MouseButton::Right,
                                         .pressed = false});

    SDL_Event inconsistentDown = MakeMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
    inconsistentDown.button.down = false;
    EXPECT_FALSE(Translate(inconsistentDown).has_value());

    SDL_Event inconsistentUp = MakeMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP);
    inconsistentUp.button.down = true;
    EXPECT_FALSE(Translate(inconsistentUp).has_value());
}

TEST_F(SdlEventTranslatorTests, NormalizesMouseWheelDirectionAndRejectsUnknownDirections)
{
    ExpectTranslatedEvent(
        Translate(MakeMouseWheelEvent(2.25F, -3.5F, SDL_MOUSEWHEEL_NORMAL, -8.75F, 11.5F,
                                      kBackendWindowId, kTimestampNanoseconds, SDL_TOUCH_MOUSEID)),
        pond::platform::MouseWheelEvent{.timestamp = MakeTimestamp(),
                                        .windowId = kWindowId,
                                        .position = {-8.75F, 11.5F},
                                        .horizontal = 2.25F,
                                        .vertical = -3.5F});

    ExpectTranslatedEvent(
        Translate(MakeMouseWheelEvent(4.5F, -6.25F, SDL_MOUSEWHEEL_FLIPPED, 7.0F, -12.0F,
                                      kBackendWindowId, kTimestampNanoseconds, SDL_PEN_MOUSEID)),
        pond::platform::MouseWheelEvent{.timestamp = MakeTimestamp(),
                                        .windowId = kWindowId,
                                        .position = {7.0F, -12.0F},
                                        .horizontal = -4.5F,
                                        .vertical = 6.25F});

    EXPECT_FALSE(
        Translate(MakeMouseWheelEvent(1.0F, 1.0F, static_cast<SDL_MouseWheelDirection>(0x7FFF)))
            .has_value());
}

TEST_F(SdlEventTranslatorTests, DropsNonFiniteMousePayloadValuesBeforeResolvingTargets)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto expectRejected = [this](const SDL_Event& event)
    {
        EXPECT_FALSE(Translate(event).has_value());
    };

    m_resolvers.windowRequests.clear();

    SDL_Event event = MakeMouseMotionEvent();
    event.motion.x = nan;
    expectRejected(event);
    event = MakeMouseMotionEvent();
    event.motion.y = infinity;
    expectRejected(event);
    event = MakeMouseMotionEvent();
    event.motion.xrel = -infinity;
    expectRejected(event);
    event = MakeMouseMotionEvent();
    event.motion.yrel = nan;
    expectRejected(event);

    event = MakeMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
    event.button.x = nan;
    expectRejected(event);
    event = MakeMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP);
    event.button.y = infinity;
    expectRejected(event);

    event = MakeMouseWheelEvent();
    event.wheel.mouse_x = -infinity;
    expectRejected(event);
    event = MakeMouseWheelEvent();
    event.wheel.mouse_y = nan;
    expectRejected(event);
    event = MakeMouseWheelEvent();
    event.wheel.x = infinity;
    expectRejected(event);
    event = MakeMouseWheelEvent();
    event.wheel.y = nan;
    expectRejected(event);

    EXPECT_TRUE(m_resolvers.windowRequests.empty());
}

TEST_F(SdlEventTranslatorTests, PreservesTargetlessInputAndDropsStaleNonzeroTargets)
{
    m_resolvers.windowRequests.clear();

    auto key = Translate(
        MakeKeyboardEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A, SDLK_A, SDL_KMOD_NONE, false, 0));
    ASSERT_TRUE(key.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::KeyboardKeyEvent>(*key));
    EXPECT_FALSE(std::get<pond::platform::KeyboardKeyEvent>(*key).windowId.has_value());

    auto text = Translate(MakeTextInputEvent("targetless", 0));
    ASSERT_TRUE(text.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::TextInputEvent>(*text));
    EXPECT_FALSE(std::get<pond::platform::TextInputEvent>(*text).windowId.has_value());

    auto composition = Translate(MakeTextCompositionEvent("targetless", 0, 0, 0));
    ASSERT_TRUE(composition.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::TextCompositionEvent>(*composition));
    EXPECT_FALSE(std::get<pond::platform::TextCompositionEvent>(*composition).windowId.has_value());

    SDL_Event motionEvent = MakeMouseMotionEvent();
    motionEvent.motion.windowID = 0;
    auto motion = Translate(motionEvent);
    ASSERT_TRUE(motion.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::MouseMotionEvent>(*motion));
    EXPECT_FALSE(std::get<pond::platform::MouseMotionEvent>(*motion).windowId.has_value());

    SDL_Event buttonEvent = MakeMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
    buttonEvent.button.windowID = 0;
    auto button = Translate(buttonEvent);
    ASSERT_TRUE(button.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::MouseButtonEvent>(*button));
    EXPECT_FALSE(std::get<pond::platform::MouseButtonEvent>(*button).windowId.has_value());

    SDL_Event wheelEvent = MakeMouseWheelEvent();
    wheelEvent.wheel.windowID = 0;
    auto wheel = Translate(wheelEvent);
    ASSERT_TRUE(wheel.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::MouseWheelEvent>(*wheel));
    EXPECT_FALSE(std::get<pond::platform::MouseWheelEvent>(*wheel).windowId.has_value());
    EXPECT_TRUE(m_resolvers.windowRequests.empty());

    EXPECT_FALSE(Translate(MakeKeyboardEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A, SDLK_A,
                                             SDL_KMOD_NONE, false, 999))
                     .has_value());
    EXPECT_FALSE(Translate(MakeTextInputEvent("stale", 999)).has_value());
    EXPECT_FALSE(Translate(MakeTextCompositionEvent("stale", 0, 0, 999)).has_value());

    motionEvent = MakeMouseMotionEvent();
    motionEvent.motion.windowID = 999;
    EXPECT_FALSE(Translate(motionEvent).has_value());
    buttonEvent = MakeMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP);
    buttonEvent.button.windowID = 999;
    EXPECT_FALSE(Translate(buttonEvent).has_value());
    wheelEvent = MakeMouseWheelEvent();
    wheelEvent.wheel.windowID = 999;
    EXPECT_FALSE(Translate(wheelEvent).has_value());
    EXPECT_EQ(m_resolvers.windowRequests,
              (std::vector<std::uint32_t>{999, 999, 999, 999, 999, 999}));
}

TEST_F(SdlEventTranslatorTests, OwnsAndValidatesUtf8TextInput)
{
    std::string source{"caf\xC3\xA9"};
    auto translated = Translate(MakeTextInputEvent(source.c_str()));
    ASSERT_TRUE(translated.has_value());
    source.assign("overwritten");

    ExpectTranslatedEvent(translated, pond::platform::TextInputEvent{.timestamp = MakeTimestamp(),
                                                                     .windowId = kWindowId,
                                                                     .text = "caf\xC3\xA9"});
    ExpectTranslatedEvent(Translate(MakeTextInputEvent("")),
                          pond::platform::TextInputEvent{
                              .timestamp = MakeTimestamp(), .windowId = kWindowId, .text = ""});

    EXPECT_FALSE(Translate(MakeTextInputEvent(nullptr)).has_value());
    const char invalidUtf8[]{static_cast<char>(0xC0), static_cast<char>(0xAF), '\0'};
    EXPECT_FALSE(Translate(MakeTextInputEvent(invalidUtf8)).has_value());
}

TEST_F(SdlEventTranslatorTests, TranslatesAndOwnsEveryDropEvent)
{
    std::string source{"source-app"};
    std::string path{"C:/tmp/molecule.sdf"};
    std::string text{"ligand \xF0\x9F\xA7\xAA", 11};

    std::optional<pond::platform::PlatformEvent> begin = Translate(MakeDropEvent(
        SDL_EVENT_DROP_BEGIN, nullptr, source.c_str(), 0.0F, 0.0F, kBackendWindowId, 100));
    std::optional<pond::platform::PlatformEvent> position = Translate(MakeDropEvent(
        SDL_EVENT_DROP_POSITION, nullptr, source.c_str(), -4.5F, 8.25F, kBackendWindowId, 200));
    std::optional<pond::platform::PlatformEvent> file = Translate(MakeDropEvent(
        SDL_EVENT_DROP_FILE, path.c_str(), source.c_str(), 1.5F, 2.25F, kBackendWindowId, 300));
    std::optional<pond::platform::PlatformEvent> droppedText = Translate(MakeDropEvent(
        SDL_EVENT_DROP_TEXT, text.c_str(), source.c_str(), 3.5F, 4.25F, kBackendWindowId, 400));
    std::optional<pond::platform::PlatformEvent> complete = Translate(MakeDropEvent(
        SDL_EVENT_DROP_COMPLETE, nullptr, source.c_str(), 5.5F, 6.25F, kBackendWindowId, 500));

    source.assign("overwritten-source");
    path.assign("overwritten-path");
    text.assign("overwritten-text");

    ExpectTranslatedEvent(begin,
                          pond::platform::DropBeginEvent{
                              .timestamp = MakeTimestamp(100),
                              .windowId = kWindowId,
                              .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectTranslatedEvent(position,
                          pond::platform::DropPositionEvent{
                              .timestamp = MakeTimestamp(200),
                              .windowId = kWindowId,
                              .position = {-4.5F, 8.25F},
                              .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectTranslatedEvent(file, pond::platform::DroppedFileEvent{
                                    .timestamp = MakeTimestamp(300),
                                    .windowId = kWindowId,
                                    .path = std::filesystem::path{"C:/tmp/molecule.sdf"},
                                    .position = {1.5F, 2.25F},
                                    .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectTranslatedEvent(droppedText,
                          pond::platform::DroppedTextEvent{
                              .timestamp = MakeTimestamp(400),
                              .windowId = kWindowId,
                              .text = std::string{"ligand \xF0\x9F\xA7\xAA", 11},
                              .position = {3.5F, 4.25F},
                              .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectTranslatedEvent(complete,
                          pond::platform::DropCompleteEvent{
                              .timestamp = MakeTimestamp(500),
                              .windowId = kWindowId,
                              .position = {5.5F, 6.25F},
                              .sourceApplication = std::optional<std::string>{"source-app"}});
}

TEST_F(SdlEventTranslatorTests, PreservesTargetlessDropsAndDoesNotRequireSequenceState)
{
    m_resolvers.windowRequests.clear();

    ExpectTranslatedEvent(
        Translate(MakeDropEvent(SDL_EVENT_DROP_BEGIN, nullptr, nullptr, 0.0F, 0.0F, 0)),
        pond::platform::DropBeginEvent{.timestamp = MakeTimestamp(),
                                       .windowId = std::nullopt,
                                       .sourceApplication = std::nullopt});
    ExpectTranslatedEvent(
        Translate(MakeDropEvent(SDL_EVENT_DROP_COMPLETE, nullptr, nullptr, 7.5F, 9.25F, 0)),
        pond::platform::DropCompleteEvent{.timestamp = MakeTimestamp(),
                                          .windowId = std::nullopt,
                                          .position = {7.5F, 9.25F},
                                          .sourceApplication = std::nullopt});
    ExpectTranslatedEvent(
        Translate(MakeDropEvent(SDL_EVENT_DROP_FILE, "targetless.sdf", nullptr, 1.0F, 2.0F, 0)),
        pond::platform::DroppedFileEvent{.timestamp = MakeTimestamp(),
                                         .windowId = std::nullopt,
                                         .path = std::filesystem::path{"targetless.sdf"},
                                         .position = {1.0F, 2.0F},
                                         .sourceApplication = std::nullopt});
    EXPECT_TRUE(m_resolvers.windowRequests.empty());

    constexpr std::uint32_t kUnknownBackendWindowId{999};
    EXPECT_FALSE(Translate(MakeDropEvent(SDL_EVENT_DROP_TEXT, "stale", nullptr, 1.0F, 2.0F,
                                         kUnknownBackendWindowId))
                     .has_value());
    ASSERT_EQ(m_resolvers.windowRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.windowRequests.back(), kUnknownBackendWindowId);
}

TEST_F(SdlEventTranslatorTests, RejectsMalformedDropPayloadsBeforeResolvingTargets)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const char invalidUtf8[]{static_cast<char>(0xC0), static_cast<char>(0xAF), '\0'};

    m_resolvers.windowRequests.clear();
    EXPECT_FALSE(Translate(MakeDropEvent(SDL_EVENT_DROP_FILE)).has_value());
    EXPECT_FALSE(Translate(MakeDropEvent(SDL_EVENT_DROP_TEXT)).has_value());
    EXPECT_FALSE(Translate(MakeDropEvent(SDL_EVENT_DROP_FILE, invalidUtf8)).has_value());
    EXPECT_FALSE(Translate(MakeDropEvent(SDL_EVENT_DROP_TEXT, invalidUtf8)).has_value());
    EXPECT_FALSE(
        Translate(MakeDropEvent(SDL_EVENT_DROP_POSITION, nullptr, invalidUtf8)).has_value());
    EXPECT_FALSE(
        Translate(MakeDropEvent(SDL_EVENT_DROP_POSITION, nullptr, nullptr, nan, 1.0F)).has_value());
    EXPECT_FALSE(Translate(MakeDropEvent(SDL_EVENT_DROP_COMPLETE, nullptr, nullptr, 1.0F, infinity))
                     .has_value());
    EXPECT_TRUE(m_resolvers.windowRequests.empty());
}

TEST_F(SdlEventTranslatorTests, OwnsCompositionTextAndDistinguishesSelectionAvailability)
{
    std::string source{"composition"};
    auto translated = Translate(MakeTextCompositionEvent(source.c_str(), 3, 2));
    ASSERT_TRUE(translated.has_value());
    source.assign("overwritten");
    ExpectTranslatedEvent(translated, pond::platform::TextCompositionEvent{
                                          .timestamp = MakeTimestamp(),
                                          .windowId = kWindowId,
                                          .text = "composition",
                                          .selection = pond::platform::TextCompositionRange{3, 2}});

    ExpectTranslatedEvent(Translate(MakeTextCompositionEvent("", 0, 0)),
                          pond::platform::TextCompositionEvent{
                              .timestamp = MakeTimestamp(),
                              .windowId = kWindowId,
                              .text = "",
                              .selection = pond::platform::TextCompositionRange{0, 0}});

    constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kUnavailableRanges{
        {{-1, -1}, {-1, 0}, {0, -1}, {-2, 4}}};
    for (const auto [start, length] : kUnavailableRanges)
    {
        ExpectTranslatedEvent(Translate(MakeTextCompositionEvent("pending", start, length)),
                              pond::platform::TextCompositionEvent{.timestamp = MakeTimestamp(),
                                                                   .windowId = kWindowId,
                                                                   .text = "pending",
                                                                   .selection = std::nullopt});
    }

    EXPECT_FALSE(Translate(MakeTextCompositionEvent(nullptr, 0, 0)).has_value());
    const char invalidUtf8[]{static_cast<char>(0xC0), static_cast<char>(0xAF), '\0'};
    EXPECT_FALSE(Translate(MakeTextCompositionEvent(invalidUtf8, 0, 0)).has_value());
}

TEST_F(SdlEventTranslatorTests, PreservesSignedMovementAndDistinctSizeUnits)
{
    SDL_Event moved = MakeWindowEvent(SDL_EVENT_WINDOW_MOVED);
    moved.window.data1 = std::numeric_limits<Sint32>::min();
    moved.window.data2 = std::numeric_limits<Sint32>::max();
    ExpectTranslatedEvent(
        Translate(moved),
        pond::platform::WindowMovedEvent{.timestamp = MakeTimestamp(),
                                         .windowId = kWindowId,
                                         .position = {std::numeric_limits<std::int32_t>::min(),
                                                      std::numeric_limits<std::int32_t>::max()}});

    SDL_Event logical = MakeWindowEvent(SDL_EVENT_WINDOW_RESIZED);
    logical.window.data1 = 640;
    logical.window.data2 = 480;
    ExpectTranslatedEvent(
        Translate(logical),
        pond::platform::WindowLogicalSizeChangedEvent{
            .timestamp = MakeTimestamp(), .windowId = kWindowId, .logicalSize = {640, 480}});

    SDL_Event pixel = MakeWindowEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
    pixel.window.data1 = 1280;
    pixel.window.data2 = 960;
    ExpectTranslatedEvent(Translate(pixel),
                          pond::platform::WindowPixelSizeChangedEvent{.timestamp = MakeTimestamp(),
                                                                      .windowId = kWindowId,
                                                                      .pixelSize = {1280, 960}});

    logical.window.data1 = 0;
    logical.window.data2 = 0;
    ExpectTranslatedEvent(
        Translate(logical),
        pond::platform::WindowLogicalSizeChangedEvent{
            .timestamp = MakeTimestamp(), .windowId = kWindowId, .logicalSize = {0, 0}});
    pixel.window.data1 = 0;
    pixel.window.data2 = 0;
    ExpectTranslatedEvent(Translate(pixel),
                          pond::platform::WindowPixelSizeChangedEvent{.timestamp = MakeTimestamp(),
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
    ExpectTranslatedEvent(Translate(event),
                          pond::platform::WindowDisplayChangedEvent{.timestamp = MakeTimestamp(),
                                                                    .windowId = kWindowId,
                                                                    .displayId = kDisplayId});
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kBackendDisplayId);

    m_resolvers.displayRequests.clear();
    event.window.data1 = 0;
    ExpectTranslatedEvent(Translate(event),
                          pond::platform::WindowDisplayChangedEvent{.timestamp = MakeTimestamp(),
                                                                    .windowId = kWindowId,
                                                                    .displayId = std::nullopt});
    EXPECT_TRUE(m_resolvers.displayRequests.empty());

    constexpr std::uint32_t kUnresolvedDisplayId{82};
    event.window.data1 = static_cast<Sint32>(kUnresolvedDisplayId);
    ExpectTranslatedEvent(Translate(event),
                          pond::platform::WindowDisplayChangedEvent{.timestamp = MakeTimestamp(),
                                                                    .windowId = kWindowId,
                                                                    .displayId = std::nullopt});
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kUnresolvedDisplayId);

    constexpr std::uint32_t kHighBitBackendDisplayId{0xF123'4567U};
    constexpr pond::platform::DisplayId kHighBitDisplayId{99};
    m_resolvers.displays.emplace(kHighBitBackendDisplayId, kHighBitDisplayId);
    m_resolvers.displayRequests.clear();
    event.window.data1 = std::bit_cast<std::int32_t>(kHighBitBackendDisplayId);
    ExpectTranslatedEvent(
        Translate(event),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = MakeTimestamp(), .windowId = kWindowId, .displayId = kHighBitDisplayId});
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kHighBitBackendDisplayId);

    constexpr std::uint32_t kInvalidMappedDisplayBackendId{83};
    m_resolvers.displays.emplace(kInvalidMappedDisplayBackendId,
                                 pond::platform::DisplayId::Invalid());
    event.window.data1 = static_cast<Sint32>(kInvalidMappedDisplayBackendId);
    ExpectTranslatedEvent(Translate(event),
                          pond::platform::WindowDisplayChangedEvent{.timestamp = MakeTimestamp(),
                                                                    .windowId = kWindowId,
                                                                    .displayId = std::nullopt});
}

TEST_F(SdlEventTranslatorTests, TranslatesEveryDisplaySignalAndOptionalModeExtent)
{
    const pond::platform::PlatformTimestamp timestamp = MakeTimestamp();

    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_ADDED)),
        pond::platform::DisplayAddedEvent{.timestamp = timestamp, .displayId = kDisplayId});
    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_REMOVED)),
        pond::platform::DisplayRemovedEvent{.timestamp = timestamp, .displayId = kDisplayId});
    ExpectTranslatedEvent(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED)),
        pond::platform::DisplayMovedEvent{.timestamp = timestamp, .displayId = kDisplayId});
    ExpectTranslatedEvent(Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED)),
                          pond::platform::DisplayContentScaleChangedEvent{.timestamp = timestamp,
                                                                          .displayId = kDisplayId});
    ExpectTranslatedEvent(Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED)),
                          pond::platform::DisplayUsableBoundsChangedEvent{.timestamp = timestamp,
                                                                          .displayId = kDisplayId});

    SDL_Event desktopMode = MakeDisplayEvent(SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED);
    desktopMode.display.data1 = 1920;
    desktopMode.display.data2 = 1080;
    ExpectTranslatedEvent(Translate(desktopMode),
                          pond::platform::DisplayDesktopModeChangedEvent{
                              .timestamp = timestamp,
                              .displayId = kDisplayId,
                              .extent = pond::platform::ScreenExtent{1920, 1080}});

    SDL_Event currentMode = MakeDisplayEvent(SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED);
    currentMode.display.data1 = 3840;
    currentMode.display.data2 = 2160;
    ExpectTranslatedEvent(Translate(currentMode),
                          pond::platform::DisplayCurrentModeChangedEvent{
                              .timestamp = timestamp,
                              .displayId = kDisplayId,
                              .extent = pond::platform::ScreenExtent{3840, 2160}});

    desktopMode.display.data1 = 0;
    desktopMode.display.data2 = 1080;
    ExpectTranslatedEvent(Translate(desktopMode),
                          pond::platform::DisplayDesktopModeChangedEvent{.timestamp = timestamp,
                                                                         .displayId = kDisplayId,
                                                                         .extent = std::nullopt});
    desktopMode.display.data1 = 1920;
    desktopMode.display.data2 = 0;
    ExpectTranslatedEvent(Translate(desktopMode),
                          pond::platform::DisplayDesktopModeChangedEvent{.timestamp = timestamp,
                                                                         .displayId = kDisplayId,
                                                                         .extent = std::nullopt});

    currentMode.display.data1 = -1;
    currentMode.display.data2 = 2160;
    ExpectTranslatedEvent(Translate(currentMode),
                          pond::platform::DisplayCurrentModeChangedEvent{.timestamp = timestamp,
                                                                         .displayId = kDisplayId,
                                                                         .extent = std::nullopt});
    currentMode.display.data1 = 3840;
    currentMode.display.data2 = -1;
    ExpectTranslatedEvent(Translate(currentMode),
                          pond::platform::DisplayCurrentModeChangedEvent{.timestamp = timestamp,
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
        {SDL_ORIENTATION_LANDSCAPE, pond::platform::DisplayOrientation::Landscape},
        {SDL_ORIENTATION_LANDSCAPE_FLIPPED, pond::platform::DisplayOrientation::LandscapeFlipped},
        {SDL_ORIENTATION_PORTRAIT, pond::platform::DisplayOrientation::Portrait},
        {SDL_ORIENTATION_PORTRAIT_FLIPPED, pond::platform::DisplayOrientation::PortraitFlipped},
        {0x7FFF, pond::platform::DisplayOrientation::Unknown},
    }};

    for (const OrientationCase& orientationCase : kCases)
    {
        SDL_Event event = MakeDisplayEvent(SDL_EVENT_DISPLAY_ORIENTATION);
        event.display.data1 = orientationCase.backend;
        ExpectTranslatedEvent(Translate(event), pond::platform::DisplayOrientationChangedEvent{
                                                    .timestamp = MakeTimestamp(),
                                                    .displayId = kDisplayId,
                                                    .orientation = orientationCase.expected});
    }
}

TEST_F(SdlEventTranslatorTests, EnforcesTimestampRepresentationBounds)
{
    constexpr std::int64_t kMaximumTimestamp = std::numeric_limits<std::int64_t>::max();
    ExpectTranslatedEvent(
        Translate(MakeQuitEvent(static_cast<std::uint64_t>(kMaximumTimestamp))),
        pond::platform::QuitRequestedEvent{.timestamp = MakeTimestamp(kMaximumTimestamp)});
    ExpectTranslatedEvent(Translate(MakeQuitEvent(0)),
                          pond::platform::QuitRequestedEvent{.timestamp = MakeTimestamp(0)});

    EXPECT_FALSE(
        Translate(MakeQuitEvent(static_cast<std::uint64_t>(kMaximumTimestamp) + 1U)).has_value());

    m_resolvers.windowRequests.clear();
    SDL_Event overflowedWindow = MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                                                 std::numeric_limits<std::uint64_t>::max());
    EXPECT_FALSE(Translate(overflowedWindow).has_value());

    SDL_Event overflowedMouse = MakeMouseMotionEvent();
    overflowedMouse.motion.timestamp = std::numeric_limits<std::uint64_t>::max();
    EXPECT_FALSE(Translate(overflowedMouse).has_value());
    EXPECT_TRUE(m_resolvers.windowRequests.empty());
}

TEST_F(SdlEventTranslatorTests, DropsRequiredZeroUnresolvedAndInvalidIds)
{
    m_resolvers.windowRequests.clear();
    EXPECT_FALSE(
        Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, kTimestampNanoseconds, 0))
            .has_value());
    EXPECT_TRUE(m_resolvers.windowRequests.empty());

    constexpr std::uint32_t kUnknownBackendWindowId{42};
    EXPECT_FALSE(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, kTimestampNanoseconds,
                                           kUnknownBackendWindowId))
                     .has_value());
    ASSERT_EQ(m_resolvers.windowRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.windowRequests.back(), kUnknownBackendWindowId);

    constexpr std::uint32_t kInvalidMappedBackendWindowId{43};
    m_resolvers.windows.emplace(kInvalidMappedBackendWindowId, pond::platform::WindowId::Invalid());
    EXPECT_FALSE(Translate(MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, kTimestampNanoseconds,
                                           kInvalidMappedBackendWindowId))
                     .has_value());

    m_resolvers.displayRequests.clear();
    EXPECT_FALSE(
        Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED, kTimestampNanoseconds, 0)).has_value());
    EXPECT_TRUE(m_resolvers.displayRequests.empty());

    constexpr std::uint32_t kUnknownBackendDisplayId{74};
    EXPECT_FALSE(Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED, kTimestampNanoseconds,
                                            kUnknownBackendDisplayId))
                     .has_value());
    ASSERT_EQ(m_resolvers.displayRequests.size(), 1U);
    EXPECT_EQ(m_resolvers.displayRequests.back(), kUnknownBackendDisplayId);

    constexpr std::uint32_t kInvalidMappedBackendDisplayId{75};
    m_resolvers.displays.emplace(kInvalidMappedBackendDisplayId,
                                 pond::platform::DisplayId::Invalid());
    EXPECT_FALSE(Translate(MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED, kTimestampNanoseconds,
                                            kInvalidMappedBackendDisplayId))
                     .has_value());

    const pond::platform::detail::EventTranslationContext missingResolvers{
        .context = &m_resolvers, .resolveWindowId = nullptr, .resolveDisplayId = nullptr};
    EXPECT_FALSE(pond::platform::detail::TranslateSdlEvent(
                     MakeWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED), missingResolvers)
                     .has_value());
    EXPECT_FALSE(pond::platform::detail::TranslateSdlEvent(
                     MakeDisplayEvent(SDL_EVENT_DISPLAY_MOVED), missingResolvers)
                     .has_value());
    EXPECT_FALSE(pond::platform::detail::TranslateSdlEvent(MakeMouseMotionEvent(), missingResolvers)
                     .has_value());
}

TEST_F(SdlEventTranslatorTests, DropsIgnoredAndUnknownKindsWithoutResolvingIds)
{
    constexpr std::array<Uint32, 7> kIgnoredTypes{
        SDL_EVENT_WINDOW_EXPOSED,
        SDL_EVENT_WINDOW_METAL_VIEW_RESIZED,
        SDL_EVENT_WINDOW_HIT_TEST,
        SDL_EVENT_WINDOW_DESTROYED,
        SDL_EVENT_TEXT_EDITING_CANDIDATES,
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

    ExpectTranslatedEvent(translated, pond::platform::WindowMovedEvent{.timestamp = MakeTimestamp(),
                                                                       .windowId = kWindowId,
                                                                       .position = {-500, 275}});
}
} // namespace
