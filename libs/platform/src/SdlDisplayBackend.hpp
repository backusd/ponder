#pragma once

#include "IPlatformDisplayBackend.hpp"

namespace pond::platform::detail
{
class SdlDisplayBackend final : public IPlatformDisplayBackend
{
public:
    SdlDisplayBackend() noexcept = default;
    ~SdlDisplayBackend() noexcept override = default;

    [[nodiscard]] core::Result<std::vector<std::uint32_t>> Enumerate() override;
    [[nodiscard]] core::Result<std::string> GetName(std::uint32_t displayId) override;
    [[nodiscard]] core::Result<BackendScreenRectangle> GetBounds(
        std::uint32_t displayId) override;
    [[nodiscard]] core::Result<BackendScreenRectangle> GetUsableBounds(
        std::uint32_t displayId) override;
    [[nodiscard]] core::Result<float> GetCurrentRefreshRate(
        std::uint32_t displayId) override;
    [[nodiscard]] BackendDisplayOrientation GetCurrentOrientation(
        std::uint32_t displayId) noexcept override;
    [[nodiscard]] core::Result<float> GetContentScale(std::uint32_t displayId) override;
    [[nodiscard]] core::Result<std::uint32_t> GetForWindow(
        BackendWindowHandle window) override;
    [[nodiscard]] core::Result<float> GetWindowPixelDensity(
        BackendWindowHandle window) override;
    [[nodiscard]] core::Result<float> GetWindowDisplayScale(
        BackendWindowHandle window) override;
};
} // namespace pond::platform::detail