#include "WindowRegistry.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <format>
#include <limits>
#include <utility>

#include "PlatformCommon.hpp"
#include "WindowImpl.hpp"

namespace ponder::platform::detail
{
class DialogParentLeaseState final
{
public:
    BackendWindowHandle backendWindow;
    std::size_t activeLeaseCount{};
};

DialogParentLease::DialogParentLease(std::shared_ptr<DialogParentLeaseState> state) noexcept :
    m_state(std::move(state))
{
    PONDER_VERIFY(m_state != nullptr, "Cannot create a dialog parent lease without state");
    PONDER_VERIFY(m_state->activeLeaseCount != std::numeric_limits<std::size_t>::max(), "Dialog parent lease count is exhausted");
    ++m_state->activeLeaseCount;
}

DialogParentLease::DialogParentLease(DialogParentLease&& other) noexcept :
    m_state(std::move(other.m_state))
{
}

DialogParentLease& DialogParentLease::operator=(DialogParentLease&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_state = std::move(other.m_state);
    }
    return *this;
}

DialogParentLease::~DialogParentLease() noexcept
{
    Reset();
}

BackendWindowHandle DialogParentLease::GetBackendWindow() const noexcept
{
    PONDER_VERIFY(m_state != nullptr, "Cannot access an empty dialog parent lease");
    return m_state->backendWindow;
}

void DialogParentLease::Reset() noexcept
{
    if (m_state == nullptr)
    {
        return;
    }

    PONDER_VERIFY(m_state->activeLeaseCount > 0, "Cannot release an unknown dialog parent lease");
    --m_state->activeLeaseCount;
    m_state.reset();
}

WindowRegistry::WindowRegistry(const RuntimeOwnerThreadGuard& ownerThread) noexcept :
    m_ownerThread(ownerThread)
{
}

WindowId WindowRegistry::GetNextWindowId() const
{
    m_ownerThread.Verify("window registration preparation");
    PONDER_VERIFY(m_nextWindowId != 0, "Platform window ID space is exhausted");
    return WindowId{m_nextWindowId};
}

void WindowRegistry::Register(WindowImpl& window, BackendWindowHandle backendWindow, std::uint32_t backendWindowId, WindowId id)
{
    m_ownerThread.Verify("window registration");
    PONDER_VERIFY(backendWindow.IsValid(), "Cannot register an invalid backend window handle");
    PONDER_VERIFY(backendWindowId != 0, "Cannot register a zero backend window ID");
    PONDER_VERIFY(m_nextWindowId != 0, "Platform window ID space is exhausted");
    PONDER_VERIFY(id.GetValue() == m_nextWindowId, "Platform window ID {} is not the next available ID {}", id, m_nextWindowId);
    PONDER_VERIFY(!m_windowIdsByBackendId.contains(backendWindowId), "Backend window ID {} is already registered", backendWindowId);
    PONDER_VERIFY(!m_windowsById.contains(id), "Platform window ID {} is already registered", id);

    auto parentState = std::make_shared<DialogParentLeaseState>(DialogParentLeaseState{.backendWindow = backendWindow});
    const auto [windowIterator, windowInserted] =
        m_windowsById.emplace(id, Record{.window = &window, .backendWindowId = backendWindowId, .dialogParentState = std::move(parentState)});
    PONDER_VERIFY(windowInserted, "Platform window ID {} is already registered", id);
    auto rollbackWindowMapping = ponder::core::MakeScopeExit(
        [this, windowIterator]() noexcept
        {
            m_windowsById.erase(windowIterator);
        });
    const auto [backendIterator, backendInserted] = m_windowIdsByBackendId.emplace(backendWindowId, id);
    PONDER_VERIFY(backendInserted, "Backend window ID {} is already registered", backendWindowId);
    auto rollbackBackendMapping = ponder::core::MakeScopeExit(
        [this, backendIterator]() noexcept
        {
            m_windowIdsByBackendId.erase(backendIterator);
        });
    ++m_nextWindowId;
    rollbackBackendMapping.Dismiss();
    rollbackWindowMapping.Dismiss();
}

void WindowRegistry::RollbackRegistration(WindowImpl& window, std::uint32_t backendWindowId, WindowId id) noexcept
{
    Unregister(window, backendWindowId, id);
    RestoreWindowIdAfterFailedConstruction(id);
}

