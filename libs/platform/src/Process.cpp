#include <ponder/core/Assert.hpp>
#include <ponder/core/String.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Process.hpp>

#include <SDL3/SDL_process.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "ProcessBackend.hpp"
#include "SdlError.hpp"

namespace pond::platform
{
namespace
{
constexpr int kSdlUnknownExitCode{-255};
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] core::Error MakeInvalidArgumentError(
    std::string message, std::source_location location = std::source_location::current())
{
    return core::Error{kInvalidArgumentCode, std::move(message), location};
}

[[nodiscard]] core::Error MakeUnsupportedTerminationError(
    ProcessTerminationMode mode, std::source_location location = std::source_location::current())
{
    const std::string_view modeName = mode == ProcessTerminationMode::Force ? "forced" : "graceful";
    std::string message;
    message.reserve(modeName.size() + 48);
    message.append("Process ");
    message.append(modeName);
    message.append(" termination is unsupported by the active backend.");
    return core::Error{kUnsupportedCode, std::move(message), location};
}

[[nodiscard]] ProcessExitStatus MakeProcessExitStatus(int exitCode)
{
    if (exitCode == kSdlUnknownExitCode)
    {
        return ProcessUnknownTermination{};
    }

    if (exitCode < 0)
    {
        return ProcessSignalTermination{.signal = -exitCode};
    }

    return ProcessNormalExit{.exitCode = exitCode};
}

[[nodiscard]] core::Result<std::string> ValidateExecutablePath(
    const std::filesystem::path& executable)
{
    std::string executableText = io::PathToUtf8(executable);
    if (executableText.empty() || !core::IsValidUtf8WithoutEmbeddedNull(executableText))
    {
        return core::Result<std::string>::FromError(MakeInvalidArgumentError(
            "Process executable must be non-empty UTF-8 without embedded nulls."));
    }

    return executableText;
}

[[nodiscard]] core::VoidResult ValidateArgument(std::string_view argument, std::size_t index)
{
    if (!core::IsValidUtf8WithoutEmbeddedNull(argument))
    {
        return core::VoidResult::FromError(
            MakeInvalidArgumentError("Process argument " + std::to_string(index) +
                                     " must be UTF-8 without embedded nulls."));
    }

    return core::VoidResult::Success();
}

[[nodiscard]] core::Result<std::vector<std::string>> BuildArgumentStorage(const ProcessDesc& desc)
{
    core::Result<std::string> executable = ValidateExecutablePath(desc.executable);
    if (!executable.HasValue())
    {
        return core::Result<std::vector<std::string>>::FromError(std::move(executable).GetError());
    }

    std::vector<std::string> arguments;
    arguments.reserve(desc.arguments.size() + 1U);
    arguments.push_back(std::move(executable).GetValue());

    for (std::size_t index = 0; index < desc.arguments.size(); ++index)
    {
        core::VoidResult validation = ValidateArgument(desc.arguments[index], index);
        if (!validation.HasValue())
        {
            return core::Result<std::vector<std::string>>::FromError(
                std::move(validation).GetError());
        }
        arguments.push_back(desc.arguments[index]);
    }

    return arguments;
}

[[nodiscard]] std::vector<const char*> MakeArgumentPointers(
    const std::vector<std::string>& arguments)
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

[[nodiscard]] void* SdlCreateProcess(void*, const char* const* arguments)
{
    return SDL_CreateProcess(arguments, false);
}

[[nodiscard]] bool SdlWaitProcess(void*, void* process, bool block, int* exitCode)
{
    return SDL_WaitProcess(static_cast<SDL_Process*>(process), block, exitCode);
}

[[nodiscard]] detail::BackendProcessKillResult SdlKillProcess(void*, void* process, bool force)
{
    return SDL_KillProcess(static_cast<SDL_Process*>(process), force)
               ? detail::BackendProcessKillResult::Succeeded
               : detail::BackendProcessKillResult::Failed;
}

void SdlDestroyProcess(void*, void* process) noexcept
{
    SDL_DestroyProcess(static_cast<SDL_Process*>(process));
}

const detail::PlatformProcessBackend kSdlProcessBackend{.context = nullptr,
                                                        .create = SdlCreateProcess,
                                                        .wait = SdlWaitProcess,
                                                        .kill = SdlKillProcess,
                                                        .destroy = SdlDestroyProcess};
} // namespace

