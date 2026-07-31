#include <ponder/core/Log.hpp>
#include <ponder/platform/Hints.hpp>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "SdlRuntime.hpp"

namespace
{
std::atomic_size_t g_platformHintErrorLogCount{};

void CapturePlatformHintError(const ponder::core::LogEntry& entry) noexcept
{
    if (entry.GetLevel() == ponder::core::LogLevel::Error && entry.GetCategory() == "platform")
    {
        g_platformHintErrorLogCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void ResetMalformedHintTestValues()
{
    static_cast<void>(SDL_ResetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH));
    static_cast<void>(SDL_ResetHint(SDL_HINT_EVENT_LOGGING));
    static_cast<void>(SDL_ResetHint(SDL_HINT_IME_IMPLEMENTED_UI));
    static_cast<void>(SDL_ResetHint(SDL_HINT_VIDEO_DRIVER));
    static_cast<void>(SDL_ResetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS));
    static_cast<void>(SDL_ResetHint(SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR));
    static_cast<void>(SDL_ResetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS));
    static_cast<void>(SDL_ResetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME));
    static_cast<void>(SDL_ResetHint(SDL_HINT_MOUSE_NORMAL_SPEED_SCALE));
}

template <typename Hint>
void ExpectMalformedHintReturnsNoValue(ponder::platform::detail::SdlRuntime& runtime, const char* name, const char* value)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE));

    const std::size_t logCountBefore = g_platformHintErrorLogCount.load(std::memory_order_relaxed);
    std::optional<Hint> result;
    EXPECT_NO_THROW(result = runtime.HintGet<Hint>());
    EXPECT_EQ(result, std::nullopt);
    ponder::core::FlushLog();
    EXPECT_GT(g_platformHintErrorLogCount.load(std::memory_order_relaxed), logCountBefore);
}

class SdlRuntimeHintGetTests : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_EQ(SDL_WasInit(0), 0U);
        ResetMalformedHintTestValues();
    }

    void TearDown() override
    {
        SDL_Quit();
        ResetMalformedHintTestValues();
    }
};

template <typename Type>
concept FormattableAndStreamable = std::formattable<Type, char> && requires(std::ostream& output, const Type& value) {
    { output << value } -> std::same_as<std::ostream&>;
};

#define PONDER_ASSERT_HINT_VALUE_CONTRACT(Type)                                                                                                      \
    static_assert(std::equality_comparable<ponder::platform::hints::Type>);                                                                          \
    static_assert(FormattableAndStreamable<ponder::platform::hints::Type>)

PONDER_ASSERT_HINT_VALUE_CONTRACT(AllowAltTabWhileGrabbed);
PONDER_ASSERT_HINT_VALUE_CONTRACT(EventLogging);
PONDER_ASSERT_HINT_VALUE_CONTRACT(ImeImplementedUi);
PONDER_ASSERT_HINT_VALUE_CONTRACT(PollSentinel);
PONDER_ASSERT_HINT_VALUE_CONTRACT(QuitOnLastWindowClose);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoAllowScreensaver);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoDoubleBuffer);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoDriver);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoForceEgl);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoMinimizeOnFocusLoss);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoSyncWindowOperations);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowActivateWhenRaised);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowActivateWhenShown);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowAllowTopmost);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowFrameUsableWhileCursorHidden);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseAutoCapture);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseDefaultSystemCursor);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseDoubleClickRadius);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseDoubleClickTime);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseDpiScaleCursors);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseEmulateWarpWithRelative);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseFocusClickThrough);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseNormalSpeedScale);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseRelativeCursorVisible);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseRelativeModeCenter);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseRelativeSpeedScale);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseRelativeSystemScale);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseRelativeWarpMotion);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MouseTouchEvents);
PONDER_ASSERT_HINT_VALUE_CONTRACT(PenMouseEvents);
PONDER_ASSERT_HINT_VALUE_CONTRACT(PenTouchEvents);
PONDER_ASSERT_HINT_VALUE_CONTRACT(TouchMouseEvents);
PONDER_ASSERT_HINT_VALUE_CONTRACT(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_ASSERT_HINT_VALUE_CONTRACT(MacCtrlClickEmulatesRightClick);
PONDER_ASSERT_HINT_VALUE_CONTRACT(MacScrollMomentum);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowsCloseOnAltF4);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowsEnableMenuMnemonics);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowsGameInput);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowsRawKeyboard);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowsRawKeyboardExcludeHotkeys);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowsRawKeyboardInputSink);
PONDER_ASSERT_HINT_VALUE_CONTRACT(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoDisplayPriority);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoWaylandAllowLibdecor);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoWaylandModeEmulation);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoWaylandPreferLibdecor);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoWaylandScaleToDisplay);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoX11NetWmBypassCompositor);
PONDER_ASSERT_HINT_VALUE_CONTRACT(VideoX11Xrandr);
#endif

