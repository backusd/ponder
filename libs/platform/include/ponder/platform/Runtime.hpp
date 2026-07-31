#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Dialogs.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Window.hpp>

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace ponder::platform
{
namespace detail
{
#ifdef PONDER_PLATFORM_USE_MOCK_RUNTIME
class MockRuntime;
using RuntimeImpl = MockRuntime;
#else
class SdlRuntime;
using RuntimeImpl = SdlRuntime;
#endif
} // namespace detail

class Runtime final
{
public:
    [[nodiscard]] static Runtime Create();

    void Initialize(std::string_view applicationName = "ponder", std::optional<std::string_view> applicationVersion = std::nullopt,
                    std::optional<std::string_view> applicationIdentifier = std::nullopt);

    ~Runtime() noexcept;

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) noexcept;
    Runtime& operator=(Runtime&&) noexcept;

    template <typename Hint>
    void HintPush(const Hint& hint) = delete;
    template <typename Hint>
    void HintPop() = delete;
    template <typename Hint>
    void HintClear() = delete;
    template <typename Hint>
    [[nodiscard]] std::optional<Hint> HintGet() const = delete;

    [[nodiscard]] ponder::core::Result<std::string> ClipboardGetText() const;
    [[nodiscard]] ponder::core::VoidResult ClipboardSetText(std::string_view text);

    [[nodiscard]] ponder::core::Result<dialogs::DialogRequestId> DialogShowOpenFile(const dialogs::OpenFileDialogDesc& desc) noexcept;
    [[nodiscard]] ponder::core::Result<dialogs::DialogRequestId> DialogShowSaveFile(const dialogs::SaveFileDialogDesc& desc) noexcept;
    [[nodiscard]] ponder::core::Result<dialogs::DialogRequestId> DialogShowOpenFolder(const dialogs::OpenFolderDialogDesc& desc) noexcept;
    [[nodiscard]] std::size_t DialogGetPendingCount() const noexcept;
    [[nodiscard]] bool DialogHasPending() const noexcept;
    [[nodiscard]] std::vector<DialogRequestInfo> DialogGetPending() const noexcept;
    [[nodiscard]] std::optional<DialogCompletedEvent> DialogPollCompletion() noexcept;
    [[nodiscard]] std::size_t DialogGetOutstandingRequestCount() const noexcept;
    [[nodiscard]] ponder::core::VoidResult DialogShutdown() noexcept;

    [[nodiscard]] ponder::core::Timestamp TimeNow() const;
    [[nodiscard]] std::optional<PlatformEvent> EventPoll();
    [[nodiscard]] std::optional<PlatformEvent> EventWait(ponder::core::Duration timeout);
    void EventWake();

    [[nodiscard]] Window WindowCreate(const WindowDesc& desc);

    [[nodiscard]] std::vector<DisplayInfo> DisplayEnumerate();
    [[nodiscard]] ponder::core::Result<DisplayInfo> DisplayGetInfo(DisplayId id);

    [[nodiscard]] ponder::core::VoidResult MouseSetCapture(bool enabled);
    [[nodiscard]] ponder::core::Result<LogicalPoint> MouseGetGlobalPosition() const;
    void MouseSetSystemCursor(SystemCursorShape shape);
    void MouseShowCursor();
    void MouseHideCursor();
    [[nodiscard]] bool MouseIsCursorVisible() const;

    [[nodiscard]] ponder::core::VoidResult UriOpenExternal(std::string_view uri);

private:
    explicit Runtime(std::unique_ptr<detail::RuntimeImpl> impl) noexcept;

    std::unique_ptr<detail::RuntimeImpl> m_impl;
    bool m_initialized{};
};

#define PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(Type)                                                                                            \
    template <>                                                                                                                                      \
    void Runtime::HintPush<hints::Type>(const hints::Type& hint);                                                                                    \
    template <>                                                                                                                                      \
    void Runtime::HintPop<hints::Type>();                                                                                                            \
    template <>                                                                                                                                      \
    void Runtime::HintClear<hints::Type>();                                                                                                          \
    template <>                                                                                                                                      \
    [[nodiscard]] std::optional<hints::Type> Runtime::HintGet<hints::Type>() const

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(AllowAltTabWhileGrabbed);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(EventLogging);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(ImeImplementedUi);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(PollSentinel);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(QuitOnLastWindowClose);

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoAllowScreensaver);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoDoubleBuffer);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoDriver);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoForceEgl);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoMinimizeOnFocusLoss);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoSyncWindowOperations);

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenRaised);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenShown);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowAllowTopmost);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowFrameUsableWhileCursorHidden);

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseAutoCapture);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDefaultSystemCursor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickRadius);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickTime);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDpiScaleCursors);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseEmulateWarpWithRelative);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseFocusClickThrough);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseNormalSpeedScale);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeCursorVisible);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeModeCenter);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSpeedScale);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSystemScale);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeWarpMotion);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseTouchEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(PenMouseEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(PenTouchEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(TouchMouseEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MacCtrlClickEmulatesRightClick);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MacScrollMomentum);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsCloseOnAltF4);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsEnableMenuMnemonics);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsGameInput);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboard);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardExcludeHotkeys);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardInputSink);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoDisplayPriority);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandAllowLibdecor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandModeEmulation);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandPreferLibdecor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandScaleToDisplay);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoX11NetWmBypassCompositor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoX11Xrandr);
#endif

#undef PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::Runtime> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const ponder::platform::Runtime&, FormatContext& context) const
    {
        return formatter<string_view>::format("platform-runtime", context);
    }
};
} // namespace std

namespace ponder::platform
{
inline std::ostream& operator<<(std::ostream& output, const Runtime& runtime)
{
    return output << std::format("{}", runtime);
}
} // namespace ponder::platform
