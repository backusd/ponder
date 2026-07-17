#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/Window.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/draw2d/Draw2DLayer.hpp>
#include <ponder/render/draw2d/Draw2DPacket.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "RenderBootstrapTestAccess.hpp"
#include "RenderLiveTestAccess.hpp"
#include "VulkanBootstrap.hpp"
#include "VulkanDiagnostics.hpp"

namespace pond::render
{
namespace
{
enum class ChildKind : std::uint8_t
{
    PreparedSurface,
    Device,
    Target
};

enum class PresentationRecreationStatus : std::uint8_t
{
    NotNeeded,
    Completed,
    Pending
};

[[nodiscard]] core::Error MakeRenderError(RenderErrorCode code, std::string message)
{
    return core::Error{ToErrorCode(code), std::move(message)};
}

[[nodiscard]] core::VoidResult MakeRenderFailure(RenderErrorCode code, std::string message)
{
    return core::VoidResult::FromError(MakeRenderError(code, std::move(message)));
}

[[nodiscard]] core::VoidResult Success() noexcept
{
    return core::VoidResult::Success();
}

[[nodiscard]] bool IsRenderError(const core::Error& error, RenderErrorCode code) noexcept
{
    return error.GetCode() == ToErrorCode(code);
}

[[nodiscard]] RenderErrorCode GetRenderErrorCode(const core::Error& error) noexcept
{
    const core::ErrorCodeValue value = error.GetCode().GetValue();
    switch (static_cast<RenderErrorCode>(value))
    {
    case RenderErrorCode::InvalidArgument:
    case RenderErrorCode::InvalidState:
    case RenderErrorCode::UnsupportedBackend:
    case RenderErrorCode::UnsupportedCapability:
    case RenderErrorCode::UnsupportedSurface:
    case RenderErrorCode::LoaderUnavailable:
    case RenderErrorCode::NoCompatibleAdapter:
    case RenderErrorCode::OutOfMemory:
    case RenderErrorCode::SurfaceLost:
    case RenderErrorCode::DeviceLost:
    case RenderErrorCode::BackendFailure:
        return static_cast<RenderErrorCode>(value);
    }

    return RenderErrorCode::BackendFailure;
}

[[nodiscard]] std::string FormatWindowId(platform::WindowId windowId)
{
    return windowId.IsValid() ? std::to_string(windowId.GetValue()) : std::string{"invalid"};
}

[[nodiscard]] std::string MakeTargetDiagnosticLabel(platform::WindowId windowId)
{
    return std::string{"target/window:"} + FormatWindowId(windowId);
}

[[nodiscard]] std::string MakeTargetDiagnosticMessage(std::string_view targetLabel,
                                                      std::string_view operation,
                                                      std::string_view message)
{
    std::string result;
    result.reserve(targetLabel.size() + operation.size() + message.size() + 32U);
    result += "target=";
    result += targetLabel;
    result += " operation=";
    result += operation;
    result += ": ";
    result += message;
    return result;
}

[[nodiscard]] BackendDiagnostic MakeBackendDiagnostic(
    const core::Error& error, std::string_view operation,
    OptionalBackendDiagnostic nativeDiagnostic = {}, platform::WindowId windowId = {},
    std::string_view targetLabel = {})
{
    BackendDiagnostic diagnostic =
        nativeDiagnostic.has_value() ? std::move(*nativeDiagnostic) : BackendDiagnostic{};
    diagnostic.backend = RenderBackendKind::Vulkan;
    diagnostic.renderCode = GetRenderErrorCode(error);
    if (diagnostic.operation.empty())
    {
        diagnostic.operation = operation;
    }
    if (diagnostic.validationContext.empty())
    {
        diagnostic.validationContext = error.GetMessage();
    }
    if (windowId.IsValid())
    {
        diagnostic.windowId = windowId;
    }
    if (!targetLabel.empty())
    {
        diagnostic.targetLabel = targetLabel;
    }
    return diagnostic;
}
inline constexpr std::string_view kRenderLogCategory{"render"};
thread_local bool g_failNextTargetStateAllocationForTesting{};
thread_local bool g_failNextFrameStateAllocationForTesting{};
thread_local bool g_failNextRetirementAllocationForTesting{};

[[nodiscard]] bool ConsumeTargetStateAllocationFailureForTesting() noexcept
{
    return std::exchange(g_failNextTargetStateAllocationForTesting, false);
}

[[nodiscard]] bool ConsumeFrameStateAllocationFailureForTesting() noexcept
{
    return std::exchange(g_failNextFrameStateAllocationForTesting, false);
}

[[nodiscard]] bool ConsumeRetirementAllocationFailureForTesting() noexcept
{
    return std::exchange(g_failNextRetirementAllocationForTesting, false);
}

struct ChildRegistry final
{
    explicit ChildRegistry(std::thread::id ownerThreadId) noexcept : ownerThread{ownerThreadId} {}

    [[nodiscard]] bool IsOwnerThread() const noexcept
    {
        return std::this_thread::get_id() == ownerThread;
    }

    [[nodiscard]] bool IsShutdown() const noexcept
    {
        std::scoped_lock lock{mutex};
        return shutdown;
    }

    [[nodiscard]] bool TryRegister(ChildKind kind)
    {
        std::scoped_lock lock{mutex};
        if (shutdown)
        {
            return false;
        }

        switch (kind)
        {
        case ChildKind::PreparedSurface:
            ++preparedSurfaceCount;
            break;
        case ChildKind::Device:
            ++deviceCount;
            break;
        case ChildKind::Target:
            ++targetCount;
            break;
        }

        return true;
    }

    void Release(ChildKind kind) noexcept
    {
        std::scoped_lock lock{mutex};
        switch (kind)
        {
        case ChildKind::PreparedSurface:
            if (preparedSurfaceCount > 0U)
            {
                --preparedSurfaceCount;
            }
            break;
        case ChildKind::Device:
            if (deviceCount > 0U)
            {
                --deviceCount;
            }
            break;
        case ChildKind::Target:
            if (targetCount > 0U)
            {
                --targetCount;
            }
            break;
        }
    }

    [[nodiscard]] bool TryShutdown() noexcept
    {
        std::scoped_lock lock{mutex};
        if (preparedSurfaceCount > 0U || deviceCount > 0U || targetCount > 0U)
        {
            return false;
        }

        shutdown = true;
        return true;
    }

    [[nodiscard]] std::uint32_t GetPreparedSurfaceCount() const noexcept
    {
        std::scoped_lock lock{mutex};
        return preparedSurfaceCount;
    }

    [[nodiscard]] std::uint32_t GetDeviceCount() const noexcept
    {
        std::scoped_lock lock{mutex};
        return deviceCount;
    }

    [[nodiscard]] std::uint32_t GetTargetCount() const noexcept
    {
        std::scoped_lock lock{mutex};
        return targetCount;
    }

    [[nodiscard]] bool HasActiveChildren() const noexcept
    {
        std::scoped_lock lock{mutex};
        return preparedSurfaceCount > 0U || deviceCount > 0U || targetCount > 0U;
    }

    std::thread::id ownerThread;
    mutable std::mutex mutex;
    bool shutdown{};
    std::uint32_t preparedSurfaceCount{};
    std::uint32_t deviceCount{};
    std::uint32_t targetCount{};
};

struct ChildToken final
{
    ChildToken() noexcept = default;
    ChildToken(std::shared_ptr<ChildRegistry> inputRegistry, ChildKind inputKind)
        : registry{std::move(inputRegistry)}, kind{inputKind}
    {
        active = registry != nullptr && registry->TryRegister(kind);
    }

    ChildToken(const ChildToken&) = delete;
    ChildToken& operator=(const ChildToken&) = delete;

    ChildToken(ChildToken&& other) noexcept
        : registry{std::move(other.registry)}, kind{other.kind}, active{other.active}
    {
        other.active = false;
    }

    ChildToken& operator=(ChildToken&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            registry = std::move(other.registry);
            kind = other.kind;
            active = other.active;
            other.active = false;
        }

        return *this;
    }

    ~ChildToken()
    {
        Release();
    }

    void Release() noexcept
    {
        if (active && registry != nullptr)
        {
            registry->Release(kind);
        }
        active = false;
    }

    [[nodiscard]] bool IsActive() const noexcept
    {
        return active && registry != nullptr && !registry->IsShutdown();
    }

    std::shared_ptr<ChildRegistry> registry;
    ChildKind kind{ChildKind::PreparedSurface};
    bool active{};
};

[[nodiscard]] core::VoidResult ValidateLiveRegistry(const ChildToken& token,
                                                    std::string_view roleName)
{
    if (!token.active || token.registry == nullptr)
    {
        LOG_WARNING_CATEGORY(kRenderLogCategory,
                             "lifecycle_misuse operation=ValidateLiveRegistry role={} "
                             "reason=moved_or_empty",
                             roleName);
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 std::string{roleName} + " is moved-from or empty.");
    }

    if (token.registry->IsShutdown())
    {
        LOG_WARNING_CATEGORY(kRenderLogCategory,
                             "lifecycle_misuse operation=ValidateLiveRegistry role={} "
                             "reason=bootstrap_shutdown",
                             roleName);
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 std::string{roleName} + " belongs to a shut-down bootstrap.");
    }

    return Success();
}

[[nodiscard]] core::VoidResult ValidateCurrentThread(std::thread::id expectedThread,
                                                     std::string_view operation)
{
    if (std::this_thread::get_id() != expectedThread)
    {
        LOG_WARNING_CATEGORY(kRenderLogCategory,
                             "lifecycle_misuse operation={} reason=wrong_thread", operation);
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 std::string{operation} + " was called from the wrong thread.");
    }

    return Success();
}

[[nodiscard]] core::VoidResult ValidateTargetSnapshotSuccessor(RenderTargetSnapshot current,
                                                               RenderTargetSnapshot next,
                                                               bool allowIdentical)
{
    if (!pond::render::IsValid(current) || !pond::render::IsValid(next))
    {
        return MakeRenderFailure(RenderErrorCode::InvalidArgument,
                                 "Target snapshot succession contains invalid state.");
    }

    if (current.GetWindowId() != next.GetWindowId())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidArgument,
                                 "Target snapshot succession references the wrong window.");
    }

    if (next.GetRevision() < current.GetRevision())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "Target snapshot succession is older than the current state.");
    }

    if (next.GetPresentationEnvironmentRevision().GetValue() <
        current.GetPresentationEnvironmentRevision().GetValue())
    {
        return MakeRenderFailure(
            RenderErrorCode::InvalidState,
            "Target snapshot succession regresses the presentation-environment revision.");
    }

    if (next.GetRevision() == current.GetRevision())
    {
        if (allowIdentical && next == current)
        {
            return Success();
        }

        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 next == current
                                     ? "Target snapshot succession does not advance the revision."
                                     : "Target snapshots conflict at the same revision.");
    }

    return Success();
}

[[nodiscard]] constexpr TargetStatus DetermineTargetStatus(RenderTargetSnapshot snapshot) noexcept
{
    if (snapshot.GetWindowState() == platform::WindowState::Minimized)
    {
        return TargetStatus::Minimized;
    }

    if (!snapshot.IsVisible())
    {
        return TargetStatus::Hidden;
    }

    const platform::PixelSize pixelSize = snapshot.GetPixelSize();
    if (pixelSize.width == 0U || pixelSize.height == 0U)
    {
        return TargetStatus::Suspended;
    }

    return TargetStatus::Active;
}

[[nodiscard]] constexpr bool IsSuspendedStatus(TargetStatus status) noexcept
{
    return status == TargetStatus::Hidden || status == TargetStatus::Minimized ||
           status == TargetStatus::Suspended;
}

} // namespace

struct RenderBootstrap::State final
{
    State(std::shared_ptr<ChildRegistry> inputRegistry,
          RenderValidationMode inputValidationMode) noexcept
        : registry{std::move(inputRegistry)}, validationMode{inputValidationMode},
          diagnostics{.backend = RenderBackendKind::Vulkan,
                      .requestedValidationMode = inputValidationMode}
    {
    }

    ~State()
    {
        VerifyDestruction();
        PONDER_VERIFY(registry->TryShutdown(),
                      "Cannot destroy RenderBootstrap while renderer children are still active");
        std::scoped_lock lock{instanceMutex};
        vulkanInstance.Reset();
    }

    void VerifyDestruction() const noexcept
    {
        PONDER_VERIFY(registry->IsOwnerThread(),
                      "Cannot destroy RenderBootstrap from a thread other than its owner thread");
        PONDER_VERIFY(!registry->HasActiveChildren(),
                      "Cannot destroy RenderBootstrap while renderer children are still active");
    }

    void RecordInstanceInfo(const detail::VulkanInstanceInfo& info) noexcept
    {
        diagnostics.enabledValidationMode = info.enabledValidationMode;
        diagnostics.negotiatedApiVersion = info.apiVersion;
        diagnostics.validationEnabled = info.validationEnabled;
        diagnostics.debugInstrumentation =
            RenderDebugInstrumentation{.objectNames = info.debugUtilityHooks.objectNames,
                                       .commandLabels = info.debugUtilityHooks.commandLabels,
                                       .timingMarkers = info.debugUtilityHooks.timingMarkers,
                                       .captureRegions = info.debugUtilityHooks.captureRegions};
    }

    void RecordFailure(const core::Error& error, std::string_view operation,
                       OptionalBackendDiagnostic nativeDiagnostic = {},
                       platform::WindowId windowId = {}, std::string_view targetLabel = {})
    {
        if (!nativeDiagnostic.has_value())
        {
            nativeDiagnostic = detail::VulkanDiagnosticScope::TakeCurrentLastFailure();
        }
        diagnostics.lastFailure = MakeBackendDiagnostic(
            error, operation, std::move(nativeDiagnostic), windowId, targetLabel);
    }

    std::shared_ptr<ChildRegistry> registry;
    std::mutex instanceMutex;
    RenderValidationMode validationMode{RenderValidationMode::Default};
    RenderBootstrapDiagnostics diagnostics{};
    detail::VulkanGlobalDispatch vulkanDispatch{detail::CreateVolkGlobalDispatch()};
    detail::VulkanInstanceBootstrap vulkanInstance;
};

struct RetiredPresentationResources final
{
    std::uint64_t generation{};
    detail::VulkanSwapchainOwner vulkanSwapchain;
    detail::VulkanFrameResourcesOwner vulkanFrameResources;
    detail::VulkanPresentationTrackerOwner vulkanPresentationTracker;
    bool canBeUsedAsOldSwapchain{};
};

struct OrphanedPresentationResources final
{
    detail::VulkanSurfaceOwner vulkanSurface;
    std::vector<RetiredPresentationResources> retiredResources{};
    RetiredPresentationResources currentResources{};
};

[[nodiscard]] bool HasPresentationResources(const RetiredPresentationResources& resources) noexcept
{
    return resources.vulkanSwapchain.IsValid() || resources.vulkanFrameResources.IsValid() ||
           resources.vulkanPresentationTracker.IsValid();
}

void RecordCompletedFrameSubmissions(RetiredPresentationResources& resources) noexcept
{
    for (const std::uint32_t slotIndex :
         resources.vulkanFrameResources.ConsumeCompletedSubmissionSlots())
    {
        resources.vulkanPresentationTracker.RecordFrameFenceCompletion(slotIndex);
    }
}

struct RenderDevice::State final
{
    State(ChildToken inputToken, detail::VulkanDeviceOwner inputVulkanDevice) noexcept
        : token{std::move(inputToken)}, renderThread{std::this_thread::get_id()},
          vulkanDispatch{inputVulkanDevice.GetDispatch()},
          vulkanDevice{std::move(inputVulkanDevice)}
    {
    }

    ~State()
    {
        VerifyDestruction();
        [[maybe_unused]] core::VoidResult shutdown = WaitForOrphanedPresentationResources();
    }

    void VerifyDestruction() const noexcept
    {
        PONDER_VERIFY(std::this_thread::get_id() == renderThread,
                      "RenderDevice destruction must occur on its render thread");
        PONDER_VERIFY(GetTargetCount() == 0U,
                      "RenderDevice destruction requires every RenderTarget to be destroyed "
                      "first");
        PONDER_VERIFY(GetDraw2DLayerCount() == 0U,
                      "RenderDevice destruction requires every Draw2DLayer to be destroyed "
                      "first");
    }

