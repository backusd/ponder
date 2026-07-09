#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Timing.hpp>
#include <ponder/platform/Window.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pond::platform
{
struct PlatformRuntimeDesc final
{
    std::string applicationName{"ponder"};
    std::optional<std::string> applicationVersion;
    std::optional<std::string> applicationIdentifier;
};

namespace detail
{
class PlatformRuntimeState;
}

class PlatformRuntime final
{
public:
    [[nodiscard]] static core::Result<PlatformRuntime> Create(
        const PlatformRuntimeDesc& desc);

    ~PlatformRuntime() noexcept;

    PlatformRuntime(const PlatformRuntime&) = delete;
    PlatformRuntime& operator=(const PlatformRuntime&) = delete;
    PlatformRuntime(PlatformRuntime&&) noexcept;
    PlatformRuntime& operator=(PlatformRuntime&&) noexcept;

    [[nodiscard]] PlatformTimestamp Now() const;
    [[nodiscard]] std::optional<PlatformEvent> PollEvent();
    [[nodiscard]] core::Result<Window> CreateWindow(const WindowDesc& desc);
    [[nodiscard]] core::Result<std::vector<DisplayInfo>> EnumerateDisplays();
    [[nodiscard]] core::Result<DisplayInfo> GetDisplayInfo(DisplayId id);

private:
    explicit PlatformRuntime(std::unique_ptr<detail::PlatformRuntimeState> state) noexcept;

    std::unique_ptr<detail::PlatformRuntimeState> m_state;
};
} // namespace pond::platform
