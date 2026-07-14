#pragma once

#include <ponder/core/Numbers.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/WindowGraphics.hpp>
#include <ponder/render/RenderError.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pond::platform
{
class Window;
}

namespace pond::render
{
inline constexpr std::string_view kExperimentalApiNotice{
    "pond::render is experimental during early development and does not promise ABI or source "
    "compatibility."};

enum class RenderBackendKind : std::uint8_t
{
    Unknown = 0,
    Vulkan = 1
};

enum class RenderAdapterType : std::uint8_t
{
    Unknown = 0,
    IntegratedGpu = 1,
    DiscreteGpu = 2,
    VirtualGpu = 3,
    Cpu = 4
};

enum class RenderAdapterPreference : std::uint8_t
{
    Default = 0,
    HighPerformance = 1,
    LowPower = 2,
    Software = 3
};

enum class RenderValidationMode : std::uint8_t
{
    Default = 0,
    Disabled = 1,
    Standard = 2,
    Enabled = Standard,
    Synchronization = 3,
    BestPractices = 4,
    GpuAssisted = 5
};

enum class PresentationPolicy : std::uint8_t
{
    Default = 0,
    Fifo = 1,
    PreferMailbox = 2,
    PreferImmediate = 3
};

enum class SelectedPresentMode : std::uint8_t
{
    Fifo = 0,
    Mailbox = 1,
    Immediate = 2
};

enum class SelectedCompositeAlpha : std::uint8_t
{
    Opaque = 0,
    PreMultiplied = 1,
    PostMultiplied = 2,
    Inherit = 3
};

enum class TargetStatus : std::uint8_t
{
    Active = 0,
    Hidden = 1,
    Minimized = 2,
    Suspended = 3
};

enum class FrameStatus : std::uint8_t
{
    Ready = 0,
    SkippedSuspended = 1,
    TimedOut = 2,
    Presented = 3,
    Suboptimal = 4,
    Recreated = 5
};

enum class TargetRecreationReason : std::uint8_t
{
    None = 0,
    SizeChanged = 1,
    Restored = 2,
    SurfaceLost = 3,
    PresentationChanged = 4
};

enum class SurfacePreparationReason : std::uint8_t
{
    Initial = 0,
    SurfaceLossRecovery = 1
};

struct QueuedFrameLatency final
{
    static constexpr std::uint32_t kMinimumFrames = 1;
    static constexpr std::uint32_t kDefaultFrames = 2;
    static constexpr std::uint32_t kMaximumFrames = 3;

    std::uint32_t frameCount{kDefaultFrames};

    [[nodiscard]] friend constexpr bool operator==(const QueuedFrameLatency& lhs,
                                                   const QueuedFrameLatency& rhs) noexcept =
        default;
};

struct RenderAdapterIdentity final
{
    std::uint32_t vendorId{};
    std::uint32_t deviceId{};
    RenderAdapterType adapterType{RenderAdapterType::Unknown};
    std::string name{};

    [[nodiscard]] friend bool operator==(const RenderAdapterIdentity& lhs,
                                         const RenderAdapterIdentity& rhs) = default;
};

struct RenderAdapterId final
{
    static constexpr std::size_t kUuidSize = 16;
    static constexpr std::size_t kLuidSize = 8;

    std::array<std::uint8_t, kUuidSize> deviceUuid{};
    std::array<std::uint8_t, kLuidSize> deviceLuid{};
    std::uint32_t deviceNodeMask{};
    bool deviceLuidValid{};

    [[nodiscard]] friend constexpr bool operator==(const RenderAdapterId& lhs,
                                                   const RenderAdapterId& rhs) noexcept = default;
};

struct RenderAdapterDriverIdentity final
{
    std::uint32_t driverVersion{};
    std::string driverName{};
    std::string driverInfo{};

    [[nodiscard]] friend bool operator==(const RenderAdapterDriverIdentity& lhs,
                                         const RenderAdapterDriverIdentity& rhs) = default;
};

struct RenderAdapterMemoryHeap final
{
    std::uint64_t sizeBytes{};
    bool deviceLocal{};

    [[nodiscard]] friend constexpr bool operator==(const RenderAdapterMemoryHeap& lhs,
                                                   const RenderAdapterMemoryHeap& rhs) noexcept =
        default;
};

struct RenderAdapterLimits final
{
    std::uint32_t maxImageDimension2D{};
    std::uint32_t maxFramebufferWidth{};
    std::uint32_t maxFramebufferHeight{};
    std::uint32_t maxColorAttachments{};

