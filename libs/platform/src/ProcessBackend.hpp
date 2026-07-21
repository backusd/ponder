#pragma once

#include <ponder/platform/Process.hpp>

#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <string_view>

namespace pond::platform::detail
{
enum class BackendProcessExitKind
{
    Normal,
    Signal,
    Unknown
};

struct BackendProcessExitStatus final
{
    BackendProcessExitKind kind{BackendProcessExitKind::Unknown};
    std::uint32_t value{};

    [[nodiscard]] friend constexpr bool operator==(BackendProcessExitStatus,
                                                   BackendProcessExitStatus) noexcept = default;
};

enum class BackendProcessKillResult
{
    Succeeded,
    Failed,
    Unsupported
};

struct PlatformProcessBackend final
{
    void* context{};
    void* (*create)(void* context, const char* const* arguments){};
    bool (*wait)(void* context, void* process, bool block, BackendProcessExitStatus* status){};
    BackendProcessKillResult (*kill)(void* context, void* process, bool force){};
    void (*destroy)(void* context, void* process) noexcept {};
};

struct AbandonedProcessEntry final
{
    PlatformProcessBackend backend;
    void* process{};
    AbandonedProcessEntry* next{};
};

struct PlatformProcessReaperBackend final
{
    void* context{};
    bool (*ensureStarted)(void* context) noexcept {};
    void (*enqueue)(void* context, AbandonedProcessEntry* entry) noexcept {};
};

[[nodiscard]] PlatformProcessBackend GetPlatformProcessBackend() noexcept;
[[nodiscard]] PlatformProcessReaperBackend GetPlatformProcessReaperBackend();
[[nodiscard]] core::Result<Process> LaunchProcess(const ProcessDesc& desc,
                                                  PlatformProcessBackend backend);
[[nodiscard]] core::Result<Process> LaunchProcess(const ProcessDesc& desc,
                                                  PlatformProcessBackend backend,
                                                  PlatformProcessReaperBackend reaperBackend);
} // namespace pond::platform::detail

namespace std
{
template <>
struct formatter<pond::platform::detail::BackendProcessExitKind> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendProcessExitKind kind,
                FormatContext& context) const
    {
        using pond::platform::detail::BackendProcessExitKind;

        string_view name{"unknown"};
        switch (kind)
        {
        case BackendProcessExitKind::Normal:
            name = "normal";
            break;
        case BackendProcessExitKind::Signal:
            name = "signal";
            break;
        case BackendProcessExitKind::Unknown:
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendProcessExitStatus> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendProcessExitStatus status,
                FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}({})", status.kind, status.value), context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendProcessKillResult> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendProcessKillResult result,
                FormatContext& context) const
    {
        using pond::platform::detail::BackendProcessKillResult;

        string_view name{"unknown"};
        switch (result)
        {
        case BackendProcessKillResult::Succeeded:
            name = "succeeded";
            break;
        case BackendProcessKillResult::Failed:
            name = "failed";
            break;
        case BackendProcessKillResult::Unsupported:
            name = "unsupported";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};
} // namespace std

namespace pond::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, BackendProcessExitKind kind)
{
    return output << std::format("{}", kind);
}

inline std::ostream& operator<<(std::ostream& output, BackendProcessExitStatus status)
{
    return output << std::format("{}", status);
}

inline std::ostream& operator<<(std::ostream& output, BackendProcessKillResult result)
{
    return output << std::format("{}", result);
}
} // namespace pond::platform::detail