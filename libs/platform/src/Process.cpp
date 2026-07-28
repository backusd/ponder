#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/String.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Process.hpp>

#include <SDL3/SDL_process.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <new>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "ProcessBackend.hpp"
#include "SdlError.hpp"

namespace ponder::platform
{
namespace
{
constexpr int kSdlUnknownExitCode{-255};
constexpr ponder::core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr ponder::core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr ponder::core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] ponder::core::Error MakeInvalidArgumentError(std::string message, std::source_location location = std::source_location::current())
{
    return ponder::core::Error{kInvalidArgumentCode, std::move(message), location};
}

[[nodiscard]] ponder::core::Error MakeUnsupportedTerminationError(ProcessTerminationMode mode,
                                                                  std::source_location location = std::source_location::current())
{
    const std::string_view modeName = mode == ProcessTerminationMode::Force ? "forced" : "graceful";
    return ponder::core::Error{kUnsupportedCode, std::format("Process {} termination is unsupported by the active backend.", modeName), location};
}

[[nodiscard]] detail::BackendProcessExitStatus MakeSdlProcessExitStatus(int exitCode) noexcept
{
#if defined(_WIN32)
    return detail::BackendProcessExitStatus{.kind = detail::BackendProcessExitKind::Normal, .value = static_cast<std::uint32_t>(exitCode)};
#else
    if (exitCode == kSdlUnknownExitCode)
    {
        return detail::BackendProcessExitStatus{.kind = detail::BackendProcessExitKind::Unknown};
    }

    if (exitCode < 0)
    {
        const long long signal = -static_cast<long long>(exitCode);
        if (!std::in_range<int>(signal))
        {
            return detail::BackendProcessExitStatus{.kind = detail::BackendProcessExitKind::Unknown};
        }
        return detail::BackendProcessExitStatus{.kind = detail::BackendProcessExitKind::Signal, .value = static_cast<std::uint32_t>(signal)};
    }

    return detail::BackendProcessExitStatus{.kind = detail::BackendProcessExitKind::Normal, .value = static_cast<std::uint32_t>(exitCode)};
#endif
}

[[nodiscard]] ProcessExitStatus MakeProcessExitStatus(detail::BackendProcessExitStatus status)
{
    switch (status.kind)
    {
    case detail::BackendProcessExitKind::Normal:
        return ProcessNormalExit{.exitCode = status.value};
    case detail::BackendProcessExitKind::Signal:
        PONDER_VERIFY(status.value > 0 && std::in_range<int>(status.value), "Process backend returned an invalid signal termination value");
        return ProcessSignalTermination{.signal = static_cast<int>(status.value)};
    case detail::BackendProcessExitKind::Unknown:
        return ProcessUnknownTermination{};
    }

    PONDER_VERIFY(false, "Process backend returned an invalid exit-status kind");
    return ProcessUnknownTermination{};
}

[[nodiscard]] ponder::core::Result<std::string> ValidateExecutablePath(const std::filesystem::path& executable)
{
    std::string executableText = ::pond::io::PathToUtf8(executable);
    if (executableText.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(executableText))
    {
        return ponder::core::Result<std::string>::FromError(
            MakeInvalidArgumentError("Process executable must be non-empty UTF-8 without embedded nulls."));
    }

    return executableText;
}

[[nodiscard]] ponder::core::VoidResult ValidateArgument(std::string_view argument, std::size_t index)
{
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(argument))
    {
        return ponder::core::VoidResult::FromError(
            MakeInvalidArgumentError(std::format("Process argument {} must be UTF-8 without embedded nulls.", index)));
    }

    return ponder::core::VoidResult::Success();
}