namespace detail
{
class ProcessState final
{
public:
    ProcessState(PlatformProcessBackend backend, void* process) noexcept
        : m_backend(backend), m_process(process), m_launchingThread(std::this_thread::get_id())
    {
    }

    ~ProcessState() noexcept
    {
        VerifyLaunchingThread("destruction");
        if (m_process != nullptr)
        {
            m_backend.destroy(m_backend.context, m_process);
        }
    }

    ProcessState(const ProcessState&) = delete;
    ProcessState& operator=(const ProcessState&) = delete;
    ProcessState(ProcessState&&) = delete;
    ProcessState& operator=(ProcessState&&) = delete;

    [[nodiscard]] core::Result<ProcessExitStatus> Wait()
    {
        VerifyLaunchingThread("wait");
        int exitCode = kSdlUnknownExitCode;
        if (!m_backend.wait(m_backend.context, m_process, true, &exitCode))
        {
            return core::Result<ProcessExitStatus>::FromError(
                CaptureSdlFailure(kBackendFailureCode, "SDL_WaitProcess", "process"));
        }

        return MakeProcessExitStatus(exitCode);
    }

    [[nodiscard]] core::VoidResult Terminate(ProcessTerminationMode mode)
    {
        VerifyLaunchingThread("termination");
        const bool force = mode == ProcessTerminationMode::Force;
        const BackendProcessKillResult result = m_backend.kill(m_backend.context, m_process, force);
        if (result == BackendProcessKillResult::Succeeded)
        {
            return core::VoidResult::Success();
        }
        if (result == BackendProcessKillResult::Unsupported)
        {
            if (mode == ProcessTerminationMode::GracefulPreferred)
            {
                return Terminate(ProcessTerminationMode::Force);
            }
            return core::VoidResult::FromError(MakeUnsupportedTerminationError(mode));
        }

        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_KillProcess", "process"));
    }

private:
    void VerifyLaunchingThread(std::string_view operation) const
    {
        PONDER_VERIFY(std::this_thread::get_id() == m_launchingThread,
                      "Process {} must run on its launching thread", operation);
    }

    PlatformProcessBackend m_backend;
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

core::Result<Process> LaunchProcess(const ProcessDesc& desc, PlatformProcessBackend backend)
{
    VerifyBackend(backend);
    core::Result<std::vector<std::string>> argumentStorage = BuildArgumentStorage(desc);
    if (!argumentStorage.HasValue())
    {
        return core::Result<Process>::FromError(std::move(argumentStorage).GetError());
    }

    const std::vector<const char*> arguments = MakeArgumentPointers(argumentStorage.GetValue());
    void* const process = backend.create(backend.context, arguments.data());
    if (process == nullptr)
    {
        return core::Result<Process>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_CreateProcess", "process"));
    }

    return ProcessFactory::Create(std::make_unique<ProcessState>(backend, process));
}
} // namespace detail

Process::Process(std::unique_ptr<detail::ProcessState> state) noexcept : m_state(std::move(state))
{
}

Process::~Process() noexcept = default;
Process::Process(Process&&) noexcept = default;
Process& Process::operator=(Process&&) noexcept = default;

core::Result<ProcessExitStatus> Process::Wait()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Process");
    return m_state->Wait();
}

core::VoidResult Process::Terminate(ProcessTerminationMode mode)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Process");
    return m_state->Terminate(mode);
}

core::Result<Process> LaunchProcess(const ProcessDesc& desc)
{
    return detail::LaunchProcess(desc, detail::GetPlatformProcessBackend());
}
} // namespace pond::platform
