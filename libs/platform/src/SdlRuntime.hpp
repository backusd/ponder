#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Runtime.hpp>
#include <ponder/platform/Window.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <format>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "IPlatformWindowBackend.hpp"
#include "PlatformCommon.hpp"
#include "RuntimeChildRegistry.hpp"
#include "SdlDisplayBackend.hpp"
#include "SdlRuntimeTypes.hpp"
#include "SdlWindowBackend.hpp"
#include "WindowRegistry.hpp"

namespace ponder::platform::detail
{
class DisplayTopologyUpdate;
class DialogCallbackHandoff;
class DialogRequestState;
class SdlRuntime;
class WindowImpl;

class DialogBackendCompletion final
{
public:
    DialogBackendCompletion() noexcept = default;

    void CompleteSelection(std::span<const std::string> paths, int selectedFilter) const noexcept;
    void CompleteCancellation() const noexcept;
    void CompleteFailure(std::string_view message) const noexcept;
    void FailCallback() const noexcept;
    [[nodiscard]] bool HasLiveRequest() const noexcept
    {
        return !m_request.expired();
    }

private:
    friend class SdlRuntime;

    explicit DialogBackendCompletion(const std::shared_ptr<DialogRequestState>& request) noexcept;

    std::weak_ptr<DialogRequestState> m_request;
};

struct DialogBackendRequest final
{
    DialogKind kind{DialogKind::OpenFile};
    std::optional<BackendWindowHandle> parentWindow;
    std::span<const DialogFileFilter> filters;
    std::optional<std::string_view> defaultLocation;
    bool allowMultipleSelection{};
    DialogBackendCompletion completion;
};

void ShowSdlDialog(const DialogBackendRequest& request);

enum class RuntimeDisplayLifecycleEventKind : std::uint8_t
{
    Added,
    Removed
};

struct RuntimeDisplayLifecycleEvent final
{
    RuntimeDisplayLifecycleEventKind kind{RuntimeDisplayLifecycleEventKind::Added};
    DisplayId id;
};

struct RuntimePendingDisplayLifecycleRecovery final
{
    RuntimeDisplayLifecycleEventKind kind{RuntimeDisplayLifecycleEventKind::Added};
    std::uint32_t backendId{};
    // Removal target, or the old generation to disconnect before recovering an addition.
    std::optional<DisplayId> id;
    bool validateAgainstLiveTopology{};
};

struct RuntimeDisplayRecord final
{
    DisplayId id;
    bool connected{};
    std::deque<RuntimeDisplayLifecycleEvent> pendingLifecycleEvents;
    std::optional<DisplayId> lastRemovedEventId;
    // A failed removal validation awaiting a same-backend addition as corroboration.
    std::optional<DisplayId> unconfirmedRemovedEventId;
};

struct RuntimeBackendDisplayRecord final
{
    std::uint32_t backendId{};
    bool connected{};
};

struct SdlHintValueState final
{
    std::optional<std::string> originalValue;
    std::vector<std::string> values;
    bool originalCaptured{};
    bool everPushed{};
};

class SdlRuntime final
{
public:
    SdlRuntime();
    ~SdlRuntime() noexcept;

    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;
    SdlRuntime(SdlRuntime&&) = delete;
    SdlRuntime& operator=(SdlRuntime&&) = delete;

    // ========================================================================
    // Initialization
    // ========================================================================
    void Initialize(const RuntimeDesc& desc);

    // ========================================================================
    // Hints
    // ========================================================================
    template <typename Hint>
    void HintPush(const Hint& hint) = delete;
    template <typename Hint>
    void HintPop() = delete;
    template <typename Hint>
    void HintClear() = delete;
    template <typename Hint>
    [[nodiscard]] std::optional<Hint> HintGet() const = delete;

    // ========================================================================
    // Clipboard
    // ========================================================================
    [[nodiscard]] ponder::core::Result<std::string> ClipboardGetText() const;
    [[nodiscard]] ponder::core::VoidResult ClipboardSetText(std::string_view text);