[[nodiscard]] ponder::core::Result<std::vector<std::string>> BuildArgumentStorage(const ProcessDesc& desc)
{
    ponder::core::Result<std::string> executable = ValidateExecutablePath(desc.executable);
    if (!executable)
    {
        return ponder::core::Result<std::vector<std::string>>::FromError(std::move(executable).GetError());
    }

    std::vector<std::string> arguments;
    arguments.reserve(desc.arguments.size() + 1U);
    arguments.push_back(std::move(executable).GetValue());

    for (std::size_t index = 0; index < desc.arguments.size(); ++index)
    {
        ponder::core::VoidResult validation = ValidateArgument(desc.arguments[index], index);
        if (!validation)
        {
            return ponder::core::Result<std::vector<std::string>>::FromError(std::move(validation).GetError());
        }
        arguments.push_back(desc.arguments[index]);
    }

    return arguments;
}

[[nodiscard]] std::vector<const char*> MakeArgumentPointers(const std::vector<std::string>& arguments)
{
    std::vector<const char*> pointers;
    pointers.reserve(arguments.size() + 1U);
    for (const std::string& argument : arguments)
    {
        pointers.push_back(argument.c_str());
    }
    pointers.push_back(nullptr);
    return pointers;
}

void VerifyBackend(const detail::PlatformProcessBackend& backend)
{
    PONDER_VERIFY(backend.create != nullptr, "Platform process backend is missing create");
    PONDER_VERIFY(backend.wait != nullptr, "Platform process backend is missing wait");
    PONDER_VERIFY(backend.kill != nullptr, "Platform process backend is missing kill");
    PONDER_VERIFY(backend.destroy != nullptr, "Platform process backend is missing destroy");
}

void VerifyReaperBackend(const detail::PlatformProcessReaperBackend& backend)
{
    PONDER_VERIFY(backend.ensureStarted != nullptr, "Platform process reaper backend is missing ensureStarted");
    PONDER_VERIFY(backend.enqueue != nullptr, "Platform process reaper backend is missing enqueue");
}

[[nodiscard]] void* SdlCreateProcess(void*, const char* const* arguments)
{
    return SDL_CreateProcess(arguments, false);
}

[[nodiscard]] bool SdlWaitProcess(void*, void* process, bool block, detail::BackendProcessExitStatus* status)
{
    int exitCode = kSdlUnknownExitCode;
    if (!SDL_WaitProcess(static_cast<SDL_Process*>(process), block, &exitCode))
    {
        return false;
    }

    if (status != nullptr)
    {
        *status = MakeSdlProcessExitStatus(exitCode);
    }
    return true;
}

[[nodiscard]] detail::BackendProcessKillResult SdlKillProcess(void*, void* process, bool force)
{
    return SDL_KillProcess(static_cast<SDL_Process*>(process), force) ? detail::BackendProcessKillResult::Succeeded
                                                                      : detail::BackendProcessKillResult::Failed;
}

void SdlDestroyProcess(void*, void* process) noexcept
{
    SDL_DestroyProcess(static_cast<SDL_Process*>(process));
}

[[nodiscard]] bool TryCleanupAbandonedProcess(detail::AbandonedProcessEntry& entry) noexcept
{
    PONDER_VERIFY(entry.process != nullptr, "Cannot clean up an empty abandoned process");
    PONDER_VERIFY(entry.backend.wait != nullptr, "Abandoned process backend is missing nonblocking wait");
    PONDER_VERIFY(entry.backend.destroy != nullptr, "Abandoned process backend is missing destruction");

    try
    {
        if (!entry.exitConfirmed)
        {
            if (!entry.backend.wait(entry.backend.context, entry.process, false, nullptr))
            {
                return false;
            }
            entry.exitConfirmed = true;
        }
    }
    catch (const ponder::core::Exception&)
    {
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }

    entry.backend.destroy(entry.backend.context, entry.process);
    entry.process = nullptr;
    return true;
}

void PauseAbandonedProcessPolling() noexcept
{
    try
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    catch (const ponder::core::Exception&)
    {
    }
    catch (const std::exception&)
    {
    }
    catch (...)
    {
    }
}

