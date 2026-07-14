#include <ponder/render/Bootstrap.hpp>

#include "VulkanBootstrap.hpp"

#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/Window.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

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

    void Shutdown() noexcept
    {
        std::scoped_lock lock{mutex};
        shutdown = true;
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
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 std::string{roleName} + " is moved-from or empty.");
    }

    if (token.registry->IsShutdown())
    {
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
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 std::string{operation} + " was called from the wrong thread.");
    }

    return Success();
}

[[nodiscard]] constexpr TargetStatus DetermineTargetStatus(RenderTargetSnapshot snapshot) noexcept
{
    if (snapshot.IsMinimized())
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
    return status != TargetStatus::Active;
}

} // namespace

struct RenderBootstrap::State final
{
    State(std::shared_ptr<ChildRegistry> inputRegistry,
          RenderValidationMode inputValidationMode) noexcept
        : registry{std::move(inputRegistry)}, validationMode{inputValidationMode}
    {
    }

    ~State()
    {
        std::scoped_lock lock{instanceMutex};
        registry->Shutdown();
        vulkanInstance.Reset();
    }

    std::shared_ptr<ChildRegistry> registry;
    std::mutex instanceMutex;
    RenderValidationMode validationMode{RenderValidationMode::Default};
    detail::VulkanGlobalDispatch vulkanDispatch{detail::CreateVolkGlobalDispatch()};
    detail::VulkanInstanceBootstrap vulkanInstance;
};

struct RenderDevice::State final
{
    State(ChildToken inputToken, detail::VulkanGlobalDispatch inputVulkanDispatch,
          detail::VulkanDeviceOwner inputVulkanDevice) noexcept
        : token{std::move(inputToken)},
          renderThread{std::this_thread::get_id()},
          vulkanDispatch{inputVulkanDispatch},
          vulkanDevice{std::move(inputVulkanDevice)}
    {
        const detail::VulkanDeviceInfo info = vulkanDevice.GetInfo();
        selectedAdapter = info.selectedAdapter;
        queuePlan = info.queuePlan;
        optionalCapabilities = info.optionalCapabilities;
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

    ChildToken token;
    std::thread::id renderThread;
    detail::VulkanGlobalDispatch vulkanDispatch{};
    detail::VulkanDeviceOwner vulkanDevice;
    RenderAdapterSnapshot selectedAdapter{};
    RenderDeviceQueuePlan queuePlan{};
    RenderDeviceOptionalCapabilities optionalCapabilities{};
    mutable std::mutex targetMutex;
    mutable std::mutex queueMutex;
    std::uint32_t targetCount{};
};

struct PreparedSurface::State final
{
    State(ChildToken inputToken, RenderTargetSnapshot inputSnapshot) noexcept
        : token{std::move(inputToken)}, targetSnapshot{inputSnapshot}
    {
    }

    State(ChildToken inputToken, RenderTargetSnapshot inputSnapshot,
          detail::VulkanSurfaceOwner inputVulkanSurface) noexcept
        : token{std::move(inputToken)},
          targetSnapshot{inputSnapshot},
          vulkanSurface{std::move(inputVulkanSurface)}
    {
    }

    ChildToken token;
    RenderTargetSnapshot targetSnapshot{};
    std::optional<std::thread::id> renderThread;
    detail::VulkanSurfaceOwner vulkanSurface;
};

struct RenderTarget::State final
{
    State(ChildToken inputToken, std::shared_ptr<RenderDevice::State> inputDeviceState,
          RenderTargetDesc inputDesc, RenderDeviceQueuePlan inputQueuePlan,
          detail::VulkanSurfaceOwner inputVulkanSurface,
          detail::VulkanSwapchainOwner inputVulkanSwapchain,
          detail::VulkanFrameResourcesOwner inputVulkanFrameResources) noexcept
        : token{std::move(inputToken)},
          deviceState{std::move(inputDeviceState)},
          vulkanSurface{std::move(inputVulkanSurface)},
          vulkanSwapchain{std::move(inputVulkanSwapchain)},
          vulkanFrameResources{std::move(inputVulkanFrameResources)},
          targetDesc{inputDesc},
          targetSnapshot{inputDesc.targetSnapshot},
          status{DetermineTargetStatus(inputDesc.targetSnapshot)},
          queuePlan{std::move(inputQueuePlan)},
          swapchainGeneration{vulkanSwapchain.IsValid() ? 1U : 0U},
          renderThread{std::this_thread::get_id()}
    {
    }

