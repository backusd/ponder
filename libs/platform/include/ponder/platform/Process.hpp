#pragma once

#include <ponder/core/Result.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace pond::platform
{
struct ProcessDesc;
class Process;

namespace detail
{
class ProcessState;
struct PlatformProcessBackend;
struct ProcessFactory;

[[nodiscard]] core::Result<Process> LaunchProcess(
    const ProcessDesc& desc, PlatformProcessBackend backend);
} // namespace detail

struct ProcessDesc final
{
    std::filesystem::path executable;
    std::vector<std::string> arguments;

    [[nodiscard]] friend bool operator==(const ProcessDesc& lhs,
                                         const ProcessDesc& rhs) = default;
};

struct ProcessNormalExit final
{
    int exitCode{};

    [[nodiscard]] friend constexpr bool operator==(ProcessNormalExit,
                                                  ProcessNormalExit) noexcept = default;
};

struct ProcessSignalTermination final
{
    int signal{};

    [[nodiscard]] friend constexpr bool operator==(ProcessSignalTermination,
                                                  ProcessSignalTermination) noexcept = default;
};

struct ProcessUnknownTermination final
{
    [[nodiscard]] friend constexpr bool operator==(ProcessUnknownTermination,
                                                  ProcessUnknownTermination) noexcept = default;
};

using ProcessExitStatus =
    std::variant<ProcessNormalExit, ProcessSignalTermination, ProcessUnknownTermination>;

enum class ProcessTerminationMode : std::uint8_t
{
    GracefulPreferred,
    Force
};

class Process final
{
public:
    ~Process() noexcept;

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) noexcept;
    Process& operator=(Process&&) noexcept;

    [[nodiscard]] core::Result<ProcessExitStatus> Wait();
    [[nodiscard]] core::VoidResult Terminate(ProcessTerminationMode mode);

private:
    friend struct detail::ProcessFactory;

    explicit Process(std::unique_ptr<detail::ProcessState> state) noexcept;

    std::unique_ptr<detail::ProcessState> m_state;
};

[[nodiscard]] core::Result<Process> LaunchProcess(const ProcessDesc& desc);
} // namespace pond::platform
