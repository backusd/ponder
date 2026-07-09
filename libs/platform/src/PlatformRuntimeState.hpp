#pragma once

#include "PlatformRuntimeBackend.hpp"
#include "RuntimeChildRegistry.hpp"

#include <ponder/core/Result.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Identifiers.hpp>
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
class WindowState;

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
};

struct RuntimeBackendDisplayRecord final
{
    std::uint32_t backendId{};
    bool connected{};
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

    [[nodiscard]] WindowId RegisterWindow(const void* window,
                                          std::uint32_t backendWindowId);
    void BeginWindowDestruction(const void* window, std::uint32_t backendWindowId,
                                WindowId id);
    void FinishWindowDestruction(const void* window);
    [[nodiscard]] std::optional<WindowId> FindWindowId(
        std::uint32_t backendWindowId) const;

    [[nodiscard]] PlatformTimestamp Now() const;
    [[nodiscard]] core::Result<std::vector<DisplayInfo>> EnumerateDisplays();
    [[nodiscard]] core::Result<DisplayInfo> GetDisplayInfo(DisplayId id);
    [[nodiscard]] core::Result<DisplayId> GetDisplayIdForWindow(
        void* nativeWindow, WindowId windowId);
    [[nodiscard]] core::Result<float> GetPixelDensityForWindow(
        void* nativeWindow, WindowId windowId) const;
    [[nodiscard]] core::Result<float> GetDisplayScaleForWindow(
        void* nativeWindow, WindowId windowId) const;

private:
    friend class WindowState;

    [[nodiscard]] core::Result<std::vector<std::uint32_t>> RefreshDisplays();
    [[nodiscard]] core::Result<DisplayInfo> QueryDisplayInfo(
        DisplayId id, std::uint32_t backendDisplayId) const;

    PlatformRuntimeBackend m_backend;
    PlatformWindowBackend m_windowBackend;
    PlatformDisplayBackend m_displayBackend;
    RuntimeHintSnapshot m_focusClickThrough;
    RuntimeHintSnapshot m_autoCapture;
    RuntimeMetadataSnapshots m_metadata;
    std::thread::id m_ownerThread;
    RuntimeChildRegistry m_registry;
    std::unordered_map<std::uint32_t, WindowId> m_windowIdsByBackendId;
    WindowId::ValueType m_nextWindowId{1};
    std::unordered_map<std::uint32_t, RuntimeDisplayRecord> m_displaysByBackendId;
    std::unordered_map<DisplayId, RuntimeBackendDisplayRecord> m_displaysById;
    DisplayId::ValueType m_nextDisplayId{1};
};
} // namespace pond::platform::detail
