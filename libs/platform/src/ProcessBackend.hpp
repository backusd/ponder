#pragma once

#include <ponder/platform/Process.hpp>

namespace pond::platform::detail
{
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
    bool (*wait)(void* context, void* process, bool block, int* exitCode){};
    BackendProcessKillResult (*kill)(void* context, void* process, bool force){};
    void (*destroy)(void* context, void* process) noexcept{};
};

[[nodiscard]] PlatformProcessBackend GetPlatformProcessBackend() noexcept;
} // namespace pond::platform::detail