class AbandonedProcessReaper final
{
public:
    AbandonedProcessReaper() :
        m_worker(
            [this]() noexcept
            {
                Run();
            })
    {
    }

    void Enqueue(detail::AbandonedProcessEntry* entry) noexcept
    {
        PONDER_VERIFY(entry != nullptr, "Cannot enqueue a null abandoned process");
        PONDER_VERIFY(entry->process != nullptr, "Cannot enqueue an empty abandoned process");

        detail::AbandonedProcessEntry* head = m_pending.load(std::memory_order_relaxed);
        do
        {
            entry->next = head;
        } while (!m_pending.compare_exchange_weak(head, entry, std::memory_order_release, std::memory_order_relaxed));
        m_pending.notify_one();
    }

private:
    void Run() noexcept
    {
        detail::AbandonedProcessEntry* active{};
        while (true)
        {
            detail::AbandonedProcessEntry* incoming = m_pending.exchange(nullptr, std::memory_order_acquire);
            while (incoming != nullptr)
            {
                detail::AbandonedProcessEntry* const next = incoming->next;
                incoming->next = active;
                active = incoming;
                incoming = next;
            }

            detail::AbandonedProcessEntry** current = &active;
            while (*current != nullptr)
            {
                detail::AbandonedProcessEntry* const entry = *current;
                if (!TryCleanupAbandonedProcess(*entry))
                {
                    current = &entry->next;
                    continue;
                }

                *current = entry->next;
                std::unique_ptr<detail::AbandonedProcessEntry> completed{entry};
            }

            if (active != nullptr)
            {
                PauseAbandonedProcessPolling();
            }
            else
            {
                m_pending.wait(nullptr, std::memory_order_acquire);
            }
        }
    }

    std::atomic<detail::AbandonedProcessEntry*> m_pending{};
    std::jthread m_worker;
};

struct ProcessLifetimeReaperStorage final
{
    alignas(AbandonedProcessReaper) std::byte bytes[sizeof(AbandonedProcessReaper)]{};
    std::once_flag initialize;
};

// The reaper is intentionally constructed in constant-initialized storage and
// never destroyed by C++ static teardown. This keeps late/static Process
// destruction non-blocking and leaves final thread teardown to process exit.
static_assert(std::is_trivially_destructible_v<ProcessLifetimeReaperStorage>);
constinit ProcessLifetimeReaperStorage processLifetimeReaperStorage;

[[nodiscard]] AbandonedProcessReaper& GetAbandonedProcessReaper()
{
    auto* const reaper = reinterpret_cast<AbandonedProcessReaper*>(processLifetimeReaperStorage.bytes);
    std::call_once(processLifetimeReaperStorage.initialize,
                   [reaper]()
                   {
                       static_cast<void>(std::construct_at(reaper));
                   });
    return *std::launder(reaper);
}

[[nodiscard]] bool EnsureAbandonedProcessReaper(void* context) noexcept
{
    return context != nullptr;
}

void EnqueueAbandonedProcess(void* context, detail::AbandonedProcessEntry* entry) noexcept
{
    PONDER_VERIFY(context != nullptr, "Abandoned-process cleanup service is unavailable");
    static_cast<AbandonedProcessReaper*>(context)->Enqueue(entry);
}

const detail::PlatformProcessBackend kSdlProcessBackend{.context = nullptr,
                                                        .create = SdlCreateProcess,
                                                        .wait = SdlWaitProcess,
                                                        .kill = SdlKillProcess,
                                                        .destroy = SdlDestroyProcess};
const detail::PlatformProcessReaperBackend kSdlProcessReaperBackend{
    .context = nullptr,
    .ensureStarted = EnsureAbandonedProcessReaper,
    .enqueue = EnqueueAbandonedProcess,
};
} // namespace

