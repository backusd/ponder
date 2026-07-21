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
    [[nodiscard]] core::VoidResult SetHintOverride(const char* name,
                                                   const char* value) override;
    [[nodiscard]] core::VoidResult ResetHint(const char* name) override;
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
    [[nodiscard]] core::VoidResult SetMouseCapture(bool enabled) override;
    [[nodiscard]] core::Result<CursorHandle> CreateSystemCursor(SystemCursorShape shape) override;
    [[nodiscard]] core::VoidResult SetCursor(CursorHandle cursor) override;
    void DestroyCursor(CursorHandle cursor) noexcept override;
    [[nodiscard]] core::VoidResult ShowCursor() override;
    [[nodiscard]] core::VoidResult HideCursor() override;
    [[nodiscard]] bool IsCursorVisible() noexcept override;
    [[nodiscard]] bool SupportsClipboardText() noexcept override;
    [[nodiscard]] core::Result<std::string> GetClipboardText() override;
    [[nodiscard]] core::VoidResult SetClipboardText(std::string_view text) override;
    [[nodiscard]] core::VoidResult OpenExternalUri(std::string_view uri) override;
    void ShowDialog(const BackendDialogRequestDesc& desc) noexcept override;

private:
    SdlHintBackend m_hintBackend;
    HintManager m_hintManager;
};
} // namespace pond::platform::detail
