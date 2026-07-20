#include <ponder/core/PonderException.hpp>
#include <ponder/platform/HintManager.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <SDL3/SDL_error.h>
#include <chrono>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "HintManagerBackend.hpp"

namespace
{
class FakeHintBackend final : public pond::platform::detail::IHintBackend
{
public:
    [[nodiscard]] bool IsMainThread() const noexcept override
    {
        return mainThread;
    }

    [[nodiscard]] bool HasInitializedSubsystems() const noexcept override
    {
        return initializedSubsystems;
    }

    [[nodiscard]] const char* GetHint(const char* name) const noexcept override
    {
        calls.emplace_back(std::string{"get:"} + name);
        const auto iterator = hints.find(name);
        return iterator != hints.end() ? iterator->second.c_str() : nullptr;
    }

    [[nodiscard]] bool SetHintOverride(const char* name, const char* value) noexcept override
    {
        calls.emplace_back(std::string{"set:"} + name + "=" + value);
        if (setFailuresRemaining > 0)
        {
            --setFailuresRemaining;
            static_cast<void>(SDL_SetError("synthetic hint set failure"));
            return false;
        }
        hints[name] = value;
        return true;
    }

    [[nodiscard]] bool ResetHint(const char* name) noexcept override
    {
        calls.emplace_back(std::string{"reset:"} + name);
        if (resetFailuresRemaining > 0)
        {
            --resetFailuresRemaining;
            static_cast<void>(SDL_SetError("synthetic hint reset failure"));
            return false;
        }
        hints.erase(name);
        return true;
    }

    bool mainThread{true};
    bool initializedSubsystems{};
    int setFailuresRemaining{};
    int resetFailuresRemaining{};
    std::unordered_map<std::string, std::string> hints;
    mutable std::vector<std::string> calls;
};

class HintManagerTests : public testing::Test
{
public:
    HintManagerTests() : manager(pond::platform::detail::HintManagerAccess::Create(backend)) {}

protected:
    void SetUp() override
    {
        static_cast<void>(SDL_ClearError());
    }

    void TearDown() override
    {
        static_cast<void>(SDL_ClearError());
    }