    void AdoptPresentationResourcesForFinalShutdown(
        detail::VulkanSurfaceOwner&& surface, detail::VulkanSwapchainOwner&& swapchain,
        detail::VulkanFrameResourcesOwner&& frameResources,
        detail::VulkanPresentationTrackerOwner&& presentationTracker,
        std::vector<RetiredPresentationResources>&& resources) noexcept
    {
        const bool hasCurrentResources =
            swapchain.IsValid() || frameResources.IsValid() || presentationTracker.IsValid();
        if (!hasCurrentResources && resources.empty())
        {
            return;
        }

        bool adoptionSlotReserved = false;
        {
            std::scoped_lock lock{retirementMutex};
            try
            {
                if (ConsumeRetirementAllocationFailureForTesting())
                {
                    throw std::bad_alloc{};
                }

                orphanedPresentationResources.reserve(orphanedPresentationResources.size() + 1U);
                adoptionSlotReserved = true;
            }
            catch (...)
            {
                // Target destruction cannot report an allocation failure. The exceptional
                // terminal fallback below makes the current resources safe to destroy instead.
            }

            if (adoptionSlotReserved)
            {
                PONDER_VERIFY(orphanedPresentationResources.size() <
                                  orphanedPresentationResources.capacity(),
                              "Final presentation-resource adoption requires a reserved slot");
                orphanedPresentationResources.push_back(OrphanedPresentationResources{
                    .vulkanSurface = std::move(surface),
                    .retiredResources = std::move(resources),
                    .currentResources = RetiredPresentationResources{
                        .vulkanSwapchain = std::move(swapchain),
                        .vulkanFrameResources = std::move(frameResources),
                        .vulkanPresentationTracker = std::move(presentationTracker)}});
                return;
            }
        }

        // A host-allocation failure must not terminate target destruction or release resources
        // still owned by presentation. This is an exceptional terminal-only compatibility
        // fallback; ordinary frame, resize, and suspension paths never take it.
        ++practicalWaitFallbackCount;
        try
        {
            if (vulkanDevice.IsValid())
            {
                core::VoidResult wait = vulkanDevice.WaitIdle();
                if (!wait)
                {
                    RecordDeviceFailure(wait.GetError());
                }
            }
        }
        catch (...)
        {
            // Destructors cannot propagate diagnostics-allocation failures. Vulkan resource
            // owners below still provide deterministic native cleanup, including after loss.
        }

        presentationTracker.Reset();
        frameResources.Reset();
        swapchain.Reset();
        resources.clear();
        surface.Reset();
    }

    void ClearOrphanedPresentationResourcesAfterIdle() noexcept
    {
        std::scoped_lock lock{retirementMutex};
        orphanedPresentationResources.clear();
    }

    [[nodiscard]] core::VoidResult WaitForOrphanedPresentationResources()
    {
        std::scoped_lock lock{retirementMutex};
        if (orphanedPresentationResources.empty())
        {
            return Success();
        }

        constexpr std::uint64_t kInfiniteTimeout = std::numeric_limits<std::uint64_t>::max();
        bool requiresPracticalFallback = false;
        const auto pollPresentation = [this, &requiresPracticalFallback](
                                          RetiredPresentationResources& retired) -> core::VoidResult
        {
            if (!HasPresentationResources(retired))
            {
                return Success();
            }
            if (!retired.vulkanPresentationTracker.IsValid() || !retired.vulkanSwapchain.IsValid())
            {
                requiresPracticalFallback = true;
                return Success();
            }

            core::Result<detail::VulkanPresentationCompletionResult> completion =
                retired.vulkanPresentationTracker.PollCompletion(
                    vulkanDispatch, retired.vulkanSwapchain.GetHandle(), kInfiniteTimeout);
            if (!completion)
            {
                RecordDeviceFailure(completion.GetError());
                return core::VoidResult::FromError(std::move(completion).GetError());
            }

            requiresPracticalFallback = requiresPracticalFallback ||
                                        completion.GetValue().status !=
                                            detail::VulkanPresentationCompletionStatus::Complete;
            return Success();
        };
        for (OrphanedPresentationResources& orphaned : orphanedPresentationResources)
        {
            if (core::VoidResult poll = pollPresentation(orphaned.currentResources); !poll)
            {
                orphanedPresentationResources.clear();
                return poll;
            }
            for (RetiredPresentationResources& retired : orphaned.retiredResources)
            {
                if (core::VoidResult poll = pollPresentation(retired); !poll)
                {
                    orphanedPresentationResources.clear();
                    return poll;
                }
            }
        }

        core::VoidResult wait = Success();
        if (requiresPracticalFallback)
        {
            // Core Vulkan 1.2 has no formal terminal presentation-completion signal.
            ++practicalWaitFallbackCount;
            if (vulkanDevice.IsValid())
            {
                wait = vulkanDevice.WaitIdle();
                if (!wait)
                {
                    RecordDeviceFailure(wait.GetError());
                }
            }
        }
        if (!wait)
        {
            orphanedPresentationResources.clear();
            return wait;
        }

        const auto waitForGraphics =
            [this](RetiredPresentationResources& retired) -> core::VoidResult
        {
            if (!retired.vulkanFrameResources.IsValid())
            {
                return Success();
            }

            core::Result<bool> graphicsComplete =
                retired.vulkanFrameResources.AreAllFencesSignaled(vulkanDispatch, kInfiniteTimeout);
            if (retired.vulkanPresentationTracker.IsValid())
            {
                RecordCompletedFrameSubmissions(retired);
            }
            if (!graphicsComplete)
            {
                RecordDeviceFailure(graphicsComplete.GetError());
                return core::VoidResult::FromError(std::move(graphicsComplete).GetError());
            }
            if (!graphicsComplete.GetValue())
            {
                return MakeRenderFailure(
                    RenderErrorCode::BackendFailure,
                    "An infinite Vulkan frame-resource wait returned incomplete.");
            }

            return Success();
        };
        for (OrphanedPresentationResources& orphaned : orphanedPresentationResources)
        {
            if (core::VoidResult graphics = waitForGraphics(orphaned.currentResources); !graphics)
            {
                orphanedPresentationResources.clear();
                return graphics;
            }
            for (RetiredPresentationResources& retired : orphaned.retiredResources)
            {
                if (core::VoidResult graphics = waitForGraphics(retired); !graphics)
                {
                    orphanedPresentationResources.clear();
                    return graphics;
                }
            }
        }

        orphanedPresentationResources.clear();
        return Success();
    }
    [[nodiscard]] bool TryRegisterTarget()
    {
        std::scoped_lock lock{targetMutex};
        if (!token.IsActive())
        {
            return false;
        }

        ++targetCount;
        return true;
    }

    void ReleaseTarget() noexcept
    {
        std::scoped_lock lock{targetMutex};
        if (targetCount > 0U)
        {
            --targetCount;
        }
    }

    [[nodiscard]] std::uint32_t GetTargetCount() const noexcept
    {
        std::scoped_lock lock{targetMutex};
        return targetCount;
    }

    [[nodiscard]] bool TryRegisterDraw2DLayer()
    {
        std::scoped_lock lock{draw2DLayerMutex};
        if (!token.IsActive())
        {
            return false;
        }

        ++draw2DLayerCount;
        return true;
    }

    void ReleaseDraw2DLayer() noexcept
    {
        std::scoped_lock lock{draw2DLayerMutex};
        if (draw2DLayerCount > 0U)
        {
            --draw2DLayerCount;
        }
    }

    [[nodiscard]] std::uint32_t GetDraw2DLayerCount() const noexcept
    {
        std::scoped_lock lock{draw2DLayerMutex};
        return draw2DLayerCount;
    }

    [[nodiscard]] bool IsDeviceLost() const noexcept
    {
        return deviceLostError.has_value();
    }

    [[nodiscard]] core::VoidResult CheckDeviceUsable() const
    {
        if (deviceLostError.has_value())
        {
            return core::VoidResult::FromError(*deviceLostError);
        }

        return Success();
    }

    void RecordDeviceFailure(const core::Error& error, std::string_view operation = "device",
                             OptionalBackendDiagnostic nativeDiagnostic = {},
                             platform::WindowId windowId = {}, std::string_view targetLabel = {})
    {
        if (!nativeDiagnostic.has_value())
        {
            nativeDiagnostic = detail::VulkanDiagnosticScope::CopyCurrentLastFailure();
        }
        lastFailure = MakeBackendDiagnostic(error, operation, std::move(nativeDiagnostic), windowId,
                                            targetLabel);
        if (IsRenderError(error, RenderErrorCode::DeviceLost) && !deviceLostError.has_value())
        {
            deviceLostError = error;
            LOG_ERROR_CATEGORY(kRenderLogCategory, "device_lost message={}", error.GetMessage());
        }
    }

    ChildToken token;
    std::thread::id renderThread;
    detail::VulkanGlobalDispatch vulkanDispatch{};
    detail::VulkanDeviceOwner vulkanDevice;
    detail::VulkanDraw2DPipelineCache vulkanDraw2DPipelineCache;
    mutable std::mutex targetMutex;
    mutable std::mutex draw2DLayerMutex;
    mutable std::mutex retirementMutex;
    std::vector<OrphanedPresentationResources> orphanedPresentationResources{};
    std::uint32_t targetCount{};
    std::uint32_t draw2DLayerCount{};
    std::uint64_t targetCreateAttempts{};
    std::uint64_t targetCreateSuccesses{};
    std::uint64_t targetCreateFailures{};
    std::uint64_t waitIdleCalls{};
    std::uint64_t practicalWaitFallbackCount{};
    std::optional<core::Error> deviceLostError{};
    OptionalBackendDiagnostic lastFailure{};
};

struct PreparedSurface::State final
{
    State(ChildToken inputToken, RenderTargetSnapshot inputSnapshot,
          SurfacePreparationReason inputReason) noexcept
        : token{std::move(inputToken)}, targetSnapshot{inputSnapshot}, reason{inputReason}
    {
    }

    State(ChildToken inputToken, RenderTargetSnapshot inputSnapshot,
          SurfacePreparationReason inputReason,
          detail::VulkanSurfaceOwner inputVulkanSurface) noexcept
        : token{std::move(inputToken)}, targetSnapshot{inputSnapshot}, reason{inputReason},
          vulkanSurface{std::move(inputVulkanSurface)}
    {
    }

    ~State()
    {
        VerifyDestruction();
    }

    void VerifyDestruction() const noexcept
    {
        const bool isExpectedThread =
            renderThread.has_value() ? std::this_thread::get_id() == *renderThread
                                     : token.registry != nullptr && token.registry->IsOwnerThread();
        PONDER_VERIFY(
            isExpectedThread,
            "PreparedSurface destruction must occur on its owner thread before transfer or its "
            "bound render thread after transfer");
    }

    ChildToken token;
    RenderTargetSnapshot targetSnapshot{};
    SurfacePreparationReason reason{SurfacePreparationReason::Initial};
    std::optional<std::thread::id> renderThread;
    detail::VulkanSurfaceOwner vulkanSurface;
};

struct RenderTarget::State final
{
    State(ChildToken inputToken, std::shared_ptr<RenderDevice::State> inputDeviceState,
          RenderTargetDesc inputDesc, std::string inputTargetLabel,
          detail::VulkanDeviceQueuePlan inputQueuePlan,
          detail::VulkanSurfaceOwner inputVulkanSurface,
          detail::VulkanSwapchainOwner inputVulkanSwapchain,
          detail::VulkanFrameResourcesOwner inputVulkanFrameResources,
          detail::VulkanPresentationTrackerOwner inputVulkanPresentationTracker) noexcept
        : token{std::move(inputToken)}, deviceState{std::move(inputDeviceState)},
          vulkanSurface{std::move(inputVulkanSurface)},
          vulkanSwapchain{std::move(inputVulkanSwapchain)},
          vulkanFrameResources{std::move(inputVulkanFrameResources)},
          vulkanPresentationTracker{std::move(inputVulkanPresentationTracker)},
          targetDesc{inputDesc}, targetSnapshot{inputDesc.targetSnapshot},
          status{DetermineTargetStatus(inputDesc.targetSnapshot)},
          targetLabel{std::move(inputTargetLabel)}, queuePlan{std::move(inputQueuePlan)},
          swapchainGeneration{vulkanSwapchain.IsValid() ? 1U : 0U},
          renderThread{std::this_thread::get_id()}
    {
    }

    ~State()
    {
        VerifyDestruction();
        if (deviceState != nullptr)
        {
            if (vulkanSwapchain.IsValid() || vulkanFrameResources.IsValid() ||
                vulkanPresentationTracker.IsValid() || !retiredPresentationResources.empty())
            {
                deviceState->AdoptPresentationResourcesForFinalShutdown(
                    std::move(vulkanSurface), std::move(vulkanSwapchain),
                    std::move(vulkanFrameResources), std::move(vulkanPresentationTracker),
                    std::move(retiredPresentationResources));
            }

            vulkanPresentationTracker.Reset();
            vulkanFrameResources.Reset();
            vulkanSwapchain.Reset();
            deviceState->ReleaseTarget();
        }
    }

    void VerifyDestruction() const noexcept
    {
        PONDER_VERIFY(std::this_thread::get_id() == renderThread,
                      "RenderTarget destruction must occur on its render thread");
        PONDER_VERIFY(!activeFrame,
                      "RenderTarget destruction requires its live RenderFrame token to be "
                      "finished or abandoned first");
    }

    void MarkWindowRecreationPending(TargetRecreationReason reason, std::uint64_t previousRevision,
                                     std::uint64_t currentRevision) noexcept;
    void MarkBackendRecreationPending(TargetRecreationReason reason) noexcept;
    void ClearRecreationPending() noexcept;
    [[nodiscard]] TargetStatus GetEffectiveStatus() const noexcept;
    [[nodiscard]] core::VoidResult PreparePresentationResourcesForRetirement();
    [[nodiscard]] bool HasUsablePresentationResources() const noexcept;
    [[nodiscard]] VkSwapchainKHR FindOldSwapchainForReplacement() const noexcept;
    [[nodiscard]] core::VoidResult ReserveRetirementSlotForOldSwapchain(
        VkSwapchainKHR oldSwapchain);
    [[nodiscard]] core::VoidResult ReserveRetirementSlotForCurrentPresentationResources();
    [[nodiscard]] core::VoidResult RetirePresentationResources(
        bool canBeUsedAsOldSwapchain = false);
    void RecordNativeSwapchainRetirement(VkSwapchainKHR oldSwapchain) noexcept;
    void RecordCorePresentationCompletion() noexcept;
    [[nodiscard]] core::Error RecordFailure(core::Error error, std::string_view operation,
                                            OptionalBackendDiagnostic nativeDiagnostic = {});
    [[nodiscard]] core::VoidResult DrainRetiredPresentationResources();
    [[nodiscard]] core::VoidResult UsePracticalPresentationWaitFallback();
    [[nodiscard]] core::VoidResult MarkSurfaceLost();
    [[nodiscard]] core::Result<PresentationRecreationStatus> RecreatePresentationResources();

    ChildToken token;
    std::shared_ptr<RenderDevice::State> deviceState;
    detail::VulkanSurfaceOwner vulkanSurface;
    detail::VulkanSwapchainOwner vulkanSwapchain;
    detail::VulkanFrameResourcesOwner vulkanFrameResources;
    detail::VulkanPresentationTrackerOwner vulkanPresentationTracker;
    RenderTargetDesc targetDesc{};
    RenderTargetSnapshot targetSnapshot{};
    TargetStatus status{TargetStatus::Suspended};
    std::string targetLabel{};
    detail::VulkanDeviceQueuePlan queuePlan{};
    std::uint64_t swapchainGeneration{};
    std::uint32_t nextFrameSlot{};
    bool activeFrame{};
    bool recreationPending{};
    bool surfaceLost{};
    TargetRecreationInfo pendingRecreation{};
    std::vector<RetiredPresentationResources> retiredPresentationResources{};
    std::uint64_t retiredResourceSetCount{};
    std::uint64_t practicalWaitFallbackCount{};
    std::uint64_t recreationCount{};
    std::uint64_t suspensionCount{};
    std::uint64_t frameAcquireAttempts{};
    std::uint64_t frameAcquireFailures{};
    std::uint64_t framesPresented{};
    std::uint64_t frameTimeouts{};
    std::uint64_t frameFailures{};
    OptionalBackendDiagnostic lastFailure{};
    bool usedExplicitPresentationCompletion{};
    bool usedCoreAcquireHistory{};
    std::thread::id renderThread;
};

struct RenderFrame::State final
{
    State(std::shared_ptr<RenderTarget::State> inputTargetState, FrameStatus inputStatus,
          bool inputHoldsActiveFrame,
          detail::VulkanFrameRecordingState inputRecording = {}) noexcept
        : targetState{std::move(inputTargetState)}, status{inputStatus},
          targetStatus{targetState == nullptr ? TargetStatus::Suspended
                                              : targetState->GetEffectiveStatus()},
          clearColor{targetState == nullptr ? ClearColor{} : targetState->targetDesc.clearColor},
          recording{std::move(inputRecording)}, renderThread{std::this_thread::get_id()},
          holdsActiveFrame{inputHoldsActiveFrame}
    {
        if (targetState != nullptr)
        {
            const RenderTargetSnapshot snapshot = targetState->targetSnapshot;
            platform::PixelSize pixelSize = snapshot.GetPixelSize();
            if (recording.IsActive() && targetState->vulkanSwapchain.IsValid())
            {
                const VkExtent2D extent = targetState->vulkanSwapchain.GetConfig().extent;
                pixelSize = platform::PixelSize{.width = extent.width, .height = extent.height};
            }
            metrics =
                RenderFrameMetrics{.windowId = snapshot.GetWindowId(),
                                   .logicalSize = snapshot.GetLogicalSize(),
                                   .pixelSize = pixelSize,
                                   .metricsRevision = snapshot.GetPresentationEnvironmentRevision(),
                                   .targetRevision = snapshot.GetRevision()};
        }
    }

