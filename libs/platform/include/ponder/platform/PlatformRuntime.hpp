#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Dialog.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Timing.hpp>
#include <ponder/platform/Window.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pond::platform
{
struct PlatformRuntimeDesc final
{
    std::string applicationName{"ponder"};
    std::optional<std::string> applicationVersion;
    std::optional<std::string> applicationIdentifier;
};

namespace detail
{
class PlatformRuntimeState;
}

class PlatformRuntime final
{
public:
    [[nodiscard]] static core::Result<PlatformRuntime> Create(
        const PlatformRuntimeDesc& desc);

    ~PlatformRuntime() noexcept;

    PlatformRuntime(const PlatformRuntime&) = delete;
    PlatformRuntime& operator=(const PlatformRuntime&) = delete;
    PlatformRuntime(PlatformRuntime&&) noexcept;
    PlatformRuntime& operator=(PlatformRuntime&&) noexcept;

    [[nodiscard]] PlatformTimestamp Now() const;
    [[nodiscard]] std::optional<PlatformEvent> PollEvent();
    [[nodiscard]] core::Result<Window> CreateWindow(const WindowDesc& desc);
    [[nodiscard]] core::Result<std::vector<DisplayInfo>> EnumerateDisplays();
    [[nodiscard]] core::Result<DisplayInfo> GetDisplayInfo(DisplayId id);

    [[nodiscard]] core::VoidResult SetMouseCapture(bool enabled);
    [[nodiscard]] core::Result<LogicalPoint> GetGlobalMousePosition() const;
    [[nodiscard]] core::VoidResult SetSystemCursor(SystemCursorShape shape);
    [[nodiscard]] core::VoidResult ShowCursor();
    [[nodiscard]] core::VoidResult HideCursor();
    [[nodiscard]] bool IsCursorVisible() const;

    [[nodiscard]] core::Result<std::string> GetClipboardText() const;
    [[nodiscard]] core::VoidResult SetClipboardText(std::string_view text);
    [[nodiscard]] core::VoidResult OpenExternalUri(std::string_view uri);

    [[nodiscard]] core::Result<DialogRequestId> ShowOpenFileDialog(
        const OpenFileDialogDesc& desc);
    [[nodiscard]] core::Result<DialogRequestId> ShowSaveFileDialog(
        const SaveFileDialogDesc& desc);
    [[nodiscard]] core::Result<DialogRequestId> ShowOpenFolderDialog(
        const OpenFolderDialogDesc& desc);

private:
    explicit PlatformRuntime(std::unique_ptr<detail::PlatformRuntimeState> state) noexcept;

    std::unique_ptr<detail::PlatformRuntimeState> m_state;
};
} // namespace pond::platform
