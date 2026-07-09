#include <ponder/platform/PlatformRuntime.hpp>

#include "PlatformRuntimeState.hpp"
#include "SdlError.hpp"
#include "StringValidation.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace pond::platform
{
namespace
{
constexpr std::string_view kLogCategory{"platform"};
constexpr core::ErrorCode kInvalidArgumentCode =
    ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kRuntimeAlreadyActiveCode =
    ToErrorCode(PlatformErrorCode::RuntimeAlreadyActive);
constexpr core::ErrorCode kBackendFailureCode =
    ToErrorCode(PlatformErrorCode::BackendFailure);

enum class RuntimeSlotState : std::uint8_t
{
    Available,
    Creating,
    Active,
    Destroying
};

std::atomic<RuntimeSlotState> runtimeSlot{RuntimeSlotState::Available};
const std::thread::id processEntryThread = std::this_thread::get_id();

using HintSnapshot = detail::RuntimeHintSnapshot;
using MetadataSnapshot = detail::RuntimeMetadataSnapshot;
using MetadataSnapshots = detail::RuntimeMetadataSnapshots;

[[nodiscard]] core::VoidResult Validate(const PlatformRuntimeDesc& desc)
{
    if (desc.applicationName.empty() ||
        !detail::IsValidUtf8WithoutEmbeddedNull(desc.applicationName))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Platform runtime application name must be non-empty UTF-8 without embedded nulls."});
    }

    if (desc.applicationVersion.has_value() &&
        !detail::IsValidUtf8WithoutEmbeddedNull(*desc.applicationVersion))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Platform runtime application version must be UTF-8 without embedded nulls."});
    }

    if (desc.applicationVersion.has_value() && desc.applicationVersion->empty())
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Platform runtime application version must be absent or non-empty."});
    }

    if (desc.applicationIdentifier.has_value() &&
        !detail::IsValidUtf8WithoutEmbeddedNull(*desc.applicationIdentifier))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Platform runtime application identifier must be UTF-8 without embedded nulls."});
    }

    if (desc.applicationIdentifier.has_value() && desc.applicationIdentifier->empty())
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Platform runtime application identifier must be absent or non-empty."});
    }

    return core::VoidResult::Success();
}

void VerifyBackend(const detail::PlatformRuntimeBackend& backend)
{
    PONDER_VERIFY(backend.isMainThread != nullptr,
                  "Platform runtime backend is missing isMainThread");
    PONDER_VERIFY(backend.hasInitializedSubsystems != nullptr,
                  "Platform runtime backend is missing hasInitializedSubsystems");
    PONDER_VERIFY(backend.hasExpectedRuntimeSubsystems != nullptr,
                  "Platform runtime backend is missing hasExpectedRuntimeSubsystems");
    PONDER_VERIFY(backend.getAppMetadataProperty != nullptr,
                  "Platform runtime backend is missing getAppMetadataProperty");
    PONDER_VERIFY(backend.setAppMetadataProperty != nullptr,
                  "Platform runtime backend is missing setAppMetadataProperty");
    PONDER_VERIFY(backend.getHint != nullptr, "Platform runtime backend is missing getHint");
    PONDER_VERIFY(backend.setHintOverride != nullptr,
                  "Platform runtime backend is missing setHintOverride");
    PONDER_VERIFY(backend.resetHint != nullptr,
                  "Platform runtime backend is missing resetHint");
    PONDER_VERIFY(backend.initializeVideo != nullptr,
                  "Platform runtime backend is missing initializeVideo");
    PONDER_VERIFY(backend.quit != nullptr, "Platform runtime backend is missing quit");
    PONDER_VERIFY(backend.getTicksNanoseconds != nullptr,
                  "Platform runtime backend is missing getTicksNanoseconds");
    PONDER_VERIFY(backend.pollEvent != nullptr,
                  "Platform runtime backend is missing pollEvent");
}