    [[nodiscard]] friend constexpr bool operator==(const RenderAdapterLimits& lhs,
                                                   const RenderAdapterLimits& rhs) noexcept = default;
};

struct RenderQueueFamilySnapshot final
{
    std::uint32_t familyIndex{};
    std::uint32_t queueCount{};
    bool supportsGraphics{};
    bool supportsPresentation{};

    [[nodiscard]] friend constexpr bool operator==(const RenderQueueFamilySnapshot& lhs,
                                                   const RenderQueueFamilySnapshot& rhs) noexcept =
        default;
};

struct RenderSurfaceFormatSnapshot final
{
    std::int64_t formatCode{};
    std::string formatName{};
    std::int64_t colorSpaceCode{};
    std::string colorSpaceName{};

    [[nodiscard]] friend bool operator==(const RenderSurfaceFormatSnapshot& lhs,
                                         const RenderSurfaceFormatSnapshot& rhs) = default;
};

struct RenderAdapterSnapshot final
{
    RenderAdapterId adapterId{};
    RenderBackendKind backend{RenderBackendKind::Unknown};
    std::uint32_t apiVersion{};
    RenderAdapterIdentity identity{};
    RenderAdapterDriverIdentity driver{};
    RenderAdapterLimits limits{};
    std::vector<RenderAdapterMemoryHeap> memoryHeaps{};
    std::vector<RenderQueueFamilySnapshot> queueFamilies{};
    std::vector<RenderSurfaceFormatSnapshot> surfaceFormats{};
    std::vector<SelectedPresentMode> presentModes{};
    bool supportsSwapchain{};
    bool supportsSurfacePresentation{};

    [[nodiscard]] friend bool operator==(const RenderAdapterSnapshot& lhs,
                                         const RenderAdapterSnapshot& rhs) = default;
};

struct RenderAdapterRejection final
{
    RenderAdapterId adapterId{};
    RenderAdapterIdentity identity{};
    std::vector<std::string> reasons{};

    [[nodiscard]] friend bool operator==(const RenderAdapterRejection& lhs,
                                         const RenderAdapterRejection& rhs) = default;
};

struct RenderAdapterSelectionDesc final
{
    RenderAdapterPreference adapterPreference{RenderAdapterPreference::Default};
    std::optional<RenderAdapterId> explicitAdapterId{};

    [[nodiscard]] friend bool operator==(const RenderAdapterSelectionDesc& lhs,
                                         const RenderAdapterSelectionDesc& rhs) = default;
};

struct RenderAdapterSelection final
{
    RenderAdapterSelectionDesc request{};
    RenderAdapterSnapshot selectedAdapter{};
    std::vector<RenderAdapterSnapshot> compatibleAdapters{};
    std::vector<RenderAdapterRejection> rejectedAdapters{};
    bool selectedByPreferenceFallback{};

    [[nodiscard]] friend bool operator==(const RenderAdapterSelection& lhs,
                                         const RenderAdapterSelection& rhs) = default;
};

struct RenderDeviceOptionalCapabilities final
{
    bool swapchainMaintenance1{};
    bool presentId{};
    bool presentWait{};
    bool vmaAllocator{};

    [[nodiscard]] friend constexpr bool operator==(
        const RenderDeviceOptionalCapabilities& lhs,
        const RenderDeviceOptionalCapabilities& rhs) noexcept = default;
};

struct RenderDeviceQueuePlan final
{
    std::uint32_t graphicsQueueFamilyIndex{};
    std::uint32_t presentationQueueFamilyIndex{};
    std::vector<std::uint32_t> provisionedQueueFamilyIndices{};
    bool usesDistinctPresentationQueue{};

    [[nodiscard]] friend bool operator==(const RenderDeviceQueuePlan& lhs,
                                         const RenderDeviceQueuePlan& rhs) = default;
};

struct ClearColor final
{
    // Components are linear floating-point values. When the selected target format is sRGB, the
    // framebuffer performs the corresponding encoding during presentation.
    float red{};
    float green{};
    float blue{};
    float alpha{1.0F};