    // ========================================================================
    // Dialogs
    // ========================================================================
    [[nodiscard]] DialogRequestId DialogShowOpenFile(const OpenFileDialogDesc& desc);
    [[nodiscard]] DialogRequestId DialogShowSaveFile(const SaveFileDialogDesc& desc);
    [[nodiscard]] DialogRequestId DialogShowOpenFolder(const OpenFolderDialogDesc& desc);
    [[nodiscard]] std::size_t DialogGetPendingCount() const;
    [[nodiscard]] bool DialogHasPending() const;
    [[nodiscard]] std::vector<DialogRequestInfo> DialogGetPending() const;
    [[nodiscard]] std::optional<DialogCompletedEvent> DialogPollCompletion();
    [[nodiscard]] std::size_t DialogGetOutstandingRequestCount() const;
    void DialogShutdown();

    // ========================================================================
    // Time
    // ========================================================================
    [[nodiscard]] ponder::core::Timestamp TimeNow() const;

    // ========================================================================
    // Events
    // ========================================================================
    [[nodiscard]] std::optional<PlatformEvent> EventPoll();

    // ========================================================================
    // Window
    // ========================================================================
    [[nodiscard]] Window WindowCreate(const WindowDesc& desc);

    // ========================================================================
    // Displays
    // ========================================================================
    [[nodiscard]] std::vector<DisplayInfo> DisplayEnumerate();
    [[nodiscard]] ponder::core::Result<DisplayInfo> DisplayGetInfo(DisplayId id);

    // ========================================================================
    // Mouse
    // ========================================================================
    [[nodiscard]] ponder::core::VoidResult MouseSetCapture(bool enabled);
    [[nodiscard]] ponder::core::Result<LogicalPoint> MouseGetGlobalPosition() const;
    void MouseSetSystemCursor(SystemCursorShape shape);
    void MouseShowCursor();
    void MouseHideCursor();
    [[nodiscard]] bool MouseIsCursorVisible() const;

    // ========================================================================
    // URI
    // ========================================================================
    [[nodiscard]] ponder::core::VoidResult UriOpenExternal(std::string_view uri);

    // ========================================================================
    // Threading
    // ========================================================================
    void VerifyOwnerThread(std::string_view operation) const;
    void VerifyOwnerThreadForDestruction(std::string_view object) const noexcept;

    void RegisterChild(const void* child);
    void UnregisterChild(const void* child);

    [[nodiscard]] WindowId GetNextWindowIdForRegistration() const;
    void RegisterWindow(WindowImpl& window, BackendWindowHandle backendWindow, std::uint32_t backendWindowId, WindowId id);
    void BeginWindowDestruction(WindowImpl& window, std::uint32_t backendWindowId, WindowId id);
    void FinishWindowDestruction(WindowImpl& window);
    void RestoreWindowIdAfterFailedConstruction(WindowId id) noexcept;
    [[nodiscard]] std::optional<WindowId> FindWindowId(std::uint32_t backendWindowId) const;
    [[nodiscard]] std::optional<DisplayId> FindConnectedDisplayId(std::uint32_t backendDisplayId) const;
    [[nodiscard]] std::optional<DisplayId> FindDisplayIdForRemoval(std::uint32_t backendDisplayId) const;

    [[nodiscard]] ponder::core::Result<DisplayId> GetDisplayIdForWindow(BackendWindowHandle window, std::string_view windowContext);
    [[nodiscard]] float GetPixelDensityForWindow(BackendWindowHandle window, std::string_view windowContext) const;
    [[nodiscard]] float GetDisplayScaleForWindow(BackendWindowHandle window, std::string_view windowContext) const;

private:
    friend class DialogCallbackHandoff;
    friend class DisplayTopologyUpdate;
    friend class WindowImpl;

    void VerifyInitialized(std::string_view operation) const;
    [[nodiscard]] IPlatformWindowBackend& GetWindowBackend() noexcept;