namespace detail
{
class ProcessState final
{
public:
    ProcessState(PlatformProcessBackend backend, PlatformProcessReaperBackend reaperBackend) :
        m_backend(backend),
        m_reaperBackend(reaperBackend),
        m_abandonedProcess(std::make_unique<AbandonedProcessEntry>(AbandonedProcessEntry{.backend = backend})),
        m_launchingThread(std::this_thread::get_id())
    {
    }

    ~ProcessState() noexcept
    {
        VerifyLaunchingThread("destruction");
        if (m_process != nullptr)
        {
            ReleaseProcess();
        }
    }

    ProcessState(const ProcessState&) = delete;
    ProcessState& operator=(const ProcessState&) = delete;
    ProcessState(ProcessState&&) = delete;
    ProcessState& operator=(ProcessState&&) = delete;

    void AttachProcess(void* process) noexcept
    {
        PONDER_VERIFY(process != nullptr, "Cannot attach a null process");
        PONDER_VERIFY(m_process == nullptr, "Process state already has a process");
        PONDER_VERIFY(m_abandonedProcess != nullptr, "Process state has no preallocated cleanup entry");
        m_process = process;
        m_abandonedProcess->process = process;
    }

    [[nodiscard]] ponder::core::Result<ProcessExitStatus> Wait()
    {
        VerifyOperationalState("wait");
        detail::BackendProcessExitStatus status{.kind = static_cast<detail::BackendProcessExitKind>(-1)};
        if (!m_backend.wait(m_backend.context, m_process, true, &status))
        {
            return ponder::core::Result<ProcessExitStatus>::FromError(CaptureSdlFailure(kBackendFailureCode, "SDL_WaitProcess", "process"));
        }

        m_abandonedProcess->exitConfirmed = true;
        return MakeProcessExitStatus(status);
    }

    [[nodiscard]] ponder::core::VoidResult Terminate(ProcessTerminationMode mode)
    {
        VerifyOperationalState("termination");
        bool force{};
        switch (mode)
        {
        case ProcessTerminationMode::GracefulPreferred:
            break;
        case ProcessTerminationMode::Force:
            force = true;
            break;
        default:
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Process termination mode is invalid.");
        }

        const BackendProcessKillResult result = m_backend.kill(m_backend.context, m_process, force);
        switch (result)
        {
        case BackendProcessKillResult::Succeeded:
            return ponder::core::VoidResult::Success();
        case BackendProcessKillResult::Unsupported:
            if (mode == ProcessTerminationMode::GracefulPreferred)
            {
                return Terminate(ProcessTerminationMode::Force);
            }
            return ponder::core::VoidResult::FromError(MakeUnsupportedTerminationError(mode));
        case BackendProcessKillResult::Failed:
            return ponder::core::VoidResult::FromError(CaptureSdlFailure(kBackendFailureCode, "SDL_KillProcess", "process"));
        }

        PONDER_VERIFY(false, "Process backend returned an invalid termination result");
        return ponder::core::VoidResult::Success();
    }

private:
    void ReleaseProcess() noexcept
    {
        PONDER_VERIFY(m_abandonedProcess != nullptr, "Running process has no preallocated cleanup entry");
        PONDER_VERIFY(m_abandonedProcess->process == m_process, "Process cleanup entry does not own the tracked process");

        m_process = nullptr;
        if (TryCleanupAbandonedProcess(*m_abandonedProcess))
        {
            return;
        }

        m_reaperBackend.enqueue(m_reaperBackend.context, m_abandonedProcess.release());
    }

    void VerifyLaunchingThread(std::string_view operation) const
    {
        PONDER_VERIFY(std::this_thread::get_id() == m_launchingThread, "Process {} must run on its launching thread", operation);
    }