    ~State()
    {
        VerifyDestruction();
        if (holdsActiveFrame && !completed && targetState != nullptr && recording.IsActive())
        {
            detail::AbandonVulkanFrame(targetState->vulkanFrameResources,
                                       targetState->vulkanPresentationTracker, recording);
            if (targetState->deviceState != nullptr && !targetState->deviceState->IsDeviceLost())
            {
                targetState->MarkBackendRecreationPending(
                    TargetRecreationReason::PresentationChanged);
            }

            ++targetState->frameFailures;
        }

        ReleaseActiveFrame();
    }

    void VerifyDestruction() const noexcept
    {
        PONDER_VERIFY(std::this_thread::get_id() == renderThread,
                      "RenderFrame destruction must occur on its render thread");
    }

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    void ReleaseActiveFrame() noexcept
    {
        if (holdsActiveFrame && targetState != nullptr)
        {
            targetState->activeFrame = false;
        }

        holdsActiveFrame = false;
    }

    std::shared_ptr<RenderTarget::State> targetState;
    FrameStatus status{FrameStatus::SkippedSuspended};
    TargetStatus targetStatus{TargetStatus::Suspended};
    ClearColor clearColor{};
    detail::VulkanFrameRecordingState recording{};
    RenderFrameMetrics metrics{};
    std::thread::id renderThread;
    bool clearRecorded{};
    bool completed{};
    bool holdsActiveFrame{};
};

core::Result<detail::RenderLifetimeContractOwners> detail::RenderBackendTestAccess::
    CreateLifetimeContractOwners(const RenderTargetDesc& desc)
{
    if (!pond::render::IsValid(desc) ||
        !IsSuspendedStatus(DetermineTargetStatus(desc.targetSnapshot)))
    {
        return core::Result<detail::RenderLifetimeContractOwners>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Render lifetime test access requires a valid suspended target descriptor."));
    }

    auto registry = std::make_shared<ChildRegistry>(std::this_thread::get_id());
    ChildToken deviceToken{registry, ChildKind::Device};
    if (!deviceToken.active)
    {
        return core::Result<detail::RenderLifetimeContractOwners>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the lifetime test device."));
    }

    auto deviceState =
        std::make_shared<RenderDevice::State>(std::move(deviceToken), detail::VulkanDeviceOwner{});
    ChildToken targetToken{registry, ChildKind::Target};
    if (!targetToken.active)
    {
        return core::Result<detail::RenderLifetimeContractOwners>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the lifetime test target token."));
    }

    std::string targetLabel = MakeTargetDiagnosticLabel(desc.targetSnapshot.GetWindowId());
    auto targetState = std::make_shared<RenderTarget::State>(
        std::move(targetToken), deviceState, desc, std::move(targetLabel),
        detail::VulkanDeviceQueuePlan{}, detail::VulkanSurfaceOwner{},
        detail::VulkanSwapchainOwner{}, detail::VulkanFrameResourcesOwner{},
        detail::VulkanPresentationTrackerOwner{});
    if (!deviceState->TryRegisterTarget())
    {
        return core::Result<detail::RenderLifetimeContractOwners>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the lifetime test target."));
    }

    detail::RenderLifetimeContractOwners owners;
    owners.device = RenderDevice{std::move(deviceState)};
    owners.target = RenderTarget{std::move(targetState)};
    return core::Result<detail::RenderLifetimeContractOwners>::FromValue(std::move(owners));
}

core::Result<detail::RenderDeviceSurfaceTestOwners> detail::RenderBackendTestAccess::
    CreateDeviceAndPreparedSurface(detail::VulkanGlobalDispatch, detail::VulkanDeviceOwner&& device,
                                   detail::VulkanSurfaceOwner&& surface,
                                   const SurfacePreparationDesc& desc)
{
    if (!device.IsValid() || !surface.IsValid() || !pond::render::IsValid(desc))
    {
        return core::Result<detail::RenderDeviceSurfaceTestOwners>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Render test access requires a live device, surface, and valid surface descriptor."));
    }

    auto registry = std::make_shared<ChildRegistry>(std::this_thread::get_id());
    ChildToken deviceToken{registry, ChildKind::Device};
    ChildToken surfaceToken{registry, ChildKind::PreparedSurface};
    if (!deviceToken.active || !surfaceToken.active)
    {
        return core::Result<detail::RenderDeviceSurfaceTestOwners>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "Could not register the render device and prepared surface test owners."));
    }

    auto deviceState =
        std::make_shared<RenderDevice::State>(std::move(deviceToken), std::move(device));
    auto surfaceState = std::make_unique<PreparedSurface::State>(
        std::move(surfaceToken), desc.targetSnapshot, desc.reason, std::move(surface));
    surfaceState->renderThread = std::this_thread::get_id();

    detail::RenderDeviceSurfaceTestOwners owners;
    owners.device = RenderDevice{std::move(deviceState)};
    owners.surface = PreparedSurface{std::move(surfaceState)};
    return core::Result<detail::RenderDeviceSurfaceTestOwners>::FromValue(std::move(owners));
}

core::Result<detail::RenderPublicLifecycleTestOwners> detail::RenderBackendTestAccess::
    CreatePublicLifecycleOwners(detail::VulkanDeviceOwner&& device,
                                detail::VulkanSurfaceOwner&& surface,
                                detail::VulkanSwapchainOwner&& swapchain,
                                detail::VulkanFrameResourcesOwner&& frameResources,
                                detail::VulkanPresentationTrackerOwner&& presentationTracker,
                                const RenderTargetDesc& desc)
{
    using OwnersResult = core::Result<detail::RenderPublicLifecycleTestOwners>;
    if (!pond::render::IsValid(desc) ||
        DetermineTargetStatus(desc.targetSnapshot) != TargetStatus::Active || !device.IsValid() ||
        !surface.IsValid() || !swapchain.IsValid() || !frameResources.IsValid() ||
        !presentationTracker.IsValid())
    {
        return OwnersResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Public lifecycle test access requires a complete active target resource set."));
    }

    const detail::VulkanDeviceQueuePlan queuePlan = device.GetInfo().queuePlan;
    const detail::VulkanGlobalDispatch dispatch = device.GetDispatch();
    auto registry = std::make_shared<ChildRegistry>(std::this_thread::get_id());
    auto bootstrapState =
        std::make_unique<RenderBootstrap::State>(registry, RenderValidationMode::Disabled);
    bootstrapState->vulkanDispatch = dispatch;

    ChildToken deviceToken{registry, ChildKind::Device};
    if (!deviceToken.active)
    {
        return OwnersResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the public lifecycle test device."));
    }

    auto deviceState =
        std::make_shared<RenderDevice::State>(std::move(deviceToken), std::move(device));
    ChildToken targetToken{registry, ChildKind::Target};
    if (!targetToken.active || !deviceState->TryRegisterTarget())
    {
        return OwnersResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the public lifecycle test target."));
    }

    std::string targetLabel = MakeTargetDiagnosticLabel(desc.targetSnapshot.GetWindowId());
    auto targetState = std::make_shared<RenderTarget::State>(
        std::move(targetToken), deviceState, desc, std::move(targetLabel), queuePlan,
        std::move(surface), std::move(swapchain), std::move(frameResources),
        std::move(presentationTracker));

    detail::RenderPublicLifecycleTestOwners owners;
    owners.bootstrap = RenderBootstrap{std::move(bootstrapState)};
    owners.device = RenderDevice{std::move(deviceState)};
    owners.target = RenderTarget{std::move(targetState)};
    return OwnersResult::FromValue(std::move(owners));
}

core::Result<RenderTarget> detail::RenderBackendTestAccess::CreateAdditionalTarget(
    RenderDevice& device, detail::VulkanSurfaceOwner&& surface,
    detail::VulkanSwapchainOwner&& swapchain, detail::VulkanFrameResourcesOwner&& frameResources,
    detail::VulkanPresentationTrackerOwner&& presentationTracker, const RenderTargetDesc& desc)
{
    if (device.m_state == nullptr || !device.m_state->token.IsActive() ||
        std::this_thread::get_id() != device.m_state->renderThread ||
        device.m_state->IsDeviceLost() || !pond::render::IsValid(desc) ||
        DetermineTargetStatus(desc.targetSnapshot) != TargetStatus::Active || !surface.IsValid() ||
        !swapchain.IsValid() || !frameResources.IsValid() || !presentationTracker.IsValid())
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Additional target test access requires a compatible device and complete active "
            "target resource set."));
    }

    ChildToken targetToken{device.m_state->token.registry, ChildKind::Target};
    if (!targetToken.active || !device.m_state->TryRegisterTarget())
    {
        return core::Result<RenderTarget>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Could not register the additional public lifecycle test target."));
    }

    const detail::VulkanDeviceQueuePlan queuePlan =
        device.m_state->vulkanDevice.GetInfo().queuePlan;
    std::string targetLabel = MakeTargetDiagnosticLabel(desc.targetSnapshot.GetWindowId());
    return core::Result<RenderTarget>::FromValue(RenderTarget{std::make_shared<RenderTarget::State>(
        std::move(targetToken), device.m_state, desc, std::move(targetLabel), queuePlan,
        std::move(surface), std::move(swapchain), std::move(frameResources),
        std::move(presentationTracker))});
}

void detail::RenderBackendTestAccess::FailNextTargetStateAllocation() noexcept
{
    g_failNextTargetStateAllocationForTesting = true;
}

void detail::RenderBackendTestAccess::FailNextFrameStateAllocation() noexcept
{
    g_failNextFrameStateAllocationForTesting = true;
}

void detail::RenderBackendTestAccess::FailNextRetirementAllocation() noexcept
{
    g_failNextRetirementAllocationForTesting = true;
}

std::uint32_t detail::RenderBackendTestAccess::GetBootstrapTargetCount(
    const RenderDevice& device) noexcept
{
    if (device.m_state == nullptr || device.m_state->token.registry == nullptr)
    {
        return 0U;
    }

    return device.m_state->token.registry->GetTargetCount();
}

detail::Draw2DDeviceLiveStats detail::RenderBackendTestAccess::GetDraw2DDeviceStats(
    const RenderDevice& device) noexcept
{
    if (device.m_state == nullptr)
    {
        return {};
    }

    const detail::VulkanDraw2DPipelineCacheStats pipelineStats =
        device.m_state->vulkanDraw2DPipelineCache.GetStats();
    return detail::Draw2DDeviceLiveStats{.pipelineCreationCount = pipelineStats.creationCount,
                                         .pipelineReuseCount = pipelineStats.reuseCount,
                                         .pipelineReplacementCount = pipelineStats.replacementCount,
                                         .activeLayerCount = device.m_state->GetDraw2DLayerCount(),
                                         .hasPipeline =
                                             device.m_state->vulkanDraw2DPipelineCache.IsValid()};
}

detail::Draw2DTargetLiveStats detail::RenderBackendTestAccess::GetDraw2DTargetStats(
    const RenderTarget& target) noexcept
{
    if (target.m_state == nullptr || !target.m_state->vulkanFrameResources.IsValid())
    {
        return {};
    }

    const detail::VulkanDraw2DUploadArena& arena =
        target.m_state->vulkanFrameResources.GetDraw2DUploadArena();
    if (!arena.IsValid())
    {
        return {};
    }

    const detail::VulkanDraw2DUploadStats uploadStats = arena.GetStats();
    detail::Draw2DTargetLiveStats stats{
        .currentCapacityBytes = static_cast<std::uint64_t>(uploadStats.currentCapacityBytes),
        .reservedBytes = static_cast<std::uint64_t>(uploadStats.reservedBytes),
        .uploadedBytes = static_cast<std::uint64_t>(uploadStats.uploadedBytes),
        .highWaterReservedBytes = static_cast<std::uint64_t>(uploadStats.highWaterReservedBytes),
        .allocationCount = uploadStats.allocationCount,
        .growthCount = uploadStats.growthCount,
        .generationCount = uploadStats.generationCount,
        .flushCount = uploadStats.flushCount,
        .retirementCount = uploadStats.retirementCount,
        .slotCount = arena.GetSlotCount(),
        .hasUploadArena = true};
    for (std::uint32_t slotIndex = 0U; slotIndex < stats.slotCount; ++slotIndex)
    {
        switch (arena.GetSlotSnapshot(slotIndex).state)
        {
        case detail::VulkanDraw2DUploadSlotState::Idle:
            ++stats.idleSlotCount;
            break;
        case detail::VulkanDraw2DUploadSlotState::Reserved:
            ++stats.reservedSlotCount;
            break;
        case detail::VulkanDraw2DUploadSlotState::Submitted:
            ++stats.submittedSlotCount;
            break;
        }
    }
    return stats;
}

detail::Draw2DDeviceLiveStats detail::RenderLiveTestAccess::GetDraw2DDeviceStats(
    const RenderDevice& device) noexcept
{
    return detail::RenderBackendTestAccess::GetDraw2DDeviceStats(device);
}

detail::Draw2DTargetLiveStats detail::RenderLiveTestAccess::GetDraw2DTargetStats(
    const RenderTarget& target) noexcept
{
    return detail::RenderBackendTestAccess::GetDraw2DTargetStats(target);
}

core::VoidResult detail::RenderBackendTestAccess::DrainOrphanedPresentationResources(
    RenderDevice& device)
{
    if (core::VoidResult renderThread = device.VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    return device.m_state->WaitForOrphanedPresentationResources();
}

core::VoidResult detail::RenderBackendTestAccess::WaitPresentationQueueIdle(RenderDevice& device)
{
    if (core::VoidResult renderThread = device.VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    return device.m_state->vulkanDevice.WaitQueueIdle(
        device.m_state->vulkanDevice.GetInfo().queuePlan.presentationQueueFamilyIndex);
}

core::Result<RenderTarget> detail::RenderBackendTestAccess::CreateTarget(
    detail::VulkanGlobalDispatch, detail::VulkanDeviceOwner&& device,
    detail::VulkanSurfaceOwner&& surface, detail::VulkanSwapchainOwner&& swapchain,
    detail::VulkanFrameResourcesOwner&& frameResources,
    detail::VulkanPresentationTrackerOwner&& presentationTracker, const RenderTargetDesc& desc)
{
    if (!pond::render::IsValid(desc) ||
        DetermineTargetStatus(desc.targetSnapshot) != TargetStatus::Active || !device.IsValid() ||
        !surface.IsValid() || !swapchain.IsValid() || !frameResources.IsValid() ||
        !presentationTracker.IsValid())
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Render backend test access requires a complete active target resource set."));
    }

    const detail::VulkanDeviceQueuePlan queuePlan = device.GetInfo().queuePlan;
    auto registry = std::make_shared<ChildRegistry>(std::this_thread::get_id());
    ChildToken deviceToken{registry, ChildKind::Device};
    if (!deviceToken.active)
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the test render device."));
    }

    auto deviceState =
        std::make_shared<RenderDevice::State>(std::move(deviceToken), std::move(device));
    if (!deviceState->TryRegisterTarget())
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the test render target."));
    }

    ChildToken targetToken{registry, ChildKind::Target};
    if (!targetToken.active)
    {
        deviceState->ReleaseTarget();
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not create the test render target token."));
    }

    std::string targetLabel = MakeTargetDiagnosticLabel(desc.targetSnapshot.GetWindowId());
    return core::Result<RenderTarget>::FromValue(RenderTarget{std::make_shared<RenderTarget::State>(
        std::move(targetToken), std::move(deviceState), desc, std::move(targetLabel), queuePlan,
        std::move(surface), std::move(swapchain), std::move(frameResources),
        std::move(presentationTracker))});
}

core::Result<PreparedSurface> detail::RenderBackendTestAccess::CreateRecoverySurface(
    RenderTarget& target, detail::VulkanSurfaceOwner&& surface, RenderTargetSnapshot snapshot)
{
    if (target.m_state == nullptr || !target.m_state->token.IsActive() || !surface.IsValid() ||
        !pond::render::IsValid(snapshot) ||
        snapshot.GetWindowId() != target.m_state->targetSnapshot.GetWindowId() ||
        snapshot.GetRevision() < target.m_state->targetSnapshot.GetRevision() ||
        std::this_thread::get_id() != target.m_state->renderThread)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Render backend test recovery requires a compatible live target and surface."));
    }

    ChildToken surfaceToken{target.m_state->token.registry, ChildKind::PreparedSurface};
    if (!surfaceToken.active)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Could not register the test recovery surface."));
    }

    auto state = std::make_unique<PreparedSurface::State>(
        std::move(surfaceToken), snapshot, SurfacePreparationReason::SurfaceLossRecovery,
        std::move(surface));
    state->renderThread = std::this_thread::get_id();
    return core::Result<PreparedSurface>::FromValue(PreparedSurface{std::move(state)});
}