void WindowRegistry::RestoreWindowIdAfterFailedConstruction(WindowId id) noexcept
{
    PONDER_VERIFY(id.IsValid(), "Cannot restore an invalid platform window ID");
    PONDER_VERIFY(m_nextWindowId == id.GetValue() + WindowId::ValueType{1} ||
                      (id.GetValue() == std::numeric_limits<WindowId::ValueType>::max() && m_nextWindowId == 0),
                  "Cannot restore platform window ID {} from next ID {}", id, m_nextWindowId);
    PONDER_VERIFY(!m_windowsById.contains(id), "Cannot restore platform window ID {} while it is registered", id);
    m_nextWindowId = id.GetValue();
}

void WindowRegistry::Unregister(WindowImpl& window, std::uint32_t backendWindowId, WindowId id)
{
    m_ownerThread.Verify("window destruction");
    PONDER_VERIFY(id.IsValid(), "Cannot unregister an invalid platform window ID");

    const auto windowIterator = m_windowsById.find(id);
    PONDER_VERIFY(windowIterator != m_windowsById.end(), "Platform window ID {} is not registered", id);

    const Record& record = windowIterator->second;
    PONDER_VERIFY(record.window == &window, "Platform window ID {} does not match its registered window", id);
    PONDER_VERIFY(record.backendWindowId == backendWindowId, "Platform window ID {} does not match backend window ID {}", id, backendWindowId);
    PONDER_VERIFY(record.dialogParentState != nullptr, "Platform window ID {} has no dialog parent state", id);
    PONDER_VERIFY(record.dialogParentState->activeLeaseCount == 0, "Cannot destroy a platform window with {} pending dialog requests",
                  record.dialogParentState->activeLeaseCount);

    const auto backendIterator = m_windowIdsByBackendId.find(backendWindowId);
    PONDER_VERIFY(backendIterator != m_windowIdsByBackendId.end(), "Backend window ID {} is not registered", backendWindowId);
    PONDER_VERIFY(backendIterator->second == id, "Backend window ID {} does not match project window ID {}", backendWindowId, id);

    m_windowIdsByBackendId.erase(backendIterator);
    m_windowsById.erase(windowIterator);
}

DialogParentLease WindowRegistry::AcquireDialogLease(WindowId id)
{
    m_ownerThread.Verify("dialog parent lookup");
    if (!id.IsValid())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Dialog parent window ID must be valid.");
    }

    const auto iterator = m_windowsById.find(id);
    if (iterator == m_windowsById.end())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::NotFound, "Dialog parent window {} was not found.", id);
    }

    Record& record = iterator->second;
    PONDER_VERIFY(record.window != nullptr, "Dialog parent window {} has no owning window", id);
    PONDER_VERIFY(record.dialogParentState != nullptr, "Dialog parent window {} has no lease state", id);
    record.window->VerifyUsable("dialog parent lookup");
    return DialogParentLease{record.dialogParentState};
}

std::optional<WindowId> WindowRegistry::FindWindowId(std::uint32_t backendWindowId) const
{
    m_ownerThread.Verify("window lookup");
    const auto backendIterator = m_windowIdsByBackendId.find(backendWindowId);
    if (backendIterator == m_windowIdsByBackendId.end())
    {
        return std::nullopt;
    }

    PONDER_VERIFY(m_windowsById.contains(backendIterator->second), "Backend window ID {} maps to an unknown platform window ID {}", backendWindowId,
                  backendIterator->second);
    return backendIterator->second;
}

void WindowRegistry::ObserveWindowShownEvent(std::uint32_t backendWindowId) const
{
    m_ownerThread.Verify("window shown-event observation");
    const auto backendIterator = m_windowIdsByBackendId.find(backendWindowId);
    if (backendIterator == m_windowIdsByBackendId.end())
    {
        return;
    }

    const auto windowIterator = m_windowsById.find(backendIterator->second);
    PONDER_VERIFY(windowIterator != m_windowsById.end(), "Backend window ID {} maps to an unknown platform window ID {}", backendWindowId,
                  backendIterator->second);
    PONDER_VERIFY(windowIterator->second.window != nullptr, "Registered backend window {} has no owning window", backendWindowId);
    windowIterator->second.window->ObserveShownEvent();
}

bool WindowRegistry::IsEmpty() const noexcept
{
    return m_windowsById.empty() && m_windowIdsByBackendId.empty();
}

std::size_t WindowRegistry::GetWindowCount() const noexcept
{
    PONDER_VERIFY(m_windowsById.size() == m_windowIdsByBackendId.size(), "Window registry indices contain different entry counts");
    return m_windowsById.size();
}
} // namespace ponder::platform::detail
