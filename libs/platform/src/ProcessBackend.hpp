#pragma once

#include <ponder/platform/Process.hpp>

#include <cstdint>

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