void RenderTarget::State::MarkWindowRecreationPending(TargetRecreationReason reason,
                                                      std::uint64_t previousRevision,
                                                      std::uint64_t currentRevision) noexcept
{
    const TargetRecreationInfo nextRecreation{
        .reason = reason, .previousRevision = previousRevision, .currentRevision = currentRevision};
    if (!recreationPending || pendingRecreation != nextRecreation)
    {
        LOG_INFO_CATEGORY(kRenderLogCategory,
                          "target_recreation_requested target={} window_id={} reason={} "
                          "previous_revision={} current_revision={}",
                          targetLabel, targetSnapshot.GetWindowId().GetValue(),
                          static_cast<std::uint32_t>(reason), previousRevision, currentRevision);
    }
    recreationPending = true;
    pendingRecreation = nextRecreation;
}

void RenderTarget::State::MarkBackendRecreationPending(TargetRecreationReason reason) noexcept
{
    const TargetRecreationInfo nextRecreation{.reason = reason};
    if (!recreationPending || pendingRecreation != nextRecreation)
    {
        LOG_INFO_CATEGORY(kRenderLogCategory,
                          "target_recreation_requested target={} window_id={} reason={} "
                          "previous_revision=none current_revision=none",
                          targetLabel, targetSnapshot.GetWindowId().GetValue(),
                          static_cast<std::uint32_t>(reason));
    }
    recreationPending = true;
    pendingRecreation = nextRecreation;
}

void RenderTarget::State::ClearRecreationPending() noexcept
{
    recreationPending = false;
    pendingRecreation = {};
}

TargetStatus RenderTarget::State::GetEffectiveStatus() const noexcept
{
    if (deviceState != nullptr && deviceState->IsDeviceLost())
    {
        return TargetStatus::DeviceLost;
    }

    if (surfaceLost)
    {
        return TargetStatus::SurfaceLost;
    }

    return status;
}
core::VoidResult RenderTarget::State::PreparePresentationResourcesForRetirement()
{
    constexpr std::uint64_t kPresentCompletionTimeoutNanoseconds = 100'000'000ULL;

    if (deviceState == nullptr || !vulkanPresentationTracker.IsValid() ||
        !vulkanSwapchain.IsValid())
    {
        return Success();
    }

    if (vulkanPresentationTracker.GetCompletionPath() ==
        detail::VulkanPresentationCompletionPath::SwapchainMaintenanceFence)
    {
        return Success();
    }

    core::Result<detail::VulkanPresentationCompletionResult> completion =
        vulkanPresentationTracker.PollCompletion(deviceState->vulkanDispatch,
                                                 vulkanSwapchain.GetHandle(),
                                                 kPresentCompletionTimeoutNanoseconds);
    if (!completion)
    {
        deviceState->RecordDeviceFailure(completion.GetError());
        return core::VoidResult::FromError(std::move(completion).GetError());
    }

    if (completion.GetValue().status == detail::VulkanPresentationCompletionStatus::Complete)
    {
        usedExplicitPresentationCompletion =
            usedExplicitPresentationCompletion || completion.GetValue().usedExplicitCompletion;
        return Success();
    }

    vulkanPresentationTracker.MarkAwaitingSuccessorPresentation();
    return Success();
}

bool RenderTarget::State::HasUsablePresentationResources() const noexcept
{
    return vulkanSwapchain.IsValid() && vulkanFrameResources.IsValid() &&
           !vulkanFrameResources.IsPoisoned() && vulkanPresentationTracker.IsValid();
}

VkSwapchainKHR RenderTarget::State::FindOldSwapchainForReplacement() const noexcept
{
    if (vulkanSwapchain.IsValid())
    {
        return vulkanSwapchain.GetHandle();
    }

    const auto eligible = std::ranges::find_if(
        retiredPresentationResources.rbegin(), retiredPresentationResources.rend(),
        [](const RetiredPresentationResources& retired) noexcept
        {
            return retired.canBeUsedAsOldSwapchain && retired.vulkanSwapchain.IsValid();
        });
    return eligible == retiredPresentationResources.rend() ? VK_NULL_HANDLE
                                                           : eligible->vulkanSwapchain.GetHandle();
}

core::VoidResult RenderTarget::State::ReserveRetirementSlotForOldSwapchain(
    VkSwapchainKHR oldSwapchain)
{
    if (oldSwapchain == VK_NULL_HANDLE || vulkanSwapchain.GetHandle() != oldSwapchain)
    {
        return Success();
    }

    return ReserveRetirementSlotForCurrentPresentationResources();
}

core::VoidResult RenderTarget::State::ReserveRetirementSlotForCurrentPresentationResources()
{
    if ((!vulkanSwapchain.IsValid() && !vulkanFrameResources.IsValid() &&
         !vulkanPresentationTracker.IsValid()) ||
        retiredPresentationResources.size() < retiredPresentationResources.capacity())
    {
        return Success();
    }

    if (retiredPresentationResources.size() == retiredPresentationResources.max_size())
    {
        return MakeRenderFailure(
            RenderErrorCode::OutOfMemory,
            "The Vulkan presentation-resource retirement queue reached its maximum size.");
    }

    try
    {
        if (ConsumeRetirementAllocationFailureForTesting())
        {
            throw std::bad_alloc{};
        }

        retiredPresentationResources.reserve(retiredPresentationResources.size() + 1U);
    }
    catch (const std::bad_alloc&)
    {
        return MakeRenderFailure(
            RenderErrorCode::OutOfMemory,
            "Could not reserve the rollback-safe Vulkan presentation-resource retirement slot.");
    }

    return Success();
}

core::VoidResult RenderTarget::State::RetirePresentationResources(bool canBeUsedAsOldSwapchain)
{
    if (!vulkanSwapchain.IsValid() && !vulkanFrameResources.IsValid() &&
        !vulkanPresentationTracker.IsValid())
    {
        nextFrameSlot = 0U;
        return Success();
    }

    if (core::VoidResult reserve = ReserveRetirementSlotForCurrentPresentationResources(); !reserve)
    {
        return reserve;
    }

    PONDER_VERIFY(retiredPresentationResources.size() < retiredPresentationResources.capacity(),
                  "Presentation retirement requires a pre-reserved ownership slot");
    retiredPresentationResources.push_back(RetiredPresentationResources{
        .generation = swapchainGeneration,
        .vulkanSwapchain = std::move(vulkanSwapchain),
        .vulkanFrameResources = std::move(vulkanFrameResources),
        .vulkanPresentationTracker = std::move(vulkanPresentationTracker),
        .canBeUsedAsOldSwapchain = canBeUsedAsOldSwapchain});
    nextFrameSlot = 0U;
    return Success();
}

void RenderTarget::State::RecordNativeSwapchainRetirement(VkSwapchainKHR oldSwapchain) noexcept
{
    if (oldSwapchain == VK_NULL_HANDLE)
    {
        return;
    }

    if (vulkanSwapchain.GetHandle() == oldSwapchain)
    {
        const core::VoidResult retirement = RetirePresentationResources(false);
        PONDER_VERIFY(retirement.HasValue(),
                      "Native swapchain retirement must use its pre-reserved ownership slot");
        return;
    }

    const auto retired =
        std::ranges::find_if(retiredPresentationResources,
                             [oldSwapchain](const RetiredPresentationResources& resources) noexcept
                             {
                                 return resources.vulkanSwapchain.GetHandle() == oldSwapchain;
                             });
    if (retired != retiredPresentationResources.end())
    {
        retired->canBeUsedAsOldSwapchain = false;
    }
}

void RenderTarget::State::RecordCorePresentationCompletion() noexcept
{
    bool completedRetiredGeneration = false;
    for (RetiredPresentationResources& retired : retiredPresentationResources)
    {
        if (retired.vulkanPresentationTracker.GetCompletionPath() ==
                detail::VulkanPresentationCompletionPath::CoreAcquireHistory &&
            retired.vulkanPresentationTracker.HasQueuedPresentation())
        {
            retired.vulkanPresentationTracker.MarkSuccessorPresentationComplete();
            completedRetiredGeneration = true;
        }
    }

    usedCoreAcquireHistory = usedCoreAcquireHistory || completedRetiredGeneration;
}
core::Error RenderTarget::State::RecordFailure(core::Error error, std::string_view operation,
                                               OptionalBackendDiagnostic nativeDiagnostic)
{
    const RenderErrorCode renderCode = GetRenderErrorCode(error);
    if (!nativeDiagnostic.has_value())
    {
        nativeDiagnostic = detail::VulkanDiagnosticScope::TakeCurrentLastFailure();
    }
    lastFailure = MakeBackendDiagnostic(error, operation, std::move(nativeDiagnostic),
                                        targetSnapshot.GetWindowId(), targetLabel);
    LOG_WARNING_CATEGORY(kRenderLogCategory,
                         "target_failure target={} operation={} code={} message={}", targetLabel,
                         operation, static_cast<std::uint32_t>(ToErrorCode(renderCode).GetValue()),
                         error.GetMessage());
    if (renderCode == RenderErrorCode::InvalidState)
    {
        LOG_WARNING_CATEGORY(kRenderLogCategory,
                             "lifecycle_misuse operation={} target={} message={}", operation,
                             targetLabel, error.GetMessage());
    }
    return core::Error{error.GetCode(),
                       MakeTargetDiagnosticMessage(targetLabel, operation, error.GetMessage())};
}

core::VoidResult RenderTarget::State::DrainRetiredPresentationResources()
{
    if (deviceState == nullptr || retiredPresentationResources.empty())
    {
        return Success();
    }

    auto retired = retiredPresentationResources.begin();
    while (retired != retiredPresentationResources.end())
    {
        if (!retired->vulkanPresentationTracker.IsValid())
        {
            return MakeRenderFailure(
                RenderErrorCode::BackendFailure,
                "Retired Vulkan resources are missing presentation completion tracking.");
        }

        core::Result<detail::VulkanPresentationCompletionResult> completion =
            retired->vulkanPresentationTracker.PollCompletion(
                deviceState->vulkanDispatch, retired->vulkanSwapchain.GetHandle(), 0U);
        if (!completion)
        {
            deviceState->RecordDeviceFailure(completion.GetError());
            return core::VoidResult::FromError(std::move(completion).GetError());
        }

        bool canDestroy =
            completion.GetValue().status == detail::VulkanPresentationCompletionStatus::Complete;
        usedExplicitPresentationCompletion =
            usedExplicitPresentationCompletion || completion.GetValue().usedExplicitCompletion;

        if (canDestroy && retired->vulkanFrameResources.IsValid())
        {
            core::Result<bool> fencesSignaled =
                retired->vulkanFrameResources.AreAllFencesSignaled(deviceState->vulkanDispatch);
            RecordCompletedFrameSubmissions(*retired);
            if (!fencesSignaled)
            {
                deviceState->RecordDeviceFailure(fencesSignaled.GetError());
                return core::VoidResult::FromError(std::move(fencesSignaled).GetError());
            }

            canDestroy = fencesSignaled.GetValue();
        }

        if (!canDestroy)
        {
            ++retired;
            continue;
        }

        ++retiredResourceSetCount;
        retired = retiredPresentationResources.erase(retired);
    }

    return Success();
}

core::VoidResult RenderTarget::State::UsePracticalPresentationWaitFallback()
{
    if (deviceState == nullptr || !deviceState->vulkanDevice.IsValid())
    {
        return Success();
    }

    ++practicalWaitFallbackCount;
    ++deviceState->practicalWaitFallbackCount;
    LOG_WARNING_CATEGORY(kRenderLogCategory,
                         "presentation_queue_idle_fallback target={} queue_family={}", targetLabel,
                         queuePlan.presentationQueueFamilyIndex);

    core::VoidResult wait =
        deviceState->vulkanDevice.WaitQueueIdle(queuePlan.presentationQueueFamilyIndex);
    if (!wait)
    {
        deviceState->RecordDeviceFailure(wait.GetError());
    }

    return wait;
}
core::VoidResult RenderTarget::State::MarkSurfaceLost()
{
    if (core::VoidResult reserve = ReserveRetirementSlotForCurrentPresentationResources(); !reserve)
    {
        surfaceLost = true;
        ClearRecreationPending();
        return reserve;
    }

    if (!surfaceLost)
    {
        LOG_WARNING_CATEGORY(kRenderLogCategory, "surface_lost target={} revision={}", targetLabel,
                             targetSnapshot.GetRevision());
    }

    surfaceLost = true;
    ClearRecreationPending();
    if (core::VoidResult retirement = RetirePresentationResources(); !retirement)
    {
        return retirement;
    }

    constexpr std::uint64_t kInfiniteTimeout = std::numeric_limits<std::uint64_t>::max();
    bool requiresPracticalFallback = false;
    for (RetiredPresentationResources& retired : retiredPresentationResources)
    {
        core::Result<detail::VulkanPresentationCompletionResult> completion =
            retired.vulkanPresentationTracker.PollCompletion(
                deviceState->vulkanDispatch, retired.vulkanSwapchain.GetHandle(), kInfiniteTimeout);
        if (!completion)
        {
            deviceState->RecordDeviceFailure(completion.GetError());
            return core::VoidResult::FromError(std::move(completion).GetError());
        }

        if (completion.GetValue().status != detail::VulkanPresentationCompletionStatus::Complete)
        {
            requiresPracticalFallback = true;
            continue;
        }

        usedExplicitPresentationCompletion =
            usedExplicitPresentationCompletion || completion.GetValue().usedExplicitCompletion;
    }

    if (requiresPracticalFallback)
    {
        if (core::VoidResult wait = UsePracticalPresentationWaitFallback(); !wait)
        {
            return wait;
        }
    }

    for (RetiredPresentationResources& retired : retiredPresentationResources)
    {
        core::Result<bool> graphicsComplete = retired.vulkanFrameResources.AreAllFencesSignaled(
            deviceState->vulkanDispatch, kInfiniteTimeout);
        RecordCompletedFrameSubmissions(retired);
        if (!graphicsComplete)
        {
            deviceState->RecordDeviceFailure(graphicsComplete.GetError());
            return core::VoidResult::FromError(std::move(graphicsComplete).GetError());
        }
        if (!graphicsComplete.GetValue())
        {
            return MakeRenderFailure(RenderErrorCode::BackendFailure,
                                     "An infinite Vulkan graphics-fence wait returned incomplete.");
        }
    }

    retiredPresentationResources.clear();
    nextFrameSlot = 0U;
    vulkanSurface.Reset();
    return Success();
}