    ~State()
    {
        if (deviceState != nullptr)
        {
            if (deviceState->vulkanDevice.IsValid())
            {
                [[maybe_unused]] core::VoidResult idle = deviceState->vulkanDevice.WaitIdle();
            }
            deviceState->ReleaseTarget();
        }
    }

    ChildToken token;
    std::shared_ptr<RenderDevice::State> deviceState;
    detail::VulkanSurfaceOwner vulkanSurface;
    detail::VulkanSwapchainOwner vulkanSwapchain;
    detail::VulkanFrameResourcesOwner vulkanFrameResources;
    RenderTargetDesc targetDesc{};
    RenderTargetSnapshot targetSnapshot{};
    TargetStatus status{TargetStatus::Suspended};
    RenderDeviceQueuePlan queuePlan{};
    std::uint64_t swapchainGeneration{};
    std::uint32_t nextFrameSlot{};
    bool activeFrame{};
    std::thread::id renderThread;
};

struct RenderFrame::State final
{
    State(std::shared_ptr<RenderTarget::State> inputTargetState, FrameStatus inputStatus,
          bool inputHoldsActiveFrame) noexcept
        : targetState{std::move(inputTargetState)},
          status{inputStatus},
          targetStatus{targetState == nullptr ? TargetStatus::Suspended : targetState->status},
          clearColor{targetState == nullptr ? ClearColor{} : targetState->targetDesc.clearColor},
          renderThread{std::this_thread::get_id()},
          holdsActiveFrame{inputHoldsActiveFrame}
    {
    }

    ~State()
    {
        ReleaseActiveFrame();
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
    std::thread::id renderThread;
    bool clearRecorded{};
    bool completed{};
    bool holdsActiveFrame{};
};

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
    if (!pond::render::IsValid(current) || !pond::render::IsValid(next))
    {
        return MakeRenderFailure(RenderErrorCode::InvalidArgument,
                                 "Target snapshot update contains invalid state.");
    }

    if (current.GetWindowId() != next.GetWindowId())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidArgument,
                                 "Target snapshot update references the wrong window.");
    }

    if (next.GetRevision() <= current.GetRevision())
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "Target snapshot update is stale.");
    }

    return Success();
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
RenderBootstrap& RenderBootstrap::operator=(RenderBootstrap&& other) noexcept = default;
RenderBootstrap::~RenderBootstrap() = default;

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

void RenderBootstrap::Shutdown() noexcept
{
    m_state.reset();
}

core::Result<PreparedSurface> RenderBootstrap::PrepareSurface(
    platform::Window& window, const SurfacePreparationDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "PrepareSurface requires a valid RenderBootstrap."));
    }

    if (m_state->registry->IsShutdown())
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "PrepareSurface cannot run after shutdown."));
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
        return core::Result<PreparedSurface>::FromError(std::move(validation).GetError());
    }

    core::Result<platform::NativeWindowHandle> nativeHandleResult = window.GetNativeHandle();
    if (!nativeHandleResult)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedSurface,
            "Window native handle is unavailable for Vulkan surface creation: " +
                std::string{nativeHandleResult.GetError().GetMessage()}));
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
            return core::Result<PreparedSurface>::FromError(std::move(instance).GetError());
        }

        core::Result<detail::VulkanSurfaceOwner> surface =
            detail::CreateVulkanSurfaceForNativeWindow(m_state->vulkanDispatch,
                                                       m_state->vulkanInstance.GetOwner(),
                                                       nativeHandle);
        if (!surface)
        {
            return core::Result<PreparedSurface>::FromError(std::move(surface).GetError());
        }

        vulkanSurface = std::move(surface).GetValue();
    }

    ChildToken token{m_state->registry, ChildKind::PreparedSurface};
    if (!token.active)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Render bootstrap is shutting down."));
    }

    return core::Result<PreparedSurface>::FromValue(PreparedSurface{
        std::make_unique<PreparedSurface::State>(std::move(token), desc.targetSnapshot,
                                                 std::move(vulkanSurface))});
}
core::Result<PreparedSurface> RenderBootstrap::CreatePreparedSurfaceForCompletedSurface(
    const SurfacePreparationDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "CreatePreparedSurfaceForCompletedSurface requires a valid RenderBootstrap."));
    }

    if (m_state->registry->IsShutdown())
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "CreatePreparedSurfaceForCompletedSurface cannot run after shutdown."));
    }

    if (!IsOwnerThread())
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Prepared surface creation must run on the owner thread."));
    }

    if (!pond::render::IsValid(desc))
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Surface preparation descriptor is invalid."));
    }

    ChildToken token{m_state->registry, ChildKind::PreparedSurface};
    if (!token.active)
    {
        return core::Result<PreparedSurface>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Render bootstrap is shutting down."));
    }

    return core::Result<PreparedSurface>::FromValue(PreparedSurface{
        std::make_unique<PreparedSurface::State>(std::move(token), desc.targetSnapshot)});
}

