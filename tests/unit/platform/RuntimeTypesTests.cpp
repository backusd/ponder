#include <ponder/platform/Runtime.hpp>

#include <concepts>
#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using ponder::platform::Runtime;

template <typename Hint>
concept RuntimeHintPushContract = requires(Runtime& runtime, const Hint& hint) {
    { runtime.HintPush(hint) } -> std::same_as<void>;
};

template <typename Hint>
concept RuntimeHintPopContract = requires(Runtime& runtime) {
    { runtime.template HintPop<Hint>() } -> std::same_as<void>;
};

template <typename Hint>
concept RuntimeHintClearContract = requires(Runtime& runtime) {
    { runtime.template HintClear<Hint>() } -> std::same_as<void>;
};

template <typename Hint>
concept RuntimeHintGetContract = requires(const Runtime& runtime) {
    { runtime.template HintGet<Hint>() } -> std::same_as<std::optional<Hint>>;
};

template <typename Hint>
concept RuntimeHintContract =
    RuntimeHintPushContract<Hint> && RuntimeHintPopContract<Hint> && RuntimeHintClearContract<Hint> && RuntimeHintGetContract<Hint>;

template <typename Hint>
concept LegacyRuntimeHintContract = requires(Runtime& runtime, const Runtime& constRuntime, const Hint& hint) {
    runtime.HintPop(hint);
    runtime.HintClear(hint);
    constRuntime.HintGet(hint);
};

static_assert(!std::is_copy_constructible_v<Runtime>);
static_assert(!std::is_copy_assignable_v<Runtime>);
static_assert(std::is_nothrow_move_constructible_v<Runtime>);
static_assert(std::is_nothrow_move_assignable_v<Runtime>);
static_assert(std::is_nothrow_destructible_v<Runtime>);
static_assert(std::is_same_v<decltype(Runtime::Create(std::declval<const ponder::platform::RuntimeDesc&>())), Runtime>);
static_assert(std::is_same_v<ponder::platform::ConfigureHintsBeforeInitialization, void (*)(Runtime&)>);
#define PONDER_ASSERT_RUNTIME_HINT_CONTRACT(Type) static_assert(RuntimeHintContract<ponder::platform::hints::Type>)

PONDER_ASSERT_RUNTIME_HINT_CONTRACT(AllowAltTabWhileGrabbed);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(EventLogging);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(ImeImplementedUi);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(PollSentinel);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(QuitOnLastWindowClose);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoAllowScreensaver);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoDoubleBuffer);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoDriver);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoForceEgl);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoMinimizeOnFocusLoss);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoSyncWindowOperations);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowActivateWhenRaised);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowActivateWhenShown);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowAllowTopmost);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowFrameUsableWhileCursorHidden);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseAutoCapture);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseDefaultSystemCursor);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseDoubleClickRadius);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseDoubleClickTime);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseDpiScaleCursors);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseEmulateWarpWithRelative);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseFocusClickThrough);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseNormalSpeedScale);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseRelativeCursorVisible);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseRelativeModeCenter);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseRelativeSpeedScale);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseRelativeSystemScale);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseRelativeWarpMotion);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MouseTouchEvents);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(PenMouseEvents);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(PenTouchEvents);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(TouchMouseEvents);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MacCtrlClickEmulatesRightClick);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(MacScrollMomentum);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowsCloseOnAltF4);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowsEnableMenuMnemonics);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowsGameInput);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowsRawKeyboard);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowsRawKeyboardExcludeHotkeys);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowsRawKeyboardInputSink);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoDisplayPriority);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoWaylandAllowLibdecor);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoWaylandModeEmulation);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoWaylandPreferLibdecor);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoWaylandScaleToDisplay);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoX11NetWmBypassCompositor);
PONDER_ASSERT_RUNTIME_HINT_CONTRACT(VideoX11Xrandr);
#endif

#undef PONDER_ASSERT_RUNTIME_HINT_CONTRACT

