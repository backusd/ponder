#include <ponder/application/Application.hpp>
#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/Log.hpp>

#include <algorithm>
#include <chrono>
#include <concepts>
#include <exception>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace ponder::application
{
namespace
{
constexpr std::string_view kLogCategory{"application"};
constexpr std::size_t kMaximumEventBatchSize{256};
constexpr ponder::core::Duration kIdleWait{std::chrono::hours{24}};
constexpr ponder::core::Duration kProcessObservationWait{std::chrono::milliseconds{50}};

[[nodiscard]] std::optional<std::string_view> ToView(const std::optional<std::string>& value) noexcept
{
    return value.has_value() ? std::optional<std::string_view>{*value} : std::nullopt;
}

template <typename ResultType>
[[nodiscard]] ResultType MakeContainedDialogError(std::string_view operation, const ponder::core::Exception& exception)
{
    LOG_ERROR_CATEGORY(kLogCategory, "Application {} contained a ponder::core::Exception: {} ({}:{})", operation, exception.GetMessage(),
                       exception.GetLocation().file_name(), exception.GetLocation().line());
    return ResultType::FromError(
        ponder::core::Error{ToErrorCode(ApplicationErrorCode::InternalFailure),
                            std::format("Application {} caught ponder::core::Exception: {}", operation, exception.GetMessage()),
                            exception.GetStackTrace(), exception.GetLocation()});
}

template <typename ResultType>
[[nodiscard]] ResultType MakeContainedDialogError(std::string_view operation, const std::exception& exception)
{
    LOG_ERROR_CATEGORY(kLogCategory, "Application {} contained a std::exception: {}", operation, exception.what());
    return ResultType::FromError(ponder::core::Error{ToErrorCode(ApplicationErrorCode::InternalFailure),
                                                     std::format("Application {} caught std::exception: {}", operation, exception.what())});
}

template <typename ResultType>
[[nodiscard]] ResultType MakeContainedUnknownDialogError(std::string_view operation)
{
    LOG_ERROR_CATEGORY(kLogCategory, "Application {} contained an unknown exception", operation);
    return ResultType::FromError(ponder::core::Error{ToErrorCode(ApplicationErrorCode::InternalFailure),
                                                     std::format("Application {} caught an unknown exception.", operation)});
}

[[noreturn]] void ThrowDialogResultFailure(std::string_view operation, const ponder::core::Error& error)
{
    throw APPLICATION_EXCEPTION(ApplicationErrorCode::InternalFailure, "Application {} failed: {}", operation, error);
}

void RequireDialogSuccess(ponder::core::VoidResult result, std::string_view operation)
{
    if (!result)
    {
        ThrowDialogResultFailure(operation, result.GetError());
    }
}

template <typename>
inline constexpr bool kUnhandledEvent = false;
} // namespace

Application::Application(ApplicationDesc desc) :
    m_desc(std::move(desc))
{
}

Application::~Application() noexcept
{
    PONDER_VERIFY(!m_running.load(std::memory_order_acquire), "Cannot destroy a running Application");
    PONDER_VERIFY(m_runtime == std::nullopt && m_windows.empty() && m_processes.empty(),
                  "Application resources must be released before base destruction");
}

int Application::Run()
{
    if (m_hasRun)
    {
        throw APPLICATION_EXCEPTION(ApplicationErrorCode::InvalidState, "Application::Run may only be called once per Application instance.");
    }

    m_hasRun = true;
    m_ownerThread = std::this_thread::get_id();
    m_callbacksEnabled = true;
    bool runtimeReady = false;
    bool wakeActive = false;
    bool startEntered = false;
    std::exception_ptr failure;

    try
    {
        m_runtime.emplace(ponder::platform::Runtime::Create());
        PrePlatformInitialization(*m_runtime);
        m_runtime->Initialize(m_desc.applicationName, ToView(m_desc.applicationVersion), ToView(m_desc.applicationIdentifier));
        ActivateWake();
        wakeActive = true;
        m_running.store(true, std::memory_order_release);
        runtimeReady = true;

        startEntered = true;
        OnStart();
        m_lastUpdateTime = m_runtime->TimeNow();
        m_updateRequested.store(true, std::memory_order_release);
        m_renderRequested.store(true, std::memory_order_release);

        if (!HasLogicallyOpenWindows())
        {
            BeginShutdown();
        }

        while (!m_shutdownRequested)
        {
            const bool handledEvent = DrainEvents();
            if (handledEvent)
            {
                m_updateRequested.store(true, std::memory_order_release);
                m_renderRequested.store(true, std::memory_order_release);
            }

            PollProcesses();
            if (!HasLogicallyOpenWindows())
            {
                BeginShutdown();
                break;
            }

            if (m_updateRequested.exchange(false, std::memory_order_acq_rel))
            {
                const ponder::core::Timestamp now = m_runtime->TimeNow();
                const ponder::core::Duration deltaTime = now - m_lastUpdateTime;
                m_lastUpdateTime = now;
                OnUpdate(deltaTime);
            }

            if (!m_shutdownRequested && m_renderRequested.exchange(false, std::memory_order_acq_rel))
            {
                OnRender();
            }

            if (!HasLogicallyOpenWindows())
            {
                BeginShutdown();
                break;
            }

            const ponder::core::Duration waitDuration = m_processes.empty() ? kIdleWait : kProcessObservationWait;
            if (std::optional<ponder::platform::PlatformEvent> event = m_runtime->EventWait(waitDuration); event.has_value())
            {
                m_updateRequested.store(true, std::memory_order_release);
                m_renderRequested.store(true, std::memory_order_release);
                DispatchEvent(*event);
            }
        }
    }
    catch (...)
    {
        failure = std::current_exception();
        m_callbacksEnabled = false;
    }

    if (m_runtime.has_value())
    {
        const auto runCleanupStage = [this, &failure](auto&& stage) noexcept
        {
            try
            {
                std::forward<decltype(stage)>(stage)();
            }
            catch (...)
            {
                m_callbacksEnabled = false;
                if (failure == nullptr)
                {
                    failure = std::current_exception();
                }
            }
        };

        if (runtimeReady)
        {
            runCleanupStage(
                [this]
                {
                    BeginShutdown();
                });
            runCleanupStage(
                [this]
                {
                    DrainShutdown();
                });
            runCleanupStage(
                [this]
                {
                    ShutdownProcesses();
                });
            runCleanupStage(
                [this]
                {
                    RequireDialogSuccess(m_runtime->DialogShutdown(), "dialog-service shutdown");
                });

            if (startEntered)
            {
                runCleanupStage(
                    [this]
                    {
                        OnStop();
                    });
            }
        }

        m_running.store(false, std::memory_order_release);
        if (wakeActive)
        {
            DeactivateWake();
        }
        m_runtime.reset();
    }

    if (failure != nullptr)
    {
        std::rethrow_exception(failure);
    }
    return m_exitCode.load(std::memory_order_acquire);
}

void Application::Wake() noexcept
{
    try
    {
        const std::scoped_lock lock{m_wakeMutex};
        if (m_wakeRuntime != nullptr)
        {
            m_wakeRuntime->EventWake();
        }
    }
    catch (const ponder::core::Exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Failed to wake the Application event loop: {}", exception.GetMessage());
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Failed to wake the Application event loop: {}", exception.what());
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Failed to wake the Application event loop with an unknown failure");
    }
}

const ApplicationDesc& Application::GetDescription() const noexcept
{
    return m_desc;
}

bool Application::IsRunning() const noexcept
{
    return m_running.load(std::memory_order_acquire);
}

ponder::core::Result<std::string> Application::ClipboardGetText() const
{
    VerifyRunning("clipboard text query");
    return m_runtime->ClipboardGetText();
}

ponder::core::VoidResult Application::ClipboardSetText(std::string_view text)
{
    VerifyRunning("clipboard text update");
    return m_runtime->ClipboardSetText(text);
}

ponder::platform::Window& Application::WindowCreate(const ponder::platform::WindowDesc& desc)
{
    VerifyCanCreateWork("window creation");
    ponder::platform::Window window = m_runtime->WindowCreate(desc);
    const ponder::platform::WindowId id = window.GetId();
    const auto [iterator, inserted] = m_windows.emplace(id, std::move(window));
    PONDER_VERIFY(inserted, "Application window {} is already registered", id);
    return iterator->second;
}

ponder::platform::Window* Application::WindowFind(ponder::platform::WindowId id)
{
    VerifyRunning("window lookup");
    const auto iterator = m_windows.find(id);
    return iterator != m_windows.end() ? std::addressof(iterator->second) : nullptr;
}

const ponder::platform::Window* Application::WindowFind(ponder::platform::WindowId id) const
{
    VerifyRunning("window lookup");
    const auto iterator = m_windows.find(id);
    return iterator != m_windows.end() ? std::addressof(iterator->second) : nullptr;
}

ponder::platform::Window& Application::WindowGet(ponder::platform::WindowId id)
{
    VerifyRunning("window access");
    if (ponder::platform::Window* window = WindowFind(id); window != nullptr)
    {
        return *window;
    }
    throw APPLICATION_EXCEPTION(ApplicationErrorCode::NotFound, "Application does not own window {}.", id);
}

const ponder::platform::Window& Application::WindowGet(ponder::platform::WindowId id) const
{
    VerifyRunning("window access");
    if (const ponder::platform::Window* window = WindowFind(id); window != nullptr)
    {
        return *window;
    }
    throw APPLICATION_EXCEPTION(ApplicationErrorCode::NotFound, "Application does not own window {}.", id);
}

std::vector<ponder::platform::WindowId> Application::WindowGetIds() const
{
    VerifyRunning("window enumeration");
    std::vector<ponder::platform::WindowId> ids;
    ids.reserve(m_windows.size());
    for (const auto& [id, window] : m_windows)
    {
        static_cast<void>(window);
        ids.push_back(id);
    }
    std::ranges::sort(ids);
    return ids;
}

std::size_t Application::WindowGetCount() const
{
    VerifyRunning("window count query");
    return m_windows.size();
}

void Application::WindowClose(ponder::platform::WindowId id)
{
    VerifyRunning("window closure");
    if (m_windows.find(id) == m_windows.end())
    {
        throw APPLICATION_EXCEPTION(ApplicationErrorCode::NotFound, "Application does not own window {}.", id);
    }

    MarkWindowForClosure(id);
    FinalizeWindowClosures();
    if (!HasLogicallyOpenWindows())
    {
        BeginShutdown();
    }
}

void Application::WindowCloseAll()
{
    VerifyRunning("window closure");
    std::vector<ponder::platform::WindowId> ids;
    ids.reserve(m_windows.size());
    for (const auto& [id, window] : m_windows)
    {
        static_cast<void>(window);
        ids.push_back(id);
    }
    for (ponder::platform::WindowId id : ids)
    {
        MarkWindowForClosure(id);
    }
    FinalizeWindowClosures();
    if (!HasLogicallyOpenWindows())
    {
        m_shutdownRequested = true;
    }
}

ponder::core::Result<ponder::platform::dialogs::DialogRequestId> Application::DialogShowOpenFile(
    const ponder::platform::dialogs::OpenFileDialogDesc& desc) noexcept
{
    using Result = ponder::core::Result<ponder::platform::dialogs::DialogRequestId>;
    constexpr std::string_view operation{"open-file dialog submission"};

    try
    {
        if (ponder::core::VoidResult validation = ValidateDialogOperation(operation, true); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        if (ponder::core::VoidResult validation = ValidateDialogParent(desc.parentWindowId, operation); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        return m_runtime->DialogShowOpenFile(desc);
    }
    catch (const ponder::core::Exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (const std::exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (...)
    {
        return MakeContainedUnknownDialogError<Result>(operation);
    }
}

ponder::core::Result<ponder::platform::dialogs::DialogRequestId> Application::DialogShowSaveFile(
    const ponder::platform::dialogs::SaveFileDialogDesc& desc) noexcept
{
    using Result = ponder::core::Result<ponder::platform::dialogs::DialogRequestId>;
    constexpr std::string_view operation{"save-file dialog submission"};

    try
    {
        if (ponder::core::VoidResult validation = ValidateDialogOperation(operation, true); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        if (ponder::core::VoidResult validation = ValidateDialogParent(desc.parentWindowId, operation); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        return m_runtime->DialogShowSaveFile(desc);
    }
    catch (const ponder::core::Exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (const std::exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (...)
    {
        return MakeContainedUnknownDialogError<Result>(operation);
    }
}

ponder::core::Result<ponder::platform::dialogs::DialogRequestId> Application::DialogShowOpenFolder(
    const ponder::platform::dialogs::OpenFolderDialogDesc& desc) noexcept
{
    using Result = ponder::core::Result<ponder::platform::dialogs::DialogRequestId>;
    constexpr std::string_view operation{"open-folder dialog submission"};

    try
    {
        if (ponder::core::VoidResult validation = ValidateDialogOperation(operation, true); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        if (ponder::core::VoidResult validation = ValidateDialogParent(desc.parentWindowId, operation); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        return m_runtime->DialogShowOpenFolder(desc);
    }
    catch (const ponder::core::Exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (const std::exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (...)
    {
        return MakeContainedUnknownDialogError<Result>(operation);
    }
}

ponder::core::Result<std::size_t> Application::DialogGetPendingCount() const noexcept
{
    using Result = ponder::core::Result<std::size_t>;
    constexpr std::string_view operation{"dialog pending-count query"};

    try
    {
        if (ponder::core::VoidResult validation = ValidateDialogOperation(operation, false); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        return Result::FromValue(m_runtime->DialogGetPendingCount());
    }
    catch (const ponder::core::Exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (const std::exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (...)
    {
        return MakeContainedUnknownDialogError<Result>(operation);
    }
}

ponder::core::Result<bool> Application::DialogHasPending() const noexcept
{
    using Result = ponder::core::Result<bool>;
    constexpr std::string_view operation{"dialog pending-state query"};

    try
    {
        if (ponder::core::VoidResult validation = ValidateDialogOperation(operation, false); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        return Result::FromValue(m_runtime->DialogHasPending());
    }
    catch (const ponder::core::Exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (const std::exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (...)
    {
        return MakeContainedUnknownDialogError<Result>(operation);
    }
}

ponder::core::Result<std::vector<ponder::platform::DialogRequestInfo>> Application::DialogGetPending() const noexcept
{
    using Result = ponder::core::Result<std::vector<ponder::platform::DialogRequestInfo>>;
    constexpr std::string_view operation{"dialog pending-list query"};

    try
    {
        if (ponder::core::VoidResult validation = ValidateDialogOperation(operation, false); !validation)
        {
            return Result::FromError(std::move(validation).GetError());
        }
        return Result::FromValue(m_runtime->DialogGetPending());
    }
    catch (const ponder::core::Exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (const std::exception& exception)
    {
        return MakeContainedDialogError<Result>(operation, exception);
    }
    catch (...)
    {
        return MakeContainedUnknownDialogError<Result>(operation);
    }
}

ponder::core::Timestamp Application::TimeNow() const
{
    VerifyRunning("time query");
    return m_runtime->TimeNow();
}

std::vector<ponder::platform::DisplayInfo> Application::DisplayEnumerate()
{
    VerifyRunning("display enumeration");
    return m_runtime->DisplayEnumerate();
}

ponder::core::Result<ponder::platform::DisplayInfo> Application::DisplayGetInfo(ponder::platform::DisplayId id)
{
    VerifyRunning("display query");
    return m_runtime->DisplayGetInfo(id);
}

ponder::core::VoidResult Application::MouseSetCapture(bool enabled)
{
    VerifyRunning("mouse capture update");
    return m_runtime->MouseSetCapture(enabled);
}

ponder::core::Result<ponder::platform::LogicalPoint> Application::MouseGetGlobalPosition() const
{
    VerifyRunning("global mouse position query");
    return m_runtime->MouseGetGlobalPosition();
}

void Application::MouseSetSystemCursor(ponder::platform::SystemCursorShape shape)
{
    VerifyRunning("system cursor update");
    m_runtime->MouseSetSystemCursor(shape);
}

void Application::MouseShowCursor()
{
    VerifyRunning("cursor visibility update");
    m_runtime->MouseShowCursor();
}

void Application::MouseHideCursor()
{
    VerifyRunning("cursor visibility update");
    m_runtime->MouseHideCursor();
}

bool Application::MouseIsCursorVisible() const
{
    VerifyRunning("cursor visibility query");
    return m_runtime->MouseIsCursorVisible();
}

ponder::core::VoidResult Application::UriOpenExternal(std::string_view uri)
{
    VerifyRunning("external URI open");
    return m_runtime->UriOpenExternal(uri);
}

ponder::core::Result<BackgroundProcessId> Application::ProcessLaunch(const BackgroundProcessDesc& desc)
{
    VerifyCanCreateWork("background process launch");
    if (m_nextProcessId == 0)
    {
        throw APPLICATION_EXCEPTION(ApplicationErrorCode::InvalidState, "Application background process ID space is exhausted.");
    }

    ponder::core::Result<ponder::platform::Process> processResult = ponder::platform::LaunchProcess(desc.process);
    if (!processResult)
    {
        return ponder::core::Result<BackgroundProcessId>::FromError(std::move(processResult).GetError());
    }

    const BackgroundProcessId id{m_nextProcessId};
    const auto [iterator, inserted] = m_processes.emplace(id, std::move(processResult).GetValue());
    static_cast<void>(iterator);
    PONDER_VERIFY(inserted, "Application background process {} is already registered", id);

    try
    {
        if (desc.forceProcessTerminationOnApplicationExit)
        {
            m_forceProcessTerminationOnExit.insert(id);
        }
    }
    catch (...)
    {
        m_processes.erase(id);
        throw;
    }

    ++m_nextProcessId;
    return id;
}

ponder::core::VoidResult Application::ProcessTerminate(BackgroundProcessId id, ponder::platform::ProcessTerminationMode mode)
{
    VerifyRunning("background process termination");
    const auto process = m_processes.find(id);
    if (process == m_processes.end())
    {
        return ponder::core::VoidResult::FromError(
            ponder::core::Error{ToErrorCode(ApplicationErrorCode::NotFound), std::format("Application does not own background process {}.", id)});
    }
    return process->second.Terminate(mode);
}

std::size_t Application::ProcessGetCount() const
{
    VerifyRunning("background process count query");
    return m_processes.size();
}

void Application::RequestUpdate() noexcept
{
    m_updateRequested.store(true, std::memory_order_release);
    Wake();
}

void Application::RequestRender() noexcept
{
    m_renderRequested.store(true, std::memory_order_release);
    Wake();
}

void Application::SetExitCode(int exitCode) noexcept
{
    m_exitCode.store(exitCode, std::memory_order_release);
}

void Application::VerifyOwnerThread(std::string_view operation) const
{
    if (std::this_thread::get_id() != m_ownerThread)
    {
        throw APPLICATION_EXCEPTION(ApplicationErrorCode::WrongThread, "Application {} must run on its Run thread.", operation);
    }
}

void Application::VerifyRunning(std::string_view operation) const
{
    VerifyOwnerThread(operation);
    if (!m_running.load(std::memory_order_acquire) || !m_runtime.has_value())
    {
        throw APPLICATION_EXCEPTION(ApplicationErrorCode::InvalidState, "Application {} requires an active Run call.", operation);
    }
}

void Application::VerifyCanCreateWork(std::string_view operation) const
{
    VerifyRunning(operation);
    if (m_shutdownRequested)
    {
        throw APPLICATION_EXCEPTION(ApplicationErrorCode::InvalidState, "Cannot perform Application {} after shutdown begins.", operation);
    }
}

ponder::core::VoidResult Application::ValidateDialogOperation(std::string_view operation, bool canCreateWork) const
{
    if (!m_running.load(std::memory_order_acquire))
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{ToErrorCode(ApplicationErrorCode::InvalidState),
                                                                       std::format("Application {} requires an active Run call.", operation)});
    }
    if (std::this_thread::get_id() != m_ownerThread)
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{ToErrorCode(ApplicationErrorCode::WrongThread),
                                                                       std::format("Application {} must run on its Run thread.", operation)});
    }
    if (!m_runtime.has_value())
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{ToErrorCode(ApplicationErrorCode::InvalidState),
                                                                       std::format("Application {} requires an active Run call.", operation)});
    }
    if (canCreateWork && m_shutdownRequested)
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{
            ToErrorCode(ApplicationErrorCode::InvalidState), std::format("Cannot perform Application {} after shutdown begins.", operation)});
    }
    return ponder::core::VoidResult::Success();
}

ponder::core::VoidResult Application::ValidateDialogParent(std::optional<ponder::platform::WindowId> parentWindowId, std::string_view operation) const
{
    if (!parentWindowId.has_value())
    {
        return ponder::core::VoidResult::Success();
    }
    if (!parentWindowId->IsValid())
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{
            ToErrorCode(ApplicationErrorCode::InvalidArgument), std::format("Cannot perform {} with an invalid parent window ID.", operation)});
    }
    if (!m_windows.contains(*parentWindowId))
    {
        return ponder::core::VoidResult::FromError(
            ponder::core::Error{ToErrorCode(ApplicationErrorCode::NotFound),
                                std::format("Cannot perform {} because Application does not own parent window {}.", operation, *parentWindowId)});
    }
    if (m_windowsPendingClosure.contains(*parentWindowId))
    {
        return ponder::core::VoidResult::FromError(
            ponder::core::Error{ToErrorCode(ApplicationErrorCode::InvalidState),
                                std::format("Cannot perform {} with closing parent window {}.", operation, *parentWindowId)});
    }
    return ponder::core::VoidResult::Success();
}

void Application::DispatchEvent(const ponder::platform::PlatformEvent& event)
{
    std::visit(
        [this](const auto& value)
        {
            using Event = std::remove_cvref_t<decltype(value)>;

            if constexpr (requires { value.windowId; })
            {
                using WindowTarget = std::remove_cvref_t<decltype(value.windowId)>;
                if constexpr (std::same_as<WindowTarget, ponder::platform::WindowId>)
                {
                    if (!m_windows.contains(value.windowId))
                    {
                        return;
                    }
                }
                else if constexpr (std::same_as<WindowTarget, std::optional<ponder::platform::WindowId>>)
                {
                    if (value.windowId.has_value() && !m_windows.contains(*value.windowId))
                    {
                        return;
                    }
                }
            }

            if constexpr (std::same_as<Event, ponder::platform::QuitRequestedEvent>)
            {
                m_shutdownRequested = true;
                if (m_callbacksEnabled)
                {
                    OnQuitRequestedEvent(value);
                }
                WindowCloseAll();
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowCloseRequestedEvent>)
            {
                if (m_windows.find(value.windowId) == m_windows.end())
                {
                    return;
                }
                m_windowsPendingClosure.insert(value.windowId);
                if (!HasLogicallyOpenWindows())
                {
                    // Closing the last logical window commits the fixed exit policy before user code runs. The hook may inspect the borrowed window,
                    // but managed APIs cannot create replacement work to veto shutdown.
                    m_shutdownRequested = true;
                }
                if (m_callbacksEnabled)
                {
                    OnWindowCloseRequestedEvent(value);
                }
                MarkWindowForClosure(value.windowId);
                FinalizeWindowClosures();
                if (!HasLogicallyOpenWindows())
                {
                    BeginShutdown();
                }
            }
            else if constexpr (std::same_as<Event, ponder::platform::DialogCompletedEvent>)
            {
                try
                {
                    if (m_callbacksEnabled)
                    {
                        OnDialogCompletedEvent(value);
                    }
                }
                catch (...)
                {
                    const std::exception_ptr callbackFailure = std::current_exception();
                    try
                    {
                        FinalizeWindowClosures();
                    }
                    catch (const ponder::core::Exception& exception)
                    {
                        LOG_ERROR_CATEGORY(kLogCategory, "Failed to finalize window closures after a dialog callback failure: {}",
                                           exception.GetMessage());
                    }
                    catch (const std::exception& exception)
                    {
                        LOG_ERROR_CATEGORY(kLogCategory, "Failed to finalize window closures after a dialog callback failure: {}", exception.what());
                    }
                    catch (...)
                    {
                        LOG_ERROR_CATEGORY(kLogCategory, "Failed to finalize window closures after a dialog callback failure with an unknown error");
                    }
                    std::rethrow_exception(callbackFailure);
                }
                FinalizeWindowClosures();
            }
            else if (!m_callbacksEnabled)
            {
                return;
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowMovedEvent>)
            {
                OnWindowMovedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowLogicalSizeChangedEvent>)
            {
                OnWindowLogicalSizeChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowPixelSizeChangedEvent>)
            {
                OnWindowPixelSizeChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowFocusChangedEvent>)
            {
                OnWindowFocusChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowVisibilityChangedEvent>)
            {
                OnWindowVisibilityChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowStateChangedEvent>)
            {
                OnWindowStateChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowPresentationChangedEvent>)
            {
                OnWindowPresentationChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowDisplayChangedEvent>)
            {
                OnWindowDisplayChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowDisplayScaleChangedEvent>)
            {
                OnWindowDisplayScaleChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowPointerEnteredEvent>)
            {
                OnWindowPointerEnteredEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::WindowPointerLeftEvent>)
            {
                OnWindowPointerLeftEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayAddedEvent>)
            {
                OnDisplayAddedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayRemovedEvent>)
            {
                OnDisplayRemovedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayMovedEvent>)
            {
                OnDisplayMovedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayDesktopModeChangedEvent>)
            {
                OnDisplayDesktopModeChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayCurrentModeChangedEvent>)
            {
                OnDisplayCurrentModeChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayOrientationChangedEvent>)
            {
                OnDisplayOrientationChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayContentScaleChangedEvent>)
            {
                OnDisplayContentScaleChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DisplayUsableBoundsChangedEvent>)
            {
                OnDisplayUsableBoundsChangedEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::KeyboardKeyEvent>)
            {
                OnKeyboardKeyEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::TextInputEvent>)
            {
                OnTextInputEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::TextCompositionEvent>)
            {
                OnTextCompositionEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::MouseMotionEvent>)
            {
                OnMouseMotionEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::MouseButtonEvent>)
            {
                OnMouseButtonEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::MouseWheelEvent>)
            {
                OnMouseWheelEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DropBeginEvent>)
            {
                OnDropBeginEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DroppedFileEvent>)
            {
                OnDroppedFileEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DroppedTextEvent>)
            {
                OnDroppedTextEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DropPositionEvent>)
            {
                OnDropPositionEvent(value);
            }
            else if constexpr (std::same_as<Event, ponder::platform::DropCompleteEvent>)
            {
                OnDropCompleteEvent(value);
            }
            else
            {
                static_assert(kUnhandledEvent<Event>, "Every PlatformEvent alternative requires an explicit Application dispatch hook");
            }
        },
        event);
}

bool Application::DrainEvents()
{
    bool handledEvent = false;
    for (std::size_t eventCount = 0; eventCount < kMaximumEventBatchSize; ++eventCount)
    {
        std::optional<ponder::platform::PlatformEvent> event = m_runtime->EventPoll();
        if (!event.has_value())
        {
            break;
        }
        handledEvent = true;
        DispatchEvent(*event);
    }
    return handledEvent;
}

void Application::PollProcesses()
{
    std::vector<std::pair<BackgroundProcessId, ponder::platform::ProcessExitStatus>> completions;
    for (auto process = m_processes.begin(); process != m_processes.end();)
    {
        ponder::core::Result<std::optional<ponder::platform::ProcessExitStatus>> statusResult = process->second.TryWait();
        if (!statusResult)
        {
            LOG_WARNING_CATEGORY(kLogCategory, "Failed to observe background process {}: {}", process->first, statusResult.GetError());
            ++process;
            continue;
        }
        if (!statusResult->has_value())
        {
            ++process;
            continue;
        }

        const BackgroundProcessId id = process->first;
        const ponder::platform::ProcessExitStatus status = std::move(**statusResult);
        m_forceProcessTerminationOnExit.erase(id);
        process = m_processes.erase(process);
        completions.emplace_back(id, status);
    }

    if (m_callbacksEnabled)
    {
        for (const auto& [id, status] : completions)
        {
            OnProcessCompleted(id, status);
        }
    }
}

void Application::BeginShutdown()
{
    VerifyRunning("shutdown");
    m_shutdownRequested = true;
    WindowCloseAll();
}

void Application::MarkWindowForClosure(ponder::platform::WindowId id)
{
    const auto window = m_windows.find(id);
    if (window == m_windows.end())
    {
        return;
    }

    m_windowsPendingClosure.insert(id);
    if (window->second.IsVisible())
    {
        window->second.Hide();
    }
}

void Application::FinalizeWindowClosures()
{
    std::unordered_set<ponder::platform::WindowId> leasedParents;
    const std::vector<ponder::platform::DialogRequestInfo> pendingRequests = m_runtime->DialogGetPending();
    for (const ponder::platform::DialogRequestInfo& request : pendingRequests)
    {
        if (request.parentWindowId.has_value())
        {
            leasedParents.insert(*request.parentWindowId);
        }
    }

    for (auto id = m_windowsPendingClosure.begin(); id != m_windowsPendingClosure.end();)
    {
        if (leasedParents.contains(*id))
        {
            ++id;
            continue;
        }
        m_windows.erase(*id);
        id = m_windowsPendingClosure.erase(id);
    }
}

bool Application::HasLogicallyOpenWindows() const noexcept
{
    return std::ranges::any_of(m_windows,
                               [this](const auto& entry)
                               {
                                   return !m_windowsPendingClosure.contains(entry.first);
                               });
}

void Application::DrainShutdown()
{
    std::exception_ptr deferredFailure;
    const auto containCleanupFailure = [this, &deferredFailure](auto&& action) noexcept
    {
        try
        {
            std::forward<decltype(action)>(action)();
        }
        catch (...)
        {
            m_callbacksEnabled = false;
            deferredFailure = deferredFailure == nullptr ? std::current_exception() : deferredFailure;
        }
    };

    containCleanupFailure(
        [this]
        {
            WindowCloseAll();
        });
    while (!m_windows.empty() || m_runtime->DialogHasPending())
    {
        containCleanupFailure(
            [this]
            {
                WindowCloseAll();
            });
        containCleanupFailure(
            [this]
            {
                static_cast<void>(DrainEvents());
            });
        containCleanupFailure(
            [this]
            {
                FinalizeWindowClosures();
            });
        containCleanupFailure(
            [this]
            {
                PollProcesses();
            });
        if (m_windows.empty() && !m_runtime->DialogHasPending())
        {
            break;
        }

        const ponder::core::Duration waitDuration = m_processes.empty() ? kIdleWait : kProcessObservationWait;
        if (std::optional<ponder::platform::PlatformEvent> event = m_runtime->EventWait(waitDuration); event.has_value())
        {
            containCleanupFailure(
                [this, &event]
                {
                    DispatchEvent(*event);
                });
        }
    }
    containCleanupFailure(
        [this]
        {
            FinalizeWindowClosures();
        });
    if (deferredFailure != nullptr)
    {
        std::rethrow_exception(deferredFailure);
    }
}

void Application::ShutdownProcesses()
{
    std::exception_ptr deferredFailure;
    try
    {
        PollProcesses();
    }
    catch (...)
    {
        m_callbacksEnabled = false;
        deferredFailure = std::current_exception();
    }

    while (!m_processes.empty())
    {
        auto process = m_processes.begin();
        const BackgroundProcessId id = process->first;
        const bool forceTermination = m_forceProcessTerminationOnExit.contains(id);

        try
        {
            if (forceTermination)
            {
                ponder::core::VoidResult terminateResult = process->second.Terminate(ponder::platform::ProcessTerminationMode::Force);
                if (!terminateResult)
                {
                    LOG_ERROR_CATEGORY(kLogCategory, "Failed to force background process {} to terminate: {}. Waiting for natural exit.", id,
                                       terminateResult.GetError());
                }

                ponder::core::Result<ponder::platform::ProcessExitStatus> waitResult = process->second.Wait();
                if (!waitResult)
                {
                    LOG_ERROR_CATEGORY(kLogCategory, "Failed to confirm background process {} exit: {}. Detaching platform tracking.", id,
                                       waitResult.GetError());
                }
                else if (m_callbacksEnabled)
                {
                    OnProcessCompleted(id, waitResult.GetValue());
                }
            }
            else
            {
                LOG_INFO_CATEGORY(kLogCategory, "Detaching background process {} because force termination on Application exit is disabled", id);
                if (m_callbacksEnabled)
                {
                    OnProcessDetached(id);
                }
            }
        }
        catch (...)
        {
            m_callbacksEnabled = false;
            deferredFailure = deferredFailure == nullptr ? std::current_exception() : deferredFailure;
        }

        m_forceProcessTerminationOnExit.erase(id);
        m_processes.erase(id);
    }

    if (deferredFailure != nullptr)
    {
        std::rethrow_exception(deferredFailure);
    }
}

void Application::ActivateWake()
{
    const std::scoped_lock lock{m_wakeMutex};
    PONDER_VERIFY(m_wakeRuntime == nullptr, "Application wake runtime is already active");
    m_wakeRuntime = std::addressof(*m_runtime);
}

void Application::DeactivateWake() noexcept
{
    const std::scoped_lock lock{m_wakeMutex};
    m_wakeRuntime = nullptr;
}

void Application::PrePlatformInitialization(ponder::platform::Runtime&) {}

void Application::OnStart() {}

void Application::OnUpdate(ponder::core::Duration) {}

void Application::OnRender() {}

void Application::OnStop() {}

void Application::OnQuitRequestedEvent(const ponder::platform::QuitRequestedEvent&) {}

void Application::OnWindowCloseRequestedEvent(const ponder::platform::WindowCloseRequestedEvent&) {}

void Application::OnWindowMovedEvent(const ponder::platform::WindowMovedEvent&) {}

void Application::OnWindowLogicalSizeChangedEvent(const ponder::platform::WindowLogicalSizeChangedEvent&) {}

void Application::OnWindowPixelSizeChangedEvent(const ponder::platform::WindowPixelSizeChangedEvent&) {}

void Application::OnWindowFocusChangedEvent(const ponder::platform::WindowFocusChangedEvent&) {}

void Application::OnWindowVisibilityChangedEvent(const ponder::platform::WindowVisibilityChangedEvent&) {}

void Application::OnWindowStateChangedEvent(const ponder::platform::WindowStateChangedEvent&) {}

void Application::OnWindowPresentationChangedEvent(const ponder::platform::WindowPresentationChangedEvent&) {}

void Application::OnWindowDisplayChangedEvent(const ponder::platform::WindowDisplayChangedEvent&) {}

void Application::OnWindowDisplayScaleChangedEvent(const ponder::platform::WindowDisplayScaleChangedEvent&) {}

void Application::OnWindowPointerEnteredEvent(const ponder::platform::WindowPointerEnteredEvent&) {}

void Application::OnWindowPointerLeftEvent(const ponder::platform::WindowPointerLeftEvent&) {}

void Application::OnDisplayAddedEvent(const ponder::platform::DisplayAddedEvent&) {}

void Application::OnDisplayRemovedEvent(const ponder::platform::DisplayRemovedEvent&) {}

void Application::OnDisplayMovedEvent(const ponder::platform::DisplayMovedEvent&) {}

void Application::OnDisplayDesktopModeChangedEvent(const ponder::platform::DisplayDesktopModeChangedEvent&) {}

void Application::OnDisplayCurrentModeChangedEvent(const ponder::platform::DisplayCurrentModeChangedEvent&) {}

void Application::OnDisplayOrientationChangedEvent(const ponder::platform::DisplayOrientationChangedEvent&) {}

void Application::OnDisplayContentScaleChangedEvent(const ponder::platform::DisplayContentScaleChangedEvent&) {}

void Application::OnDisplayUsableBoundsChangedEvent(const ponder::platform::DisplayUsableBoundsChangedEvent&) {}

void Application::OnKeyboardKeyEvent(const ponder::platform::KeyboardKeyEvent&) {}

void Application::OnTextInputEvent(const ponder::platform::TextInputEvent&) {}

void Application::OnTextCompositionEvent(const ponder::platform::TextCompositionEvent&) {}

void Application::OnMouseMotionEvent(const ponder::platform::MouseMotionEvent&) {}

void Application::OnMouseButtonEvent(const ponder::platform::MouseButtonEvent&) {}

void Application::OnMouseWheelEvent(const ponder::platform::MouseWheelEvent&) {}

void Application::OnDropBeginEvent(const ponder::platform::DropBeginEvent&) {}

void Application::OnDroppedFileEvent(const ponder::platform::DroppedFileEvent&) {}

void Application::OnDroppedTextEvent(const ponder::platform::DroppedTextEvent&) {}

void Application::OnDropPositionEvent(const ponder::platform::DropPositionEvent&) {}

void Application::OnDropCompleteEvent(const ponder::platform::DropCompleteEvent&) {}

void Application::OnDialogCompletedEvent(const ponder::platform::DialogCompletedEvent&) {}

void Application::OnProcessCompleted(BackgroundProcessId, const ponder::platform::ProcessExitStatus&) {}

void Application::OnProcessDetached(BackgroundProcessId) {}
} // namespace ponder::application