    // ========================================================================
    // Hints
    // ========================================================================
    void HintPushRaw(const char* name, bool beforeInitialization, bool pushOnce, std::string value);
    void HintPopRaw(const char* name, bool beforeInitialization);
    void HintClearRaw(const char* name, bool beforeInitialization);
    [[nodiscard]] std::optional<std::string> HintGetRaw(const char* name) const;
    void HintBeginMutation();
    void HintValidateMutationPhase(const char* name, bool beforeInitialization) const;
    [[nodiscard]] bool HintMatchesValue(const char* name, const std::optional<std::string>& value) const noexcept;
    void HintRestoreValue(const char* name, const std::optional<std::string>& value);
    void HintSetValue(const char* name, const std::string& value, const std::optional<std::string>& rollbackValue);
    void HintRestoreOriginalValue(const char* name, const SdlHintValueState& state);
    [[nodiscard]] auto HintFindActive(const char* name) -> std::vector<std::string>::iterator;
    void HintFinishActivation(std::vector<std::string>::iterator activeIterator, SdlHintValueState& state);
    void HintRestoreAll() noexcept;

    // ========================================================================
    // Dialogs
    // ========================================================================
    void DialogInitialize();
    [[nodiscard]] DialogRequestId DialogShow(DialogKind kind, std::optional<WindowId> parentWindowId,
                                             const std::optional<std::filesystem::path>& defaultLocation, std::span<const DialogFileFilter> filters,
                                             bool allowMultipleSelection);
    void DialogRollbackRequest(DialogRequestId id) noexcept;
    void DialogEnqueueCompletion(DialogRequestId id, ponder::core::Timestamp timestamp, DialogOutcome outcome);
    void DialogMarkCallbackFailure(DialogRequestId id, ponder::core::Timestamp timestamp) noexcept;
    void DialogShutdownForRuntimeDestruction() noexcept;

    // ========================================================================
    // Events
    // ========================================================================
    void RecoverPendingDisplayLifecycleEvent();
    [[nodiscard]] bool PollBackendEvent(BackendEvent& event) noexcept;
    [[nodiscard]] std::optional<PlatformEvent> TranslateBackendEvent(const BackendEvent& event, const EventTranslationContext& context) const;
    [[nodiscard]] std::optional<RuntimeDisplayLifecycleEvent> FindPendingDisplayLifecycleEvent(std::uint32_t backendDisplayId) const;
    void AcknowledgePendingDisplayLifecycleEvent(std::uint32_t backendDisplayId, const RuntimeDisplayLifecycleEvent& pending);
    [[nodiscard]] DisplayTopologyUpdate ConnectDisplayFromEvent(std::uint32_t backendDisplayId);
    [[nodiscard]] DisplayTopologyUpdate DisconnectDisplayFromEvent(std::uint32_t backendDisplayId, DisplayId id);
    [[nodiscard]] DisplayTopologyUpdate ReconcileDisplayFromEvent(std::uint32_t backendDisplayId);
    void ObserveWindowShownEvent(std::uint32_t backendWindowId);

    // ========================================================================
    // Displays
    // ========================================================================
    [[nodiscard]] std::vector<std::uint32_t> EnumerateBackendDisplays() const;
    [[nodiscard]] DisplayTopologyUpdate PrepareDisplayRefresh(std::span<const std::uint32_t> backendDisplayIds);
    [[nodiscard]] std::vector<std::uint32_t> RefreshDisplays();
    [[nodiscard]] DisplayInfo QueryDisplayInfo(DisplayId id, std::uint32_t backendDisplayId) const;
    [[nodiscard]] std::optional<DisplayId> FindKnownDisplayId(std::uint32_t backendDisplayId) const;

    // ========================================================================
    // Mouse
    // ========================================================================
    void DestroySystemCursors() noexcept;

    // Initialization
    RuntimeOwnerThreadGuard m_ownerThread;
    RuntimeChildRegistry m_registry;
    bool m_initialized{};

    // Hints
    std::unordered_map<std::string, SdlHintValueState> m_hintStates;
    std::vector<std::string> m_activeHintNames;
    bool m_hintMutationActive{};

    // Clipboard
    mutable std::mutex m_clipboardMutex;