    [[nodiscard]] friend constexpr bool operator==(const ClearColor& lhs,
                                                   const ClearColor& rhs) noexcept = default;
};

class RenderTargetSnapshot final
{
public:
    constexpr RenderTargetSnapshot() noexcept = default;
    constexpr RenderTargetSnapshot(platform::WindowId windowId, platform::PixelSize pixelSize,
                                   bool visible, bool minimized, bool restored,
                                   std::uint64_t revision) noexcept
        : m_windowId{windowId},
          m_pixelSize{pixelSize},
          m_visible{visible},
          m_minimized{minimized},
          m_restored{restored},
          m_revision{revision}
    {
    }

    [[nodiscard]] constexpr platform::WindowId GetWindowId() const noexcept
    {
        return m_windowId;
    }

    [[nodiscard]] constexpr platform::PixelSize GetPixelSize() const noexcept
    {
        return m_pixelSize;
    }

    [[nodiscard]] constexpr bool IsVisible() const noexcept
    {
        return m_visible;
    }

    [[nodiscard]] constexpr bool IsMinimized() const noexcept
    {
        return m_minimized;
    }

    [[nodiscard]] constexpr bool IsRestored() const noexcept
    {
        return m_restored;
    }

    [[nodiscard]] constexpr std::uint64_t GetRevision() const noexcept
    {
        return m_revision;
    }

    [[nodiscard]] friend constexpr bool operator==(const RenderTargetSnapshot& lhs,
                                                   const RenderTargetSnapshot& rhs) noexcept =
        default;

private:
    platform::WindowId m_windowId{};
    platform::PixelSize m_pixelSize{};
    bool m_visible{};
    bool m_minimized{};
    bool m_restored{};
    std::uint64_t m_revision{};
};

struct SelectedPresentationConfig final
{
    PresentationPolicy requestedPolicy{PresentationPolicy::Default};
    SelectedPresentMode selectedMode{SelectedPresentMode::Fifo};
    RenderSurfaceFormatSnapshot surfaceFormat{};
    SelectedCompositeAlpha compositeAlpha{SelectedCompositeAlpha::Opaque};
    platform::PixelSize pixelExtent{};
    QueuedFrameLatency queuedLatency{};
    bool optionalPreferenceUnavailable{};
    bool queuedLatencyLimitedBySurface{};

    [[nodiscard]] friend bool operator==(const SelectedPresentationConfig& lhs,
                                         const SelectedPresentationConfig& rhs) = default;
};

struct TargetRecreationInfo final
{
    TargetRecreationReason reason{TargetRecreationReason::None};
    std::uint64_t previousRevision{};
    std::uint64_t currentRevision{};

    [[nodiscard]] friend constexpr bool operator==(const TargetRecreationInfo& lhs,
                                                   const TargetRecreationInfo& rhs) noexcept =
        default;
};

struct BackendDiagnostic final
{
    RenderBackendKind backend{RenderBackendKind::Unknown};
    std::int64_t nativeCode{};
    std::string symbolicName{};
    std::string operation{};
    std::string validationContext{};

    [[nodiscard]] friend bool operator==(const BackendDiagnostic& lhs,
                                         const BackendDiagnostic& rhs) = default;
};

using OptionalBackendDiagnostic = std::optional<BackendDiagnostic>;

struct RenderBootstrapDesc final
{
    RenderValidationMode validationMode{RenderValidationMode::Default};

    [[nodiscard]] friend constexpr bool operator==(const RenderBootstrapDesc& lhs,
                                                   const RenderBootstrapDesc& rhs) noexcept =
        default;
};

struct RenderDeviceDesc final
{
    RenderAdapterPreference adapterPreference{RenderAdapterPreference::Default};

    [[nodiscard]] friend constexpr bool operator==(const RenderDeviceDesc& lhs,
                                                   const RenderDeviceDesc& rhs) noexcept =
        default;
};

struct SurfacePreparationDesc final
{
    RenderTargetSnapshot targetSnapshot{};
    SurfacePreparationReason reason{SurfacePreparationReason::Initial};

    [[nodiscard]] friend constexpr bool operator==(const SurfacePreparationDesc& lhs,
                                                   const SurfacePreparationDesc& rhs) noexcept =
        default;
};

struct RenderTargetDesc final
{
    RenderTargetSnapshot targetSnapshot{};
    PresentationPolicy presentationPolicy{PresentationPolicy::Default};
    QueuedFrameLatency queuedLatency{};
    ClearColor clearColor{};

