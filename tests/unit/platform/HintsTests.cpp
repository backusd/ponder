#include <ponder/platform/Hints.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

namespace
{
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
} // namespace
