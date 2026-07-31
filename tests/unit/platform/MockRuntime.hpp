#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Dialogs.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Runtime.hpp>

#include <any>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <format>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IPlatformWindowBackend.hpp"
#include "PlatformCommon.hpp"
#include "RuntimeChildRegistry.hpp"
#include "WindowRegistry.hpp"

namespace ponder::platform::detail
{
class MockRuntimeControl final
{
public:
    MockRuntimeControl();

    bool failInitialization{};
    bool failWindowCreation{};
    bool globalMouseSupported{true};
    ponder::core::Timestamp currentTime{};
    LogicalPoint globalMousePosition{};
    float windowPixelDensity{1.0F};
    float windowDisplayScale{1.0F};
    std::optional<DisplayId> windowDisplayId;
    std::optional<ponder::core::Error> mouseCaptureError;
    std::optional<ponder::core::Error> globalMousePositionError;
    std::optional<ponder::core::Error> externalUriError;
    std::optional<ponder::core::Error> clipboardGetError;
    std::optional<ponder::core::Error> clipboardSetError;
    std::string clipboardText;
    std::vector<DisplayInfo> displays;
    std::deque<PlatformEvent> events;
    std::deque<DialogOutcome> dialogOutcomesOnShow;
    std::exception_ptr dialogOperationException;
    std::vector<std::string> openedUris;
    std::optional<std::string> lastApplicationName;
    std::optional<std::string> lastApplicationVersion;
    std::optional<std::string> lastApplicationIdentifier;
    std::optional<SystemCursorShape> selectedCursor;
    std::size_t constructionCount{};
    std::size_t initializationAttemptCount{};
    std::size_t successfulInitializationCount{};
    std::size_t destructionCount{};
    std::size_t windowCreationCount{};
    std::size_t windowDestructionCount{};
    std::size_t liveWindowCount{};
    bool runtimeActive{};
    bool cursorVisible{true};
    bool mouseCaptured{};
};

class ScopedMockRuntimeBinding final
{
public:
    explicit ScopedMockRuntimeBinding(MockRuntimeControl& control) noexcept;
    ~ScopedMockRuntimeBinding() noexcept;

    ScopedMockRuntimeBinding(const ScopedMockRuntimeBinding&) = delete;
    ScopedMockRuntimeBinding& operator=(const ScopedMockRuntimeBinding&) = delete;
    ScopedMockRuntimeBinding(ScopedMockRuntimeBinding&&) = delete;
    ScopedMockRuntimeBinding& operator=(ScopedMockRuntimeBinding&&) = delete;

private:
    MockRuntimeControl* m_control{};
    MockRuntimeControl* m_previous{};
};

struct MockWindowState final
{
    std::uint32_t backendId{};
    std::string title;
    BackendWindowPosition position;
    BackendWindowLogicalSize logicalSize;
    BackendWindowPixelSize pixelSize;
    std::optional<BackendWindowLogicalSize> minimumSize;
    BackendWindowProperties properties;
    WindowGraphicsCompatibility graphicsCompatibility{WindowGraphicsCompatibility::Default};
    std::optional<BackendTextInputArea> textInputArea;
    bool highPixelDensity{};
    bool textInputActive{};
    bool mouseGrabbed{};
    bool relativeMouseMode{};
};

class MockWindowBackend final : public IPlatformWindowBackend
{
public:
    explicit MockWindowBackend(MockRuntimeControl& control) noexcept;
    ~MockWindowBackend() noexcept override;

    MockWindowBackend(const MockWindowBackend&) = delete;
    MockWindowBackend& operator=(const MockWindowBackend&) = delete;
    MockWindowBackend(MockWindowBackend&&) = delete;
    MockWindowBackend& operator=(MockWindowBackend&&) = delete;

