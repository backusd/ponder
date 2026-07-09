#pragma once

#include "PlatformRuntimeBackend.hpp"

#include <ponder/core/Result.hpp>
#include <ponder/platform/Window.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pond::platform::detail
{
class PlatformRuntimeState;

class WindowImpl final
{
public:
    [[nodiscard]] static core::Result<std::unique_ptr<WindowImpl>> Create(
        PlatformRuntimeState& runtime, const WindowDesc& desc);

    ~WindowImpl() noexcept;

    WindowImpl(const WindowImpl&) = delete;
    WindowImpl& operator=(const WindowImpl&) = delete;
    WindowImpl(WindowImpl&&) = delete;
    WindowImpl& operator=(WindowImpl&&) = delete;

    WindowImpl(PlatformRuntimeState& runtime, PlatformWindowBackend backend,
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
    [[nodiscard]] core::Result<WindowPresentation> GetPresentation() const;
    [[nodiscard]] core::VoidResult SetPresentation(WindowPresentation presentation);
    [[nodiscard]] core::Result<WindowDecoration> GetDecoration() const;
    [[nodiscard]] core::VoidResult SetDecoration(WindowDecoration decoration);
    [[nodiscard]] core::Result<::pond::platform::WindowState> GetState() const;
    [[nodiscard]] core::VoidResult Minimize();
    [[nodiscard]] core::VoidResult Maximize();
    [[nodiscard]] core::VoidResult Restore();
    [[nodiscard]] core::Result<bool> IsVisible() const;
    [[nodiscard]] core::Result<bool> IsResizable() const;
    [[nodiscard]] core::VoidResult SetResizable(bool resizable);
    [[nodiscard]] core::Result<bool> IsFocused() const;
    [[nodiscard]] core::Result<bool> IsAlwaysOnTop() const;
    [[nodiscard]] core::VoidResult SetAlwaysOnTop(bool alwaysOnTop);
    [[nodiscard]] core::VoidResult Show();
    [[nodiscard]] core::VoidResult Hide();

private:
    friend class PlatformRuntimeState;

    [[nodiscard]] core::Result<BackendWindowProperties> GetProperties(
        std::string_view operation) const;
    void CommitRegistration(WindowId id) noexcept;
    void ObserveShownEvent() noexcept;
    void VerifyUsable(std::string_view operation) const;
    [[nodiscard]] std::string GetErrorContext() const;

    PlatformRuntimeState* m_runtime{};
    PlatformWindowBackend m_backend;
    void* m_nativeWindow{};
    std::uint32_t m_backendWindowId{};
    WindowId m_id;
    WindowGraphicsCompatibility m_graphicsCompatibility{
        WindowGraphicsCompatibility::Default};
    std::optional<::pond::platform::WindowState> m_hiddenStateRequest;
    bool m_registered{};
};
} // namespace pond::platform::detail