    [[nodiscard]] friend constexpr bool operator==(const RenderTargetDesc& lhs,
                                                   const RenderTargetDesc& rhs) noexcept =
        default;
};

struct RenderFrameResult final
{
    FrameStatus status{FrameStatus::SkippedSuspended};
    TargetStatus targetStatus{TargetStatus::Suspended};
    platform::WindowId windowId{};
    std::uint64_t targetRevision{};
    std::uint64_t swapchainGeneration{};
    bool presented{};
    bool suboptimal{};

    [[nodiscard]] friend constexpr bool operator==(const RenderFrameResult& lhs,
                                                   const RenderFrameResult& rhs) noexcept =
        default;
};

[[nodiscard]] constexpr bool IsValid(RenderBackendKind value) noexcept
{
    switch (value)
    {
    case RenderBackendKind::Unknown:
    case RenderBackendKind::Vulkan:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(RenderAdapterType value) noexcept
{
    switch (value)
    {
    case RenderAdapterType::Unknown:
    case RenderAdapterType::IntegratedGpu:
    case RenderAdapterType::DiscreteGpu:
    case RenderAdapterType::VirtualGpu:
    case RenderAdapterType::Cpu:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(RenderAdapterPreference value) noexcept
{
    switch (value)
    {
    case RenderAdapterPreference::Default:
    case RenderAdapterPreference::HighPerformance:
    case RenderAdapterPreference::LowPower:
    case RenderAdapterPreference::Software:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(RenderValidationMode value) noexcept
{
    switch (value)
    {
    case RenderValidationMode::Default:
    case RenderValidationMode::Disabled:
    case RenderValidationMode::Standard:
    case RenderValidationMode::Synchronization:
    case RenderValidationMode::BestPractices:
    case RenderValidationMode::GpuAssisted:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(PresentationPolicy value) noexcept
{
    switch (value)
    {
    case PresentationPolicy::Default:
    case PresentationPolicy::Fifo:
    case PresentationPolicy::PreferMailbox:
    case PresentationPolicy::PreferImmediate:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(SelectedPresentMode value) noexcept
{
    switch (value)
    {
    case SelectedPresentMode::Fifo:
    case SelectedPresentMode::Mailbox:
    case SelectedPresentMode::Immediate:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(SelectedCompositeAlpha value) noexcept
{
    switch (value)
    {
    case SelectedCompositeAlpha::Opaque:
    case SelectedCompositeAlpha::PreMultiplied:
    case SelectedCompositeAlpha::PostMultiplied:
    case SelectedCompositeAlpha::Inherit:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(TargetStatus value) noexcept
{
    switch (value)
    {
    case TargetStatus::Active:
    case TargetStatus::Hidden:
    case TargetStatus::Minimized:
    case TargetStatus::Suspended:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(FrameStatus value) noexcept
{
    switch (value)
    {
    case FrameStatus::Ready:
    case FrameStatus::SkippedSuspended:
    case FrameStatus::TimedOut:
    case FrameStatus::Presented:
    case FrameStatus::Suboptimal:
    case FrameStatus::Recreated:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(TargetRecreationReason value) noexcept
{
    switch (value)
    {
    case TargetRecreationReason::None:
    case TargetRecreationReason::SizeChanged:
    case TargetRecreationReason::Restored:
    case TargetRecreationReason::SurfaceLost:
    case TargetRecreationReason::PresentationChanged:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(SurfacePreparationReason value) noexcept
{
    switch (value)
    {
    case SurfacePreparationReason::Initial:
    case SurfacePreparationReason::SurfaceLossRecovery:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(QueuedFrameLatency value) noexcept
{
    return value.frameCount >= QueuedFrameLatency::kMinimumFrames &&
           value.frameCount <= QueuedFrameLatency::kMaximumFrames;
}

[[nodiscard]] inline bool IsValid(const RenderAdapterIdentity& value) noexcept
{
    return IsValid(value.adapterType);
}

[[nodiscard]] constexpr bool IsValid(RenderAdapterId value) noexcept
{
    bool hasDeviceUuid{};
    for (const std::uint8_t valueByte : value.deviceUuid)
    {
        hasDeviceUuid = hasDeviceUuid || valueByte != 0U;
    }

    bool hasDeviceLuid{};
    for (const std::uint8_t valueByte : value.deviceLuid)
    {
        hasDeviceLuid = hasDeviceLuid || valueByte != 0U;
    }

    return hasDeviceUuid || (value.deviceLuidValid && hasDeviceLuid);
}

[[nodiscard]] inline bool IsValid(const RenderAdapterDriverIdentity&) noexcept
{
    return true;
}

[[nodiscard]] constexpr bool IsValid(RenderAdapterMemoryHeap value) noexcept
{
    return value.sizeBytes > 0U;
}

[[nodiscard]] constexpr bool IsValid(RenderAdapterLimits value) noexcept
{
    return value.maxImageDimension2D > 0U && value.maxFramebufferWidth > 0U &&
           value.maxFramebufferHeight > 0U && value.maxColorAttachments > 0U;
}

[[nodiscard]] constexpr bool IsValid(RenderQueueFamilySnapshot value) noexcept
{
    return value.queueCount > 0U;
}

[[nodiscard]] inline bool IsValid(const RenderSurfaceFormatSnapshot& value) noexcept
{
    return !value.formatName.empty() && !value.colorSpaceName.empty();
}

[[nodiscard]] inline bool IsValid(const RenderAdapterSnapshot& value) noexcept
{
    return IsValid(value.adapterId) && IsValid(value.backend) && value.apiVersion != 0U &&
           IsValid(value.identity) && IsValid(value.driver) && IsValid(value.limits) &&
           !value.queueFamilies.empty() && !value.surfaceFormats.empty() &&
           !value.presentModes.empty() && value.supportsSwapchain &&
           value.supportsSurfacePresentation;
}

[[nodiscard]] inline bool IsValid(const RenderAdapterRejection& value) noexcept
{
    return IsValid(value.adapterId) && IsValid(value.identity) && !value.reasons.empty();
}

[[nodiscard]] inline bool IsValid(const RenderAdapterSelectionDesc& value) noexcept
{
    return IsValid(value.adapterPreference) &&
           (!value.explicitAdapterId.has_value() || IsValid(*value.explicitAdapterId));
}

[[nodiscard]] inline bool IsValid(const RenderAdapterSelection& value) noexcept
{
    return IsValid(value.request) && IsValid(value.selectedAdapter) &&
           !value.compatibleAdapters.empty();
}

[[nodiscard]] constexpr bool IsValid(RenderDeviceOptionalCapabilities) noexcept
{
    return true;
}

[[nodiscard]] inline bool IsValid(const RenderDeviceQueuePlan& value) noexcept
{
    if (value.provisionedQueueFamilyIndices.empty())
    {
        return false;
    }

    bool hasGraphicsQueue{};
    bool hasPresentationQueue{};
    for (std::size_t index = 0; index < value.provisionedQueueFamilyIndices.size(); ++index)
    {
        const std::uint32_t familyIndex = value.provisionedQueueFamilyIndices[index];
        hasGraphicsQueue = hasGraphicsQueue || familyIndex == value.graphicsQueueFamilyIndex;
        hasPresentationQueue = hasPresentationQueue ||
                               familyIndex == value.presentationQueueFamilyIndex;

        for (std::size_t laterIndex = index + 1U;
             laterIndex < value.provisionedQueueFamilyIndices.size(); ++laterIndex)
        {
            if (familyIndex == value.provisionedQueueFamilyIndices[laterIndex])
            {
                return false;
            }
        }
    }

    return hasGraphicsQueue && hasPresentationQueue &&
           value.usesDistinctPresentationQueue ==
               (value.graphicsQueueFamilyIndex != value.presentationQueueFamilyIndex);
}

[[nodiscard]] constexpr bool IsValid(ClearColor value) noexcept
{
    const auto isUnitInterval = [](float component) constexpr noexcept
    {
        return core::IsFinite(component) && component >= 0.0F && component <= 1.0F;
    };

    return isUnitInterval(value.red) && isUnitInterval(value.green) &&
           isUnitInterval(value.blue) && isUnitInterval(value.alpha);
}

[[nodiscard]] constexpr bool IsValid(RenderTargetSnapshot value) noexcept
{
    if (!value.GetWindowId().IsValid() || value.GetRevision() == 0U)
    {
        return false;
    }

    if (value.IsMinimized() == value.IsRestored())
    {
        return false;
    }

    return true;
}

[[nodiscard]] inline bool IsValid(const SelectedPresentationConfig& value) noexcept
{
    return IsValid(value.requestedPolicy) && IsValid(value.selectedMode) &&
           IsValid(value.surfaceFormat) && IsValid(value.compositeAlpha) &&
           value.pixelExtent.width > 0U && value.pixelExtent.height > 0U &&
           IsValid(value.queuedLatency);
}

[[nodiscard]] constexpr bool IsValid(TargetRecreationInfo value) noexcept
{
    if (!IsValid(value.reason))
    {
        return false;
    }

    if (value.reason == TargetRecreationReason::None)
    {
        return value.previousRevision == 0U && value.currentRevision == 0U;
    }

    return value.currentRevision > value.previousRevision;
}

[[nodiscard]] inline bool IsValid(const BackendDiagnostic& value) noexcept
{
    return IsValid(value.backend);
}

[[nodiscard]] constexpr bool IsValid(RenderBootstrapDesc value) noexcept
{
    return IsValid(value.validationMode);
}

[[nodiscard]] constexpr bool IsValid(RenderDeviceDesc value) noexcept
{
    return IsValid(value.adapterPreference);
}

[[nodiscard]] constexpr bool IsValid(SurfacePreparationDesc value) noexcept
{
    return IsValid(value.targetSnapshot) && IsValid(value.reason);
}

[[nodiscard]] constexpr bool IsValid(RenderTargetDesc value) noexcept
{
    return IsValid(value.targetSnapshot) && IsValid(value.presentationPolicy) &&
           IsValid(value.queuedLatency) && IsValid(value.clearColor);
}

[[nodiscard]] constexpr bool IsValid(RenderFrameResult value) noexcept
{
    if (!IsValid(value.status) || !IsValid(value.targetStatus))
    {
        return false;
    }

    if (value.presented)
    {
        return value.windowId.IsValid() && value.targetRevision > 0U &&
               value.swapchainGeneration > 0U &&
               (value.status == FrameStatus::Presented || value.status == FrameStatus::Suboptimal ||
                value.status == FrameStatus::Recreated);
    }

    return value.status == FrameStatus::SkippedSuspended ||
           value.status == FrameStatus::TimedOut || value.status == FrameStatus::Suboptimal ||
           value.status == FrameStatus::Recreated;
}

[[nodiscard]] core::VoidResult ValidateSurfacePreparationRequest(
    platform::WindowGraphicsCompatibility compatibility, platform::WindowId actualWindowId,
    const SurfacePreparationDesc& desc);
[[nodiscard]] core::VoidResult ValidateTargetSnapshotUpdate(RenderTargetSnapshot current,
                                                            RenderTargetSnapshot next);

class PreparedSurface;
class RenderDevice;
class RenderTarget;
class RenderFrame;

class RenderBootstrap final
{
public:
    [[nodiscard]] static core::Result<RenderBootstrap> Create(const RenderBootstrapDesc& desc);

    RenderBootstrap() noexcept;
    RenderBootstrap(const RenderBootstrap&) = delete;
    RenderBootstrap& operator=(const RenderBootstrap&) = delete;
    RenderBootstrap(RenderBootstrap&& other) noexcept;
    RenderBootstrap& operator=(RenderBootstrap&& other) noexcept;
    ~RenderBootstrap();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool IsOwnerThread() const noexcept;
    [[nodiscard]] bool IsShutdown() const noexcept;
    [[nodiscard]] bool HasActiveChildren() const noexcept;
    [[nodiscard]] std::uint32_t GetActivePreparedSurfaceCount() const noexcept;
    [[nodiscard]] std::uint32_t GetActiveDeviceCount() const noexcept;
    [[nodiscard]] std::uint32_t GetActiveTargetCount() const noexcept;

    void Shutdown() noexcept;

    [[nodiscard]] core::Result<PreparedSurface> PrepareSurface(
        platform::Window& window, const SurfacePreparationDesc& desc);
    [[nodiscard]] core::Result<PreparedSurface> CreatePreparedSurfaceForCompletedSurface(
        const SurfacePreparationDesc& desc);
    [[nodiscard]] core::Result<RenderAdapterSelection> SelectAdapter(
        const PreparedSurface& firstSurface, const RenderAdapterSelectionDesc& desc);
    [[nodiscard]] core::Result<RenderDevice> CreateDevice(
        const PreparedSurface& firstSurface, const RenderAdapterSelection& adapterSelection,
        const RenderDeviceDesc& desc);
    [[nodiscard]] core::Result<RenderDevice> CreateDevice(const RenderDeviceDesc& desc);

private:
    struct State;

    explicit RenderBootstrap(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> m_state;
};

class RenderDevice final
{
public:
    RenderDevice() noexcept;
    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;
    RenderDevice(RenderDevice&& other) noexcept;
    RenderDevice& operator=(RenderDevice&& other) noexcept;
    ~RenderDevice();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] core::VoidResult VerifyRenderThread() const;
    [[nodiscard]] core::VoidResult WaitIdle() const;
    [[nodiscard]] RenderAdapterSnapshot GetSelectedAdapter() const noexcept;
    [[nodiscard]] RenderDeviceOptionalCapabilities GetOptionalCapabilities() const noexcept;
    [[nodiscard]] RenderDeviceQueuePlan GetQueuePlan() const noexcept;
    [[nodiscard]] std::uint32_t GetActiveTargetCount() const noexcept;
    [[nodiscard]] core::Result<RenderTarget> CreateRenderTarget(
        PreparedSurface&& preparedSurface, const RenderTargetDesc& desc);

private:
    friend class RenderBootstrap;
    friend class RenderTarget;
    friend class RenderFrame;

    struct State;

    explicit RenderDevice(std::shared_ptr<State> state) noexcept;

    std::shared_ptr<State> m_state;
};

class PreparedSurface final
{
public:
    PreparedSurface() noexcept;
    PreparedSurface(const PreparedSurface&) = delete;
    PreparedSurface& operator=(const PreparedSurface&) = delete;
    PreparedSurface(PreparedSurface&& other) noexcept;
    PreparedSurface& operator=(PreparedSurface&& other) noexcept;
    ~PreparedSurface();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool IsBoundToRenderThread() const noexcept;
    [[nodiscard]] RenderTargetSnapshot GetTargetSnapshot() const noexcept;
    [[nodiscard]] platform::WindowId GetWindowId() const noexcept;
    [[nodiscard]] core::VoidResult TransferToCurrentThread(RenderTargetSnapshot latestSnapshot);
    [[nodiscard]] core::VoidResult VerifyRenderThread() const;

private:
    friend class RenderBootstrap;
    friend class RenderDevice;

    struct State;

    explicit PreparedSurface(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> m_state;
};

class RenderTarget final
{
public:
    RenderTarget() noexcept;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;
    ~RenderTarget();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] core::VoidResult VerifyRenderThread() const;
    [[nodiscard]] platform::WindowId GetWindowId() const noexcept;
    [[nodiscard]] RenderTargetSnapshot GetTargetSnapshot() const noexcept;
    [[nodiscard]] TargetStatus GetStatus() const noexcept;
    [[nodiscard]] bool IsSuspended() const noexcept;
    [[nodiscard]] bool HasSwapchain() const noexcept;
    [[nodiscard]] std::uint64_t GetSwapchainGeneration() const noexcept;
    [[nodiscard]] std::optional<SelectedPresentationConfig> GetSelectedPresentationConfig()
        const noexcept;
    [[nodiscard]] core::Result<RenderFrame> AcquireFrame();
    [[nodiscard]] core::VoidResult UpdateTargetSnapshot(RenderTargetSnapshot latestSnapshot);

private:
    friend class RenderDevice;
    friend class RenderFrame;

    struct State;

    explicit RenderTarget(std::shared_ptr<State> state) noexcept;

    std::shared_ptr<State> m_state;
};

class RenderFrame final
{
public:
    RenderFrame() noexcept;
    RenderFrame(const RenderFrame&) = delete;
    RenderFrame& operator=(const RenderFrame&) = delete;
    RenderFrame(RenderFrame&& other) noexcept;
    RenderFrame& operator=(RenderFrame&& other) noexcept;
    ~RenderFrame();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] FrameStatus GetStatus() const noexcept;
    [[nodiscard]] TargetStatus GetTargetStatus() const noexcept;
    [[nodiscard]] bool IsSkipped() const noexcept;
    [[nodiscard]] core::VoidResult VerifyRenderThread() const;
    [[nodiscard]] core::VoidResult Clear();
    [[nodiscard]] core::VoidResult Clear(ClearColor clearColor);
    [[nodiscard]] core::Result<RenderFrameResult> FinishAndPresent();

private:
    friend class RenderTarget;

    struct State;

    explicit RenderFrame(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> m_state;
};

[[nodiscard]] platform::WindowGraphicsCompatibility GetRequiredWindowGraphicsCompatibility() noexcept;
} // namespace pond::render