core::Result<PresentationRecreationStatus> RenderTarget::State::RecreatePresentationResources()
{
    using RecreationResult = core::Result<PresentationRecreationStatus>;

    if (deviceState == nullptr)
    {
        return RecreationResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "RenderTarget no longer has a valid RenderDevice."));
    }

    if (core::VoidResult usable = deviceState->CheckDeviceUsable(); !usable)
    {
        return RecreationResult::FromError(
            RecordFailure(std::move(usable).GetError(), "RecreatePresentationResources"));
    }

    if (status != TargetStatus::Active)
    {
        if (core::VoidResult drain = DrainRetiredPresentationResources(); !drain)
        {
            return RecreationResult::FromError(std::move(drain).GetError());
        }

        return RecreationResult::FromValue(PresentationRecreationStatus::NotNeeded);
    }

    if (surfaceLost || !vulkanSurface.IsValid())
    {
        return RecreationResult::FromError(RecordFailure(
            MakeRenderError(RenderErrorCode::SurfaceLost,
                            "RenderTarget requires a fresh prepared surface after surface loss."),
            "RecreatePresentationResources"));
    }

    if (!recreationPending && HasUsablePresentationResources())
    {
        if (core::VoidResult drain = DrainRetiredPresentationResources(); !drain)
        {
            return RecreationResult::FromError(std::move(drain).GetError());
        }

        return RecreationResult::FromValue(PresentationRecreationStatus::NotNeeded);
    }

    const auto fail = [this](core::Error error, std::string_view operation) -> RecreationResult
    {
        if (IsRenderError(error, RenderErrorCode::DeviceLost))
        {
            deviceState->RecordDeviceFailure(error);
        }
        else if (IsRenderError(error, RenderErrorCode::SurfaceLost))
        {
            if (core::VoidResult lost = MarkSurfaceLost(); !lost)
            {
                error = std::move(lost).GetError();
            }
        }

        return RecreationResult::FromError(RecordFailure(std::move(error), operation));
    };

    core::Result<detail::VulkanSwapchainConfig> selectedConfig =
        detail::SelectVulkanSwapchainConfig(deviceState->vulkanDispatch, deviceState->vulkanDevice,
                                            vulkanSurface.GetHandle(), queuePlan, targetDesc);
    if (!selectedConfig)
    {
        return fail(std::move(selectedConfig).GetError(), "SelectVulkanSwapchainConfig");
    }

    const detail::VulkanSwapchainConfig config = std::move(selectedConfig).GetValue();
    const VkSwapchainKHR oldSwapchain = FindOldSwapchainForReplacement();
    if (core::VoidResult reserve = ReserveRetirementSlotForOldSwapchain(oldSwapchain); !reserve)
    {
        return RecreationResult::FromError(
            RecordFailure(std::move(reserve).GetError(), "ReserveRetirementSlotForOldSwapchain"));
    }

    if (core::VoidResult prepare = PreparePresentationResourcesForRetirement(); !prepare)
    {
        return fail(std::move(prepare).GetError(), "PreparePresentationResourcesForRetirement");
    }

    detail::VulkanSwapchainCreationState creationState;
    core::Result<detail::VulkanSwapchainOwner> swapchain =
        detail::CreateVulkanSwapchainForSelectedConfig(
            deviceState->vulkanDispatch, deviceState->vulkanDevice, vulkanSurface.GetHandle(),
            config, oldSwapchain, creationState);

    if (creationState.oldSwapchainRetired)
    {
        RecordNativeSwapchainRetirement(oldSwapchain);
    }

    if (!swapchain)
    {
        if (creationState.outcome == detail::VulkanSwapchainCreationOutcome::OutOfDate)
        {
            MarkBackendRecreationPending(TargetRecreationReason::PresentationChanged);
            if (core::VoidResult drain = DrainRetiredPresentationResources(); !drain)
            {
                return RecreationResult::FromError(RecordFailure(
                    std::move(drain).GetError(), "DrainRetiredPresentationResources"));
            }

            return RecreationResult::FromValue(PresentationRecreationStatus::Pending);
        }

        return fail(std::move(swapchain).GetError(), "CreateVulkanSwapchainForSelectedConfig");
    }

    detail::VulkanSwapchainOwner replacementSwapchain = std::move(swapchain).GetValue();
    core::Result<detail::VulkanFrameResourcesOwner> frameResources =
        detail::CreateVulkanFrameResourcesForTarget(
            deviceState->vulkanDispatch, deviceState->vulkanDevice, targetSnapshot.GetWindowId(),
            queuePlan, replacementSwapchain.GetConfig().presentation.actualQueuedLatency,
            replacementSwapchain.GetFramebufferCount());
    if (!frameResources)
    {
        return fail(std::move(frameResources).GetError(), "CreateVulkanFrameResourcesForTarget");
    }

    core::Result<detail::VulkanPresentationTrackerOwner> presentationTracker =
        detail::CreateVulkanPresentationTrackerForTarget(
            deviceState->vulkanDispatch, deviceState->vulkanDevice, targetSnapshot.GetWindowId(),
            frameResources.GetValue().GetSlotCount(), replacementSwapchain.GetFramebufferCount());
    if (!presentationTracker)
    {
        return fail(std::move(presentationTracker).GetError(),
                    "CreateVulkanPresentationTrackerForTarget");
    }

    vulkanFrameResources = std::move(frameResources).GetValue();
    vulkanSwapchain = std::move(replacementSwapchain);
    vulkanPresentationTracker = std::move(presentationTracker).GetValue();
    nextFrameSlot = 0U;
    ++swapchainGeneration;
    ++recreationCount;
    LOG_INFO_CATEGORY(kRenderLogCategory,
                      "target_recreation_completed target={} window_id={} reason={} generation={} "
                      "previous_revision={} current_revision={}",
                      targetLabel, targetSnapshot.GetWindowId().GetValue(),
                      static_cast<std::uint32_t>(pendingRecreation.reason), swapchainGeneration,
                      pendingRecreation.previousRevision.value_or(0U),
                      pendingRecreation.currentRevision.value_or(0U));
    ClearRecreationPending();
    if (core::VoidResult drain = DrainRetiredPresentationResources(); !drain)
    {
        return RecreationResult::FromError(
            RecordFailure(std::move(drain).GetError(), "DrainRetiredPresentationResources"));
    }

    return RecreationResult::FromValue(PresentationRecreationStatus::Completed);
}
core::VoidResult ValidateSurfacePreparationRequest(
    platform::WindowGraphicsCompatibility compatibility, platform::WindowId actualWindowId,
    const SurfacePreparationDesc& desc)
{
    if (!pond::render::IsValid(desc))
    {
        return MakeRenderFailure(RenderErrorCode::InvalidArgument,
                                 "Surface preparation descriptor is invalid.");
    }

    if (compatibility != GetRequiredWindowGraphicsCompatibility())
    {
        return MakeRenderFailure(RenderErrorCode::UnsupportedSurface,
                                 "Window graphics compatibility does not match this render build.");
    }

    if (actualWindowId != desc.targetSnapshot.GetWindowId())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidArgument,
                                 "Surface preparation descriptor references the wrong window.");
    }

    return Success();
}

core::VoidResult ValidateTargetSnapshotUpdate(RenderTargetSnapshot current,
                                              RenderTargetSnapshot next)
{
    return ValidateTargetSnapshotSuccessor(current, next, false);
}

core::Result<RenderBootstrap> RenderBootstrap::Create(const RenderBootstrapDesc& desc)
{
    if (!pond::render::IsValid(desc))
    {
        return core::Result<RenderBootstrap>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render bootstrap descriptor is invalid."));
    }

    auto registry = std::make_shared<ChildRegistry>(std::this_thread::get_id());
    return core::Result<RenderBootstrap>::FromValue(
        RenderBootstrap{std::make_unique<State>(std::move(registry), desc.validationMode)});
}

RenderBootstrap::RenderBootstrap() noexcept = default;
RenderBootstrap::RenderBootstrap(std::unique_ptr<State> state) noexcept : m_state{std::move(state)}
{
}
RenderBootstrap::RenderBootstrap(RenderBootstrap&& other) noexcept = default;
RenderBootstrap& RenderBootstrap::operator=(RenderBootstrap&& other) noexcept
{
    if (this != &other)
    {
        if (m_state != nullptr)
        {
            m_state->VerifyDestruction();
        }

        m_state = std::move(other.m_state);
    }

    return *this;
}
RenderBootstrap::~RenderBootstrap()
{
    if (m_state != nullptr)
    {
        m_state->VerifyDestruction();
    }
}

bool RenderBootstrap::IsValid() const noexcept
{
    return m_state != nullptr && !m_state->registry->IsShutdown();
}

bool RenderBootstrap::IsOwnerThread() const noexcept
{
    return m_state != nullptr && m_state->registry->IsOwnerThread();
}

bool RenderBootstrap::IsShutdown() const noexcept
{
    return m_state == nullptr || m_state->registry->IsShutdown();
}

bool RenderBootstrap::HasActiveChildren() const noexcept
{
    return m_state != nullptr && m_state->registry->HasActiveChildren();
}

std::uint32_t RenderBootstrap::GetActivePreparedSurfaceCount() const noexcept
{
    return m_state == nullptr ? 0U : m_state->registry->GetPreparedSurfaceCount();
}

std::uint32_t RenderBootstrap::GetActiveDeviceCount() const noexcept
{
    return m_state == nullptr ? 0U : m_state->registry->GetDeviceCount();
}

std::uint32_t RenderBootstrap::GetActiveTargetCount() const noexcept
{
    return m_state == nullptr ? 0U : m_state->registry->GetTargetCount();
}

RenderBootstrapDiagnostics RenderBootstrap::GetDiagnostics() const
{
    return m_state == nullptr ? RenderBootstrapDiagnostics{} : m_state->diagnostics;
}

RenderValidationReport RenderBootstrap::GetValidationReport() const noexcept
{
    return m_state == nullptr ? RenderValidationReport{}
                              : m_state->vulkanInstance.GetValidationReport();
}

core::VoidResult RenderBootstrap::Shutdown()
{
    if (m_state == nullptr)
    {
        return Success();
    }

    if (!m_state->registry->IsOwnerThread())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderBootstrap shutdown must run on its owner thread.");
    }

    if (!m_state->registry->TryShutdown())
    {
        core::VoidResult failure = MakeRenderFailure(
            RenderErrorCode::InvalidState,
            "RenderBootstrap shutdown requires every renderer child to be destroyed first.");
        m_state->RecordFailure(failure.GetError(), "Shutdown");
        return failure;
    }

    m_state.reset();
    return Success();
}

core::Result<PreparedSurface> RenderBootstrap::PrepareSurface(platform::Window& window,
                                                              const SurfacePreparationDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "PrepareSurface requires a valid RenderBootstrap."));
    }

    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    const auto fail = [this, &diagnosticScope, windowId = window.GetId()](
                          core::Error error,
                          std::string_view operation) -> core::Result<PreparedSurface>
    {
        m_state->RecordFailure(error, operation, diagnosticScope.TakeLastFailure(), windowId);
        return core::Result<PreparedSurface>::FromError(std::move(error));
    };

    if (m_state->registry->IsShutdown())
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "PrepareSurface cannot run after shutdown."),
                    "PrepareSurface");
    }

    if (!IsOwnerThread())
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Surface preparation must run on the owner thread."));
    }

    if (core::VoidResult validation = ValidateSurfacePreparationRequest(
            window.GetGraphicsCompatibility(), window.GetId(), desc);
        !validation)
    {
        return fail(std::move(validation).GetError(), "PrepareSurface");
    }

    core::Result<platform::NativeWindowHandle> nativeHandleResult = window.GetNativeHandle();
    if (!nativeHandleResult)
    {
        return fail(
            MakeRenderError(RenderErrorCode::UnsupportedSurface,
                            "Window native handle is unavailable for Vulkan surface creation: " +
                                std::string{nativeHandleResult.GetError().GetMessage()}),
            "PrepareSurface.nativeHandle");
    }

    const platform::NativeWindowHandle nativeHandle = std::move(nativeHandleResult).GetValue();
    const detail::VulkanWsiKind wsiKind = detail::GetVulkanWsiKind(nativeHandle);

    detail::VulkanSurfaceOwner vulkanSurface;
    {
        std::scoped_lock lock{m_state->instanceMutex};
        core::Result<detail::VulkanInstanceInfo> instance =
            m_state->vulkanInstance.EnsureInitialized(m_state->vulkanDispatch, wsiKind,
                                                      m_state->validationMode);
        if (!instance)
        {
            return fail(std::move(instance).GetError(), "PrepareSurface.instance");
        }
        m_state->RecordInstanceInfo(instance.GetValue());
        LOG_INFO_CATEGORY(
            kRenderLogCategory,
            "validation_configuration requested={} enabled={} active={} api_version={}",
            static_cast<std::uint32_t>(m_state->diagnostics.requestedValidationMode),
            static_cast<std::uint32_t>(m_state->diagnostics.enabledValidationMode),
            m_state->diagnostics.validationEnabled, m_state->diagnostics.negotiatedApiVersion);

        core::Result<detail::VulkanSurfaceOwner> surface =
            detail::CreateVulkanSurfaceForNativeWindow(
                m_state->vulkanDispatch, m_state->vulkanInstance.GetOwner(), nativeHandle);
        if (!surface)
        {
            return fail(std::move(surface).GetError(), "PrepareSurface.surface");
        }

        vulkanSurface = std::move(surface).GetValue();
    }

    ChildToken token{m_state->registry, ChildKind::PreparedSurface};
    if (!token.active)
    {
        return fail(
            MakeRenderError(RenderErrorCode::InvalidState, "Render bootstrap is shutting down."),
            "PrepareSurface");
    }

    return core::Result<PreparedSurface>::FromValue(
        PreparedSurface{std::make_unique<PreparedSurface::State>(
            std::move(token), desc.targetSnapshot, desc.reason, std::move(vulkanSurface))});
}
core::Result<PreparedSurface> detail::RenderBootstrapTestAccess::CreatePreparedSurface(
    RenderBootstrap& bootstrap, const SurfacePreparationDesc& desc)
{
    if (bootstrap.m_state == nullptr)
    {
        return core::Result<PreparedSurface>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Test prepared-surface creation requires a valid RenderBootstrap."));
    }

    if (bootstrap.m_state->registry->IsShutdown())
    {
        return core::Result<PreparedSurface>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Test prepared-surface creation cannot run after shutdown."));
    }

    if (!bootstrap.IsOwnerThread())
    {
        return core::Result<PreparedSurface>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Prepared surface creation must run on the owner thread."));
    }

    if (!pond::render::IsValid(desc))
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Surface preparation descriptor is invalid."));
    }

    ChildToken token{bootstrap.m_state->registry, ChildKind::PreparedSurface};
    if (!token.active)
    {
        return core::Result<PreparedSurface>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState, "Render bootstrap is shutting down."));
    }

    return core::Result<PreparedSurface>::FromValue(
        PreparedSurface{std::make_unique<PreparedSurface::State>(
            std::move(token), desc.targetSnapshot, desc.reason)});
}

core::Result<RenderAdapterSelection> RenderBootstrap::SelectAdapter(
    const PreparedSurface& firstSurface, const RenderAdapterSelectionDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "SelectAdapter requires a valid RenderBootstrap."));
    }

    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    const auto fail = [this, &diagnosticScope, windowId = firstSurface.GetWindowId()](
                          core::Error error) -> core::Result<RenderAdapterSelection>
    {
        m_state->RecordFailure(error, "SelectAdapter", diagnosticScope.TakeLastFailure(), windowId);
        return core::Result<RenderAdapterSelection>::FromError(std::move(error));
    };

    if (m_state->registry->IsShutdown())
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "SelectAdapter cannot run after shutdown."));
    }

    if (!pond::render::IsValid(desc))
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidArgument,
                                    "Render adapter selection descriptor is invalid."));
    }

    if (firstSurface.m_state == nullptr)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "SelectAdapter requires a valid first prepared surface."));
    }

    if (firstSurface.m_state->token.registry != m_state->registry)
    {
        return fail(
            MakeRenderError(RenderErrorCode::InvalidArgument,
                            "First prepared surface belongs to a different RenderBootstrap."));
    }

    if (core::VoidResult live =
            ValidateLiveRegistry(firstSurface.m_state->token, "PreparedSurface");
        !live)
    {
        return fail(std::move(live).GetError());
    }

    if (core::VoidResult renderThread = firstSurface.VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderAdapterSelection>::FromError(std::move(renderThread).GetError());
    }

    if (!firstSurface.m_state->vulkanSurface.IsValid())
    {
        return fail(MakeRenderError(
            RenderErrorCode::InvalidState,
            "SelectAdapter requires a first prepared surface with a live Vulkan surface."));
    }

    std::shared_ptr<detail::VulkanInstanceOwner> instance =
        firstSurface.m_state->vulkanSurface.GetInstanceOwner();
    const VkSurfaceKHR surface = firstSurface.m_state->vulkanSurface.GetHandle();

    core::Result<RenderAdapterSelection> selection =
        core::Result<RenderAdapterSelection>::FromError(
            MakeRenderError(RenderErrorCode::BackendFailure, "Adapter selection did not run."));
    {
        std::scoped_lock lock{m_state->instanceMutex};
        selection = detail::SelectVulkanAdapterForSurface(m_state->vulkanDispatch,
                                                          std::move(instance), surface, desc);
    }
    if (!selection)
    {
        return fail(std::move(selection).GetError());
    }

    const RenderAdapterSelection& selected = selection.GetValue();
    LOG_INFO_CATEGORY(
        kRenderLogCategory,
        "adapter_selected name={} api_version={} preference={} preference_fallback={}",
        selected.selectedAdapter.identity.name, selected.selectedAdapter.apiVersion,
        static_cast<std::uint32_t>(selected.request.adapterPreference),
        selected.selectedByPreferenceFallback);
    return selection;
}
core::Result<RenderDevice> RenderBootstrap::CreateDevice(
    const PreparedSurface& firstSurface, const RenderAdapterSelection& adapterSelection,
    const RenderDeviceDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "CreateDevice requires a valid RenderBootstrap."));
    }

    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    const auto fail = [this, &diagnosticScope, windowId = firstSurface.GetWindowId()](
                          core::Error error) -> core::Result<RenderDevice>
    {
        m_state->RecordFailure(error, "CreateDevice", diagnosticScope.TakeLastFailure(), windowId);
        return core::Result<RenderDevice>::FromError(std::move(error));
    };

    if (m_state->registry->IsShutdown())
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "CreateDevice cannot run after shutdown."));
    }

    if (!pond::render::IsValid(desc))
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidArgument,
                                    "Render device descriptor is invalid."));
    }

    if (!pond::render::IsValid(adapterSelection))
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidArgument,
                                    "Render adapter selection is invalid."));
    }

    if (firstSurface.m_state == nullptr)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "CreateDevice requires a valid first prepared surface."));
    }

    if (firstSurface.m_state->token.registry != m_state->registry)
    {
        return fail(
            MakeRenderError(RenderErrorCode::InvalidArgument,
                            "First prepared surface belongs to a different RenderBootstrap."));
    }

    if (core::VoidResult live =
            ValidateLiveRegistry(firstSurface.m_state->token, "PreparedSurface");
        !live)
    {
        return fail(std::move(live).GetError());
    }

    if (core::VoidResult renderThread = firstSurface.VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderDevice>::FromError(std::move(renderThread).GetError());
    }

    if (!firstSurface.m_state->vulkanSurface.IsValid())
    {
        return fail(MakeRenderError(
            RenderErrorCode::InvalidState,
            "CreateDevice requires a first prepared surface with a live Vulkan surface."));
    }

    std::shared_ptr<detail::VulkanInstanceOwner> instance =
        firstSurface.m_state->vulkanSurface.GetInstanceOwner();
    const VkSurfaceKHR surface = firstSurface.m_state->vulkanSurface.GetHandle();

    core::Result<detail::VulkanDeviceOwner> vulkanDevice =
        core::Result<detail::VulkanDeviceOwner>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure, "Vulkan device creation did not run."));
    {
        std::scoped_lock lock{m_state->instanceMutex};
        vulkanDevice = detail::CreateVulkanDeviceForAdapterSelection(
            m_state->vulkanDispatch, std::move(instance), surface, adapterSelection, desc);
    }

    if (!vulkanDevice)
    {
        return fail(std::move(vulkanDevice).GetError());
    }

    ChildToken token{m_state->registry, ChildKind::Device};
    if (!token.active)
    {
        return fail(
            MakeRenderError(RenderErrorCode::InvalidState, "Render bootstrap is shutting down."));
    }

    const detail::VulkanDeviceOptionalCapabilities& enabledCapabilities =
        vulkanDevice.GetValue().GetInfo().optionalCapabilities;
    LOG_INFO_CATEGORY(
        kRenderLogCategory,
        "device_requirements adapter={} swapchain_required=true allocator_ready={} "
        "swapchain_maintenance1_preference_enabled={} present_id_preference_enabled={} "
        "present_wait_preference_enabled={}",
        adapterSelection.selectedAdapter.identity.name, enabledCapabilities.vmaAllocator,
        enabledCapabilities.swapchainMaintenance1, enabledCapabilities.presentId,
        enabledCapabilities.presentWait);
    return core::Result<RenderDevice>::FromValue(RenderDevice{std::make_shared<RenderDevice::State>(
        std::move(token), std::move(vulkanDevice).GetValue())});
}