static_assert(!RuntimeHintPushContract<int>);
static_assert(!RuntimeHintPopContract<int>);
static_assert(!RuntimeHintClearContract<int>);
static_assert(!RuntimeHintGetContract<int>);
static_assert(!LegacyRuntimeHintContract<ponder::platform::hints::MouseFocusClickThrough>);
static_assert(std::is_same_v<decltype(std::declval<const Runtime&>().ClipboardGetText()), ponder::core::Result<std::string>>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().ClipboardSetText(std::declval<std::string_view>())), ponder::core::VoidResult>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().DialogShowOpenFile(std::declval<const ponder::platform::OpenFileDialogDesc&>())),
                             ponder::platform::DialogRequestId>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().DialogShowSaveFile(std::declval<const ponder::platform::SaveFileDialogDesc&>())),
                             ponder::platform::DialogRequestId>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().DialogShowOpenFolder(std::declval<const ponder::platform::OpenFolderDialogDesc&>())),
                             ponder::platform::DialogRequestId>);
static_assert(std::is_same_v<decltype(std::declval<const Runtime&>().DialogGetPendingCount()), std::size_t>);
static_assert(std::is_same_v<decltype(std::declval<const Runtime&>().DialogHasPending()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const Runtime&>().DialogGetPending()), std::vector<ponder::platform::DialogRequestInfo>>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().DialogPollCompletion()), std::optional<ponder::platform::DialogCompletedEvent>>);
static_assert(std::is_same_v<decltype(std::declval<const Runtime&>().DialogGetOutstandingRequestCount()), std::size_t>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().DialogShutdown()), void>);
static_assert(std::is_same_v<decltype(std::declval<const Runtime&>().TimeNow()), ponder::core::Timestamp>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().EventPoll()), std::optional<ponder::platform::PlatformEvent>>);
static_assert(
    std::is_same_v<decltype(std::declval<Runtime&>().WindowCreate(std::declval<const ponder::platform::WindowDesc&>())), ponder::platform::Window>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().DisplayEnumerate()), std::vector<ponder::platform::DisplayInfo>>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().DisplayGetInfo(std::declval<ponder::platform::DisplayId>())),
                             ponder::core::Result<ponder::platform::DisplayInfo>>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().MouseSetCapture(true)), ponder::core::VoidResult>);
static_assert(
    std::is_same_v<decltype(std::declval<const Runtime&>().MouseGetGlobalPosition()), ponder::core::Result<ponder::platform::LogicalPoint>>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().MouseSetSystemCursor(ponder::platform::SystemCursorShape::Default)), void>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().MouseShowCursor()), void>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().MouseHideCursor()), void>);
static_assert(std::is_same_v<decltype(std::declval<const Runtime&>().MouseIsCursorVisible()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Runtime&>().UriOpenExternal(std::declval<std::string_view>())), ponder::core::VoidResult>);

TEST(RuntimeDescTests, ProvidesStableApplicationMetadataDefaults)
{
    const ponder::platform::RuntimeDesc desc;

    EXPECT_EQ(desc.applicationName, "ponder");
    EXPECT_FALSE(desc.applicationVersion.has_value());
    EXPECT_FALSE(desc.applicationIdentifier.has_value());
    EXPECT_EQ(desc.configureHintsBeforeInitialization, nullptr);
}

TEST(RuntimeDescTests, OwnsConfiguredApplicationMetadata)
{
    const ponder::platform::RuntimeDesc desc{.applicationName = "Molecular Workbench",
                                             .applicationVersion = std::string{"1.2.3"},
                                             .applicationIdentifier = std::string{"org.ponder.workbench"}};

    EXPECT_EQ(desc.applicationName, "Molecular Workbench");
    EXPECT_EQ(desc.applicationVersion, "1.2.3");
    EXPECT_EQ(desc.applicationIdentifier, "org.ponder.workbench");
}
} // namespace
