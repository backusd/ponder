#pragma once

#include "PlatformRuntimeBackend.hpp"
#include "RuntimeChildRegistry.hpp"

#include <ponder/core/Result.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Timing.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace pond::platform::detail
{
class WindowImpl;

struct RuntimeHintSnapshot final
{
    const char* name{};
    std::optional<std::string> value;
};

struct RuntimeMetadataSnapshot final
{
    ApplicationMetadataProperty property{};
    std::optional<std::string> value;
};

using RuntimeMetadataSnapshots = std::array<RuntimeMetadataSnapshot, 3>;

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
    PlatformRuntimeState(PlatformRuntimeBackend backend,
                         PlatformWindowBackend windowBackend,
                         PlatformDisplayBackend displayBackend,
                         RuntimeHintSnapshot focusClickThrough,
                         RuntimeHintSnapshot autoCapture,
                         RuntimeMetadataSnapshots metadata) noexcept;
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

    [[nodiscard]] WindowId RegisterWindow(WindowImpl* window,
                                          std::uint32_t backendWindowId);
    void BeginWindowDestruction(WindowImpl* window, std::uint32_t backendWindowId,
                                WindowId id);
    void FinishWindowDestruction(WindowImpl* window);
    [[nodiscard]] std::optional<WindowId> FindWindowId(
        std::uint32_t backendWindowId) const;
    [[nodiscard]] std::optional<DisplayId> FindConnectedDisplayId(
        std::uint32_t backendDisplayId) const;
    [[nodiscard]] std::optional<DisplayId> FindDisplayIdForRemoval(
        std::uint32_t backendDisplayId) const;

    [[nodiscard]] PlatformTimestamp Now() const;
    [[nodiscard]] std::optional<PlatformEvent> PollEvent();
    [[nodiscard]] core::Result<std::vector<DisplayInfo>> EnumerateDisplays();
    [[nodiscard]] core::Result<DisplayInfo> GetDisplayInfo(DisplayId id);
    [[nodiscard]] core::Result<DisplayId> GetDisplayIdForWindow(
        void* nativeWindow, WindowId windowId);
    [[nodiscard]] core::Result<float> GetPixelDensityForWindow(
        void* nativeWindow, WindowId windowId) const;
    [[nodiscard]] core::Result<float> GetDisplayScaleForWindow(
        void* nativeWindow, WindowId windowId) const;

private:
    friend class WindowImpl;

    [[nodiscard]] core::Result<std::vector<std::uint32_t>> RefreshDisplays();
    [[nodiscard]] core::Result<DisplayInfo> QueryDisplayInfo(
        DisplayId id, std::uint32_t backendDisplayId) const;
    [[nodiscard]] std::optional<DisplayId> FindKnownDisplayId(
        std::uint32_t backendDisplayId) const;
    [[nodiscard]] std::optional<DisplayId> ConnectDisplayFromEvent(
        std::uint32_t backendDisplayId);
    void DisconnectDisplayFromEvent(std::uint32_t backendDisplayId);
    void ReconcileDisplayFromEvent(std::uint32_t backendDisplayId);
    void ObserveWindowShownEvent(std::uint32_t backendWindowId);

    PlatformRuntimeBackend m_backend;
    PlatformWindowBackend m_windowBackend;
    PlatformDisplayBackend m_displayBackend;
    RuntimeHintSnapshot m_focusClickThrough;
    RuntimeHintSnapshot m_autoCapture;
    RuntimeMetadataSnapshots m_metadata;
    std::thread::id m_ownerThread;
    RuntimeChildRegistry m_registry;
    std::unordered_map<std::uint32_t, RuntimeWindowRecord> m_windowsByBackendId;
    WindowId::ValueType m_nextWindowId{1};
    std::unordered_map<std::uint32_t, RuntimeDisplayRecord> m_displaysByBackendId;
    std::unordered_map<DisplayId, RuntimeBackendDisplayRecord> m_displaysById;
    DisplayId::ValueType m_nextDisplayId{1};
};
} // namespace pond::platform::detail
