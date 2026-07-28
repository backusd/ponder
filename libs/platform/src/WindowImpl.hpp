#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Runtime.hpp>
#include <ponder/platform/Window.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "IPlatformWindowBackend.hpp"

namespace ponder::platform::detail
{
class MockRuntime;
class SdlRuntime;
class WindowRegistry;

class WindowImpl final
{
public:
    [[nodiscard]] static std::unique_ptr<WindowImpl> Create(RuntimeImpl& runtime, const WindowDesc& desc);

    ~WindowImpl() noexcept;

    WindowImpl(const WindowImpl&) = delete;
    WindowImpl& operator=(const WindowImpl&) = delete;
    WindowImpl(WindowImpl&&) = delete;
    WindowImpl& operator=(WindowImpl&&) = delete;

    WindowImpl(RuntimeImpl& runtime, IPlatformWindowBackend& backend, BackendWindowHandle backendWindow, std::uint32_t backendWindowId,
               WindowGraphicsCompatibility graphicsCompatibility) noexcept;

    [[nodiscard]] WindowId GetId() const;
    [[nodiscard]] WindowGraphicsCompatibility GetGraphicsCompatibility() const;
    [[nodiscard]] ponder::core::Result<NativeWindowHandle> GetNativeHandle() const;
    [[nodiscard]] std::string GetTitle() const;
    void SetTitle(std::string_view title);
    [[nodiscard]] ScreenPosition GetPosition() const;
    void SetPosition(ScreenPosition position);
    [[nodiscard]] LogicalSize GetLogicalSize() const;
    [[nodiscard]] PixelSize GetPixelSize() const;
    void SetLogicalSize(LogicalSize size);
    [[nodiscard]] ponder::core::Result<DisplayId> GetDisplayId() const;
    [[nodiscard]] float GetPixelDensity() const;
    [[nodiscard]] float GetDisplayScale() const;
    [[nodiscard]] WindowPresentation GetPresentation() const;
    void SetPresentation(WindowPresentation presentation);
    [[nodiscard]] WindowDecoration GetDecoration() const;
    void SetDecoration(WindowDecoration decoration);
    [[nodiscard]] ::ponder::platform::WindowState GetState() const;
    void Minimize();
    void Maximize();
    void Restore();
    [[nodiscard]] bool IsVisible() const;
    [[nodiscard]] bool IsResizable() const;
    void SetResizable(bool resizable);
    [[nodiscard]] bool IsFocused() const;
    [[nodiscard]] bool IsAlwaysOnTop() const;
    void SetAlwaysOnTop(bool alwaysOnTop);
    void Show();
    void Hide();
    void StartTextInput();
    void StopTextInput();
    [[nodiscard]] bool IsTextInputActive() const;
    void ClearTextComposition();
    void SetTextInputArea(TextInputArea area);
    void ClearTextInputArea();
    void SetMouseGrab(bool grabbed);
    [[nodiscard]] bool IsMouseGrabbed() const;
    void SetRelativeMouseMode(bool enabled);
    [[nodiscard]] bool IsRelativeMouseModeEnabled() const;

private:
    friend class MockRuntime;
    friend class SdlRuntime;
    friend class WindowRegistry;

    [[nodiscard]] BackendWindowProperties GetProperties(std::string_view operation) const;
    void PrepareRegistration(WindowId id) noexcept;
    void CommitRegistration() noexcept;
    void PublishConstruction() noexcept;
    void ObserveShownEvent();
    void SynchronizeStateRequestVisibility(bool hidden) noexcept;
    void RecordStateRequest(::ponder::platform::WindowState state, bool hidden) noexcept;
    void VerifyUsable(std::string_view operation) const;
    [[nodiscard]] std::string_view GetErrorContext() const;

    static constexpr std::size_t kErrorContextCapacity = 32;

    RuntimeImpl* m_runtime{};
    IPlatformWindowBackend& m_backend;
    BackendWindowHandle m_backendWindow;
    std::uint32_t m_backendWindowId{};
    WindowId m_id;
    WindowGraphicsCompatibility m_graphicsCompatibility{WindowGraphicsCompatibility::Default};
    std::optional<WindowPresentation> m_pendingPresentationRequest;
    std::optional<::ponder::platform::WindowState> m_pendingVisibleStateRequest;
    std::optional<::ponder::platform::WindowState> m_hiddenStateRequest;
    std::array<char, kErrorContextCapacity> m_errorContext{};
    std::size_t m_errorContextLength{};
    bool m_registered{};
    bool m_constructionPublished{};
};
} // namespace ponder::platform::detail
