#pragma once

#include "IPlatformWindowBackend.hpp"

namespace pond::platform::detail
{
class SdlWindowBackend final : public IPlatformWindowBackend
{
public:
    [[nodiscard]] core::Result<BackendWindowHandle> Create(
        const BackendWindowCreateDesc& desc) override;
    void Destroy(BackendWindowHandle window) noexcept override;
    [[nodiscard]] core::Result<std::uint32_t> GetId(BackendWindowHandle window) override;
    [[nodiscard]] std::string GetTitle(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult SetTitle(BackendWindowHandle window,
                                            std::string_view title) override;
    [[nodiscard]] core::Result<BackendWindowPosition> GetPosition(
        BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult SetPosition(
        BackendWindowHandle window, BackendWindowPosition position) override;
    [[nodiscard]] core::Result<BackendWindowLogicalSize> GetSize(
        BackendWindowHandle window) override;
    [[nodiscard]] core::Result<BackendWindowPixelSize> GetSizeInPixels(
        BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult SetSize(
        BackendWindowHandle window, BackendWindowLogicalSize size) override;
    [[nodiscard]] core::VoidResult SetMinimumSize(
        BackendWindowHandle window, BackendWindowLogicalSize size) override;
    [[nodiscard]] core::VoidResult Show(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult Hide(BackendWindowHandle window) override;
    [[nodiscard]] core::Result<BackendWindowProperties> GetProperties(
        BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult SetFullscreenModeToDesktop(
        BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult SetFullscreen(
        BackendWindowHandle window, bool fullscreen) override;
    [[nodiscard]] core::VoidResult SetBordered(
        BackendWindowHandle window, bool bordered) override;
    [[nodiscard]] core::VoidResult SetResizable(
        BackendWindowHandle window, bool resizable) override;
    [[nodiscard]] core::VoidResult SetAlwaysOnTop(
        BackendWindowHandle window, bool alwaysOnTop) override;
    [[nodiscard]] core::VoidResult Minimize(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult Maximize(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult Restore(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult StartTextInput(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult StopTextInput(BackendWindowHandle window) override;
    [[nodiscard]] bool IsTextInputActive(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult ClearTextComposition(
        BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult SetTextInputArea(
        BackendWindowHandle window, std::optional<BackendTextInputArea> area) override;
    [[nodiscard]] core::VoidResult SetMouseGrab(
        BackendWindowHandle window, bool grabbed) override;
    [[nodiscard]] bool IsMouseGrabbed(BackendWindowHandle window) override;
    [[nodiscard]] core::VoidResult SetRelativeMouseMode(
        BackendWindowHandle window, bool enabled) override;
    [[nodiscard]] bool IsRelativeMouseModeEnabled(BackendWindowHandle window) override;
    [[nodiscard]] core::Result<NativeWindowHandle> GetNativeHandle(
        BackendWindowHandle window) override;
};
} // namespace pond::platform::detail