core::Result<RenderAdapterSelection> RenderBootstrap::SelectAdapter(
    const PreparedSurface& firstSurface, const RenderAdapterSelectionDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "SelectAdapter requires a valid RenderBootstrap."));
    }

    if (m_state->registry->IsShutdown())
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "SelectAdapter cannot run after shutdown."));
    }

    if (!pond::render::IsValid(desc))
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Render adapter selection descriptor is invalid."));
    }

    if (firstSurface.m_state == nullptr)
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "SelectAdapter requires a valid first prepared surface."));
    }

    if (firstSurface.m_state->token.registry != m_state->registry)
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "First prepared surface belongs to a different RenderBootstrap."));
    }

    if (core::VoidResult live = ValidateLiveRegistry(firstSurface.m_state->token,
                                                     "PreparedSurface");
        !live)
    {
        return core::Result<RenderAdapterSelection>::FromError(std::move(live).GetError());
    }

    if (core::VoidResult renderThread = firstSurface.VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderAdapterSelection>::FromError(std::move(renderThread).GetError());
    }

    if (!firstSurface.m_state->vulkanSurface.IsValid())
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "SelectAdapter requires a first prepared surface with a live Vulkan surface."));
    }

    std::shared_ptr<detail::VulkanInstanceOwner> instance =
        firstSurface.m_state->vulkanSurface.GetInstanceOwner();
    const VkSurfaceKHR surface = firstSurface.m_state->vulkanSurface.GetHandle();

    std::scoped_lock lock{m_state->instanceMutex};
    return detail::SelectVulkanAdapterForSurface(m_state->vulkanDispatch, std::move(instance),
                                                 surface, desc);
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

    if (m_state->registry->IsShutdown())
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "CreateDevice cannot run after shutdown."));
    }

    if (!pond::render::IsValid(desc))
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render device descriptor is invalid."));
    }

    if (!pond::render::IsValid(adapterSelection))
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render adapter selection is invalid."));
    }

    if (firstSurface.m_state == nullptr)
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "CreateDevice requires a valid first prepared surface."));
    }

    if (firstSurface.m_state->token.registry != m_state->registry)
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "First prepared surface belongs to a different RenderBootstrap."));
    }

    if (core::VoidResult live = ValidateLiveRegistry(firstSurface.m_state->token,
                                                     "PreparedSurface");
        !live)
    {
        return core::Result<RenderDevice>::FromError(std::move(live).GetError());
    }

    if (core::VoidResult renderThread = firstSurface.VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderDevice>::FromError(std::move(renderThread).GetError());
    }

    if (!firstSurface.m_state->vulkanSurface.IsValid())
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "CreateDevice requires a first prepared surface with a live Vulkan surface."));
    }

    std::shared_ptr<detail::VulkanInstanceOwner> instance =
        firstSurface.m_state->vulkanSurface.GetInstanceOwner();
    const VkSurfaceKHR surface = firstSurface.m_state->vulkanSurface.GetHandle();

    core::Result<detail::VulkanDeviceOwner> vulkanDevice =
        core::Result<detail::VulkanDeviceOwner>::FromError(
            MakeRenderError(RenderErrorCode::BackendFailure, "Vulkan device creation did not run."));
    {
        std::scoped_lock lock{m_state->instanceMutex};
        vulkanDevice = detail::CreateVulkanDeviceForAdapterSelection(
            m_state->vulkanDispatch, std::move(instance), surface, adapterSelection, desc);
    }

    if (!vulkanDevice)
    {
        return core::Result<RenderDevice>::FromError(std::move(vulkanDevice).GetError());
    }

    ChildToken token{m_state->registry, ChildKind::Device};
    if (!token.active)
    {
        return core::Result<RenderDevice>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState, "Render bootstrap is shutting down."));
    }

    return core::Result<RenderDevice>::FromValue(RenderDevice{std::make_shared<RenderDevice::State>(
        std::move(token), m_state->vulkanDispatch, std::move(vulkanDevice).GetValue())});
}