void VerifyWindowBackend(const detail::PlatformWindowBackend& backend)
{
    PONDER_VERIFY(backend.create != nullptr,
                  "Platform window backend is missing create");
    PONDER_VERIFY(backend.destroy != nullptr,
                  "Platform window backend is missing destroy");
    PONDER_VERIFY(backend.getId != nullptr,
                  "Platform window backend is missing getId");
    PONDER_VERIFY(backend.getTitle != nullptr,
                  "Platform window backend is missing getTitle");
    PONDER_VERIFY(backend.setTitle != nullptr,
                  "Platform window backend is missing setTitle");
    PONDER_VERIFY(backend.getPosition != nullptr,
                  "Platform window backend is missing getPosition");
    PONDER_VERIFY(backend.setPosition != nullptr,
                  "Platform window backend is missing setPosition");
    PONDER_VERIFY(backend.getSize != nullptr,
                  "Platform window backend is missing getSize");
    PONDER_VERIFY(backend.getSizeInPixels != nullptr,
                  "Platform window backend is missing getSizeInPixels");
    PONDER_VERIFY(backend.setSize != nullptr,
                  "Platform window backend is missing setSize");
    PONDER_VERIFY(backend.setMinimumSize != nullptr,
                  "Platform window backend is missing setMinimumSize");
    PONDER_VERIFY(backend.show != nullptr,
                  "Platform window backend is missing show");
    PONDER_VERIFY(backend.hide != nullptr,
                  "Platform window backend is missing hide");
    PONDER_VERIFY(backend.getProperties != nullptr,
                  "Platform window backend is missing getProperties");
    PONDER_VERIFY(backend.setFullscreenModeToDesktop != nullptr,
                  "Platform window backend is missing setFullscreenModeToDesktop");
    PONDER_VERIFY(backend.setFullscreen != nullptr,
                  "Platform window backend is missing setFullscreen");
    PONDER_VERIFY(backend.setBordered != nullptr,
                  "Platform window backend is missing setBordered");
    PONDER_VERIFY(backend.setResizable != nullptr,
                  "Platform window backend is missing setResizable");
    PONDER_VERIFY(backend.setAlwaysOnTop != nullptr,
                  "Platform window backend is missing setAlwaysOnTop");
    PONDER_VERIFY(backend.minimize != nullptr,
                  "Platform window backend is missing minimize");
    PONDER_VERIFY(backend.maximize != nullptr,
                  "Platform window backend is missing maximize");
    PONDER_VERIFY(backend.restore != nullptr,
                  "Platform window backend is missing restore");
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
}

void VerifyDisplayBackend(const detail::PlatformDisplayBackend& backend)
{
    PONDER_VERIFY(backend.enumerate != nullptr,
                  "Platform display backend is missing enumerate");
    PONDER_VERIFY(backend.getName != nullptr,
                  "Platform display backend is missing getName");
    PONDER_VERIFY(backend.getBounds != nullptr,
                  "Platform display backend is missing getBounds");
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

[[nodiscard]] HintSnapshot CaptureHint(const detail::PlatformRuntimeBackend& backend,
                                       const char* name)
{
    const char* const currentValue = backend.getHint(backend.context, name);
    return HintSnapshot{name, currentValue != nullptr
                                  ? std::optional<std::string>{currentValue}
                                  : std::nullopt};
}

[[nodiscard]] bool HintMatchesSnapshot(const detail::PlatformRuntimeBackend& backend,
                                       const HintSnapshot& snapshot)
{
    const char* const currentValue = backend.getHint(backend.context, snapshot.name);
    if (!snapshot.value.has_value())
    {
        return currentValue == nullptr;
    }

    return currentValue != nullptr && std::string_view{currentValue} == *snapshot.value;
}

void RestoreHint(const detail::PlatformRuntimeBackend& backend,
                 const HintSnapshot& snapshot) noexcept
{
    try
    {
        const bool restored = snapshot.value.has_value()
                                  ? backend.setHintOverride(backend.context, snapshot.name,
                                                            snapshot.value->c_str())
                                  : backend.resetHint(backend.context, snapshot.name);
        if (!restored)
        {
            const core::Error error = detail::CaptureSdlFailure(
                kBackendFailureCode, "SDL hint restoration", snapshot.name);
            if (!HintMatchesSnapshot(backend, snapshot))
            {
                LOG_ERROR_CATEGORY(kLogCategory, "{}", error.GetMessage());
            }
        }
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "SDL hint restoration failed for {}: {}",
                           snapshot.name, exception.what());
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "SDL hint restoration failed for {}",
                           snapshot.name);
    }
}