    // Dialogs
    std::shared_ptr<DialogCallbackHandoff> m_dialogCallbackHandoff;
    DialogRequestId::ValueType m_nextDialogRequestId{1};
    mutable std::mutex m_dialogMutex;
    std::unordered_map<DialogRequestId, std::shared_ptr<DialogRequestState>> m_dialogRequests;
    std::list<DialogRequestId> m_completedDialogRequests;
    bool m_dialogShutdown{};

    // Events

    // Window
    SdlWindowBackend m_windowBackend;
    WindowRegistry m_windowRegistry;

    // Displays
    mutable SdlDisplayBackend m_displayBackend;
    std::unordered_map<std::uint32_t, RuntimeDisplayRecord> m_displaysByBackendId;
    std::unordered_map<DisplayId, RuntimeBackendDisplayRecord> m_displaysById;
    std::vector<std::uint32_t> m_connectedBackendDisplayIds;
    DisplayId::ValueType m_nextDisplayId{1};
    bool m_hasCompletedDisplayRefresh{};
    std::optional<RuntimePendingDisplayLifecycleRecovery> m_pendingDisplayLifecycleRecovery;

    // Mouse
    std::array<CursorHandle, kSystemCursorShapeCount> m_systemCursors{};
};

#define PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(Type)                                                                                        \
    template <>                                                                                                                                      \
    void SdlRuntime::HintPush<hints::Type>(const hints::Type& hint);                                                                                 \
    template <>                                                                                                                                      \
    void SdlRuntime::HintPop<hints::Type>();                                                                                                         \
    template <>                                                                                                                                      \
    void SdlRuntime::HintClear<hints::Type>();                                                                                                       \
    template <>                                                                                                                                      \
    [[nodiscard]] std::optional<hints::Type> SdlRuntime::HintGet<hints::Type>() const

PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(AllowAltTabWhileGrabbed);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(EventLogging);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(ImeImplementedUi);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(PollSentinel);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(QuitOnLastWindowClose);

PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoAllowScreensaver);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoDoubleBuffer);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoDriver);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoForceEgl);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoMinimizeOnFocusLoss);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoSyncWindowOperations);

PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenRaised);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenShown);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowAllowTopmost);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowFrameUsableWhileCursorHidden);

PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseAutoCapture);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseDefaultSystemCursor);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickRadius);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickTime);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseDpiScaleCursors);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseEmulateWarpWithRelative);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseFocusClickThrough);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseNormalSpeedScale);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeCursorVisible);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeModeCenter);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSpeedScale);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSystemScale);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeWarpMotion);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MouseTouchEvents);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(PenMouseEvents);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(PenTouchEvents);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(TouchMouseEvents);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MacCtrlClickEmulatesRightClick);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(MacScrollMomentum);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowsCloseOnAltF4);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowsEnableMenuMnemonics);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowsGameInput);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboard);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardExcludeHotkeys);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardInputSink);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoDisplayPriority);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandAllowLibdecor);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandModeEmulation);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandPreferLibdecor);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandScaleToDisplay);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoX11NetWmBypassCompositor);
PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS(VideoX11Xrandr);
#endif

#undef PONDER_DECLARE_SDL_RUNTIME_HINT_SPECIALIZATIONS
} // namespace ponder::platform::detail

namespace std
{
template <>
struct formatter<ponder::platform::detail::DialogBackendCompletion> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::DialogBackendCompletion& completion, FormatContext& context) const
    {
        return formatter<string_view>::format(completion.HasLiveRequest() ? "live-dialog-completion" : "expired-dialog-completion", context);
    }
};