core::Result<RenderDevice> RenderBootstrap::CreateDevice(const RenderDeviceDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "CreateDevice requires a valid RenderBootstrap."));
    }

    if (m_state->registry->IsShutdown())
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "CreateDevice cannot run after shutdown."));
    }

    if (!pond::render::IsValid(desc))
    {
        return core::Result<RenderDevice>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render device descriptor is invalid."));
    }

    return core::Result<RenderDevice>::FromError(MakeRenderError(
        RenderErrorCode::InvalidState,
        "CreateDevice requires a transferred first prepared surface and a selected adapter."));
}
RenderDevice::RenderDevice() noexcept = default;
RenderDevice::RenderDevice(std::shared_ptr<State> state) noexcept : m_state{std::move(state)}
{
}
RenderDevice::RenderDevice(RenderDevice&& other) noexcept = default;
RenderDevice& RenderDevice::operator=(RenderDevice&& other) noexcept = default;
RenderDevice::~RenderDevice() = default;

bool RenderDevice::IsValid() const noexcept
{
    return m_state != nullptr && m_state->token.IsActive();
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

    return ValidateCurrentThread(m_state->renderThread, "RenderDevice operation");
}

core::VoidResult RenderDevice::WaitIdle() const
{
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    return m_state->vulkanDevice.WaitIdle();
}

RenderAdapterSnapshot RenderDevice::GetSelectedAdapter() const noexcept
{
    return m_state == nullptr ? RenderAdapterSnapshot{} : m_state->selectedAdapter;
}

RenderDeviceOptionalCapabilities RenderDevice::GetOptionalCapabilities() const noexcept
{
    return m_state == nullptr ? RenderDeviceOptionalCapabilities{} : m_state->optionalCapabilities;
}

RenderDeviceQueuePlan RenderDevice::GetQueuePlan() const noexcept
{
    return m_state == nullptr ? RenderDeviceQueuePlan{} : m_state->queuePlan;
}

std::uint32_t RenderDevice::GetActiveTargetCount() const noexcept
{
    return m_state == nullptr ? 0U : m_state->GetTargetCount();
}