    void VerifyOperationalState(std::string_view operation) const
    {
        VerifyLaunchingThread(operation);
        PONDER_VERIFY(m_process != nullptr, "Process {} has no tracked backend process", operation);
        PONDER_VERIFY(m_abandonedProcess != nullptr, "Process {} has no preallocated cleanup entry", operation);
        PONDER_VERIFY(m_abandonedProcess->process == m_process, "Process {} cleanup entry does not own the tracked process", operation);
    }

    PlatformProcessBackend m_backend;
    PlatformProcessReaperBackend m_reaperBackend;
    std::unique_ptr<AbandonedProcessEntry> m_abandonedProcess;
    void* m_process{};
    std::thread::id m_launchingThread;
};

struct ProcessFactory
{
    [[nodiscard]] static Process Create(std::unique_ptr<ProcessState> state) noexcept
    {
        return Process{std::move(state)};
    }
};

PlatformProcessBackend GetPlatformProcessBackend() noexcept
{
    return kSdlProcessBackend;
}

PlatformProcessReaperBackend GetPlatformProcessReaperBackend()
{
    PlatformProcessReaperBackend backend = kSdlProcessReaperBackend;
    backend.context = &GetAbandonedProcessReaper();
    return backend;
}

[[nodiscard]] ponder::core::Result<Process> LaunchProcessWithArguments(std::vector<std::string> argumentStorage, PlatformProcessBackend backend,
                                                                       PlatformProcessReaperBackend reaperBackend)
{
    if (!reaperBackend.ensureStarted(reaperBackend.context))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Failed to start the asynchronous process cleanup service.");
    }

    auto state = std::make_unique<ProcessState>(backend, reaperBackend);
    const std::vector<const char*> arguments = MakeArgumentPointers(argumentStorage);
    void* const process = backend.create(backend.context, arguments.data());
    if (process == nullptr)
    {
        return ponder::core::Result<Process>::FromError(CaptureSdlFailure(kBackendFailureCode, "SDL_CreateProcess", "process"));
    }

    state->AttachProcess(process);
    return ProcessFactory::Create(std::move(state));
}

ponder::core::Result<Process> LaunchProcess(const ProcessDesc& desc, PlatformProcessBackend backend)
{
    VerifyBackend(backend);
    ponder::core::Result<std::vector<std::string>> argumentStorage = BuildArgumentStorage(desc);
    if (!argumentStorage)
    {
        return ponder::core::Result<Process>::FromError(std::move(argumentStorage).GetError());
    }

    const PlatformProcessReaperBackend reaperBackend = GetPlatformProcessReaperBackend();
    VerifyReaperBackend(reaperBackend);
    return LaunchProcessWithArguments(std::move(argumentStorage).GetValue(), backend, reaperBackend);
}

ponder::core::Result<Process> LaunchProcess(const ProcessDesc& desc, PlatformProcessBackend backend, PlatformProcessReaperBackend reaperBackend)
{
    VerifyBackend(backend);
    VerifyReaperBackend(reaperBackend);
    ponder::core::Result<std::vector<std::string>> argumentStorage = BuildArgumentStorage(desc);
    if (!argumentStorage)
    {
        return ponder::core::Result<Process>::FromError(std::move(argumentStorage).GetError());
    }

    return LaunchProcessWithArguments(std::move(argumentStorage).GetValue(), backend, reaperBackend);
}
} // namespace detail

Process::Process(std::unique_ptr<detail::ProcessState> state) noexcept :
    m_state(std::move(state))
{
}

Process::~Process() noexcept = default;
Process::Process(Process&&) noexcept = default;
Process& Process::operator=(Process&&) noexcept = default;

ponder::core::Result<ProcessExitStatus> Process::Wait()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Process");
    return m_state->Wait();
}

ponder::core::VoidResult Process::Terminate(ProcessTerminationMode mode)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Process");
    return m_state->Terminate(mode);
}

ponder::core::Result<Process> LaunchProcess(const ProcessDesc& desc)
{
    return detail::LaunchProcess(desc, detail::GetPlatformProcessBackend());
}
} // namespace ponder::platform