    [[nodiscard]] BackendWindowHandle Create(const BackendWindowCreateDesc& desc) override;
    void Destroy(BackendWindowHandle window) noexcept override;
    [[nodiscard]] std::uint32_t GetId(BackendWindowHandle window) override;
    [[nodiscard]] std::string GetTitle(BackendWindowHandle window) override;
    void SetTitle(BackendWindowHandle window, std::string_view title) override;
    [[nodiscard]] BackendWindowPosition GetPosition(BackendWindowHandle window) override;
    void SetPosition(BackendWindowHandle window, BackendWindowPosition position) override;
    [[nodiscard]] BackendWindowLogicalSize GetSize(BackendWindowHandle window) override;
    [[nodiscard]] BackendWindowPixelSize GetSizeInPixels(BackendWindowHandle window) override;
    void SetSize(BackendWindowHandle window, BackendWindowLogicalSize size) override;
    void SetMinimumSize(BackendWindowHandle window, BackendWindowLogicalSize size) override;
    void Show(BackendWindowHandle window) override;
    void Hide(BackendWindowHandle window) override;
    [[nodiscard]] BackendWindowProperties GetProperties(BackendWindowHandle window) override;
    void SetFullscreenModeToDesktop(BackendWindowHandle window) override;
    void SetFullscreen(BackendWindowHandle window, bool fullscreen) override;
    void SetBordered(BackendWindowHandle window, bool bordered) override;
    void SetResizable(BackendWindowHandle window, bool resizable) override;
    void SetAlwaysOnTop(BackendWindowHandle window, bool alwaysOnTop) override;
    void Minimize(BackendWindowHandle window) override;
    void Maximize(BackendWindowHandle window) override;
    void Restore(BackendWindowHandle window) override;
    void StartTextInput(BackendWindowHandle window) override;
    void StopTextInput(BackendWindowHandle window) override;
    [[nodiscard]] bool IsTextInputActive(BackendWindowHandle window) noexcept override;
    void ClearTextComposition(BackendWindowHandle window) override;
    void SetTextInputArea(BackendWindowHandle window, std::optional<BackendTextInputArea> area) override;
    void SetMouseGrab(BackendWindowHandle window, bool grabbed) override;
    [[nodiscard]] bool IsMouseGrabbed(BackendWindowHandle window) noexcept override;
    void SetRelativeMouseMode(BackendWindowHandle window, bool enabled) override;
    [[nodiscard]] bool IsRelativeMouseModeEnabled(BackendWindowHandle window) noexcept override;
    [[nodiscard]] ponder::core::Result<NativeWindowHandle> GetNativeHandle(BackendWindowHandle window) override;

private:
    [[nodiscard]] MockWindowState& Get(BackendWindowHandle window);
    [[nodiscard]] const MockWindowState& Get(BackendWindowHandle window) const;

    MockRuntimeControl& m_control;
    std::unordered_map<BackendWindowHandle::ValueType, MockWindowState> m_windows;
    BackendWindowHandle::ValueType m_nextHandle{1};
    std::uint32_t m_nextBackendId{1};
};

class MockRuntime final
{
public:
    MockRuntime();
    ~MockRuntime() noexcept;

    MockRuntime(const MockRuntime&) = delete;
    MockRuntime& operator=(const MockRuntime&) = delete;
    MockRuntime(MockRuntime&&) = delete;
    MockRuntime& operator=(MockRuntime&&) = delete;

    void Initialize(std::string_view applicationName, std::optional<std::string_view> applicationVersion,
                    std::optional<std::string_view> applicationIdentifier);

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

    void VerifyOwnerThread(std::string_view operation) const;
    void VerifyOwnerThreadForDestruction(std::string_view object) const noexcept;
    void RegisterChild(const void* child);
    void UnregisterChild(const void* child);
    [[nodiscard]] WindowId GetNextWindowIdForRegistration() const;
    void RegisterWindow(WindowImpl& window, BackendWindowHandle backendWindow, std::uint32_t backendWindowId, WindowId id);
    void BeginWindowDestruction(WindowImpl& window, std::uint32_t backendWindowId, WindowId id);
    void FinishWindowDestruction(WindowImpl& window);
    void RestoreWindowIdAfterFailedConstruction(WindowId id) noexcept;
    [[nodiscard]] ponder::core::Result<DisplayId> GetDisplayIdForWindow(BackendWindowHandle window, std::string_view windowContext);
    [[nodiscard]] float GetPixelDensityForWindow(BackendWindowHandle window, std::string_view windowContext) const;
    [[nodiscard]] float GetDisplayScaleForWindow(BackendWindowHandle window, std::string_view windowContext) const;

private:
    friend class WindowImpl;

