#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "PlatformRuntimeState.hpp"

namespace pond::platform
{
namespace
{
constexpr std::string_view kLogCategory{"platform"};
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kRuntimeAlreadyActiveCode =
    ToErrorCode(PlatformErrorCode::RuntimeAlreadyActive);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);

enum class RuntimeSlotState : std::uint8_t
{
    Available,
    Creating,
    Active,
    Destroying
};

std::atomic<RuntimeSlotState> runtimeSlot{RuntimeSlotState::Available};
const std::thread::id processEntryThread = std::this_thread::get_id();


[[nodiscard]] core::VoidResult Validate(const PlatformRuntimeDesc& desc)
{
    using ResultType = core::VoidResult;

    if (desc.applicationName.empty() || !core::IsValidUtf8WithoutEmbeddedNull(desc.applicationName))
    {
        return ResultType::FromError(core::Error{kInvalidArgumentCode,
            "Platform runtime application name must be non-empty UTF-8 without embedded nulls."});
    }

    if (desc.applicationVersion.has_value() &&
        !core::IsValidUtf8WithoutEmbeddedNull(*desc.applicationVersion))
    {
        return ResultType::FromError(core::Error{kInvalidArgumentCode,
            "Platform runtime application version must be UTF-8 without embedded nulls."});
    }

    if (desc.applicationVersion.has_value() && desc.applicationVersion->empty())
    {
        return ResultType::FromError(core::Error{kInvalidArgumentCode,
            "Platform runtime application version must be absent or non-empty."});
    }

    if (desc.applicationIdentifier.has_value() &&
        !core::IsValidUtf8WithoutEmbeddedNull(*desc.applicationIdentifier))
    {
        return ResultType::FromError(core::Error{kInvalidArgumentCode,
            "Platform runtime application identifier must be UTF-8 without embedded nulls."});
    }

    if (desc.applicationIdentifier.has_value() && desc.applicationIdentifier->empty())
    {
        return ResultType::FromError(core::Error{kInvalidArgumentCode,
            "Platform runtime application identifier must be absent or non-empty."});
    }

    return ResultType::Success();
}

void VerifyWindowBackend(const detail::PlatformWindowBackend& backend)
{
    PONDER_VERIFY(backend.create != nullptr, "Platform window backend is missing create");
    PONDER_VERIFY(backend.destroy != nullptr, "Platform window backend is missing destroy");
    PONDER_VERIFY(backend.getId != nullptr, "Platform window backend is missing getId");
    PONDER_VERIFY(backend.getTitle != nullptr, "Platform window backend is missing getTitle");
    PONDER_VERIFY(backend.setTitle != nullptr, "Platform window backend is missing setTitle");
    PONDER_VERIFY(backend.getPosition != nullptr, "Platform window backend is missing getPosition");
    PONDER_VERIFY(backend.setPosition != nullptr, "Platform window backend is missing setPosition");
    PONDER_VERIFY(backend.getSize != nullptr, "Platform window backend is missing getSize");
    PONDER_VERIFY(backend.getSizeInPixels != nullptr,
                  "Platform window backend is missing getSizeInPixels");
    PONDER_VERIFY(backend.setSize != nullptr, "Platform window backend is missing setSize");
    PONDER_VERIFY(backend.setMinimumSize != nullptr,
                  "Platform window backend is missing setMinimumSize");
    PONDER_VERIFY(backend.show != nullptr, "Platform window backend is missing show");
    PONDER_VERIFY(backend.hide != nullptr, "Platform window backend is missing hide");
    PONDER_VERIFY(backend.getProperties != nullptr,
                  "Platform window backend is missing getProperties");
    PONDER_VERIFY(backend.setFullscreenModeToDesktop != nullptr,
                  "Platform window backend is missing setFullscreenModeToDesktop");
    PONDER_VERIFY(backend.setFullscreen != nullptr,
                  "Platform window backend is missing setFullscreen");
    PONDER_VERIFY(backend.setBordered != nullptr, "Platform window backend is missing setBordered");
    PONDER_VERIFY(backend.setResizable != nullptr,
                  "Platform window backend is missing setResizable");
    PONDER_VERIFY(backend.setAlwaysOnTop != nullptr,
                  "Platform window backend is missing setAlwaysOnTop");
    PONDER_VERIFY(backend.minimize != nullptr, "Platform window backend is missing minimize");
    PONDER_VERIFY(backend.maximize != nullptr, "Platform window backend is missing maximize");
    PONDER_VERIFY(backend.restore != nullptr, "Platform window backend is missing restore");
    PONDER_VERIFY(backend.startTextInput != nullptr,
                  "Platform window backend is missing startTextInput");
    PONDER_VERIFY(backend.stopTextInput != nullptr,
                  "Platform window backend is missing stopTextInput");
    PONDER_VERIFY(backend.isTextInputActive != nullptr,
                  "Platform window backend is missing isTextInputActive");
    PONDER_VERIFY(backend.clearTextComposition != nullptr,
                  "Platform window backend is missing clearTextComposition");
    PONDER_VERIFY(backend.setTextInputArea != nullptr,
                  "Platform window backend is missing setTextInputArea");
    PONDER_VERIFY(backend.setMouseGrab != nullptr,
                  "Platform window backend is missing setMouseGrab");
    PONDER_VERIFY(backend.isMouseGrabbed != nullptr,
                  "Platform window backend is missing isMouseGrabbed");
    PONDER_VERIFY(backend.setRelativeMouseMode != nullptr,
                  "Platform window backend is missing setRelativeMouseMode");
    PONDER_VERIFY(backend.isRelativeMouseModeEnabled != nullptr,
                  "Platform window backend is missing isRelativeMouseModeEnabled");
    PONDER_VERIFY(backend.getNativeHandle != nullptr,
                  "Platform window backend is missing getNativeHandle");
}

void VerifyDisplayBackend(const detail::PlatformDisplayBackend& backend)
{
    PONDER_VERIFY(backend.enumerate != nullptr, "Platform display backend is missing enumerate");
    PONDER_VERIFY(backend.getName != nullptr, "Platform display backend is missing getName");
    PONDER_VERIFY(backend.getBounds != nullptr, "Platform display backend is missing getBounds");
    PONDER_VERIFY(backend.getUsableBounds != nullptr,
                  "Platform display backend is missing getUsableBounds");
    PONDER_VERIFY(backend.getCurrentRefreshRate != nullptr,
                  "Platform display backend is missing getCurrentRefreshRate");
    PONDER_VERIFY(backend.getCurrentOrientation != nullptr,
                  "Platform display backend is missing getCurrentOrientation");
    PONDER_VERIFY(backend.getContentScale != nullptr,
                  "Platform display backend is missing getContentScale");
    PONDER_VERIFY(backend.getForWindow != nullptr,
                  "Platform display backend is missing getForWindow");
    PONDER_VERIFY(backend.getWindowPixelDensity != nullptr,
                  "Platform display backend is missing getWindowPixelDensity");
    PONDER_VERIFY(backend.getWindowDisplayScale != nullptr,
                  "Platform display backend is missing getWindowDisplayScale");
}


void ReleaseRuntimeReservation() noexcept
{
    runtimeSlot.store(RuntimeSlotState::Available, std::memory_order_release);
}

void MarkRuntimeActive()
{
    RuntimeSlotState expected = RuntimeSlotState::Creating;
    PONDER_VERIFY(runtimeSlot.compare_exchange_strong(expected, RuntimeSlotState::Active,
                                                      std::memory_order_acq_rel),
                  "Platform runtime slot was not creating");
}

void BeginRuntimeDestruction()
{
    RuntimeSlotState expected = RuntimeSlotState::Active;
    PONDER_VERIFY(runtimeSlot.compare_exchange_strong(expected, RuntimeSlotState::Destroying,
                                                      std::memory_order_acq_rel),
                  "Platform runtime slot was not active");
}
} // namespace