RenderDevice::RenderDevice() noexcept = default;
RenderDevice::RenderDevice(std::shared_ptr<State> state) noexcept : m_state{std::move(state)} {}
RenderDevice::RenderDevice(RenderDevice&& other) noexcept = default;
RenderDevice& RenderDevice::operator=(RenderDevice&& other) noexcept
{
    if (this != &other)
    {
        if (m_state != nullptr)
        {
            m_state->VerifyDestruction();
        }

        m_state = std::move(other.m_state);
    }

    return *this;
}
RenderDevice::~RenderDevice()
{
    if (m_state != nullptr)
    {
        m_state->VerifyDestruction();
    }
}

bool RenderDevice::IsValid() const noexcept
{
    return m_state != nullptr && m_state->token.IsActive();
}

bool RenderDevice::IsDeviceLost() const noexcept
{
    return m_state != nullptr && m_state->IsDeviceLost();
}

core::VoidResult RenderDevice::VerifyRenderThread() const
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderDevice is moved-from or empty.");
    }

    if (core::VoidResult live = ValidateLiveRegistry(m_state->token, "RenderDevice"); !live)
    {
        return live;
    }

    if (core::VoidResult usable = m_state->CheckDeviceUsable(); !usable)
    {
        return usable;
    }

    return ValidateCurrentThread(m_state->renderThread, "RenderDevice operation");
}

core::VoidResult RenderDevice::WaitIdle() const
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    ++m_state->waitIdleCalls;
    core::VoidResult wait = m_state->vulkanDevice.WaitIdle();
    if (!wait)
    {
        m_state->RecordDeviceFailure(wait.GetError());
        return wait;
    }

    m_state->ClearOrphanedPresentationResourcesAfterIdle();
    return wait;
}

const RenderAdapterSnapshot& RenderDevice::GetSelectedAdapter() const noexcept
{
    static const RenderAdapterSnapshot kEmptyAdapter{};
    return m_state == nullptr ? kEmptyAdapter : m_state->vulkanDevice.GetInfo().selectedAdapter;
}

RenderDeviceDiagnostics RenderDevice::GetDiagnostics() const
{
    if (m_state == nullptr)
    {
        return {};
    }

    return RenderDeviceDiagnostics{.selectedAdapter = GetSelectedAdapter(),
                                   .targetCreateAttempts = m_state->targetCreateAttempts,
                                   .targetCreateSuccesses = m_state->targetCreateSuccesses,
                                   .targetCreateFailures = m_state->targetCreateFailures,
                                   .waitIdleCalls = m_state->waitIdleCalls,
                                   .practicalWaitFallbacks = m_state->practicalWaitFallbackCount,
                                   .deviceLost = m_state->IsDeviceLost(),
                                   .lastFailure = m_state->lastFailure};
}

std::uint32_t RenderDevice::GetActiveTargetCount() const noexcept
{
    return m_state == nullptr ? 0U : m_state->GetTargetCount();
}

core::Result<RenderTarget> RenderDevice::CreateRenderTarget(PreparedSurface&& preparedSurface,
                                                            const RenderTargetDesc& desc)
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    std::string targetLabel;
    auto fail = [this, &diagnosticScope, &targetLabel, &desc](core::Error error)
    {
        if (m_state != nullptr)
        {
            ++m_state->targetCreateFailures;
            m_state->RecordDeviceFailure(error, "CreateRenderTarget",
                                         diagnosticScope.TakeLastFailure(),
                                         desc.targetSnapshot.GetWindowId(), targetLabel);
        }

        const RenderErrorCode renderCode = GetRenderErrorCode(error);
        if (renderCode == RenderErrorCode::InvalidState)
        {
            LOG_WARNING_CATEGORY(
                kRenderLogCategory,
                "lifecycle_misuse operation=CreateRenderTarget target={} message={}", targetLabel,
                error.GetMessage());
        }
        if (!targetLabel.empty())
        {
            return core::Result<RenderTarget>::FromError(core::Error{
                error.GetCode(), MakeTargetDiagnosticMessage(targetLabel, "CreateRenderTarget",
                                                             error.GetMessage())});
        }

        return core::Result<RenderTarget>::FromError(std::move(error));
    };

    if (m_state == nullptr)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "CreateRenderTarget requires a valid RenderDevice."));
    }

    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderTarget>::FromError(std::move(renderThread).GetError());
    }

    ++m_state->targetCreateAttempts;

    if (!pond::render::IsValid(desc))
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidArgument,
                                    "Render target descriptor is invalid."));
    }

    try
    {
        targetLabel = MakeTargetDiagnosticLabel(desc.targetSnapshot.GetWindowId());
    }
    catch (const std::bad_alloc&)
    {
        return fail(MakeRenderError(RenderErrorCode::OutOfMemory,
                                    "Could not allocate the render target diagnostic label."));
    }

    if (preparedSurface.m_state == nullptr)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "CreateRenderTarget requires a valid prepared surface."));
    }

    if (preparedSurface.m_state->token.registry != m_state->token.registry)
    {
        return fail(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Prepared surface belongs to a different RenderBootstrap than this RenderDevice."));
    }

    if (core::VoidResult live =
            ValidateLiveRegistry(preparedSurface.m_state->token, "PreparedSurface");
        !live)
    {
        return fail(std::move(live).GetError());
    }

    if (core::VoidResult surfaceThread = preparedSurface.VerifyRenderThread(); !surfaceThread)
    {
        return fail(std::move(surfaceThread).GetError());
    }

    if (!preparedSurface.m_state->vulkanSurface.IsValid())
    {
        return fail(MakeRenderError(
            RenderErrorCode::InvalidState,
            "CreateRenderTarget requires a prepared surface with a live Vulkan surface."));
    }

    const RenderTargetSnapshot preparedSnapshot = preparedSurface.m_state->targetSnapshot;
    if (core::VoidResult snapshot =
            ValidateTargetSnapshotSuccessor(preparedSnapshot, desc.targetSnapshot, true);
        !snapshot)
    {
        return fail(std::move(snapshot).GetError());
    }

    core::Result<detail::VulkanDeviceQueuePlan> targetQueuePlan =
        detail::ValidateVulkanDeviceSurfaceCompatibility(
            m_state->vulkanDispatch, m_state->vulkanDevice,
            preparedSurface.m_state->vulkanSurface.GetHandle());
    if (!targetQueuePlan)
    {
        return fail(std::move(targetQueuePlan).GetError());
    }

    detail::VulkanSwapchainOwner vulkanSwapchain;
    detail::VulkanFrameResourcesOwner vulkanFrameResources;
    detail::VulkanPresentationTrackerOwner vulkanPresentationTracker;
    if (DetermineTargetStatus(desc.targetSnapshot) == TargetStatus::Active)
    {
        core::Result<detail::VulkanSwapchainOwner> swapchain =
            detail::CreateVulkanSwapchainForTarget(
                m_state->vulkanDispatch, m_state->vulkanDevice,
                preparedSurface.m_state->vulkanSurface.GetHandle(), targetQueuePlan.GetValue(),
                desc);
        if (!swapchain)
        {
            return fail(std::move(swapchain).GetError());
        }

        vulkanSwapchain = std::move(swapchain).GetValue();

        core::Result<detail::VulkanFrameResourcesOwner> frameResources =
            detail::CreateVulkanFrameResourcesForTarget(
                m_state->vulkanDispatch, m_state->vulkanDevice, desc.targetSnapshot.GetWindowId(),
                targetQueuePlan.GetValue(),
                vulkanSwapchain.GetConfig().presentation.actualQueuedLatency,
                vulkanSwapchain.GetFramebufferCount());
        if (!frameResources)
        {
            return fail(std::move(frameResources).GetError());
        }

        core::Result<detail::VulkanPresentationTrackerOwner> presentationTracker =
            detail::CreateVulkanPresentationTrackerForTarget(
                m_state->vulkanDispatch, m_state->vulkanDevice, desc.targetSnapshot.GetWindowId(),
                frameResources.GetValue().GetSlotCount(), vulkanSwapchain.GetFramebufferCount());
        if (!presentationTracker)
        {
            return fail(std::move(presentationTracker).GetError());
        }

        vulkanFrameResources = std::move(frameResources).GetValue();
        vulkanPresentationTracker = std::move(presentationTracker).GetValue();
    }

    if (!m_state->TryRegisterTarget())
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "RenderDevice cannot create targets after shutdown."));
    }

    ChildToken targetToken{preparedSurface.m_state->token.registry, ChildKind::Target};
    if (!targetToken.active)
    {
        m_state->ReleaseTarget();
        return fail(
            MakeRenderError(RenderErrorCode::InvalidState, "Render bootstrap is shutting down."));
    }

    std::shared_ptr<RenderTarget::State> targetState;
    try
    {
        if (ConsumeTargetStateAllocationFailureForTesting())
        {
            throw std::bad_alloc{};
        }

        targetState = std::make_shared<RenderTarget::State>(
            std::move(targetToken), m_state, desc, std::move(targetLabel),
            std::move(targetQueuePlan).GetValue(),
            std::move(preparedSurface.m_state->vulkanSurface), std::move(vulkanSwapchain),
            std::move(vulkanFrameResources), std::move(vulkanPresentationTracker));
    }
    catch (const std::bad_alloc&)
    {
        m_state->ReleaseTarget();
        return fail(MakeRenderError(RenderErrorCode::OutOfMemory,
                                    "Could not allocate the render target ownership state."));
    }

    preparedSurface.m_state.reset();
    ++m_state->targetCreateSuccesses;
    LOG_INFO_CATEGORY(kRenderLogCategory, "target_created target={} revision={} active={}",
                      targetState->targetLabel, desc.targetSnapshot.GetRevision(),
                      DetermineTargetStatus(desc.targetSnapshot) == TargetStatus::Active);
    LOG_INFO_CATEGORY(
        kRenderLogCategory,
        "presentation_requirements target={} policy={} policy_strength={} queued_frames={} "
        "queued_strength={}",
        targetState->targetLabel, static_cast<std::uint32_t>(desc.presentation.policy),
        static_cast<std::uint32_t>(desc.presentation.strength),
        desc.queuedLatency.maximumQueuedFrames.frameCount,
        static_cast<std::uint32_t>(desc.queuedLatency.strength));
    if (targetState->vulkanSwapchain.IsValid())
    {
        const SelectedPresentationConfig& presentation =
            targetState->vulkanSwapchain.GetConfig().presentation;
        LOG_INFO_CATEGORY(kRenderLogCategory,
                          "presentation_settings target={} actual_policy={} policy_fallback={} "
                          "actual_queued_frames={} queued_fallback={} output={}",
                          targetState->targetLabel,
                          static_cast<std::uint32_t>(presentation.actualPolicy),
                          static_cast<std::uint32_t>(presentation.policyFallback),
                          presentation.actualQueuedLatency.frameCount,
                          static_cast<std::uint32_t>(presentation.queuedLatencyFallback),
                          static_cast<std::uint32_t>(presentation.output));
    }

    return core::Result<RenderTarget>::FromValue(RenderTarget{std::move(targetState)});
}

namespace draw2d
{
namespace
{
[[nodiscard]] bool IsNonRecordingFrameStatus(FrameStatus status) noexcept
{
    return status == FrameStatus::SkippedSuspended || status == FrameStatus::TimedOut ||
           status == FrameStatus::RecreationPending;
}

[[nodiscard]] Draw2DPixelExtent ToDraw2DPixelExtent(platform::PixelSize extent) noexcept
{
    return Draw2DPixelExtent{.width = extent.width, .height = extent.height};
}
} // namespace

Draw2DLayer::Draw2DLayer() noexcept = default;

Draw2DLayer::Draw2DLayer(std::shared_ptr<RenderDevice::State> state) noexcept
    : m_state{std::move(state)},
      m_renderThread{m_state == nullptr ? std::thread::id{} : m_state->renderThread}
{
}

Draw2DLayer::Draw2DLayer(Draw2DLayer&& other) noexcept
    : m_state{std::move(other.m_state)}, m_renderThread{std::exchange(other.m_renderThread, {})}
{
}

Draw2DLayer& Draw2DLayer::operator=(Draw2DLayer&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_state = std::move(other.m_state);
        m_renderThread = std::exchange(other.m_renderThread, {});
    }

    return *this;
}

Draw2DLayer::~Draw2DLayer()
{
    Release();
}

core::Result<Draw2DLayer> Draw2DLayer::Create(RenderDevice& device)
{
    if (core::VoidResult renderThread = device.VerifyRenderThread(); !renderThread)
    {
        return core::Result<Draw2DLayer>::FromError(std::move(renderThread).GetError());
    }

    if (!device.m_state->TryRegisterDraw2DLayer())
    {
        return core::Result<Draw2DLayer>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Draw2DLayer cannot be created after device shutdown."));
    }

    return core::Result<Draw2DLayer>::FromValue(Draw2DLayer{device.m_state});
}

bool Draw2DLayer::IsValid() const noexcept
{
    return m_state != nullptr && m_state->token.IsActive();
}

core::VoidResult Draw2DLayer::VerifyRenderThread() const
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "Draw2DLayer is moved-from or empty.");
    }

    if (core::VoidResult live = ValidateLiveRegistry(m_state->token, "RenderDevice"); !live)
    {
        return live;
    }

    if (core::VoidResult usable = m_state->CheckDeviceUsable(); !usable)
    {
        return usable;
    }

    return ValidateCurrentThread(m_renderThread, "Draw2DLayer operation");
}