    template <typename Hint>
    void HintPushValue(const Hint& hint, bool beforeInitialization, bool pushOnce = false)
    {
        m_ownerThread.Verify("hint mutation");
        if (beforeInitialization && m_initialized)
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "This platform hint can only be changed before Runtime "
                                                                         "initialization.");
        }

        const std::type_index type{typeid(Hint)};
        if (pushOnce && (m_hints.contains(type) || m_hintsEverPushed.contains(type)))
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "This platform hint can only be set once.");
        }
        m_hints[type].emplace_back(hint);
        m_hintsEverPushed.emplace(type);
    }

    template <typename Hint>
    void HintPopValue(bool beforeInitialization)
    {
        m_ownerThread.Verify("hint pop");
        if (beforeInitialization && m_initialized)
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "This platform hint can only be changed before Runtime "
                                                                         "initialization.");
        }

        const std::type_index type{typeid(Hint)};
        auto values = m_hints.find(type);
        if (values == m_hints.end() || values->second.empty())
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::NotFound, "This platform hint has no managed value to pop.");
        }
        values->second.pop_back();
        if (values->second.empty())
        {
            m_hints.erase(values);
        }
    }

    template <typename Hint>
    void HintClearValue(bool beforeInitialization)
    {
        m_ownerThread.Verify("hint clear");
        if (beforeInitialization && m_initialized)
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "This platform hint can only be changed before Runtime "
                                                                         "initialization.");
        }
        m_hints.erase(std::type_index{typeid(Hint)});
    }

    template <typename Hint>
    [[nodiscard]] std::optional<Hint> HintGetValue() const
    {
        m_ownerThread.Verify("hint query");
        const auto values = m_hints.find(std::type_index{typeid(Hint)});
        if (values == m_hints.end() || values->second.empty())
        {
            return std::nullopt;
        }
        return std::any_cast<Hint>(values->second.back());
    }

    void VerifyInitialized(std::string_view operation) const;
    [[nodiscard]] IPlatformWindowBackend& GetWindowBackend() noexcept;
    void DialogValidateAccess(std::string_view operation) const;
    [[nodiscard]] dialogs::DialogRequestId DialogShow(dialogs::DialogKind kind, std::optional<WindowId> parentWindowId, std::size_t filterCount,
                                                      bool allowMultipleSelection);

    RuntimeOwnerThreadGuard m_ownerThread;
    RuntimeChildRegistry m_registry;
    WindowRegistry m_windowRegistry;
    MockRuntimeControl* m_control{};
    MockWindowBackend m_windowBackend;
    std::unordered_map<std::type_index, std::vector<std::any>> m_hints;
    std::unordered_set<std::type_index> m_hintsEverPushed;
    std::unordered_map<dialogs::DialogRequestId, DialogRequestInfo> m_dialogRequests;
    std::unordered_map<dialogs::DialogRequestId, DialogParentLease> m_dialogParentLeases;
    std::unordered_map<dialogs::DialogRequestId, DialogOutcome> m_dialogCompletions;
    std::unordered_map<dialogs::DialogRequestId, ponder::core::Timestamp> m_dialogCompletionTimestamps;
    std::deque<dialogs::DialogRequestId> m_completedDialogRequests;
    dialogs::DialogRequestId::ValueType m_nextDialogRequestId{1};
    std::condition_variable m_eventCondition;
    std::mutex m_eventMutex;
    bool m_wakePending{};
    bool m_dialogShutdown{};
    bool m_initialized{};
};