void RestoreRuntimeHints(const detail::PlatformRuntimeBackend& backend,
                         const HintSnapshot& focusClickThrough,
                         const HintSnapshot& autoCapture) noexcept
{
    RestoreHint(backend, autoCapture);
    RestoreHint(backend, focusClickThrough);
}

[[nodiscard]] std::optional<std::string> CaptureMetadataValue(
    const detail::PlatformRuntimeBackend& backend,
    detail::ApplicationMetadataProperty property)
{
    const char* const value = backend.getAppMetadataProperty(backend.context, property);
    return value != nullptr ? std::optional<std::string>{value} : std::nullopt;
}

[[nodiscard]] MetadataSnapshots CaptureApplicationMetadata(
    const detail::PlatformRuntimeBackend& backend)
{
    return MetadataSnapshots{{
        MetadataSnapshot{detail::ApplicationMetadataProperty::Name,
                         CaptureMetadataValue(
                             backend, detail::ApplicationMetadataProperty::Name)},
        MetadataSnapshot{detail::ApplicationMetadataProperty::Version,
                         CaptureMetadataValue(
                             backend, detail::ApplicationMetadataProperty::Version)},
        MetadataSnapshot{detail::ApplicationMetadataProperty::Identifier,
                         CaptureMetadataValue(
                             backend, detail::ApplicationMetadataProperty::Identifier)},
    }};
}

[[nodiscard]] bool MetadataMatchesSnapshot(
    const detail::PlatformRuntimeBackend& backend,
    const MetadataSnapshot& snapshot)
{
    const char* const currentValue =
        backend.getAppMetadataProperty(backend.context, snapshot.property);
    if (!snapshot.value.has_value())
    {
        return currentValue == nullptr;
    }

    return currentValue != nullptr && std::string_view{currentValue} == *snapshot.value;
}

void RestoreMetadataProperty(const detail::PlatformRuntimeBackend& backend,
                             const MetadataSnapshot& snapshot) noexcept
{
    try
    {
        const bool restored = backend.setAppMetadataProperty(
            backend.context, snapshot.property,
            snapshot.value.has_value() ? snapshot.value->c_str() : nullptr);
        if (!restored)
        {
            const core::Error error = detail::CaptureSdlFailure(
                kBackendFailureCode, "SDL metadata restoration", "application metadata");
            if (!MetadataMatchesSnapshot(backend, snapshot))
            {
                LOG_ERROR_CATEGORY(kLogCategory, "{}", error.GetMessage());
            }
        }
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "SDL metadata restoration failed: {}",
                           exception.what());
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "SDL metadata restoration failed");
    }
}

void RestoreApplicationMetadata(const detail::PlatformRuntimeBackend& backend,
                                const MetadataSnapshots& snapshots) noexcept
{
    for (auto iterator = snapshots.rbegin(); iterator != snapshots.rend(); ++iterator)
    {
        RestoreMetadataProperty(backend, *iterator);
    }
}

void ReleaseRuntimeReservation() noexcept
{
    runtimeSlot.store(RuntimeSlotState::Available, std::memory_order_release);
}

void MarkRuntimeActive()
{
    RuntimeSlotState expected = RuntimeSlotState::Creating;
    PONDER_VERIFY(runtimeSlot.compare_exchange_strong(
                      expected, RuntimeSlotState::Active, std::memory_order_acq_rel),
                  "Platform runtime slot was not creating");
}

void BeginRuntimeDestruction()
{
    RuntimeSlotState expected = RuntimeSlotState::Active;
    PONDER_VERIFY(runtimeSlot.compare_exchange_strong(
                      expected, RuntimeSlotState::Destroying, std::memory_order_acq_rel),
                  "Platform runtime slot was not active");
}
} // namespace

namespace detail
{
PlatformRuntimeState::PlatformRuntimeState(PlatformRuntimeBackend backend,
                                           PlatformWindowBackend windowBackend,
                                           PlatformDisplayBackend displayBackend,
                                           RuntimeHintSnapshot focusClickThrough,
                                           RuntimeHintSnapshot autoCapture,
                                           RuntimeMetadataSnapshots metadata) noexcept
    : m_backend(backend), m_windowBackend(windowBackend),
      m_displayBackend(displayBackend),
      m_focusClickThrough(std::move(focusClickThrough)),
      m_autoCapture(std::move(autoCapture)), m_metadata(std::move(metadata)),
      m_ownerThread(std::this_thread::get_id())
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
    PONDER_VERIFY(m_backend.hasExpectedRuntimeSubsystems(m_backend.context),
                  "SDL subsystem ownership changed while PlatformRuntime was active");
    BeginRuntimeDestruction();