namespace detail
{
PlatformRuntimeState::PlatformRuntimeState(std::unique_ptr<IPlatformRuntimeBackend> backend,
                                           PlatformWindowBackend windowBackend,
                                           PlatformDisplayBackend displayBackend) noexcept
    : m_backend(std::move(backend)), m_windowBackend(windowBackend),
      m_displayBackend(displayBackend), m_ownerThread(std::this_thread::get_id())
{
    LOG_INFO_CATEGORY(kLogCategory, "Platform runtime initialized");
}

PlatformRuntimeState::~PlatformRuntimeState() noexcept
{
    VerifyOwnerThread("destruction");
    PONDER_VERIFY(m_registry.IsEmpty(),
                  "Cannot destroy PlatformRuntime with {} children and {} requests",
                  m_registry.GetChildCount(), m_registry.GetRequestCount());
    PONDER_VERIFY(m_windowsByBackendId.empty(),
                  "Cannot destroy PlatformRuntime with {} backend window mappings",
                  m_windowsByBackendId.size());
    PONDER_VERIFY(m_backend != nullptr, "Platform runtime backend is unavailable");
    PONDER_VERIFY(m_backend->HasExpectedRuntimeSubsystems(),
                  "SDL subsystem ownership changed while PlatformRuntime was active");
    BeginRuntimeDestruction();

    LOG_INFO_CATEGORY(kLogCategory, "Platform runtime shutting down");
    DestroySystemCursors();
    m_backend->Quit();
    m_backend.reset();
    LOG_INFO_CATEGORY(kLogCategory, "Platform runtime shut down");
    ReleaseRuntimeReservation();
}

void PlatformRuntimeState::VerifyOwnerThread(std::string_view operation) const
{
    PONDER_VERIFY(std::this_thread::get_id() == m_ownerThread,
                  "PlatformRuntime {} must run on its owner thread", operation);
}

void PlatformRuntimeState::RegisterChild(const void* child)
{
    VerifyOwnerThread("child registration");
    m_registry.RegisterChild(child);
}

void PlatformRuntimeState::UnregisterChild(const void* child)
{
    VerifyOwnerThread("child unregistration");
    m_registry.UnregisterChild(child);
}

void PlatformRuntimeState::RegisterRequest(const void* request)
{
    VerifyOwnerThread("request registration");
    m_registry.RegisterRequest(request);
}

void PlatformRuntimeState::UnregisterRequest(const void* request)
{
    VerifyOwnerThread("request unregistration");
    m_registry.UnregisterRequest(request);
}

WindowId PlatformRuntimeState::RegisterWindow(WindowImpl* window, std::uint32_t backendWindowId)
{
    VerifyOwnerThread("window registration");
    PONDER_VERIFY(window != nullptr, "Cannot register a null platform window");
    PONDER_VERIFY(backendWindowId != 0, "Cannot register a zero backend window ID");
    PONDER_VERIFY(m_nextWindowId != 0, "Platform window ID space is exhausted");

    const WindowId id{m_nextWindowId};
    const auto [iterator, inserted] =
        m_windowsByBackendId.emplace(backendWindowId, RuntimeWindowRecord{id, window});
    static_cast<void>(iterator);
    PONDER_VERIFY(inserted, "Backend window ID {} is already registered", backendWindowId);

    try
    {
        m_registry.RegisterChild(window);
    }
    catch (...)
    {
        m_windowsByBackendId.erase(backendWindowId);
        throw;
    }

    ++m_nextWindowId;
    return id;
}

void PlatformRuntimeState::BeginWindowDestruction(WindowImpl* window, std::uint32_t backendWindowId,
                                                  WindowId id)
{
    VerifyOwnerThread("window destruction");
    PONDER_VERIFY(window != nullptr, "Cannot destroy a null platform window");

    const auto iterator = m_windowsByBackendId.find(backendWindowId);
    PONDER_VERIFY(iterator != m_windowsByBackendId.end(), "Backend window ID {} is not registered",
                  backendWindowId);
    PONDER_VERIFY(iterator->second.id == id,
                  "Backend window ID {} does not match project window ID {}", backendWindowId,
                  id.GetValue());
    PONDER_VERIFY(iterator->second.window == window,
                  "Backend window ID {} does not match its registered window", backendWindowId);
    m_windowsByBackendId.erase(iterator);
}

void PlatformRuntimeState::FinishWindowDestruction(WindowImpl* window)
{
    VerifyOwnerThread("window destruction");
    m_registry.UnregisterChild(window);
}

std::optional<WindowId> PlatformRuntimeState::FindWindowId(std::uint32_t backendWindowId) const
{
    VerifyOwnerThread("window lookup");
    const auto iterator = m_windowsByBackendId.find(backendWindowId);
    return iterator != m_windowsByBackendId.end() ? std::optional<WindowId>{iterator->second.id}
                                                  : std::nullopt;
}

Timestamp PlatformRuntimeState::Now() const
{
    VerifyOwnerThread("timestamp query");
    return CaptureBackendTimestamp();
}

HintManager& PlatformRuntimeState::GetHintManager()
{
    VerifyOwnerThread("hint manager access");
    return m_backend->GetHintManager();
}

Timestamp PlatformRuntimeState::CaptureBackendTimestamp() const
{
    const std::uint64_t ticks = m_backend->GetTicksNanoseconds();
    constexpr auto kMaximumTicks =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    PONDER_VERIFY(ticks <= kMaximumTicks,
                  "SDL monotonic timestamp exceeds Timestamp range");
    return Timestamp{std::chrono::nanoseconds{static_cast<std::int64_t>(ticks)}};
}
} // namespace detail