template <>
struct formatter<ponder::platform::detail::DialogBackendRequest> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::DialogBackendRequest& request, FormatContext& context) const
    {
        const string parent = request.parentWindow.has_value() ? std::format("{}", *request.parentWindow) : "none";
        const string location = request.defaultLocation.has_value() ? string{*request.defaultLocation} : "none";
        return formatter<string>::format(std::format("dialog_backend_request(kind={}, parent={}, filterCount={}, "
                                                     "defaultLocation='{}', allowMultipleSelection={}, completion={})",
                                                     request.kind, parent, request.filters.size(), location, request.allowMultipleSelection,
                                                     request.completion),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::detail::RuntimeDisplayLifecycleEventKind> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::RuntimeDisplayLifecycleEventKind kind, FormatContext& context) const
    {
        using ponder::platform::detail::RuntimeDisplayLifecycleEventKind;
        string_view name{"unknown"};
        switch (kind)
        {
        case RuntimeDisplayLifecycleEventKind::Added:
            name = "added";
            break;
        case RuntimeDisplayLifecycleEventKind::Removed:
            name = "removed";
            break;
        }
        return formatter<string_view>::format(name, context);
    }
};

template <>
struct formatter<ponder::platform::detail::RuntimeDisplayLifecycleEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::RuntimeDisplayLifecycleEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(std::format("kind={}, id={}", event.kind, event.id), context);
    }
};

template <>
struct formatter<ponder::platform::detail::RuntimePendingDisplayLifecycleRecovery> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::RuntimePendingDisplayLifecycleRecovery& recovery, FormatContext& context) const
    {
        const string id = recovery.id.has_value() ? std::format("{}", *recovery.id) : "none";
        return formatter<string>::format(std::format("kind={}, backendId={}, id={}, validatesLiveTopology={}", recovery.kind, recovery.backendId, id,
                                                     recovery.validateAgainstLiveTopology),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::detail::RuntimeDisplayRecord> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::RuntimeDisplayRecord& record, FormatContext& context) const
    {
        const string lastRemoved = record.lastRemovedEventId.has_value() ? std::format("{}", *record.lastRemovedEventId) : "none";
        const string unconfirmedRemoved =
            record.unconfirmedRemovedEventId.has_value() ? std::format("{}", *record.unconfirmedRemovedEventId) : "none";
        return formatter<string>::format(std::format("id={}, connected={}, pendingEvents={}, lastRemoved={}, "
                                                     "unconfirmedRemoved={}",
                                                     record.id, record.connected, record.pendingLifecycleEvents.size(), lastRemoved,
                                                     unconfirmedRemoved),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::detail::RuntimeBackendDisplayRecord> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::RuntimeBackendDisplayRecord& record, FormatContext& context) const
    {
        return formatter<string>::format(std::format("backendId={}, connected={}", record.backendId, record.connected), context);
    }
};

template <>
struct formatter<ponder::platform::detail::SdlHintValueState> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::SdlHintValueState& state, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("originalCaptured={}, valueCount={}, everPushed={}", state.originalCaptured, state.values.size(), state.everPushed), context);
    }
};

template <>
struct formatter<ponder::platform::detail::SdlRuntime> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::SdlRuntime&, FormatContext& context) const
    {
        return formatter<string_view>::format("sdl-runtime", context);
    }
};
} // namespace std

namespace ponder::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, const DialogBackendCompletion& completion)
{
    return output << std::format("{}", completion);
}

inline std::ostream& operator<<(std::ostream& output, const DialogBackendRequest& request)
{
    return output << std::format("{}", request);
}

inline std::ostream& operator<<(std::ostream& output, RuntimeDisplayLifecycleEventKind kind)
{
    return output << std::format("{}", kind);
}

inline std::ostream& operator<<(std::ostream& output, const RuntimeDisplayLifecycleEvent& event)
{
    return output << std::format("{}", event);
}

inline std::ostream& operator<<(std::ostream& output, const RuntimePendingDisplayLifecycleRecovery& recovery)
{
    return output << std::format("{}", recovery);
}

inline std::ostream& operator<<(std::ostream& output, const RuntimeDisplayRecord& record)
{
    return output << std::format("{}", record);
}

inline std::ostream& operator<<(std::ostream& output, const RuntimeBackendDisplayRecord& record)
{
    return output << std::format("{}", record);
}

inline std::ostream& operator<<(std::ostream& output, const SdlHintValueState& state)
{
    return output << std::format("{}", state);
}

inline std::ostream& operator<<(std::ostream& output, const SdlRuntime& runtime)
{
    return output << std::format("{}", runtime);
}
} // namespace ponder::platform::detail
