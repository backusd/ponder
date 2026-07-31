#pragma once

#include <ponder/core/Result.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ponder::platform
{
struct ProcessDesc;
class Process;

namespace detail
{
class ProcessState;
struct ProcessFactory;
} // namespace detail

struct ProcessDesc final
{
    std::filesystem::path executable;
    std::vector<std::string> arguments;

    [[nodiscard]] friend bool operator==(const ProcessDesc& lhs, const ProcessDesc& rhs) = default;
};

struct ProcessNormalExit final
{
    std::uint32_t exitCode{};

    [[nodiscard]] friend constexpr bool operator==(ProcessNormalExit, ProcessNormalExit) noexcept = default;
};

struct ProcessSignalTermination final
{
    int signal{};

    [[nodiscard]] friend constexpr bool operator==(ProcessSignalTermination, ProcessSignalTermination) noexcept = default;
};

struct ProcessUnknownTermination final
{
    [[nodiscard]] friend constexpr bool operator==(ProcessUnknownTermination, ProcessUnknownTermination) noexcept = default;
};

using ProcessExitStatus = std::variant<ProcessNormalExit, ProcessSignalTermination, ProcessUnknownTermination>;

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

    // Blocks until the child process exits. Do not call from the desktop event
    // loop or any UI/platform/render pumping thread.
    [[nodiscard]] ponder::core::Result<ProcessExitStatus> Wait();

    // Checks the child process without blocking. A successful empty optional
    // means the child is still running.
    [[nodiscard]] ponder::core::Result<std::optional<ProcessExitStatus>> TryWait();
    [[nodiscard]] ponder::core::VoidResult Terminate(ProcessTerminationMode mode);

private:
    friend struct detail::ProcessFactory;

    explicit Process(std::unique_ptr<detail::ProcessState> state) noexcept;

    std::unique_ptr<detail::ProcessState> m_state;
};

[[nodiscard]] ponder::core::Result<Process> LaunchProcess(const ProcessDesc& desc);
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::ProcessDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::ProcessDesc& desc, FormatContext& context) const
    {
        return formatter<string>::format(std::format("process(executable='{}', argumentCount={})", desc.executable.string(), desc.arguments.size()),
                                         context);
    }
};
template <>
struct formatter<ponder::platform::ProcessNormalExit> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::ProcessNormalExit status, FormatContext& context) const
    {
        return formatter<string>::format(std::format("exit_code={}", status.exitCode), context);
    }
};

template <>
struct formatter<ponder::platform::ProcessSignalTermination> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::ProcessSignalTermination status, FormatContext& context) const
    {
        return formatter<string>::format(std::format("signal={}", status.signal), context);
    }
};

template <>
struct formatter<ponder::platform::ProcessUnknownTermination> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::ProcessUnknownTermination, FormatContext& context) const
    {
        return formatter<string_view>::format("unknown", context);
    }
};

template <>
struct formatter<ponder::platform::ProcessTerminationMode> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::ProcessTerminationMode mode, FormatContext& context) const
    {
        using ponder::platform::ProcessTerminationMode;

        string_view name{"unknown"};
        switch (mode)
        {
        case ProcessTerminationMode::GracefulPreferred:
            name = "graceful_preferred";
            break;
        case ProcessTerminationMode::Force:
            name = "force";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};
} // namespace std

namespace ponder::platform
{
inline std::ostream& operator<<(std::ostream& output, const ProcessDesc& desc)
{
    return output << std::format("{}", desc);
}
inline std::ostream& operator<<(std::ostream& output, ProcessNormalExit status)
{
    return output << std::format("{}", status);
}

inline std::ostream& operator<<(std::ostream& output, ProcessSignalTermination status)
{
    return output << std::format("{}", status);
}

inline std::ostream& operator<<(std::ostream& output, ProcessUnknownTermination status)
{
    return output << std::format("{}", status);
}

inline std::ostream& operator<<(std::ostream& output, ProcessTerminationMode mode)
{
    return output << std::format("{}", mode);
}
} // namespace ponder::platform