core::Result<PlatformRuntime> PlatformRuntime::Create(const PlatformRuntimeDesc& desc)
{
    using ResultType = core::Result<PlatformRuntime>;

    RuntimeSlotState expectedSlot = RuntimeSlotState::Available;
    if (!runtimeSlot.compare_exchange_strong(expectedSlot,
                                             RuntimeSlotState::Creating,
                                             std::memory_order_acq_rel))
    {
        return ResultType::FromError(core::Error{kRuntimeAlreadyActiveCode,
            "A platform runtime is already active."});
    }

    auto releaseReservation = core::MakeScopeExit(
        []() noexcept
        {
            ReleaseRuntimeReservation();
        });

    RETURN_ERROR_IF_FAILED(Validate(desc));

    PONDER_VERIFY(std::this_thread::get_id() == processEntryThread,
                  "PlatformRuntime must be created on the process entry thread");

    std::unique_ptr<detail::IPlatformRuntimeBackend> backend =
        detail::GetPlatformRuntimeBackend();

    const detail::PlatformWindowBackend windowBackend = detail::GetPlatformWindowBackend();
    VerifyWindowBackend(windowBackend);

    const detail::PlatformDisplayBackend displayBackend = detail::GetPlatformDisplayBackend();
    VerifyDisplayBackend(displayBackend);

    PONDER_VERIFY(backend->IsMainThread(),
                  "PlatformRuntime must be created on SDL's main thread");

    if (backend->HasInitializedSubsystems())
    {
        return ResultType::FromError(core::Error{kBackendFailureCode,
            "Cannot create PlatformRuntime while SDL subsystems are already initialized."});
    }

    HintManager& hintManager = backend->GetHintManager();
    RETURN_ERROR_IF_FAILED(hintManager.PushHint(hints::MouseFocusClickThrough{true}));
    RETURN_ERROR_IF_FAILED(hintManager.PushHint(hints::MouseAutoCapture{false}));

    if (desc.configureHintsBeforeInitialization != nullptr)
    {
        desc.configureHintsBeforeInitialization(hintManager);
    }

    RETURN_ERROR_IF_FAILED(backend->SetAppMetadataProperty(
        detail::ApplicationMetadataProperty::Name, desc.applicationName.c_str()));
    RETURN_ERROR_IF_FAILED(backend->SetAppMetadataProperty(
        detail::ApplicationMetadataProperty::Version,
        desc.applicationVersion.has_value() ? desc.applicationVersion->c_str() : nullptr));
    RETURN_ERROR_IF_FAILED(backend->SetAppMetadataProperty(
        detail::ApplicationMetadataProperty::Identifier,
        desc.applicationIdentifier.has_value() ? desc.applicationIdentifier->c_str() : nullptr));

    auto quitOnFailure = core::MakeScopeExit(
        [&backend]() noexcept
        {
            backend->Quit();
        });

    RETURN_ERROR_IF_FAILED(backend->InitializeVideo());

    auto state = std::make_unique<detail::PlatformRuntimeState>(
        std::move(backend), windowBackend, displayBackend);

    MarkRuntimeActive();
    quitOnFailure.Dismiss();
    releaseReservation.Dismiss();
    return PlatformRuntime{std::move(state)};
}

PlatformRuntime::PlatformRuntime(std::unique_ptr<detail::PlatformRuntimeState> state) noexcept
    : m_state(std::move(state))
{
}

PlatformRuntime::~PlatformRuntime() noexcept = default;
PlatformRuntime::PlatformRuntime(PlatformRuntime&&) noexcept = default;
PlatformRuntime& PlatformRuntime::operator=(PlatformRuntime&&) noexcept = default;

HintManager& PlatformRuntime::GetHintManager()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->GetHintManager();
}

Timestamp PlatformRuntime::Now() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->Now();
}
} // namespace pond::platform
