#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Window.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "PlatformRuntimeBackend.hpp"

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

    WindowImpl(PlatformRuntimeState& runtime, PlatformWindowBackend backend, void* nativeWindow,
               std::uint32_t backendWindowId,
               WindowGraphicsCompatibility graphicsCompatibility) noexcept;

    [[nodiscard]] WindowId GetId() const;
    [[nodiscard]] WindowGraphicsCompatibility GetGraphicsCompatibility() const;
    [[nodiscard]] core::Result<NativeWindowHandle> GetNativeHandle() const;
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
    [[nodiscard]] core::VoidResult StartTextInput();
    [[nodiscard]] core::VoidResult StopTextInput();
    [[nodiscard]] bool IsTextInputActive() const;
    [[nodiscard]] core::VoidResult ClearTextComposition();
    [[nodiscard]] core::VoidResult SetTextInputArea(TextInputArea area);
    [[nodiscard]] core::VoidResult ClearTextInputArea();
    [[nodiscard]] core::VoidResult SetMouseGrab(bool grabbed);
    [[nodiscard]] bool IsMouseGrabbed() const;
    [[nodiscard]] core::VoidResult SetRelativeMouseMode(bool enabled);
    [[nodiscard]] bool IsRelativeMouseModeEnabled() const;

private:
    friend class PlatformRuntimeState;

    [[nodiscard]] core::Result<BackendWindowProperties> GetProperties(
        std::string_view operation) const;
    void CommitRegistration(WindowId id) noexcept;
    void ObserveShownEvent() noexcept;
    void SynchronizeStateRequestVisibility(bool hidden) noexcept;
    void RecordStateRequest(::pond::platform::WindowState state, bool hidden) noexcept;
    void IncrementPendingDialogRequestCount() noexcept;
    void DecrementPendingDialogRequestCount() noexcept;
    void VerifyUsable(std::string_view operation) const;
    [[nodiscard]] std::string_view GetErrorContext() const;

    static constexpr std::size_t kErrorContextCapacity = 32;

    PlatformRuntimeState* m_runtime{};
    PlatformWindowBackend m_backend;
    void* m_nativeWindow{};
    std::uint32_t m_backendWindowId{};
    WindowId m_id;
    WindowGraphicsCompatibility m_graphicsCompatibility{WindowGraphicsCompatibility::Default};
    std::optional<WindowPresentation> m_pendingPresentationRequest;
    std::optional<::pond::platform::WindowState> m_pendingVisibleStateRequest;
    std::optional<::pond::platform::WindowState> m_hiddenStateRequest;
    std::array<char, kErrorContextCapacity> m_errorContext{};
    std::size_t m_errorContextLength{};
    std::size_t m_pendingDialogRequestCount{};
    bool m_registered{};
};
} // namespace pond::platform::detail
