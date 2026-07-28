#pragma once

#include <ponder/platform/Identifiers.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

#include "IPlatformWindowBackend.hpp"

namespace ponder::platform::detail
{
class DialogParentLeaseState;
class RuntimeOwnerThreadGuard;
class WindowImpl;

class DialogParentLease final
{
public:
    DialogParentLease(const DialogParentLease&) = delete;
    DialogParentLease& operator=(const DialogParentLease&) = delete;
    DialogParentLease(DialogParentLease&& other) noexcept;
    DialogParentLease& operator=(DialogParentLease&& other) noexcept;
    ~DialogParentLease() noexcept;

    [[nodiscard]] BackendWindowHandle GetBackendWindow() const noexcept;

private:
    friend class WindowRegistry;

    explicit DialogParentLease(std::shared_ptr<DialogParentLeaseState> state) noexcept;
    void Reset() noexcept;

    std::shared_ptr<DialogParentLeaseState> m_state;
};

class WindowRegistry final
{
public:
    explicit WindowRegistry(const RuntimeOwnerThreadGuard& ownerThread) noexcept;

    [[nodiscard]] WindowId GetNextWindowId() const;
    void Register(WindowImpl& window, BackendWindowHandle backendWindow, std::uint32_t backendWindowId, WindowId id);
    void RollbackRegistration(WindowImpl& window, std::uint32_t backendWindowId, WindowId id) noexcept;
    void RestoreWindowIdAfterFailedConstruction(WindowId id) noexcept;
    void Unregister(WindowImpl& window, std::uint32_t backendWindowId, WindowId id);

    [[nodiscard]] DialogParentLease AcquireDialogLease(WindowId id);
    [[nodiscard]] std::optional<WindowId> FindWindowId(std::uint32_t backendWindowId) const;
    void ObserveWindowShownEvent(std::uint32_t backendWindowId) const;

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] std::size_t GetWindowCount() const noexcept;

private:
    struct Record final
    {
        WindowImpl* window{};
        std::uint32_t backendWindowId{};
        std::shared_ptr<DialogParentLeaseState> dialogParentState;
    };

    const RuntimeOwnerThreadGuard& m_ownerThread;
    WindowId::ValueType m_nextWindowId{1};
    std::unordered_map<WindowId, Record> m_windowsById;
    std::unordered_map<std::uint32_t, WindowId> m_windowIdsByBackendId;
};
} // namespace ponder::platform::detail