#undef PONDER_ASSERT_HINT_VALUE_CONTRACT

static_assert(FormattableAndStreamable<ponder::platform::hints::EventLoggingLevel>);
static_assert(FormattableAndStreamable<ponder::platform::hints::ImeUiCapabilities>);
static_assert(FormattableAndStreamable<ponder::platform::hints::FullscreenFocusLossBehavior>);
static_assert(std::is_scoped_enum_v<ponder::platform::hints::EventLoggingLevel>);
static_assert(std::is_scoped_enum_v<ponder::platform::hints::ImeUiCapabilities>);
static_assert(std::is_scoped_enum_v<ponder::platform::hints::FullscreenFocusLossBehavior>);
static_assert(sizeof(ponder::platform::hints::EventLoggingLevel) == sizeof(std::uint8_t));
static_assert(sizeof(ponder::platform::hints::ImeUiCapabilities) == sizeof(std::uint8_t));
static_assert(sizeof(ponder::platform::hints::FullscreenFocusLossBehavior) == sizeof(std::uint8_t));

template <FormattableAndStreamable Type>
void ExpectStreamMatchesFormat(const Type& value)
{
    std::ostringstream output;
    output << value;
    EXPECT_EQ(output.str(), std::format("{}", value));
}

TEST(HintValueTests, ProvidesStableDefaultsForEveryValueCategory)
{
    using namespace ponder::platform;
    using namespace ponder::platform::hints;

    EXPECT_FALSE(AllowAltTabWhileGrabbed{}.value);
    EXPECT_EQ(EventLogging{}.value, EventLoggingLevel::Disabled);
    EXPECT_EQ(ImeImplementedUi{}.value, ImeUiCapabilities::None);
    EXPECT_TRUE(PollSentinel{}.value);
    EXPECT_TRUE(QuitOnLastWindowClose{}.value);
    EXPECT_TRUE(VideoDriver{}.value.empty());
    EXPECT_EQ(VideoMinimizeOnFocusLoss{}.value, FullscreenFocusLossBehavior::Automatic);
    EXPECT_EQ(MouseDefaultSystemCursor{}.value, SystemCursorShape::Default);
    EXPECT_EQ(MouseDoubleClickRadius{}.value, 0U);
    EXPECT_EQ(MouseDoubleClickTime{}.value, std::chrono::milliseconds{500});
    EXPECT_FLOAT_EQ(MouseNormalSpeedScale{}.value, 1.0F);
    EXPECT_FLOAT_EQ(MouseRelativeSpeedScale{}.value, 1.0F);
}

TEST(HintValueTests, OwnsAndComparesStringValues)
{
    std::string source{"dummy"};
    const ponder::platform::hints::VideoDriver driver{source};
    source.assign("changed");

    EXPECT_EQ(driver.value, "dummy");
    EXPECT_EQ(driver, ponder::platform::hints::VideoDriver{"dummy"});
    EXPECT_NE(driver, ponder::platform::hints::VideoDriver{"wayland"});
}