    LOG_INFO_CATEGORY(kLogCategory, "Platform runtime shutting down");
    m_backend.quit(m_backend.context);
    RestoreApplicationMetadata(m_backend, m_metadata);
    RestoreRuntimeHints(m_backend, m_focusClickThrough, m_autoCapture);
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

WindowId PlatformRuntimeState::RegisterWindow(WindowImpl* window,
                                              std::uint32_t backendWindowId)
{
    VerifyOwnerThread("window registration");
    PONDER_VERIFY(window != nullptr, "Cannot register a null platform window");
    PONDER_VERIFY(backendWindowId != 0,
                  "Cannot register a zero backend window ID");
    PONDER_VERIFY(m_nextWindowId != 0, "Platform window ID space is exhausted");

    const WindowId id{m_nextWindowId};
    const auto [iterator, inserted] =
        m_windowsByBackendId.emplace(backendWindowId,
                                     RuntimeWindowRecord{id, window});
    static_cast<void>(iterator);
    PONDER_VERIFY(inserted, "Backend window ID {} is already registered",
                  backendWindowId);

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

void PlatformRuntimeState::BeginWindowDestruction(WindowImpl* window,
                                                  std::uint32_t backendWindowId,
                                                  WindowId id)
{
    VerifyOwnerThread("window destruction");
    PONDER_VERIFY(window != nullptr, "Cannot destroy a null platform window");

    const auto iterator = m_windowsByBackendId.find(backendWindowId);
    PONDER_VERIFY(iterator != m_windowsByBackendId.end(),
                  "Backend window ID {} is not registered", backendWindowId);
    PONDER_VERIFY(iterator->second.id == id,
                  "Backend window ID {} does not match project window ID {}",
                  backendWindowId, id.GetValue());
    PONDER_VERIFY(iterator->second.window == window,
                  "Backend window ID {} does not match its registered window",
                  backendWindowId);
    m_windowsByBackendId.erase(iterator);
}

void PlatformRuntimeState::FinishWindowDestruction(WindowImpl* window)
{
    VerifyOwnerThread("window destruction");
    m_registry.UnregisterChild(window);
}

std::optional<WindowId> PlatformRuntimeState::FindWindowId(
    std::uint32_t backendWindowId) const
{
    VerifyOwnerThread("window lookup");
    const auto iterator = m_windowsByBackendId.find(backendWindowId);
    return iterator != m_windowsByBackendId.end()
             ? std::optional<WindowId>{iterator->second.id}
             : std::nullopt;
}

PlatformTimestamp PlatformRuntimeState::Now() const
{
    VerifyOwnerThread("timestamp query");
    const std::uint64_t ticks = m_backend.getTicksNanoseconds(m_backend.context);
    constexpr auto kMaximumTicks =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    PONDER_VERIFY(ticks <= kMaximumTicks,
                  "SDL monotonic timestamp exceeds PlatformTimestamp range");
    return PlatformTimestamp{
        std::chrono::nanoseconds{static_cast<std::int64_t>(ticks)}};
}
} // namespace detail

core::Result<PlatformRuntime> PlatformRuntime::Create(const PlatformRuntimeDesc& desc)
{
    RuntimeSlotState expectedSlot = RuntimeSlotState::Available;
    if (!runtimeSlot.compare_exchange_strong(expectedSlot, RuntimeSlotState::Creating,
                                             std::memory_order_acq_rel))
    {
        return core::Result<PlatformRuntime>::FromError(core::Error{
            kRuntimeAlreadyActiveCode, "A platform runtime is already active."});
    }

    auto releaseReservation = core::MakeScopeExit(
        []() noexcept
        {
            ReleaseRuntimeReservation();
        });

    core::VoidResult validation = Validate(desc);
    if (!validation.HasValue())
    {
        return core::Result<PlatformRuntime>::FromError(
            std::move(validation).GetError());
    }

    PONDER_VERIFY(std::this_thread::get_id() == processEntryThread,
                  "PlatformRuntime must be created on the process entry thread");

    const detail::PlatformRuntimeBackend backend = detail::GetPlatformRuntimeBackend();
    VerifyBackend(backend);
    const detail::PlatformWindowBackend windowBackend =
        detail::GetPlatformWindowBackend();
    VerifyWindowBackend(windowBackend);
    const detail::PlatformDisplayBackend displayBackend =
        detail::GetPlatformDisplayBackend();
    VerifyDisplayBackend(displayBackend);
    PONDER_VERIFY(backend.isMainThread(backend.context),
                  "PlatformRuntime must be created on SDL's main thread");
    if (backend.hasInitializedSubsystems(backend.context))
    {
        return core::Result<PlatformRuntime>::FromError(core::Error{
            kBackendFailureCode,
            "Cannot create PlatformRuntime while SDL subsystems are already initialized."});
    }

    HintSnapshot focusClickThrough =
        CaptureHint(backend, detail::kMouseFocusClickThroughHint);
    HintSnapshot autoCapture = CaptureHint(backend, detail::kMouseAutoCaptureHint);
    auto restoreHints = core::MakeScopeExit(
        [&backend, &focusClickThrough, &autoCapture]() noexcept
        {
            RestoreRuntimeHints(backend, focusClickThrough, autoCapture);
        });

    if (!backend.setHintOverride(backend.context, detail::kMouseFocusClickThroughHint, "1"))
    {
        return core::Result<PlatformRuntime>::FromError(detail::CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetHintWithPriority",
            detail::kMouseFocusClickThroughHint));
    }

