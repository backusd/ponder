#pragma once

#include "IPlatformWindowBackend.hpp"

namespace ponder::platform::detail
{
[[nodiscard]] NativeWindowHandle MakeWin32NativeWindowHandle(BackendWindowHandle backendWindow, void* instance, void* nativeWindow);
[[nodiscard]] NativeWindowHandle MakeX11NativeWindowHandle(BackendWindowHandle backendWindow, void* display, std::int64_t nativeWindow);
[[nodiscard]] NativeWindowHandle MakeWaylandNativeWindowHandle(BackendWindowHandle backendWindow, void* display, void* surface);

class SdlWindowBackend final : public IPlatformWindowBackend
{
public:
    [[nodiscard]] BackendWindowHandle Create(const BackendWindowCreateDesc& desc) override;
    void Destroy(BackendWindowHandle window) noexcept override;
    [[nodiscard]] std::uint32_t GetId(BackendWindowHandle window) override;
    [[nodiscard]] std::string GetTitle(BackendWindowHandle window) override;
    void SetTitle(BackendWindowHandle window, std::string_view title) override;
    [[nodiscard]] BackendWindowPosition GetPosition(BackendWindowHandle window) override;
    void SetPosition(BackendWindowHandle window, BackendWindowPosition position) override;
    [[nodiscard]] BackendWindowLogicalSize GetSize(BackendWindowHandle window) override;
    [[nodiscard]] BackendWindowPixelSize GetSizeInPixels(BackendWindowHandle window) override;
    void SetSize(BackendWindowHandle window, BackendWindowLogicalSize size) override;
    void SetMinimumSize(BackendWindowHandle window, BackendWindowLogicalSize size) override;
    void Show(BackendWindowHandle window) override;
    void Hide(BackendWindowHandle window) override;
    [[nodiscard]] BackendWindowProperties GetProperties(BackendWindowHandle window) override;
    void SetFullscreenModeToDesktop(BackendWindowHandle window) override;
    void SetFullscreen(BackendWindowHandle window, bool fullscreen) override;
    void SetBordered(BackendWindowHandle window, bool bordered) override;
    void SetResizable(BackendWindowHandle window, bool resizable) override;
    void SetAlwaysOnTop(BackendWindowHandle window, bool alwaysOnTop) override;
    void Minimize(BackendWindowHandle window) override;
    void Maximize(BackendWindowHandle window) override;
    void Restore(BackendWindowHandle window) override;
    void StartTextInput(BackendWindowHandle window) override;
    void StopTextInput(BackendWindowHandle window) override;
    [[nodiscard]] bool IsTextInputActive(BackendWindowHandle window) noexcept override;
    void ClearTextComposition(BackendWindowHandle window) override;
    void SetTextInputArea(BackendWindowHandle window, std::optional<BackendTextInputArea> area) override;
    void SetMouseGrab(BackendWindowHandle window, bool grabbed) override;
    [[nodiscard]] bool IsMouseGrabbed(BackendWindowHandle window) noexcept override;
    void SetRelativeMouseMode(BackendWindowHandle window, bool enabled) override;
    [[nodiscard]] bool IsRelativeMouseModeEnabled(BackendWindowHandle window) noexcept override;
    [[nodiscard]] ponder::core::Result<NativeWindowHandle> GetNativeHandle(BackendWindowHandle window) override;
};
} // namespace ponder::platform::detail