core::VoidResult Draw2DLayer::ValidateFrameAccess(RenderFrame& frame) const
{
    if (core::VoidResult layerThread = VerifyRenderThread(); !layerThread)
    {
        return layerThread;
    }

    if (core::VoidResult frameThread = frame.VerifyRenderThread(); !frameThread)
    {
        return frameThread;
    }

    RenderFrame::State& frameState = *frame.m_state;
    RenderTarget::State& target = *frameState.targetState;
    if (target.deviceState == nullptr || target.deviceState.get() != m_state.get())
    {
        return core::VoidResult::FromError(target.RecordFailure(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Draw2DLayer belongs to a different RenderDevice than the frame."),
            "Draw2DStage"));
    }

    return Success();
}
core::VoidResult Draw2DLayer::Record(RenderFrame& frame, const Draw2DPacket& packet)
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    if (core::VoidResult layerThread = VerifyRenderThread(); !layerThread)
    {
        return layerThread;
    }

    if (core::VoidResult frameThread = frame.VerifyRenderThread(); !frameThread)
    {
        return frameThread;
    }

    RenderFrame::State& frameState = *frame.m_state;
    RenderTarget::State& target = *frameState.targetState;
    if (target.deviceState == nullptr || target.deviceState.get() != m_state.get())
    {
        return core::VoidResult::FromError(target.RecordFailure(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Draw2DLayer belongs to a different RenderDevice than the frame."),
            "Draw2DStage"));
    }

    if (IsNonRecordingFrameStatus(frameState.status))
    {
        return Success();
    }

    core::Result<Draw2DPacketStats> packetStats = ValidateDraw2DPacket(packet);
    if (!packetStats)
    {
        return core::VoidResult::FromError(
            target.RecordFailure(std::move(packetStats).GetError(), "Draw2DStagePacket"));
    }

    const Draw2DPixelExtent frameExtent = ToDraw2DPixelExtent(frame.GetMetrics().pixelSize);
    if (packet.GetPixelExtent() != frameExtent)
    {
        return core::VoidResult::FromError(target.RecordFailure(
            MakeRenderError(RenderErrorCode::InvalidArgument,
                            "Draw2D packet extent does not match the active frame pixel extent."),
            "Draw2DStageTarget"));
    }

    if (!frameState.clearRecorded)
    {
        return core::VoidResult::FromError(target.RecordFailure(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Draw2D stage recording requires a cleared active frame."),
            "Draw2DStageFrame"));
    }

    core::VoidResult recorded = ::pond::render::detail::RecordVulkanDraw2DStage(
        target.deviceState->vulkanDispatch, target.deviceState->vulkanDevice,
        target.vulkanSwapchain, target.vulkanFrameResources,
        target.deviceState->vulkanDraw2DPipelineCache, frameState.recording, packet);
    if (!recorded)
    {
        ++target.frameFailures;
        core::Error error = std::move(recorded).GetError();
        if (IsRenderError(error, RenderErrorCode::DeviceLost))
        {
            target.deviceState->RecordDeviceFailure(
                error, "RecordVulkanDraw2DStage",
                ::pond::render::detail::VulkanDiagnosticScope::CopyCurrentLastFailure(),
                target.targetSnapshot.GetWindowId(), target.targetLabel);
        }
        return core::VoidResult::FromError(target.RecordFailure(
            std::move(error), "RecordVulkanDraw2DStage", diagnosticScope.TakeLastFailure()));
    }

    return Success();
}

void Draw2DLayer::Release() noexcept
{
    if (m_state == nullptr)
    {
        return;
    }

    PONDER_VERIFY(std::this_thread::get_id() == m_renderThread,
                  "Draw2DLayer destruction must occur on its render thread");
    m_state->ReleaseDraw2DLayer();
    m_state.reset();
    m_renderThread = {};
}
} // namespace draw2d
PreparedSurface::PreparedSurface() noexcept = default;
PreparedSurface::PreparedSurface(std::unique_ptr<State> state) noexcept : m_state{std::move(state)}
{
}
PreparedSurface::PreparedSurface(PreparedSurface&& other) noexcept = default;
PreparedSurface& PreparedSurface::operator=(PreparedSurface&& other) noexcept
{
    if (this != &other)
    {
        if (m_state != nullptr)
        {
            m_state->VerifyDestruction();
        }

        m_state = std::move(other.m_state);
    }

    return *this;
}
PreparedSurface::~PreparedSurface()
{
    if (m_state != nullptr)
    {
        m_state->VerifyDestruction();
    }
}

bool PreparedSurface::IsValid() const noexcept
{
    return m_state != nullptr && m_state->token.IsActive();
}

bool PreparedSurface::IsBoundToRenderThread() const noexcept
{
    return IsValid() && m_state->renderThread.has_value();
}

RenderTargetSnapshot PreparedSurface::GetTargetSnapshot() const noexcept
{
    return m_state == nullptr ? RenderTargetSnapshot{} : m_state->targetSnapshot;
}

platform::WindowId PreparedSurface::GetWindowId() const noexcept
{
    return GetTargetSnapshot().GetWindowId();
}

core::VoidResult PreparedSurface::TransferToCurrentThread(RenderTargetSnapshot latestSnapshot)
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "PreparedSurface is moved-from or empty.");
    }

    if (core::VoidResult live = ValidateLiveRegistry(m_state->token, "PreparedSurface"); !live)
    {
        return live;
    }

    if (m_state->renderThread.has_value())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "PreparedSurface has already been transferred.");
    }

    if (core::VoidResult update =
            ValidateTargetSnapshotSuccessor(m_state->targetSnapshot, latestSnapshot, true);
        !update)
    {
        return update;
    }

    m_state->targetSnapshot = latestSnapshot;
    m_state->renderThread = std::this_thread::get_id();
    return Success();
}

core::VoidResult PreparedSurface::VerifyRenderThread() const
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "PreparedSurface is moved-from or empty.");
    }

    if (core::VoidResult live = ValidateLiveRegistry(m_state->token, "PreparedSurface"); !live)
    {
        return live;
    }

    if (!m_state->renderThread.has_value())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "PreparedSurface has not been transferred yet.");
    }

    return ValidateCurrentThread(*m_state->renderThread, "PreparedSurface operation");
}

RenderTarget::RenderTarget() noexcept = default;
RenderTarget::RenderTarget(std::shared_ptr<State> state) noexcept : m_state{std::move(state)} {}
RenderTarget::RenderTarget(RenderTarget&& other) noexcept = default;
RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept
{
    if (this != &other)
    {
        if (m_state != nullptr)
        {
            m_state->VerifyDestruction();
        }

        m_state = std::move(other.m_state);
    }

    return *this;
}
RenderTarget::~RenderTarget()
{
    if (m_state != nullptr)
    {
        m_state->VerifyDestruction();
    }
}

bool RenderTarget::IsValid() const noexcept
{
    return m_state != nullptr && m_state->token.IsActive();
}

bool RenderTarget::IsDeviceLost() const noexcept
{
    return m_state != nullptr && m_state->deviceState != nullptr &&
           m_state->deviceState->IsDeviceLost();
}

core::VoidResult RenderTarget::VerifyRenderThread() const
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderTarget is moved-from or empty.");
    }

    if (core::VoidResult live = ValidateLiveRegistry(m_state->token, "RenderTarget"); !live)
    {
        return live;
    }

    if (m_state->deviceState == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderTarget no longer has a valid RenderDevice.");
    }

    if (core::VoidResult deviceLive =
            ValidateLiveRegistry(m_state->deviceState->token, "RenderDevice");
        !deviceLive)
    {
        return deviceLive;
    }

    if (core::VoidResult usable = m_state->deviceState->CheckDeviceUsable(); !usable)
    {
        return usable;
    }

    return ValidateCurrentThread(m_state->renderThread, "RenderTarget operation");
}

platform::WindowId RenderTarget::GetWindowId() const noexcept
{
    return GetTargetSnapshot().GetWindowId();
}

RenderTargetSnapshot RenderTarget::GetTargetSnapshot() const noexcept
{
    return m_state == nullptr ? RenderTargetSnapshot{} : m_state->targetSnapshot;
}

TargetStatus RenderTarget::GetStatus() const noexcept
{
    return m_state == nullptr ? TargetStatus::Suspended : m_state->GetEffectiveStatus();
}

bool RenderTarget::IsSuspended() const noexcept
{
    return m_state != nullptr && IsSuspendedStatus(m_state->status);
}

bool RenderTarget::IsSurfaceLost() const noexcept
{
    return m_state != nullptr && m_state->surfaceLost;
}

bool RenderTarget::HasSwapchain() const noexcept
{
    return m_state != nullptr && m_state->HasUsablePresentationResources();
}

std::uint64_t RenderTarget::GetSwapchainGeneration() const noexcept
{
    return HasSwapchain() ? m_state->swapchainGeneration : 0U;
}

bool RenderTarget::HasPendingRecreation() const noexcept
{
    return m_state != nullptr && m_state->recreationPending;
}

std::optional<TargetRecreationInfo> RenderTarget::GetPendingRecreationInfo() const noexcept
{
    if (!HasPendingRecreation())
    {
        return std::nullopt;
    }

    return m_state->pendingRecreation;
}

PresentationRetirementStats RenderTarget::GetPresentationRetirementStats() const noexcept
{
    if (m_state == nullptr)
    {
        return {};
    }

    return PresentationRetirementStats{
        .pendingResourceSets = m_state->retiredPresentationResources.size(),
        .retiredResourceSets = m_state->retiredResourceSetCount,
        .practicalWaitFallbacks = m_state->practicalWaitFallbackCount,
        .usedExplicitPresentationCompletion = m_state->usedExplicitPresentationCompletion,
        .usedCoreAcquireHistory = m_state->usedCoreAcquireHistory,
        .surfaceLost = m_state->surfaceLost,
        .deviceLost = m_state->deviceState != nullptr && m_state->deviceState->IsDeviceLost()};
}

std::optional<SelectedPresentationConfig> RenderTarget::GetSelectedPresentationConfig()
    const noexcept
{
    if (!HasSwapchain())
    {
        return std::nullopt;
    }

    return m_state->vulkanSwapchain.GetConfig().presentation;
}

RenderTargetDiagnostics RenderTarget::GetDiagnostics() const
{
    if (m_state == nullptr)
    {
        return {};
    }

    return RenderTargetDiagnostics{.windowId = m_state->targetSnapshot.GetWindowId(),
                                   .targetLabel = m_state->targetLabel,
                                   .targetSnapshot = m_state->targetSnapshot,
                                   .status = m_state->GetEffectiveStatus(),
                                   .swapchainGeneration = GetSwapchainGeneration(),
                                   .recreationCount = m_state->recreationCount,
                                   .suspensionCount = m_state->suspensionCount,
                                   .frameAcquireAttempts = m_state->frameAcquireAttempts,
                                   .frameAcquireFailures = m_state->frameAcquireFailures,
                                   .framesPresented = m_state->framesPresented,
                                   .frameTimeouts = m_state->frameTimeouts,
                                   .frameFailures = m_state->frameFailures,
                                   .retirement = GetPresentationRetirementStats(),
                                   .pendingRecreation = GetPendingRecreationInfo(),
                                   .selectedPresentation = GetSelectedPresentationConfig(),
                                   .lastFailure = m_state->lastFailure};
}

