#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Timing.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "PlatformRuntimeBackend.hpp"
#include "RuntimeChildRegistry.hpp"

namespace pond::platform::detail
{
class WindowImpl;
struct DialogRequestState;

struct DialogCompletionRecord final
{
    Timestamp timestamp{};
    DialogRequestId requestId;
    DialogOutcome outcome;
};

struct RuntimeDisplayRecord final
{
    DisplayId id;
    bool connected{};
    bool removalEventPending{};
};

struct RuntimeBackendDisplayRecord final
{
    std::uint32_t backendId{};
    bool connected{};
};

struct RuntimeWindowRecord final
{
    WindowId id;
    WindowImpl* window{};
};

class PlatformRuntimeState final
{
public:
    PlatformRuntimeState(std::unique_ptr<IPlatformRuntimeBackend> backend,
                         std::unique_ptr<IPlatformWindowBackend> windowBackend,
                         std::unique_ptr<IPlatformDisplayBackend> displayBackend) noexcept;
    ~PlatformRuntimeState() noexcept;

    PlatformRuntimeState(const PlatformRuntimeState&) = delete;
    PlatformRuntimeState& operator=(const PlatformRuntimeState&) = delete;
    PlatformRuntimeState(PlatformRuntimeState&&) = delete;
    PlatformRuntimeState& operator=(PlatformRuntimeState&&) = delete;

    void VerifyOwnerThread(std::string_view operation) const;

    void RegisterChild(const void* child);
    void UnregisterChild(const void* child);
    void RegisterRequest(const void* request);
    void UnregisterRequest(const void* request);

    [[nodiscard]] WindowId RegisterWindow(WindowImpl* window, std::uint32_t backendWindowId);
    void BeginWindowDestruction(WindowImpl* window, std::uint32_t backendWindowId, WindowId id);
    void FinishWindowDestruction(WindowImpl* window);
    [[nodiscard]] std::optional<WindowId> FindWindowId(std::uint32_t backendWindowId) const;
    [[nodiscard]] std::optional<DisplayId> FindConnectedDisplayId(
        std::uint32_t backendDisplayId) const;
    [[nodiscard]] std::optional<DisplayId> FindDisplayIdForRemoval(
        std::uint32_t backendDisplayId) const;

    [[nodiscard]] Timestamp Now() const;
    [[nodiscard]] HintManager& GetHintManager();
    [[nodiscard]] Timestamp CaptureBackendTimestamp() const;
    [[nodiscard]] std::optional<PlatformEvent> PollEvent();
    [[nodiscard]] core::Result<std::vector<DisplayInfo>> EnumerateDisplays();
    [[nodiscard]] core::Result<DisplayInfo> GetDisplayInfo(DisplayId id);
    [[nodiscard]] core::Result<DisplayId> GetDisplayIdForWindow(BackendWindowHandle window);
    [[nodiscard]] core::Result<float> GetPixelDensityForWindow(
        BackendWindowHandle window, std::string_view windowContext) const;
    [[nodiscard]] core::Result<float> GetDisplayScaleForWindow(
        BackendWindowHandle window, std::string_view windowContext) const;
    [[nodiscard]] core::VoidResult SetMouseCapture(bool enabled);
    [[nodiscard]] core::Result<LogicalPoint> GetGlobalMousePosition() const;
    [[nodiscard]] core::VoidResult SetSystemCursor(SystemCursorShape shape);
    [[nodiscard]] core::VoidResult ShowCursor();
    [[nodiscard]] core::VoidResult HideCursor();
    [[nodiscard]] bool IsCursorVisible() const;
    [[nodiscard]] core::Result<std::string> GetClipboardText() const;
    [[nodiscard]] core::VoidResult SetClipboardText(std::string_view text);
    [[nodiscard]] core::VoidResult OpenExternalUri(std::string_view uri);
    [[nodiscard]] core::Result<DialogRequestId> ShowOpenFileDialog(const OpenFileDialogDesc& desc);
    [[nodiscard]] core::Result<DialogRequestId> ShowSaveFileDialog(const SaveFileDialogDesc& desc);
    [[nodiscard]] core::Result<DialogRequestId> ShowOpenFolderDialog(
        const OpenFolderDialogDesc& desc);
    [[nodiscard]] std::shared_ptr<DialogRequestState> AcquireDialogRequest(DialogRequestId id);
    void EnqueueDialogCompletion(DialogRequestId id, Timestamp timestamp,
                                 DialogOutcome outcome);
    void MarkDialogCallbackFailure(DialogRequestId id, Timestamp timestamp) noexcept;

private:
    friend class WindowImpl;

    [[nodiscard]] core::Result<std::vector<std::uint32_t>> RefreshDisplays();
    [[nodiscard]] core::Result<DisplayInfo> QueryDisplayInfo(DisplayId id,
                                                             std::uint32_t backendDisplayId) const;
    [[nodiscard]] core::Result<DialogRequestId> ShowDialog(
        BackendDialogKind kind, std::optional<WindowId> parentWindowId,
        const std::optional<std::filesystem::path>& defaultLocation,
        std::span<const DialogFileFilter> filters, bool allowMultipleSelection);
    [[nodiscard]] std::optional<DisplayId> FindKnownDisplayId(std::uint32_t backendDisplayId) const;
    [[nodiscard]] std::optional<DisplayId> ConnectDisplayFromEvent(std::uint32_t backendDisplayId);
    void DisconnectDisplayFromEvent(std::uint32_t backendDisplayId);
    void ReconcileDisplayFromEvent(std::uint32_t backendDisplayId);
    void ObserveWindowShownEvent(std::uint32_t backendWindowId);
    [[nodiscard]] std::optional<PlatformEvent> PollDialogCompletion();
    void DestroySystemCursors() noexcept;

    std::unique_ptr<IPlatformRuntimeBackend> m_backend;
    std::unique_ptr<IPlatformWindowBackend> m_windowBackend;
    std::unique_ptr<IPlatformDisplayBackend> m_displayBackend;
    std::thread::id m_ownerThread;
    RuntimeChildRegistry m_registry;
    std::unordered_map<std::uint32_t, RuntimeWindowRecord> m_windowsByBackendId;
    WindowId::ValueType m_nextWindowId{1};
    std::unordered_map<std::uint32_t, RuntimeDisplayRecord> m_displaysByBackendId;
    std::unordered_map<DisplayId, RuntimeBackendDisplayRecord> m_displaysById;
    std::vector<std::uint32_t> m_connectedBackendDisplayIds;
    DisplayId::ValueType m_nextDisplayId{1};
    DialogRequestId::ValueType m_nextDialogRequestId{1};
    std::mutex m_dialogMutex;
    std::unordered_map<DialogRequestId, std::shared_ptr<DialogRequestState>> m_dialogRequests;
    DialogRequestState* m_firstCompletedDialogRequest{};
    DialogRequestState* m_lastCompletedDialogRequest{};
    std::array<CursorHandle, kSystemCursorShapeCount> m_systemCursors{};
};
} // namespace pond::platform::detail