#define PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(Type)                                                                                       \
    template <>                                                                                                                                      \
    void MockRuntime::HintPush<hints::Type>(const hints::Type& hint);                                                                                \
    template <>                                                                                                                                      \
    void MockRuntime::HintPop<hints::Type>();                                                                                                        \
    template <>                                                                                                                                      \
    void MockRuntime::HintClear<hints::Type>();                                                                                                      \
    template <>                                                                                                                                      \
    [[nodiscard]] std::optional<hints::Type> MockRuntime::HintGet<hints::Type>() const

PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(AllowAltTabWhileGrabbed);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(EventLogging);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(ImeImplementedUi);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(PollSentinel);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(QuitOnLastWindowClose);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoAllowScreensaver);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoDoubleBuffer);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoDriver);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoForceEgl);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoMinimizeOnFocusLoss);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoSyncWindowOperations);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenRaised);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenShown);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowAllowTopmost);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowFrameUsableWhileCursorHidden);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseAutoCapture);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseDefaultSystemCursor);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickRadius);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickTime);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseDpiScaleCursors);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseEmulateWarpWithRelative);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseFocusClickThrough);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseNormalSpeedScale);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeCursorVisible);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeModeCenter);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSpeedScale);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSystemScale);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeWarpMotion);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MouseTouchEvents);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(PenMouseEvents);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(PenTouchEvents);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(TouchMouseEvents);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MacCtrlClickEmulatesRightClick);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(MacScrollMomentum);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowsCloseOnAltF4);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowsEnableMenuMnemonics);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowsGameInput);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboard);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardExcludeHotkeys);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardInputSink);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoDisplayPriority);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandAllowLibdecor);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandModeEmulation);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandPreferLibdecor);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandScaleToDisplay);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoX11NetWmBypassCompositor);
PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS(VideoX11Xrandr);
#endif

#undef PONDER_DECLARE_MOCK_RUNTIME_HINT_SPECIALIZATIONS
} // namespace ponder::platform::detail

namespace std
{
template <>
struct formatter<ponder::platform::detail::MockRuntimeControl> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::MockRuntimeControl& control, FormatContext& context) const
    {
        return formatter<string>::format(std::format("constructions={}, initializations={}, destructions={}, liveWindows={}",
                                                     control.constructionCount, control.successfulInitializationCount, control.destructionCount,
                                                     control.liveWindowCount),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::detail::ScopedMockRuntimeBinding> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::ScopedMockRuntimeBinding&, FormatContext& context) const
    {
        return formatter<string_view>::format("mock-runtime-binding", context);
    }
};

template <>
struct formatter<ponder::platform::detail::MockWindowState> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::MockWindowState& state, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("backendId={}, title='{}', logicalSize={}, properties={}", state.backendId, state.title, state.logicalSize, state.properties),
            context);
    }
};

template <>
struct formatter<ponder::platform::detail::MockWindowBackend> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::MockWindowBackend&, FormatContext& context) const
    {
        return formatter<string_view>::format("mock-window-backend", context);
    }
};

template <>
struct formatter<ponder::platform::detail::MockRuntime> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::MockRuntime&, FormatContext& context) const
    {
        return formatter<string_view>::format("mock-runtime", context);
    }
};
} // namespace std

namespace ponder::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, const MockRuntimeControl& control)
{
    return output << std::format("{}", control);
}

inline std::ostream& operator<<(std::ostream& output, const ScopedMockRuntimeBinding& binding)
{
    return output << std::format("{}", binding);
}

inline std::ostream& operator<<(std::ostream& output, const MockWindowState& state)
{
    return output << std::format("{}", state);
}

inline std::ostream& operator<<(std::ostream& output, const MockWindowBackend& backend)
{
    return output << std::format("{}", backend);
}

inline std::ostream& operator<<(std::ostream& output, const MockRuntime& runtime)
{
    return output << std::format("{}", runtime);
}
} // namespace ponder::platform::detail