TEST(HintFormattingTests, FormatsEachValueCategoryAndMatchesStreamInsertion)
{
    using namespace ponder::platform;
    using namespace ponder::platform::hints;

    EXPECT_EQ(std::format("{}", AllowAltTabWhileGrabbed{true}), "1");
    EXPECT_EQ(std::format("{}", EventLogging{EventLoggingLevel::Verbose}), "2");
    EXPECT_EQ(std::format("{}", ImeImplementedUi{ImeUiCapabilities::CompositionAndCandidates}), "composition,candidates");
    EXPECT_EQ(std::format("{}", VideoMinimizeOnFocusLoss{FullscreenFocusLossBehavior::KeepFullscreen}), "0");
    EXPECT_EQ(std::format("{}", VideoDriver{"dummy"}), "dummy");
    EXPECT_EQ(std::format("{}", MouseDefaultSystemCursor{SystemCursorShape::Pointer}), "11");
    EXPECT_EQ(std::format("{}", MouseDoubleClickRadius{12U}), "12");
    EXPECT_EQ(std::format("{}", MouseDoubleClickTime{std::chrono::milliseconds{750}}), "750");
    EXPECT_EQ(std::format("{}", MouseNormalSpeedScale{1.25F}), "1.25");

    ExpectStreamMatchesFormat(EventLogging{EventLoggingLevel::Common});
    ExpectStreamMatchesFormat(ImeImplementedUi{ImeUiCapabilities::Candidates});
    ExpectStreamMatchesFormat(VideoDriver{"dummy"});
    ExpectStreamMatchesFormat(MouseDefaultSystemCursor{SystemCursorShape::Wait});
}

TEST(HintFormattingTests, UsesExplicitFallbacksForForgedEnumValues)
{
    using namespace ponder::platform::hints;

    EXPECT_EQ(std::format("{}", static_cast<EventLoggingLevel>(0xFF)), "<invalid>");
    EXPECT_EQ(std::format("{}", static_cast<ImeUiCapabilities>(0xFF)), "<invalid>");
    EXPECT_EQ(std::format("{}", static_cast<FullscreenFocusLossBehavior>(0xFF)), "<invalid>");
}

TEST_F(SdlRuntimeHintGetTests, LogsAndReturnsNoValueForEveryMalformedDecoderFamily)
{
    using namespace ponder::platform::hints;

    g_platformHintErrorLogCount.store(0, std::memory_order_relaxed);
    const ponder::core::ScopedMinimumLogLevel minimumLogLevel{ponder::core::LogLevel::Error};
    const ponder::core::ScopedLogSinkHandler logSink{CapturePlatformHintError};
    ponder::platform::detail::SdlRuntime runtime;

    ExpectMalformedHintReturnsNoValue<MouseFocusClickThrough>(runtime, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "");
    ExpectMalformedHintReturnsNoValue<EventLogging>(runtime, SDL_HINT_EVENT_LOGGING, "unexpected");
    ExpectMalformedHintReturnsNoValue<ImeImplementedUi>(runtime, SDL_HINT_IME_IMPLEMENTED_UI, "unexpected");
    ExpectMalformedHintReturnsNoValue<VideoDriver>(runtime, SDL_HINT_VIDEO_DRIVER, "");
    ExpectMalformedHintReturnsNoValue<VideoMinimizeOnFocusLoss>(runtime, SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "unexpected");
    ExpectMalformedHintReturnsNoValue<MouseDefaultSystemCursor>(runtime, SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR, "unexpected");
    ExpectMalformedHintReturnsNoValue<MouseDoubleClickRadius>(runtime, SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS, "-1");
    ExpectMalformedHintReturnsNoValue<MouseDoubleClickTime>(runtime, SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, "-1");
    ExpectMalformedHintReturnsNoValue<MouseNormalSpeedScale>(runtime, SDL_HINT_MOUSE_NORMAL_SPEED_SCALE, "unexpected");
}

TEST_F(SdlRuntimeHintGetTests, PreservesSdlBooleanInterpretationForNonEmptyValues)
{
    ponder::platform::detail::SdlRuntime runtime;

    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "enabled", SDL_HINT_OVERRIDE));
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>(), ponder::platform::hints::MouseFocusClickThrough{true});

    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "FALSE", SDL_HINT_OVERRIDE));
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>(), ponder::platform::hints::MouseFocusClickThrough{false});

    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "0-with-suffix", SDL_HINT_OVERRIDE));
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>(), ponder::platform::hints::MouseFocusClickThrough{false});
}
} // namespace
