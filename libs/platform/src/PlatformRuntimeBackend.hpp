#pragma once

#include <ponder/platform/WindowGraphics.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "IPlatformDisplayBackend.hpp"
#include "IPlatformRuntimeBackend.hpp"
#include "IPlatformWindowBackend.hpp"

namespace pond::platform::detail
{
class IPlatformRuntimeBackendFactory
{
public:
    virtual ~IPlatformRuntimeBackendFactory() noexcept = default;

    IPlatformRuntimeBackendFactory(const IPlatformRuntimeBackendFactory&) = delete;
    IPlatformRuntimeBackendFactory& operator=(const IPlatformRuntimeBackendFactory&) = delete;
    IPlatformRuntimeBackendFactory(IPlatformRuntimeBackendFactory&&) = delete;
    IPlatformRuntimeBackendFactory& operator=(IPlatformRuntimeBackendFactory&&) = delete;

    [[nodiscard]] virtual std::unique_ptr<IPlatformRuntimeBackend> Create() const = 0;

protected:
    IPlatformRuntimeBackendFactory() noexcept = default;
};

inline constexpr std::size_t kSystemCursorShapeCount{11};

[[nodiscard]] bool IsWindowGraphicsCompatibilitySupported(
    WindowGraphicsCompatibility compatibility) noexcept;
[[nodiscard]] BackendNativeWindowDriver GetNativeWindowDriver(std::string_view driverName) noexcept;
[[nodiscard]] std::uint64_t BuildSdlWindowFlags(const BackendWindowCreateDesc& desc) noexcept;
[[nodiscard]] bool IsReservedSdlWindowPosition(std::int32_t value) noexcept;

[[nodiscard]] std::unique_ptr<IPlatformRuntimeBackend> GetPlatformRuntimeBackend();
[[nodiscard]] std::unique_ptr<IPlatformWindowBackend> GetPlatformWindowBackend();
[[nodiscard]] std::unique_ptr<IPlatformDisplayBackend> GetPlatformDisplayBackend();

// The factories and any state borrowed by their backends must outlive each created runtime.
void SetPlatformRuntimeBackendForTesting(const IPlatformRuntimeBackendFactory* factory) noexcept;
void SetPlatformWindowBackendForTesting(const IPlatformWindowBackendFactory* factory) noexcept;
void SetPlatformDisplayBackendForTesting(const IPlatformDisplayBackendFactory* factory) noexcept;
} // namespace pond::platform::detail