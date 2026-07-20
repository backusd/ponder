#pragma once

#include "HintManagerBackend.hpp"
#include "IPlatformRuntimeBackend.hpp"

namespace pond::platform::detail
{
class SdlHintBackend final : public IHintBackend
{
public:
    [[nodiscard]] bool IsMainThread() const noexcept override;
    [[nodiscard]] bool HasInitializedSubsystems() const noexcept override;
    [[nodiscard]] const char* GetHint(const char* name) const noexcept override;
    [[nodiscard]] bool SetHintOverride(const char* name, const char* value) noexcept override;
    [[nodiscard]] bool ResetHint(const char* name) noexcept override;
};

class SdlRuntimeBackend final : public IPlatformRuntimeBackend
{
public:
    SdlRuntimeBackend();
    ~SdlRuntimeBackend() noexcept override = default;

    [[nodiscard]] HintManager& GetHintManager() noexcept override;
    [[nodiscard]] bool IsMainThread() noexcept override;
    [[nodiscard]] bool HasInitializedSubsystems() noexcept override;
    [[nodiscard]] bool HasExpectedRuntimeSubsystems() noexcept override;
    [[nodiscard]] core::VoidResult SetAppMetadataProperty(
        ApplicationMetadataProperty property, const char* value) override;
    [[nodiscard]] core::VoidResult InitializeVideo() override;
    void Quit() noexcept override;
    [[nodiscard]] std::uint64_t GetTicksNanoseconds() noexcept override;
    [[nodiscard]] bool PollEvent(SDL_Event* event) noexcept override;
    [[nodiscard]] bool SupportsGlobalMouse() noexcept override;
    [[nodiscard]] MousePosition GetGlobalMousePosition() noexcept override;
    [[nodiscard]] bool SetMouseCapture(bool enabled) noexcept override;
    [[nodiscard]] CursorHandle CreateSystemCursor(SystemCursorShape shape) noexcept override;
    [[nodiscard]] bool SetCursor(CursorHandle cursor) noexcept override;
    void DestroyCursor(CursorHandle cursor) noexcept override;
    [[nodiscard]] bool ShowCursor() noexcept override;
    [[nodiscard]] bool HideCursor() noexcept override;
    [[nodiscard]] bool IsCursorVisible() noexcept override;
    [[nodiscard]] bool SupportsClipboardText() noexcept override;
    [[nodiscard]] BackendClipboardTextResult GetClipboardText() override;
    [[nodiscard]] bool SetClipboardText(std::string_view text) override;
    [[nodiscard]] bool OpenExternalUri(std::string_view uri) override;
    void ShowDialog(const BackendDialogRequestDesc& desc) noexcept override;

private:
    SdlHintBackend m_hintBackend;
    HintManager m_hintManager;
};
} // namespace pond::platform::detail