    FakeHintBackend backend;
    pond::platform::HintManager manager;
};

template <typename Hint>
void ExpectBooleanHintRoundTrip(pond::platform::HintManager& manager, const Hint& hint)
{
    ASSERT_TRUE(manager.PushHint(hint).HasValue());

    auto result = manager.GetHint(Hint{});
    ASSERT_TRUE(result.HasValue());
    EXPECT_TRUE(result.GetValue().value);
}

template <typename Hint>
void ExpectHintClear(pond::platform::HintManager& manager, const Hint& hint)
{
    EXPECT_TRUE(manager.ClearHints(hint).HasValue());
}

template <typename Hint>
void ExpectHintFormatting(const Hint& hint)
{
    const std::string formatted = std::format("{}", hint);
    std::ostringstream output;
    output << hint;
    EXPECT_EQ(output.str(), formatted);
}

TEST_F(HintManagerTests, MaintainsIndependentValueStacks)
{
    using namespace pond::platform::hints;

    backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"] = "original";

    ASSERT_TRUE(manager.PushHint(MouseFocusClickThrough{false}).HasValue());
    ASSERT_TRUE(manager.PushHint(MouseAutoCapture{true}).HasValue());
    ASSERT_TRUE(manager.PushHint(MouseFocusClickThrough{true}).HasValue());
    EXPECT_EQ(backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"], "1");

    ASSERT_TRUE(manager.PopHint(MouseFocusClickThrough{}).HasValue());
    EXPECT_EQ(backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"], "0");
    EXPECT_EQ(backend.hints["SDL_MOUSE_AUTO_CAPTURE"], "1");

    ASSERT_TRUE(manager.ClearHints(MouseFocusClickThrough{}).HasValue());
    EXPECT_EQ(backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"], "original");
    EXPECT_EQ(backend.hints["SDL_MOUSE_AUTO_CAPTURE"], "1");

    ASSERT_TRUE(manager.ClearHints(MouseAutoCapture{}).HasValue());
    EXPECT_FALSE(backend.hints.contains("SDL_MOUSE_AUTO_CAPTURE"));
    EXPECT_TRUE(manager.ClearHints(MouseAutoCapture{}).HasValue());
}

TEST_F(HintManagerTests, SupportsEveryBooleanHintAvailableOnTheHost)
{
    using namespace pond::platform::hints;

    const auto booleanHints = std::tuple{
        AllowAltTabWhileGrabbed{true},
        PollSentinel{true},
        QuitOnLastWindowClose{true},
        VideoAllowScreensaver{true},
        VideoDoubleBuffer{true},
        VideoForceEgl{true},
        VideoSyncWindowOperations{true},
        WindowActivateWhenRaised{true},
        WindowActivateWhenShown{true},
        WindowAllowTopmost{true},
        WindowFrameUsableWhileCursorHidden{true},
        MouseAutoCapture{true},
        MouseDpiScaleCursors{true},
        MouseEmulateWarpWithRelative{true},
        MouseFocusClickThrough{true},
        MouseRelativeCursorVisible{true},
        MouseRelativeModeCenter{true},
        MouseRelativeSystemScale{true},
        MouseRelativeWarpMotion{true},
        MouseTouchEvents{true},
        PenMouseEvents{true},
        PenTouchEvents{true},
        TouchMouseEvents{true},
        TrackpadIsTouchOnly{true},
#if defined(__APPLE__)
        MacCtrlClickEmulatesRightClick{true},
        MacScrollMomentum{true},
        VideoMacFullscreenSpaces{true},
#endif
#if defined(_WIN32)
        WindowsCloseOnAltF4{true},
        WindowsEnableMenuMnemonics{true},
        WindowsGameInput{true},
        WindowsRawKeyboard{true},
        WindowsRawKeyboardExcludeHotkeys{true},
        WindowsRawKeyboardInputSink{true},
        WindowsRawMouseNoLegacy{true},
#endif
#if defined(__linux__)
        VideoWaylandAllowLibdecor{true},
        VideoWaylandModeEmulation{true},
        VideoWaylandPreferLibdecor{true},
        VideoWaylandScaleToDisplay{true},
        VideoX11NetWmBypassCompositor{true},
        VideoX11Xrandr{true},
#endif
    };

    std::apply(
        [this](const auto&... hint)
        {
            (ExpectBooleanHintRoundTrip(manager, hint), ...);
        },
        booleanHints);
    EXPECT_EQ(backend.hints.size(), std::tuple_size_v<decltype(booleanHints)>);

    std::apply(
        [this](const auto&... hint)
        {
            (ExpectHintClear(manager, hint), ...);
        },
        booleanHints);
    EXPECT_TRUE(backend.hints.empty());
}

TEST_F(HintManagerTests, RoundTripsEverySupportedValueCategory)
{
    using namespace std::chrono_literals;
    using namespace pond::platform::hints;
    using pond::platform::SystemCursorShape;

    ASSERT_TRUE(manager.PushHint(MouseFocusClickThrough{true}).HasValue());
    ASSERT_TRUE(manager.PushHint(MouseDoubleClickRadius{7U}).HasValue());
    ASSERT_TRUE(manager.PushHint(MouseDoubleClickTime{650ms}).HasValue());
    ASSERT_TRUE(manager.PushHint(MouseRelativeSpeedScale{1.25F}).HasValue());
    ASSERT_TRUE(manager.PushHint(VideoDriver{"dummy,offscreen"}).HasValue());
    ASSERT_TRUE(manager.PushHint(MouseDefaultSystemCursor{SystemCursorShape::Pointer}).HasValue());
    ASSERT_TRUE(manager.PushHint(EventLogging{EventLoggingLevel::Verbose}).HasValue());
    ASSERT_TRUE(
        manager.PushHint(ImeImplementedUi{ImeUiCapabilities::CompositionAndCandidates}).HasValue());
    ASSERT_TRUE(
        manager.PushHint(VideoMinimizeOnFocusLoss{FullscreenFocusLossBehavior::KeepFullscreen})
            .HasValue());

    EXPECT_TRUE(manager.GetHint(MouseFocusClickThrough{}).GetValue().value);
    EXPECT_EQ(manager.GetHint(MouseDoubleClickRadius{}).GetValue().value, 7U);
    EXPECT_EQ(manager.GetHint(MouseDoubleClickTime{}).GetValue().value, 650ms);
    EXPECT_FLOAT_EQ(manager.GetHint(MouseRelativeSpeedScale{}).GetValue().value, 1.25F);
    EXPECT_EQ(manager.GetHint(VideoDriver{}).GetValue().value, "dummy,offscreen");
    EXPECT_EQ(manager.GetHint(MouseDefaultSystemCursor{}).GetValue().value,
              SystemCursorShape::Pointer);
    EXPECT_EQ(manager.GetHint(EventLogging{}).GetValue().value, EventLoggingLevel::Verbose);
    EXPECT_EQ(manager.GetHint(ImeImplementedUi{}).GetValue().value,
              ImeUiCapabilities::CompositionAndCandidates);
    EXPECT_EQ(manager.GetHint(VideoMinimizeOnFocusLoss{}).GetValue().value,
              FullscreenFocusLossBehavior::KeepFullscreen);
}

TEST(HintFormattingTests, FormatsAndStreamsEveryHintAvailableOnTheHost)
{
    using namespace std::chrono_literals;
    using namespace pond::platform::hints;
    using pond::platform::SystemCursorShape;

    EXPECT_EQ(std::format("{}", MouseFocusClickThrough{true}), "1");
    EXPECT_EQ(std::format("{}", MouseDoubleClickTime{650ms}), "650");
    EXPECT_EQ(std::format("{}", EventLoggingLevel::Verbose), "2");
    EXPECT_EQ(std::format("{}", ImeUiCapabilities::CompositionAndCandidates),
              "composition,candidates");
    EXPECT_EQ(std::format("{}", FullscreenFocusLossBehavior::Automatic), "auto");
    EXPECT_EQ(std::format("{}", MouseDefaultSystemCursor{SystemCursorShape::Pointer}), "11");

    ExpectHintFormatting(EventLoggingLevel::Common);
    ExpectHintFormatting(ImeUiCapabilities::Candidates);
    ExpectHintFormatting(FullscreenFocusLossBehavior::Minimize);

    const auto hints = std::tuple{
        AllowAltTabWhileGrabbed{},
        EventLogging{},
        ImeImplementedUi{},
        PollSentinel{},
        QuitOnLastWindowClose{},
        VideoAllowScreensaver{},
        VideoDoubleBuffer{},
        VideoDriver{"dummy"},
        VideoForceEgl{},
        VideoMinimizeOnFocusLoss{},
        VideoSyncWindowOperations{},
        WindowActivateWhenRaised{},
        WindowActivateWhenShown{},
        WindowAllowTopmost{},
        WindowFrameUsableWhileCursorHidden{},
        MouseAutoCapture{},
        MouseDefaultSystemCursor{},
        MouseDoubleClickRadius{7U},
        MouseDoubleClickTime{650ms},
        MouseDpiScaleCursors{},
        MouseEmulateWarpWithRelative{},
        MouseFocusClickThrough{},
        MouseNormalSpeedScale{},
        MouseRelativeCursorVisible{},
        MouseRelativeModeCenter{},
        MouseRelativeSpeedScale{},
        MouseRelativeSystemScale{},
        MouseRelativeWarpMotion{},
        MouseTouchEvents{},
        PenMouseEvents{},
        PenTouchEvents{},
        TouchMouseEvents{},
        TrackpadIsTouchOnly{},
#if defined(__APPLE__)
        MacCtrlClickEmulatesRightClick{},
        MacScrollMomentum{},
        VideoMacFullscreenSpaces{},
#endif
#if defined(_WIN32)
        WindowsCloseOnAltF4{},
        WindowsEnableMenuMnemonics{},
        WindowsGameInput{},
        WindowsRawKeyboard{},
        WindowsRawKeyboardExcludeHotkeys{},
        WindowsRawKeyboardInputSink{},
        WindowsRawMouseNoLegacy{},
#endif
#if defined(__linux__)
        VideoDisplayPriority{"DP-1"},
        VideoWaylandAllowLibdecor{},
        VideoWaylandModeEmulation{},
        VideoWaylandPreferLibdecor{},
        VideoWaylandScaleToDisplay{},
        VideoX11NetWmBypassCompositor{},
        VideoX11Xrandr{},
#endif
    };

    std::apply(
        [](const auto&... hint)
        {
            (ExpectHintFormatting(hint), ...);
        },
        hints);
}
TEST_F(HintManagerTests, RejectsInvalidValues)
{
    using namespace std::chrono_literals;
    using namespace pond::platform::hints;
    using pond::platform::PlatformErrorCode;
    using pond::platform::SystemCursorShape;

    const auto expectInvalid = [](const auto& result)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(PlatformErrorCode::InvalidArgument));
    };

    expectInvalid(
        manager.PushHint(MouseDoubleClickRadius{std::numeric_limits<std::uint32_t>::max()}));
    expectInvalid(manager.PushHint(MouseDoubleClickTime{-1ms}));
    expectInvalid(
        manager.PushHint(MouseRelativeSpeedScale{std::numeric_limits<float>::infinity()}));
    expectInvalid(manager.PushHint(VideoDriver{}));
    expectInvalid(manager.PushHint(VideoDriver{std::string{"bad\0value", 9}}));
    expectInvalid(manager.PushHint(MouseDefaultSystemCursor{
        static_cast<SystemCursorShape>(std::numeric_limits<std::uint8_t>::max())}));
    expectInvalid(manager.PushHint(
        EventLogging{static_cast<EventLoggingLevel>(std::numeric_limits<std::uint8_t>::max())}));
    expectInvalid(manager.PushHint(ImeImplementedUi{
        static_cast<ImeUiCapabilities>(std::numeric_limits<std::uint8_t>::max())}));
}

TEST_F(HintManagerTests, EnforcesBeforeInitializationMutations)
{
    using namespace pond::platform::hints;
    using pond::platform::PlatformErrorCode;

    ASSERT_TRUE(manager.PushHint(ImeImplementedUi{ImeUiCapabilities::Composition}).HasValue());
    backend.initializedSubsystems = true;

    const auto pushResult = manager.PushHint(VideoDriver{"dummy"});
    const auto popResult = manager.PopHint(ImeImplementedUi{});
    const auto clearResult = manager.ClearHints(ImeImplementedUi{});
    for (const pond::core::VoidResult* result : {&pushResult, &popResult, &clearResult})
    {
        ASSERT_FALSE(result->HasValue());
        EXPECT_EQ(result->GetError().GetCode(),
                  pond::platform::ToErrorCode(PlatformErrorCode::InvalidArgument));
        EXPECT_NE(result->GetError().GetMessage().find("before SDL is initialized"),
                  std::string_view::npos);
    }

    EXPECT_EQ(manager.GetHint(ImeImplementedUi{}).GetValue().value, ImeUiCapabilities::Composition);
    EXPECT_TRUE(manager.PushHint(WindowActivateWhenShown{false}).HasValue());

    backend.initializedSubsystems = false;
    EXPECT_TRUE(manager.PopHint(ImeImplementedUi{}).HasValue());
}

TEST_F(HintManagerTests, EnforcesOneShotHints)
{
    using namespace pond::platform::hints;
    using pond::platform::PlatformErrorCode;

    backend.hints["SDL_VIDEO_ALLOW_SCREENSAVER"] = "0";
    auto result = manager.PushHint(VideoAllowScreensaver{true});
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(PlatformErrorCode::InvalidArgument));

    backend.hints.erase("SDL_VIDEO_ALLOW_SCREENSAVER");
    ASSERT_TRUE(manager.PushHint(VideoAllowScreensaver{true}).HasValue());
    result = manager.PushHint(VideoAllowScreensaver{false});
    ASSERT_FALSE(result.HasValue());

    ASSERT_TRUE(manager.PopHint(VideoAllowScreensaver{}).HasValue());
    EXPECT_FALSE(backend.hints.contains("SDL_VIDEO_ALLOW_SCREENSAVER"));
    result = manager.PushHint(VideoAllowScreensaver{false});
    ASSERT_FALSE(result.HasValue());
}

TEST_F(HintManagerTests, PreservesStacksWhenBackendMutationsFail)
{
    using namespace pond::platform::hints;
    using pond::platform::PlatformErrorCode;

    backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"] = "original";
    backend.setFailuresRemaining = 1;
    auto result = manager.PushHint(MouseFocusClickThrough{false});
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(PlatformErrorCode::BackendFailure));
    EXPECT_EQ(backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"], "original");

    ASSERT_TRUE(manager.PushHint(MouseFocusClickThrough{false}).HasValue());
    backend.setFailuresRemaining = 1;
    result = manager.PopHint(MouseFocusClickThrough{});
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"], "0");
    ASSERT_TRUE(manager.PopHint(MouseFocusClickThrough{}).HasValue());
    EXPECT_EQ(backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"], "original");

    ASSERT_TRUE(manager.PushHint(MouseAutoCapture{false}).HasValue());
    backend.resetFailuresRemaining = 1;
    result = manager.ClearHints(MouseAutoCapture{});
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(backend.hints["SDL_MOUSE_AUTO_CAPTURE"], "0");
    ASSERT_TRUE(manager.ClearHints(MouseAutoCapture{}).HasValue());
    EXPECT_FALSE(backend.hints.contains("SDL_MOUSE_AUTO_CAPTURE"));
}

TEST(HintManagerLifetimeTests, RestoresPresentEmptyAndAbsentValuesAtDestruction)
{
    using namespace pond::platform::hints;

    FakeHintBackend backend;
    backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"] = "";

    {
        auto manager = pond::platform::detail::HintManagerAccess::Create(backend);
        ASSERT_TRUE(manager.PushHint(MouseFocusClickThrough{true}).HasValue());
        ASSERT_TRUE(manager.PushHint(MouseAutoCapture{false}).HasValue());
    }

    ASSERT_TRUE(backend.hints.contains("SDL_MOUSE_FOCUS_CLICKTHROUGH"));
    EXPECT_TRUE(backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"].empty());
    EXPECT_FALSE(backend.hints.contains("SDL_MOUSE_AUTO_CAPTURE"));
}

TEST_F(HintManagerTests, ReportsMissingMalformedAndWrongThreadQueries)
{
    using namespace pond::platform::hints;
    using pond::platform::PlatformErrorCode;

    auto missing = manager.GetHint(MouseFocusClickThrough{});
    ASSERT_FALSE(missing.HasValue());
    EXPECT_EQ(missing.GetError().GetCode(),
              pond::platform::ToErrorCode(PlatformErrorCode::NotFound));

    backend.hints["SDL_MOUSE_DOUBLE_CLICK_RADIUS"] = "-1";
    auto malformed = manager.GetHint(MouseDoubleClickRadius{});
    ASSERT_FALSE(malformed.HasValue());
    EXPECT_EQ(malformed.GetError().GetCode(),
              pond::platform::ToErrorCode(PlatformErrorCode::BackendFailure));

    backend.hints["SDL_MOUSE_FOCUS_CLICKTHROUGH"] = "enabled";
    EXPECT_TRUE(manager.GetHint(MouseFocusClickThrough{}).GetValue().value);

    backend.mainThread = false;
    EXPECT_THROW(static_cast<void>(manager.GetHint(MouseFocusClickThrough{})),
                 pond::core::PonderException);
}
} // namespace