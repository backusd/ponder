#pragma once

#include "IPlatformDisplayBackend.hpp"

namespace ponder::platform::detail
{
class SdlDisplayBackend final : public IPlatformDisplayBackend
{
public:
    SdlDisplayBackend() noexcept = default;
    ~SdlDisplayBackend() noexcept override = default;

    [[nodiscard]] std::vector<std::uint32_t> Enumerate() override;
    [[nodiscard]] std::string GetName(std::uint32_t displayId) override;
    [[nodiscard]] BackendScreenRectangle GetBounds(std::uint32_t displayId) override;
    [[nodiscard]] BackendScreenRectangle GetUsableBounds(std::uint32_t displayId) override;
    [[nodiscard]] float GetCurrentRefreshRate(std::uint32_t displayId) override;
    [[nodiscard]] BackendDisplayOrientation GetCurrentOrientation(std::uint32_t displayId) noexcept override;
    [[nodiscard]] float GetContentScale(std::uint32_t displayId) override;
    [[nodiscard]] std::uint32_t GetForWindow(BackendWindowHandle window) override;
    [[nodiscard]] float GetWindowPixelDensity(BackendWindowHandle window) override;
    [[nodiscard]] float GetWindowDisplayScale(BackendWindowHandle window) override;
};
} // namespace ponder::platform::detail