core::Result<RenderTarget> RenderDevice::CreateRenderTarget(PreparedSurface&& preparedSurface,
                                                            const RenderTargetDesc& desc)
{
    if (m_state == nullptr)
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "CreateRenderTarget requires a valid RenderDevice."));
    }

    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderTarget>::FromError(std::move(renderThread).GetError());
    }

    if (!pond::render::IsValid(desc))
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render target descriptor is invalid."));
    }

    if (preparedSurface.m_state == nullptr)
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "CreateRenderTarget requires a valid prepared surface."));
    }

    if (preparedSurface.m_state->token.registry != m_state->token.registry)
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Prepared surface belongs to a different RenderBootstrap than this RenderDevice."));
    }

    if (core::VoidResult live = ValidateLiveRegistry(preparedSurface.m_state->token,
                                                     "PreparedSurface");
        !live)
    {
        return core::Result<RenderTarget>::FromError(std::move(live).GetError());
    }

    if (core::VoidResult surfaceThread = preparedSurface.VerifyRenderThread(); !surfaceThread)
    {
        return core::Result<RenderTarget>::FromError(std::move(surfaceThread).GetError());
    }

    if (!preparedSurface.m_state->vulkanSurface.IsValid())
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "CreateRenderTarget requires a prepared surface with a live Vulkan surface."));
    }

    const RenderTargetSnapshot preparedSnapshot = preparedSurface.m_state->targetSnapshot;
    if (desc.targetSnapshot.GetWindowId() != preparedSnapshot.GetWindowId())
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Render target descriptor references the wrong prepared surface window."));
    }

    if (desc.targetSnapshot.GetRevision() < preparedSnapshot.GetRevision())
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "Render target descriptor is older than the prepared surface snapshot."));
    }

    core::Result<RenderDeviceQueuePlan> targetQueuePlan =
        detail::ValidateVulkanDeviceSurfaceCompatibility(
            m_state->vulkanDispatch, m_state->vulkanDevice,
            preparedSurface.m_state->vulkanSurface.GetHandle());
    if (!targetQueuePlan)
    {
        return core::Result<RenderTarget>::FromError(std::move(targetQueuePlan).GetError());
    }

    detail::VulkanSwapchainOwner vulkanSwapchain;
    detail::VulkanFrameResourcesOwner vulkanFrameResources;
    if (DetermineTargetStatus(desc.targetSnapshot) == TargetStatus::Active)
    {
        core::Result<detail::VulkanSwapchainOwner> swapchain =
            detail::CreateVulkanSwapchainForTarget(
                m_state->vulkanDispatch, m_state->vulkanDevice,
                preparedSurface.m_state->vulkanSurface.GetHandle(), targetQueuePlan.GetValue(),
                desc);
        if (!swapchain)
        {
            return core::Result<RenderTarget>::FromError(std::move(swapchain).GetError());
        }

        vulkanSwapchain = std::move(swapchain).GetValue();

        core::Result<detail::VulkanFrameResourcesOwner> frameResources =
            detail::CreateVulkanFrameResourcesForTarget(
                m_state->vulkanDispatch, m_state->vulkanDevice, targetQueuePlan.GetValue(),
                vulkanSwapchain.GetConfig().presentation.queuedLatency);
        if (!frameResources)
        {
            return core::Result<RenderTarget>::FromError(std::move(frameResources).GetError());
        }

        vulkanFrameResources = std::move(frameResources).GetValue();
    }

    if (!m_state->TryRegisterTarget())
    {
        return core::Result<RenderTarget>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "RenderDevice cannot create targets after shutdown."));
    }

    ChildToken targetToken{preparedSurface.m_state->token.registry, ChildKind::Target};
    if (!targetToken.active)
    {
        m_state->ReleaseTarget();
        return core::Result<RenderTarget>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState, "Render bootstrap is shutting down."));
    }

    detail::VulkanSurfaceOwner vulkanSurface = std::move(preparedSurface.m_state->vulkanSurface);
    preparedSurface.m_state.reset();

    return core::Result<RenderTarget>::FromValue(RenderTarget{std::make_shared<RenderTarget::State>(
        std::move(targetToken), m_state, desc, std::move(targetQueuePlan).GetValue(),
        std::move(vulkanSurface), std::move(vulkanSwapchain), std::move(vulkanFrameResources))});
}

