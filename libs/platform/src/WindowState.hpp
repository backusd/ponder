#pragma once

#include "PlatformRuntimeBackend.hpp"

#include <ponder/core/Result.hpp>
#include <ponder/platform/Window.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace pond::platform::detail
{
class PlatformRuntimeState;

class WindowState final
{
public:
    [[nodiscard]] static core::Result<std::unique_ptr<WindowState>> Create(
        PlatformRuntimeState& runtime, const WindowDesc& desc);

    ~WindowState() noexcept;

    WindowState(const WindowState&) = delete;
    WindowState& operator=(const WindowState&) = delete;
    WindowState(WindowState&&) = delete;
    WindowState& operator=(WindowState&&) = delete;

    WindowState(PlatformRuntimeState& runtime, PlatformWindowBackend backend,
                void* nativeWindow, std::uint32_t backendWindowId,
                WindowGraphicsCompatibility graphicsCompatibility) noexcept;

    [[nodiscard]] WindowId GetId() const;
    [[nodiscard]] WindowGraphicsCompatibility GetGraphicsCompatibility() const;
    [[nodiscard]] std::string GetTitle() const;
    [[nodiscard]] core::VoidResult SetTitle(std::string_view title);
    [[nodiscard]] core::Result<ScreenPosition> GetPosition() const;
    [[nodiscard]] core::VoidResult SetPosition(ScreenPosition position);
    [[nodiscard]] core::Result<LogicalSize> GetLogicalSize() const;
    [[nodiscard]] core::Result<PixelSize> GetPixelSize() const;
    [[nodiscard]] core::VoidResult SetLogicalSize(LogicalSize size);
    [[nodiscard]] core::Result<DisplayId> GetDisplayId() const;
    [[nodiscard]] core::Result<float> GetPixelDensity() const;
    [[nodiscard]] core::Result<float> GetDisplayScale() const;
    [[nodiscard]] core::VoidResult Show();
    [[nodiscard]] core::VoidResult Hide();

private:
    void CommitRegistration(WindowId id) noexcept;
    void VerifyUsable(std::string_view operation) const;
    [[nodiscard]] std::string GetErrorContext() const;

    PlatformRuntimeState* m_runtime{};
    PlatformWindowBackend m_backend;
    void* m_nativeWindow{};
    std::uint32_t m_backendWindowId{};
    WindowId m_id;
    WindowGraphicsCompatibility m_graphicsCompatibility{
        WindowGraphicsCompatibility::Default};
    bool m_registered{};
};
} // namespace pond::platform::detail
