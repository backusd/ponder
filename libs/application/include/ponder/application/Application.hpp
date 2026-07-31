#pragma once

#include <ponder/application/ApplicationError.hpp>
#include <ponder/application/Identifiers.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Dialogs.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Process.hpp>
#include <ponder/platform/Runtime.hpp>
#include <ponder/platform/Window.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ponder::application
{
struct ApplicationDesc final
{
    std::string applicationName{"ponder"};
    std::optional<std::string> applicationVersion;
    std::optional<std::string> applicationIdentifier;

    [[nodiscard]] friend bool operator==(const ApplicationDesc& lhs, const ApplicationDesc& rhs) = default;
};

struct BackgroundProcessDesc final
{
    ponder::platform::ProcessDesc process;
    bool forceProcessTerminationOnApplicationExit{};

    [[nodiscard]] friend bool operator==(const BackgroundProcessDesc& lhs, const BackgroundProcessDesc& rhs) = default;
};

class Application
{
public:
    virtual ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    [[nodiscard]] int Run();
    void Wake() noexcept;

    [[nodiscard]] const ApplicationDesc& GetDescription() const noexcept;
    [[nodiscard]] bool IsRunning() const noexcept;

protected:
    explicit Application(ApplicationDesc desc = {});

    [[nodiscard]] ponder::core::Result<std::string> ClipboardGetText() const;
    [[nodiscard]] ponder::core::VoidResult ClipboardSetText(std::string_view text);

    [[nodiscard]] ponder::platform::Window& WindowCreate(const ponder::platform::WindowDesc& desc);
    [[nodiscard]] ponder::platform::Window* WindowFind(ponder::platform::WindowId id);
    [[nodiscard]] const ponder::platform::Window* WindowFind(ponder::platform::WindowId id) const;
    [[nodiscard]] ponder::platform::Window& WindowGet(ponder::platform::WindowId id);
    [[nodiscard]] const ponder::platform::Window& WindowGet(ponder::platform::WindowId id) const;
    [[nodiscard]] std::vector<ponder::platform::WindowId> WindowGetIds() const;
    [[nodiscard]] std::size_t WindowGetCount() const;
    void WindowClose(ponder::platform::WindowId id);
    void WindowCloseAll();

    [[nodiscard]] ponder::core::Result<ponder::platform::dialogs::DialogRequestId> DialogShowOpenFile(
        const ponder::platform::dialogs::OpenFileDialogDesc& desc) noexcept;
    [[nodiscard]] ponder::core::Result<ponder::platform::dialogs::DialogRequestId> DialogShowSaveFile(
        const ponder::platform::dialogs::SaveFileDialogDesc& desc) noexcept;
    [[nodiscard]] ponder::core::Result<ponder::platform::dialogs::DialogRequestId> DialogShowOpenFolder(
        const ponder::platform::dialogs::OpenFolderDialogDesc& desc) noexcept;
    [[nodiscard]] ponder::core::Result<std::size_t> DialogGetPendingCount() const noexcept;
    [[nodiscard]] ponder::core::Result<bool> DialogHasPending() const noexcept;
    [[nodiscard]] ponder::core::Result<std::vector<ponder::platform::DialogRequestInfo>> DialogGetPending() const noexcept;

    [[nodiscard]] ponder::core::Timestamp TimeNow() const;

    [[nodiscard]] std::vector<ponder::platform::DisplayInfo> DisplayEnumerate();
    [[nodiscard]] ponder::core::Result<ponder::platform::DisplayInfo> DisplayGetInfo(ponder::platform::DisplayId id);

    [[nodiscard]] ponder::core::VoidResult MouseSetCapture(bool enabled);
    [[nodiscard]] ponder::core::Result<ponder::platform::LogicalPoint> MouseGetGlobalPosition() const;
    void MouseSetSystemCursor(ponder::platform::SystemCursorShape shape);
    void MouseShowCursor();
    void MouseHideCursor();
    [[nodiscard]] bool MouseIsCursorVisible() const;

    [[nodiscard]] ponder::core::VoidResult UriOpenExternal(std::string_view uri);

    [[nodiscard]] ponder::core::Result<BackgroundProcessId> ProcessLaunch(const BackgroundProcessDesc& desc);
    [[nodiscard]] ponder::core::VoidResult ProcessTerminate(BackgroundProcessId id, ponder::platform::ProcessTerminationMode mode);
    [[nodiscard]] std::size_t ProcessGetCount() const;

    void RequestUpdate() noexcept;
    void RequestRender() noexcept;
    void SetExitCode(int exitCode) noexcept;

    virtual void PrePlatformInitialization(ponder::platform::Runtime& runtime);
    virtual void OnStart();
    virtual void OnUpdate(ponder::core::Duration deltaTime);
    virtual void OnRender();
    virtual void OnStop();

    virtual void OnQuitRequestedEvent(const ponder::platform::QuitRequestedEvent& event);
    virtual void OnWindowCloseRequestedEvent(const ponder::platform::WindowCloseRequestedEvent& event);
    virtual void OnWindowMovedEvent(const ponder::platform::WindowMovedEvent& event);
    virtual void OnWindowLogicalSizeChangedEvent(const ponder::platform::WindowLogicalSizeChangedEvent& event);
    virtual void OnWindowPixelSizeChangedEvent(const ponder::platform::WindowPixelSizeChangedEvent& event);
    virtual void OnWindowFocusChangedEvent(const ponder::platform::WindowFocusChangedEvent& event);
    virtual void OnWindowVisibilityChangedEvent(const ponder::platform::WindowVisibilityChangedEvent& event);
    virtual void OnWindowStateChangedEvent(const ponder::platform::WindowStateChangedEvent& event);
    virtual void OnWindowPresentationChangedEvent(const ponder::platform::WindowPresentationChangedEvent& event);
    virtual void OnWindowDisplayChangedEvent(const ponder::platform::WindowDisplayChangedEvent& event);
    virtual void OnWindowDisplayScaleChangedEvent(const ponder::platform::WindowDisplayScaleChangedEvent& event);
    virtual void OnWindowPointerEnteredEvent(const ponder::platform::WindowPointerEnteredEvent& event);
    virtual void OnWindowPointerLeftEvent(const ponder::platform::WindowPointerLeftEvent& event);

    virtual void OnDisplayAddedEvent(const ponder::platform::DisplayAddedEvent& event);
    virtual void OnDisplayRemovedEvent(const ponder::platform::DisplayRemovedEvent& event);
    virtual void OnDisplayMovedEvent(const ponder::platform::DisplayMovedEvent& event);
    virtual void OnDisplayDesktopModeChangedEvent(const ponder::platform::DisplayDesktopModeChangedEvent& event);
    virtual void OnDisplayCurrentModeChangedEvent(const ponder::platform::DisplayCurrentModeChangedEvent& event);
    virtual void OnDisplayOrientationChangedEvent(const ponder::platform::DisplayOrientationChangedEvent& event);
    virtual void OnDisplayContentScaleChangedEvent(const ponder::platform::DisplayContentScaleChangedEvent& event);
    virtual void OnDisplayUsableBoundsChangedEvent(const ponder::platform::DisplayUsableBoundsChangedEvent& event);

    virtual void OnKeyboardKeyEvent(const ponder::platform::KeyboardKeyEvent& event);
    virtual void OnTextInputEvent(const ponder::platform::TextInputEvent& event);
    virtual void OnTextCompositionEvent(const ponder::platform::TextCompositionEvent& event);

    virtual void OnMouseMotionEvent(const ponder::platform::MouseMotionEvent& event);
    virtual void OnMouseButtonEvent(const ponder::platform::MouseButtonEvent& event);
    virtual void OnMouseWheelEvent(const ponder::platform::MouseWheelEvent& event);

    virtual void OnDropBeginEvent(const ponder::platform::DropBeginEvent& event);
    virtual void OnDroppedFileEvent(const ponder::platform::DroppedFileEvent& event);
    virtual void OnDroppedTextEvent(const ponder::platform::DroppedTextEvent& event);
    virtual void OnDropPositionEvent(const ponder::platform::DropPositionEvent& event);
    virtual void OnDropCompleteEvent(const ponder::platform::DropCompleteEvent& event);

    virtual void OnDialogCompletedEvent(const ponder::platform::DialogCompletedEvent& event);
    virtual void OnProcessCompleted(BackgroundProcessId id, const ponder::platform::ProcessExitStatus& status);
    virtual void OnProcessDetached(BackgroundProcessId id);

private:
    void VerifyOwnerThread(std::string_view operation) const;
    void VerifyRunning(std::string_view operation) const;
    void VerifyCanCreateWork(std::string_view operation) const;
    [[nodiscard]] ponder::core::VoidResult ValidateDialogOperation(std::string_view operation, bool canCreateWork) const;
    [[nodiscard]] ponder::core::VoidResult ValidateDialogParent(std::optional<ponder::platform::WindowId> parentWindowId,
                                                                std::string_view operation) const;
    void DispatchEvent(const ponder::platform::PlatformEvent& event);
    [[nodiscard]] bool DrainEvents();
    void PollProcesses();
    void BeginShutdown();
    void MarkWindowForClosure(ponder::platform::WindowId id);
    void FinalizeWindowClosures();
    [[nodiscard]] bool HasLogicallyOpenWindows() const noexcept;
    void DrainShutdown();
    void ShutdownProcesses();
    void ActivateWake();
    void DeactivateWake() noexcept;

    ApplicationDesc m_desc;
    std::optional<ponder::platform::Runtime> m_runtime;
    std::unordered_map<ponder::platform::WindowId, ponder::platform::Window> m_windows;
    std::unordered_set<ponder::platform::WindowId> m_windowsPendingClosure;
    std::unordered_map<BackgroundProcessId, ponder::platform::Process> m_processes;
    std::unordered_set<BackgroundProcessId> m_forceProcessTerminationOnExit;
    std::uint64_t m_nextProcessId{1};
    ponder::core::Timestamp m_lastUpdateTime{};
    std::thread::id m_ownerThread;
    std::atomic_bool m_running{};
    std::atomic_bool m_updateRequested{};
    std::atomic_bool m_renderRequested{};
    mutable std::mutex m_wakeMutex;
    ponder::platform::Runtime* m_wakeRuntime{};
    bool m_hasRun{};
    bool m_shutdownRequested{};
    bool m_callbacksEnabled{true};
    std::atomic_int m_exitCode{};
};
} // namespace ponder::application

namespace std
{
template <>
struct formatter<ponder::application::ApplicationDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::application::ApplicationDesc& desc, FormatContext& context) const
    {
        const string version = desc.applicationVersion.has_value() ? *desc.applicationVersion : "none";
        const string identifier = desc.applicationIdentifier.has_value() ? *desc.applicationIdentifier : "none";
        return formatter<string>::format(
            std::format("application(name='{}', version='{}', identifier='{}')", desc.applicationName, version, identifier), context);
    }
};

template <>
struct formatter<ponder::application::BackgroundProcessDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::application::BackgroundProcessDesc& desc, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("background_process(process={}, forceTerminationOnExit={})", desc.process, desc.forceProcessTerminationOnApplicationExit),
            context);
    }
};

template <>
struct formatter<ponder::application::Application> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::application::Application& application, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}", application.GetDescription()), context);
    }
};
} // namespace std

namespace ponder::application
{
inline std::ostream& operator<<(std::ostream& output, const ApplicationDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const BackgroundProcessDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const Application& application)
{
    return output << std::format("{}", application);
}
} // namespace ponder::application