    if (!backend.setHintOverride(backend.context, detail::kMouseAutoCaptureHint, "0"))
    {
        return core::Result<PlatformRuntime>::FromError(detail::CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetHintWithPriority", detail::kMouseAutoCaptureHint));
    }

    MetadataSnapshots metadataSnapshots = CaptureApplicationMetadata(backend);
    auto restoreMetadata = core::MakeScopeExit(
        [&backend, &metadataSnapshots]() noexcept
        {
            RestoreApplicationMetadata(backend, metadataSnapshots);
        });

    struct MetadataValue final
    {
        detail::ApplicationMetadataProperty property;
        const char* value;
        std::string_view context;
    };

    const MetadataValue metadata[]{
        {detail::ApplicationMetadataProperty::Name, desc.applicationName.c_str(), "name"},
        {detail::ApplicationMetadataProperty::Version,
         desc.applicationVersion.has_value() ? desc.applicationVersion->c_str() : nullptr,
         "version"},
        {detail::ApplicationMetadataProperty::Identifier,
         desc.applicationIdentifier.has_value() ? desc.applicationIdentifier->c_str() : nullptr,
         "identifier"}};

    for (const MetadataValue& value : metadata)
    {
        if (!backend.setAppMetadataProperty(backend.context, value.property, value.value))
        {
            return core::Result<PlatformRuntime>::FromError(detail::CaptureSdlFailure(
                kBackendFailureCode, "SDL_SetAppMetadataProperty", value.context));
        }
    }

    auto quitOnFailure = core::MakeScopeExit(
        [&backend]() noexcept
        {
            backend.quit(backend.context);
        });

    if (!backend.initializeVideo(backend.context))
    {
        return core::Result<PlatformRuntime>::FromError(detail::CaptureSdlFailure(
            kBackendFailureCode, "SDL_Init", "SDL_INIT_VIDEO"));
    }

    auto state = std::make_unique<detail::PlatformRuntimeState>(
        backend, windowBackend, displayBackend, std::move(focusClickThrough),
        std::move(autoCapture), std::move(metadataSnapshots));

    MarkRuntimeActive();
    quitOnFailure.Dismiss();
    restoreMetadata.Dismiss();
    restoreHints.Dismiss();
    releaseReservation.Dismiss();
    return PlatformRuntime{std::move(state)};
}

PlatformRuntime::PlatformRuntime(
    std::unique_ptr<detail::PlatformRuntimeState> state) noexcept
    : m_state(std::move(state))
{
}

PlatformRuntime::~PlatformRuntime() noexcept = default;
PlatformRuntime::PlatformRuntime(PlatformRuntime&&) noexcept = default;
PlatformRuntime& PlatformRuntime::operator=(PlatformRuntime&&) noexcept = default;

PlatformTimestamp PlatformRuntime::Now() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->Now();
}
} // namespace pond::platform