PreparedSurface::PreparedSurface() noexcept = default;
PreparedSurface::PreparedSurface(std::unique_ptr<State> state) noexcept : m_state{std::move(state)}
{
}
PreparedSurface::PreparedSurface(PreparedSurface&& other) noexcept = default;
PreparedSurface& PreparedSurface::operator=(PreparedSurface&& other) noexcept = default;
PreparedSurface::~PreparedSurface() = default;

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

    if (core::VoidResult update = ValidateTargetSnapshotUpdate(m_state->targetSnapshot,
                                                               latestSnapshot);
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
RenderTarget::RenderTarget(std::shared_ptr<State> state) noexcept : m_state{std::move(state)}
{
}
RenderTarget::RenderTarget(RenderTarget&& other) noexcept = default;
RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept = default;
RenderTarget::~RenderTarget() = default;

bool RenderTarget::IsValid() const noexcept
{
    return m_state != nullptr && m_state->token.IsActive();
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

    if (core::VoidResult deviceLive = ValidateLiveRegistry(m_state->deviceState->token,
                                                           "RenderDevice");
        !deviceLive)
    {
        return deviceLive;
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
    return m_state == nullptr ? TargetStatus::Suspended : m_state->status;
}

bool RenderTarget::IsSuspended() const noexcept
{
    return m_state != nullptr && IsSuspendedStatus(m_state->status);
}

bool RenderTarget::HasSwapchain() const noexcept
{
    return m_state != nullptr && m_state->vulkanSwapchain.IsValid();
}

std::uint64_t RenderTarget::GetSwapchainGeneration() const noexcept
{
    return HasSwapchain() ? m_state->swapchainGeneration : 0U;
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

core::Result<RenderFrame> RenderTarget::AcquireFrame()
{
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderFrame>::FromError(std::move(renderThread).GetError());
    }

    if (m_state->activeFrame)
    {
        return core::Result<RenderFrame>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "RenderTarget already has an active frame."));
    }

    if (IsSuspendedStatus(m_state->status))
    {
        return core::Result<RenderFrame>::FromValue(RenderFrame{std::make_unique<RenderFrame::State>(
            m_state, FrameStatus::SkippedSuspended, false)});
    }

    if (!m_state->vulkanSwapchain.IsValid() || !m_state->vulkanFrameResources.IsValid())
    {
        return core::Result<RenderFrame>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Active RenderTarget is missing Vulkan frame resources."));
    }

    m_state->activeFrame = true;
    return core::Result<RenderFrame>::FromValue(RenderFrame{
        std::make_unique<RenderFrame::State>(m_state, FrameStatus::Ready, true)});
}

core::VoidResult RenderTarget::UpdateTargetSnapshot(RenderTargetSnapshot latestSnapshot)
{
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    if (core::VoidResult update = ValidateTargetSnapshotUpdate(m_state->targetSnapshot,
                                                               latestSnapshot);
        !update)
    {
        return update;
    }

    if (m_state->activeFrame)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "Cannot update a render target while a frame is active.");
    }

    RenderTargetDesc nextDesc = m_state->targetDesc;
    nextDesc.targetSnapshot = latestSnapshot;
    const TargetStatus nextStatus = DetermineTargetStatus(latestSnapshot);

    detail::VulkanSwapchainOwner replacementSwapchain;
    detail::VulkanFrameResourcesOwner replacementFrameResources;
    const bool shouldCreateSwapchain = nextStatus == TargetStatus::Active;
    if (shouldCreateSwapchain)
    {
        core::Result<detail::VulkanSwapchainOwner> swapchain =
            detail::CreateVulkanSwapchainForTarget(
                m_state->deviceState->vulkanDispatch, m_state->deviceState->vulkanDevice,
                m_state->vulkanSurface.GetHandle(), m_state->queuePlan, nextDesc);
        if (!swapchain)
        {
            return core::VoidResult::FromError(std::move(swapchain).GetError());
        }

        replacementSwapchain = std::move(swapchain).GetValue();

        core::Result<detail::VulkanFrameResourcesOwner> frameResources =
            detail::CreateVulkanFrameResourcesForTarget(
                m_state->deviceState->vulkanDispatch, m_state->deviceState->vulkanDevice,
                m_state->queuePlan, replacementSwapchain.GetConfig().presentation.queuedLatency);
        if (!frameResources)
        {
            return core::VoidResult::FromError(std::move(frameResources).GetError());
        }

        replacementFrameResources = std::move(frameResources).GetValue();
    }

    if (m_state->deviceState != nullptr && m_state->deviceState->vulkanDevice.IsValid())
    {
        if (core::VoidResult idle = m_state->deviceState->vulkanDevice.WaitIdle(); !idle)
        {
            return idle;
        }
    }

    m_state->targetDesc = nextDesc;
    m_state->targetSnapshot = latestSnapshot;
    m_state->status = nextStatus;

    if (shouldCreateSwapchain)
    {
        m_state->vulkanFrameResources = std::move(replacementFrameResources);
        m_state->vulkanSwapchain = std::move(replacementSwapchain);
        m_state->nextFrameSlot = 0U;
        ++m_state->swapchainGeneration;
    }
    else
    {
        m_state->vulkanFrameResources.Reset();
        m_state->vulkanSwapchain.Reset();
        m_state->nextFrameSlot = 0U;
    }

    return Success();
}