core::Result<RenderFrame> RenderTarget::AcquireFrame()
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderFrame>::FromError(std::move(renderThread).GetError());
    }

    ++m_state->frameAcquireAttempts;
    auto fail = [this](core::Error error)
    {
        ++m_state->frameAcquireFailures;
        return core::Result<RenderFrame>::FromError(
            m_state->RecordFailure(std::move(error), "AcquireFrame"));
    };
    auto makeFrame =
        [this, &fail](FrameStatus status,
                      detail::VulkanFrameRecordingState recording) -> core::Result<RenderFrame>
    {
        try
        {
            if (ConsumeFrameStateAllocationFailureForTesting())
            {
                throw std::bad_alloc{};
            }

            auto frameState =
                std::make_unique<RenderFrame::State>(m_state, status, true, std::move(recording));
            m_state->activeFrame = true;
            return core::Result<RenderFrame>::FromValue(RenderFrame{std::move(frameState)});
        }
        catch (const std::bad_alloc&)
        {
            if (recording.IsActive())
            {
                detail::AbandonVulkanFrame(m_state->vulkanFrameResources,
                                           m_state->vulkanPresentationTracker, recording);
                if (m_state->deviceState != nullptr && !m_state->deviceState->IsDeviceLost())
                {
                    m_state->MarkBackendRecreationPending(
                        TargetRecreationReason::PresentationChanged);
                }
                ++m_state->frameFailures;
            }

            return fail(
                MakeRenderError(RenderErrorCode::OutOfMemory,
                                "Could not allocate the acquired render frame ownership state."));
        }
    };

    if (m_state->activeFrame)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "RenderTarget already has an active frame."));
    }

    if (m_state->surfaceLost)
    {
        return fail(MakeRenderError(
            RenderErrorCode::SurfaceLost,
            "RenderTarget requires RecoverSurface before another frame can be acquired."));
    }

    if (IsSuspendedStatus(m_state->status))
    {
        return makeFrame(FrameStatus::SkippedSuspended, {});
    }

    core::Result<PresentationRecreationStatus> recreation =
        m_state->RecreatePresentationResources();
    if (!recreation)
    {
        ++m_state->frameAcquireFailures;
        return core::Result<RenderFrame>::FromError(std::move(recreation).GetError());
    }

    if (recreation.GetValue() == PresentationRecreationStatus::Pending)
    {
        return makeFrame(FrameStatus::RecreationPending, {});
    }

    if (!m_state->HasUsablePresentationResources())
    {
        return fail(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Active RenderTarget is missing usable Vulkan frame resources."));
    }

    core::Result<detail::VulkanFrameBeginResult> begin = detail::BeginVulkanFrame(
        m_state->deviceState->vulkanDispatch, m_state->deviceState->vulkanDevice,
        m_state->vulkanSwapchain, m_state->vulkanFrameResources, m_state->vulkanPresentationTracker,
        m_state->queuePlan, m_state->nextFrameSlot);

    if (m_state->vulkanPresentationTracker.ConsumeCorePresentationCompletion())
    {
        m_state->RecordCorePresentationCompletion();
    }

    if (!begin)
    {
        core::Error error = std::move(begin).GetError();
        if (IsRenderError(error, RenderErrorCode::DeviceLost))
        {
            m_state->deviceState->RecordDeviceFailure(error);
        }
        else if (IsRenderError(error, RenderErrorCode::SurfaceLost))
        {
            if (core::VoidResult lost = m_state->MarkSurfaceLost(); !lost)
            {
                error = std::move(lost).GetError();
            }
        }
        else if (m_state->vulkanFrameResources.IsPoisoned())
        {
            m_state->MarkBackendRecreationPending(TargetRecreationReason::PresentationChanged);
        }

        return fail(std::move(error));
    }

    detail::VulkanFrameBeginResult beginResult = std::move(begin).GetValue();
    if (beginResult.status == FrameStatus::TimedOut)
    {
        ++m_state->frameTimeouts;
        return makeFrame(FrameStatus::TimedOut, {});
    }

    if (beginResult.status == FrameStatus::RecreationPending)
    {
        m_state->MarkBackendRecreationPending(TargetRecreationReason::PresentationChanged);
        return makeFrame(FrameStatus::RecreationPending, {});
    }

    if (beginResult.status != FrameStatus::Ready || !beginResult.recording.IsActive())
    {
        return fail(MakeRenderError(RenderErrorCode::BackendFailure,
                                    "Vulkan frame begin returned an invalid recording state."));
    }

    const FrameStatus frameStatus = recreation.GetValue() == PresentationRecreationStatus::Completed
                                        ? FrameStatus::Recreated
                                        : FrameStatus::Ready;
    return makeFrame(frameStatus, std::move(beginResult.recording));
}
core::VoidResult RenderTarget::UpdateTargetSnapshot(RenderTargetSnapshot latestSnapshot)
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    const auto fail = [this](core::Error error, std::string_view operation) -> core::VoidResult
    {
        return core::VoidResult::FromError(m_state->RecordFailure(std::move(error), operation));
    };

    if (core::VoidResult update =
            ValidateTargetSnapshotUpdate(m_state->targetSnapshot, latestSnapshot);
        !update)
    {
        return fail(std::move(update).GetError(), "UpdateTargetSnapshot");
    }

    if (m_state->activeFrame)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "Cannot update a render target while a frame is active."),
                    "UpdateTargetSnapshot");
    }

    const RenderTargetSnapshot previousSnapshot = m_state->targetSnapshot;
    const TargetStatus previousStatus = m_state->status;
    const bool alreadyPending = m_state->recreationPending;
    const TargetRecreationInfo previousPending = m_state->pendingRecreation;

    RenderTargetDesc nextDesc = m_state->targetDesc;
    nextDesc.targetSnapshot = latestSnapshot;
    const TargetStatus nextStatus = DetermineTargetStatus(latestSnapshot);

    if (m_state->surfaceLost)
    {
        if (IsSuspendedStatus(nextStatus) && !IsSuspendedStatus(previousStatus))
        {
            ++m_state->suspensionCount;
        }

        m_state->targetDesc = nextDesc;
        m_state->targetSnapshot = latestSnapshot;
        m_state->status = nextStatus;
        m_state->ClearRecreationPending();
        return Success();
    }

    if (IsSuspendedStatus(nextStatus))
    {
        if (core::VoidResult reserve =
                m_state->ReserveRetirementSlotForCurrentPresentationResources();
            !reserve)
        {
            return fail(std::move(reserve).GetError(),
                        "ReserveRetirementSlotForCurrentPresentationResources");
        }

        if (core::VoidResult prepare = m_state->PreparePresentationResourcesForRetirement();
            !prepare)
        {
            return fail(std::move(prepare).GetError(), "PreparePresentationResourcesForRetirement");
        }

        if (core::VoidResult retirement = m_state->RetirePresentationResources(true); !retirement)
        {
            return fail(std::move(retirement).GetError(), "RetirePresentationResources");
        }

        if (!IsSuspendedStatus(previousStatus))
        {
            ++m_state->suspensionCount;
            LOG_INFO_CATEGORY(kRenderLogCategory, "target_suspended target={} status={}",
                              m_state->targetLabel, static_cast<std::uint32_t>(nextStatus));
        }

        m_state->targetDesc = nextDesc;
        m_state->targetSnapshot = latestSnapshot;
        m_state->status = nextStatus;
        m_state->ClearRecreationPending();
        if (core::VoidResult drain = m_state->DrainRetiredPresentationResources(); !drain)
        {
            return fail(std::move(drain).GetError(), "DrainRetiredPresentationResources");
        }

        return Success();
    }

    m_state->targetDesc = nextDesc;
    m_state->targetSnapshot = latestSnapshot;
    m_state->status = nextStatus;

    const bool pixelSizeChanged = latestSnapshot.GetPixelSize() != previousSnapshot.GetPixelSize();
    const bool restored = IsSuspendedStatus(previousStatus);
    const bool presentationEnvironmentChanged =
        latestSnapshot.GetPresentationEnvironmentRevision().GetValue() !=
        previousSnapshot.GetPresentationEnvironmentRevision().GetValue();
    const bool missingPresentationResources = !m_state->HasUsablePresentationResources();
    if (restored || pixelSizeChanged || presentationEnvironmentChanged)
    {
        TargetRecreationReason reason = restored ? TargetRecreationReason::Restored
                                        : pixelSizeChanged
                                            ? TargetRecreationReason::SizeChanged
                                            : TargetRecreationReason::PresentationChanged;
        std::uint64_t previousRevision = previousSnapshot.GetRevision();
        if (alreadyPending && previousPending.previousRevision.has_value() &&
            previousPending.currentRevision.has_value())
        {
            reason = previousPending.reason;
            previousRevision = *previousPending.previousRevision;
        }

        m_state->MarkWindowRecreationPending(reason, previousRevision,
                                             latestSnapshot.GetRevision());
    }
    else if (alreadyPending)
    {
        if (previousPending.previousRevision.has_value() &&
            previousPending.currentRevision.has_value())
        {
            m_state->MarkWindowRecreationPending(previousPending.reason,
                                                 *previousPending.previousRevision,
                                                 latestSnapshot.GetRevision());
        }
        else
        {
            m_state->MarkBackendRecreationPending(previousPending.reason);
        }
    }
    else if (missingPresentationResources)
    {
        m_state->MarkBackendRecreationPending(TargetRecreationReason::PresentationChanged);
    }

    return Success();
}
core::VoidResult RenderTarget::RecoverSurface(PreparedSurface&& preparedSurface)
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    const auto fail = [this, &diagnosticScope](core::Error error, std::string_view operation,
                                               bool recordDeviceFailure = false) -> core::VoidResult
    {
        OptionalBackendDiagnostic nativeDiagnostic = diagnosticScope.TakeLastFailure();
        if (recordDeviceFailure && m_state->deviceState != nullptr)
        {
            m_state->deviceState->RecordDeviceFailure(error, operation, nativeDiagnostic,
                                                      m_state->targetSnapshot.GetWindowId(),
                                                      m_state->targetLabel);
        }
        return core::VoidResult::FromError(
            m_state->RecordFailure(std::move(error), operation, std::move(nativeDiagnostic)));
    };

    if (!m_state->surfaceLost)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "RecoverSurface is only valid after surface loss."),
                    "RecoverSurface");
    }

    if (m_state->activeFrame)
    {
        return fail(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Cannot recover a render target surface while a frame is active."),
            "RecoverSurface");
    }

    if (preparedSurface.m_state == nullptr)
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "RecoverSurface requires a valid prepared surface."),
                    "RecoverSurface");
    }

    if (preparedSurface.m_state->token.registry != m_state->token.registry)
    {
        return fail(
            MakeRenderError(RenderErrorCode::InvalidArgument,
                            "Replacement surface belongs to a different RenderBootstrap than this "
                            "RenderTarget."),
            "RecoverSurface");
    }

    if (core::VoidResult live =
            ValidateLiveRegistry(preparedSurface.m_state->token, "PreparedSurface");
        !live)
    {
        return fail(std::move(live).GetError(), "RecoverSurface");
    }

    if (core::VoidResult surfaceThread = preparedSurface.VerifyRenderThread(); !surfaceThread)
    {
        return fail(std::move(surfaceThread).GetError(), "RecoverSurface");
    }

    if (preparedSurface.m_state->reason != SurfacePreparationReason::SurfaceLossRecovery)
    {
        return fail(
            MakeRenderError(
                RenderErrorCode::InvalidArgument,
                "RecoverSurface requires a surface prepared with SurfaceLossRecovery reason."),
            "RecoverSurface");
    }

    if (!preparedSurface.m_state->vulkanSurface.IsValid())
    {
        return fail(MakeRenderError(RenderErrorCode::InvalidState,
                                    "RecoverSurface requires a live Vulkan replacement surface."),
                    "RecoverSurface");
    }

    const RenderTargetSnapshot replacementSnapshot = preparedSurface.m_state->targetSnapshot;
    if (core::VoidResult snapshot =
            ValidateTargetSnapshotSuccessor(m_state->targetSnapshot, replacementSnapshot, true);
        !snapshot)
    {
        return fail(std::move(snapshot).GetError(), "RecoverSurface");
    }

    core::Result<detail::VulkanDeviceQueuePlan> replacementQueuePlan =
        detail::ValidateVulkanDeviceSurfaceCompatibility(
            m_state->deviceState->vulkanDispatch, m_state->deviceState->vulkanDevice,
            preparedSurface.m_state->vulkanSurface.GetHandle());
    if (!replacementQueuePlan)
    {
        return fail(std::move(replacementQueuePlan).GetError(),
                    "ValidateVulkanDeviceSurfaceCompatibility", true);
    }

    const TargetStatus replacementStatus = DetermineTargetStatus(replacementSnapshot);
    RenderTargetDesc replacementDesc = m_state->targetDesc;
    replacementDesc.targetSnapshot = replacementSnapshot;

    if (replacementStatus == TargetStatus::Active)
    {
        core::Result<detail::VulkanSwapchainConfig> replacementConfig =
            detail::SelectVulkanSwapchainConfig(m_state->deviceState->vulkanDispatch,
                                                m_state->deviceState->vulkanDevice,
                                                preparedSurface.m_state->vulkanSurface.GetHandle(),
                                                replacementQueuePlan.GetValue(), replacementDesc);
        if (!replacementConfig)
        {
            return fail(std::move(replacementConfig).GetError(), "SelectVulkanSwapchainConfig",
                        true);
        }
    }
    else
    {
        core::VoidResult outputCompatibility = detail::ValidateVulkanSurfaceOutputCompatibility(
            m_state->deviceState->vulkanDispatch, m_state->deviceState->vulkanDevice,
            preparedSurface.m_state->vulkanSurface.GetHandle());
        if (!outputCompatibility)
        {
            return fail(std::move(outputCompatibility).GetError(),
                        "ValidateVulkanSurfaceOutputCompatibility", true);
        }
    }

    if (core::VoidResult retired = m_state->MarkSurfaceLost(); !retired)
    {
        return fail(std::move(retired).GetError(), "MarkSurfaceLost");
    }

    m_state->vulkanSurface = std::move(preparedSurface.m_state->vulkanSurface);
    preparedSurface.m_state.reset();
    m_state->queuePlan = std::move(replacementQueuePlan).GetValue();
    m_state->targetDesc.targetSnapshot = replacementSnapshot;
    m_state->targetSnapshot = replacementSnapshot;
    m_state->status = replacementStatus;
    m_state->surfaceLost = false;

    if (m_state->status == TargetStatus::Active)
    {
        m_state->MarkBackendRecreationPending(TargetRecreationReason::SurfaceLost);
    }
    else
    {
        m_state->ClearRecreationPending();
    }

    LOG_INFO_CATEGORY(
        kRenderLogCategory, "surface_recovered target={} window_id={} revision={} status={}",
        m_state->targetLabel, m_state->targetSnapshot.GetWindowId().GetValue(),
        m_state->targetSnapshot.GetRevision(), static_cast<std::uint32_t>(m_state->status));
    return Success();
}
RenderFrame::RenderFrame() noexcept = default;
RenderFrame::RenderFrame(std::unique_ptr<State> state) noexcept : m_state{std::move(state)} {}
RenderFrame::RenderFrame(RenderFrame&& other) noexcept = default;
RenderFrame& RenderFrame::operator=(RenderFrame&& other) noexcept
{
    if (this != &other)
    {
        if (m_state != nullptr)
        {
            m_state->VerifyDestruction();
        }

        m_state = std::move(other.m_state);
    }

    return *this;
}
RenderFrame::~RenderFrame()
{
    if (m_state != nullptr)
    {
        m_state->VerifyDestruction();
    }
}

bool RenderFrame::IsValid() const noexcept
{
    return m_state != nullptr && !m_state->completed;
}

FrameStatus RenderFrame::GetStatus() const noexcept
{
    return m_state == nullptr ? FrameStatus::SkippedSuspended : m_state->status;
}

TargetStatus RenderFrame::GetTargetStatus() const noexcept
{
    return m_state == nullptr ? TargetStatus::Suspended : m_state->targetStatus;
}

RenderFrameMetrics RenderFrame::GetMetrics() const noexcept
{
    if (m_state == nullptr || m_state->completed || m_state->targetState == nullptr)
    {
        return {};
    }

    return m_state->metrics;
}

bool RenderFrame::IsSkipped() const noexcept
{
    return m_state != nullptr && m_state->status == FrameStatus::SkippedSuspended;
}

core::VoidResult RenderFrame::VerifyRenderThread() const
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderFrame is moved-from or empty.");
    }

    if (m_state->completed)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderFrame is already completed.");
    }

    if (m_state->targetState == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderFrame no longer has a valid RenderTarget.");
    }

    if (core::VoidResult live = ValidateLiveRegistry(m_state->targetState->token, "RenderTarget");
        !live)
    {
        return live;
    }

    if (m_state->targetState->deviceState == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderFrame no longer has a valid RenderDevice.");
    }

    if (core::VoidResult deviceLive =
            ValidateLiveRegistry(m_state->targetState->deviceState->token, "RenderDevice");
        !deviceLive)
    {
        return deviceLive;
    }

    if (core::VoidResult usable = m_state->targetState->deviceState->CheckDeviceUsable(); !usable)
    {
        return usable;
    }

    return ValidateCurrentThread(m_state->renderThread, "RenderFrame operation");
}

core::VoidResult RenderFrame::Clear()
{
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    return Clear(m_state->targetState->targetDesc.clearColor);
}

core::VoidResult RenderFrame::Clear(ClearColor clearColor)
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    RenderTarget::State& target = *m_state->targetState;

    if (!pond::render::IsValid(clearColor))
    {
        return core::VoidResult::FromError(
            target.RecordFailure(MakeRenderError(RenderErrorCode::InvalidArgument,
                                                 "RenderFrame clear color is invalid."),
                                 "Clear"));
    }

    if (m_state->status == FrameStatus::SkippedSuspended ||
        m_state->status == FrameStatus::TimedOut ||
        m_state->status == FrameStatus::RecreationPending)
    {
        return Success();
    }

    if (m_state->status != FrameStatus::Ready && m_state->status != FrameStatus::Recreated)
    {
        return core::VoidResult::FromError(target.RecordFailure(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "RenderFrame cannot clear from its current status."),
            "Clear"));
    }

    if (m_state->clearRecorded)
    {
        return core::VoidResult::FromError(target.RecordFailure(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "RenderFrame has already established its clear pass."),
            "Clear"));
    }

    core::VoidResult clear =
        detail::RecordVulkanFrameClear(target.deviceState->vulkanDispatch, target.vulkanSwapchain,
                                       target.vulkanFrameResources, m_state->recording, clearColor);
    if (!clear)
    {
        ++target.frameFailures;
        return core::VoidResult::FromError(
            target.RecordFailure(std::move(clear).GetError(), "RecordVulkanFrameClear"));
    }

    m_state->clearColor = clearColor;
    m_state->clearRecorded = true;
    return Success();
}

core::Result<RenderFrameResult> RenderFrame::FinishAndPresent()
{
    ::pond::render::detail::VulkanDiagnosticScope diagnosticScope;
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderFrameResult>::FromError(std::move(renderThread).GetError());
    }

    auto makeResult = [this](FrameStatus status, bool presented, bool suboptimal) noexcept
    {
        const RenderTargetSnapshot snapshot = m_state->targetState->targetSnapshot;
        return RenderFrameResult{.status = status,
                                 .targetStatus = m_state->targetState->GetEffectiveStatus(),
                                 .windowId = snapshot.GetWindowId(),
                                 .targetRevision = snapshot.GetRevision(),
                                 .swapchainGeneration = m_state->targetState->swapchainGeneration,
                                 .presented = presented,
                                 .suboptimal = suboptimal};
    };

    if (m_state->status == FrameStatus::SkippedSuspended ||
        m_state->status == FrameStatus::TimedOut ||
        m_state->status == FrameStatus::RecreationPending)
    {
        RenderFrameResult result = makeResult(m_state->status, false, false);
        m_state->completed = true;
        m_state.reset();
        return core::Result<RenderFrameResult>::FromValue(result);
    }

    RenderTarget::State& target = *m_state->targetState;
    RenderDevice::State& device = *target.deviceState;

    if (!m_state->clearRecorded)
    {
        ++target.frameFailures;
        return core::Result<RenderFrameResult>::FromError(target.RecordFailure(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "RenderFrame must be cleared before it is presented."),
            "FinishAndPresent"));
    }

    core::Result<detail::VulkanFramePresentationResult> frameResult =
        detail::FinishAndPresentVulkanFrame(device.vulkanDispatch, device.vulkanDevice,
                                            target.vulkanSwapchain, target.vulkanFrameResources,
                                            target.vulkanPresentationTracker, target.queuePlan,
                                            m_state->recording);

    if (!frameResult)
    {
        ++target.frameFailures;
        core::Error error = std::move(frameResult).GetError();
        if (IsRenderError(error, RenderErrorCode::DeviceLost))
        {
            device.RecordDeviceFailure(error);
        }
        else if (IsRenderError(error, RenderErrorCode::SurfaceLost))
        {
            if (core::VoidResult surfaceLost = target.MarkSurfaceLost(); !surfaceLost)
            {
                error = std::move(surfaceLost).GetError();
            }
        }
        else if (target.vulkanFrameResources.IsPoisoned())
        {
            target.MarkBackendRecreationPending(TargetRecreationReason::PresentationChanged);
        }

        error = target.RecordFailure(std::move(error), "FinishAndPresentVulkanFrame");
        m_state->ReleaseActiveFrame();
        m_state->completed = true;
        m_state.reset();
        return core::Result<RenderFrameResult>::FromError(std::move(error));
    }

    const detail::VulkanFramePresentationResult vulkanResult = std::move(frameResult).GetValue();
    if (vulkanResult.presented)
    {
        ++target.framesPresented;
    }

    if (vulkanResult.presented && target.vulkanFrameResources.GetSlotCount() > 0U)
    {
        target.nextFrameSlot =
            (target.nextFrameSlot + 1U) % target.vulkanFrameResources.GetSlotCount();
    }

    if (vulkanResult.status == FrameStatus::RecreationPending ||
        vulkanResult.status == FrameStatus::Suboptimal)
    {
        target.MarkBackendRecreationPending(TargetRecreationReason::PresentationChanged);
    }

    if (core::VoidResult drain = target.DrainRetiredPresentationResources(); !drain)
    {
        ++target.frameFailures;
        m_state->ReleaseActiveFrame();
        m_state->completed = true;
        core::Error error =
            target.RecordFailure(std::move(drain).GetError(), "DrainRetiredPresentationResources");
        m_state.reset();
        return core::Result<RenderFrameResult>::FromError(std::move(error));
    }

    FrameStatus resultStatus = vulkanResult.status;
    if (vulkanResult.presented && m_state->status == FrameStatus::Recreated)
    {
        resultStatus = FrameStatus::Recreated;
    }

    RenderFrameResult result =
        makeResult(resultStatus, vulkanResult.presented, vulkanResult.suboptimal);
    m_state->ReleaseActiveFrame();
    m_state->completed = true;
    m_state.reset();
    return core::Result<RenderFrameResult>::FromValue(result);
}
platform::WindowGraphicsCompatibility GetRequiredWindowGraphicsCompatibility() noexcept
{
    return platform::WindowGraphicsCompatibility::Vulkan;
}
} // namespace pond::render