RenderFrame::RenderFrame() noexcept = default;
RenderFrame::RenderFrame(std::unique_ptr<State> state) noexcept : m_state{std::move(state)}
{
}
RenderFrame::RenderFrame(RenderFrame&& other) noexcept = default;
RenderFrame& RenderFrame::operator=(RenderFrame&& other) noexcept = default;
RenderFrame::~RenderFrame() = default;

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

bool RenderFrame::IsSkipped() const noexcept
{
    return m_state != nullptr && m_state->status == FrameStatus::SkippedSuspended;
}

core::VoidResult RenderFrame::VerifyRenderThread() const
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState, "RenderFrame is moved-from or empty.");
    }

    if (m_state->completed)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState, "RenderFrame is already completed.");
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

    if (core::VoidResult deviceLive = ValidateLiveRegistry(
            m_state->targetState->deviceState->token, "RenderDevice");
        !deviceLive)
    {
        return deviceLive;
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
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return renderThread;
    }

    if (!pond::render::IsValid(clearColor))
    {
        return MakeRenderFailure(RenderErrorCode::InvalidArgument, "RenderFrame clear color is invalid.");
    }

    if (m_state->status == FrameStatus::SkippedSuspended)
    {
        return Success();
    }

    if (m_state->status != FrameStatus::Ready)
    {
        return MakeRenderFailure(RenderErrorCode::InvalidState,
                                 "RenderFrame cannot clear from its current status.");
    }

    m_state->clearColor = clearColor;
    m_state->clearRecorded = true;
    return Success();
}

core::Result<RenderFrameResult> RenderFrame::FinishAndPresent()
{
    if (core::VoidResult renderThread = VerifyRenderThread(); !renderThread)
    {
        return core::Result<RenderFrameResult>::FromError(std::move(renderThread).GetError());
    }

    auto makeResult = [this](FrameStatus status, bool presented, bool suboptimal) noexcept
    {
        const RenderTargetSnapshot snapshot = m_state->targetState->targetSnapshot;
        return RenderFrameResult{.status = status,
                                 .targetStatus = m_state->targetState->status,
                                 .windowId = snapshot.GetWindowId(),
                                 .targetRevision = snapshot.GetRevision(),
                                 .swapchainGeneration = m_state->targetState->swapchainGeneration,
                                 .presented = presented,
                                 .suboptimal = suboptimal};
    };

    if (m_state->status == FrameStatus::SkippedSuspended)
    {
        RenderFrameResult result = makeResult(FrameStatus::SkippedSuspended, false, false);
        m_state->completed = true;
        m_state.reset();
        return core::Result<RenderFrameResult>::FromValue(result);
    }

    if (!m_state->clearRecorded)
    {
        return core::Result<RenderFrameResult>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "RenderFrame must be cleared before it is presented."));
    }

    RenderTarget::State& target = *m_state->targetState;
    RenderDevice::State& device = *target.deviceState;
    const std::uint32_t frameSlot = target.nextFrameSlot;

    core::Result<detail::VulkanFramePresentationResult> frameResult =
        [&]() -> core::Result<detail::VulkanFramePresentationResult>
    {
        std::scoped_lock lock{device.queueMutex};
        return detail::ClearSubmitAndPresentVulkanFrame(
            device.vulkanDispatch, device.vulkanDevice, target.vulkanSwapchain,
            target.vulkanFrameResources, target.queuePlan, frameSlot, m_state->clearColor);
    }();

    if (!frameResult)
    {
        m_state->ReleaseActiveFrame();
        m_state->completed = true;
        core::Error error = std::move(frameResult).GetError();
        m_state.reset();
        return core::Result<RenderFrameResult>::FromError(std::move(error));
    }

    const detail::VulkanFramePresentationResult vulkanResult = std::move(frameResult).GetValue();
    if (vulkanResult.presented && target.vulkanFrameResources.GetSlotCount() > 0U)
    {
        target.nextFrameSlot = (target.nextFrameSlot + 1U) % target.vulkanFrameResources.GetSlotCount();
    }

    RenderFrameResult result = makeResult(vulkanResult.status, vulkanResult.presented,
                                          vulkanResult.suboptimal);
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


