#pragma once

#include <ponder/core/Numbers.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/WindowGraphics.hpp>
#include <ponder/platform/WindowState.hpp>
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
namespace detail
{
class RenderBackendTestAccess;
class RenderBootstrapTestAccess;
} // namespace detail

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
    VSync = 0,
    LowLatencyVSync = 1,
    Uncapped = 2
};

enum class RequirementStrength : std::uint8_t
{
    Preferred = 0,
    Required = 1
};

enum class PresentationPolicyFallbackReason : std::uint8_t
{
    None = 0,
    UnavailableForTarget = 1
};

enum class QueuedFrameLatencyFallbackReason : std::uint8_t
{
    None = 0,
    TargetMaximumExceeded = 1
};

enum class PresentationOutput : std::uint8_t
{
    OpaqueSdrSrgb = 0
};

enum class TargetStatus : std::uint8_t
{
    Active = 0,
    Hidden = 1,
    Minimized = 2,
    Suspended = 3,
    SurfaceLost = 4,
    DeviceLost = 5
};

enum class FrameStatus : std::uint8_t
{
    Ready = 0,
    SkippedSuspended = 1,
    TimedOut = 2,
    Presented = 3,
    Suboptimal = 4,
    Recreated = 5,
    RecreationPending = 6
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

    [[nodiscard]] friend constexpr bool operator==(
        const QueuedFrameLatency& lhs, const QueuedFrameLatency& rhs) noexcept = default;
};

struct PresentationPolicyRequest final
{
    PresentationPolicy policy{PresentationPolicy::VSync};
    RequirementStrength strength{RequirementStrength::Preferred};

    [[nodiscard]] friend constexpr bool operator==(const PresentationPolicyRequest& lhs,
                                                   const PresentationPolicyRequest& rhs) noexcept =
        default;
};

struct QueuedFrameLatencyRequest final
{
    QueuedFrameLatency maximumQueuedFrames{};
    RequirementStrength strength{RequirementStrength::Preferred};

    [[nodiscard]] friend constexpr bool operator==(const QueuedFrameLatencyRequest& lhs,
                                                   const QueuedFrameLatencyRequest& rhs) noexcept =
        default;
};

struct QueuedFrameLatencyRange final
{
    QueuedFrameLatency minimum{QueuedFrameLatency::kMinimumFrames};
    QueuedFrameLatency maximum{QueuedFrameLatency::kMaximumFrames};

    [[nodiscard]] friend constexpr bool operator==(
        const QueuedFrameLatencyRange& lhs, const QueuedFrameLatencyRange& rhs) noexcept = default;
};

struct RenderPresentationCapabilities final
{
    std::vector<PresentationPolicy> supportedPolicies{};
    QueuedFrameLatencyRange queuedLatency{};
    bool supportsWindowPresentation{};
    bool supportsOpaqueSdrSrgbOutput{};

    [[nodiscard]] friend bool operator==(const RenderPresentationCapabilities& lhs,
                                         const RenderPresentationCapabilities& rhs) = default;
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

    [[nodiscard]] friend constexpr bool operator==(
        const RenderAdapterMemoryHeap& lhs, const RenderAdapterMemoryHeap& rhs) noexcept = default;
};

struct RenderAdapterLimits final
{
    std::uint32_t maxImageDimension2D{};
    std::uint32_t maxFramebufferWidth{};
    std::uint32_t maxFramebufferHeight{};
    std::uint32_t maxColorAttachments{};

    [[nodiscard]] friend constexpr bool operator==(
        const RenderAdapterLimits& lhs, const RenderAdapterLimits& rhs) noexcept = default;
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
    RenderPresentationCapabilities presentation{};

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

struct ClearColor final
{
    // Components are linear floating-point values. An sRGB target format encodes RGB when the
    // clear is written to the attachment; opaque presentation ignores the stored alpha.
    float red{};
    float green{};
    float blue{};
    float alpha{1.0F};

    [[nodiscard]] friend constexpr bool operator==(const ClearColor& lhs,
                                                   const ClearColor& rhs) noexcept = default;
};

class PresentationEnvironmentRevision final
{
public:
    constexpr PresentationEnvironmentRevision() noexcept = default;
    explicit constexpr PresentationEnvironmentRevision(std::uint64_t value) noexcept
        : m_value{value}
    {
    }

    [[nodiscard]] constexpr std::uint64_t GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const PresentationEnvironmentRevision& lhs,
        const PresentationEnvironmentRevision& rhs) noexcept = default;

private:
    std::uint64_t m_value{};
};

class RenderTargetSnapshot final
{
public:
    constexpr RenderTargetSnapshot() noexcept = default;
    constexpr RenderTargetSnapshot(platform::WindowId windowId, platform::PixelSize pixelSize,
                                   bool visible, platform::WindowState windowState,
                                   PresentationEnvironmentRevision presentationEnvironmentRevision,
                                   std::uint64_t revision) noexcept
        : m_windowId{windowId}, m_pixelSize{pixelSize}, m_visible{visible},
          m_windowState{windowState},
          m_presentationEnvironmentRevision{presentationEnvironmentRevision}, m_revision{revision}
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

    [[nodiscard]] constexpr platform::WindowState GetWindowState() const noexcept
    {
        return m_windowState;
    }

    [[nodiscard]] constexpr PresentationEnvironmentRevision GetPresentationEnvironmentRevision()
        const noexcept
    {
        return m_presentationEnvironmentRevision;
    }

    [[nodiscard]] constexpr std::uint64_t GetRevision() const noexcept
    {
        return m_revision;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const RenderTargetSnapshot& lhs, const RenderTargetSnapshot& rhs) noexcept = default;

private:
    platform::WindowId m_windowId{};
    platform::PixelSize m_pixelSize{};
    bool m_visible{};
    platform::WindowState m_windowState{platform::WindowState::Normal};
    PresentationEnvironmentRevision m_presentationEnvironmentRevision{};
    std::uint64_t m_revision{};
};

struct SelectedPresentationConfig final
{
    PresentationPolicyRequest requestedPolicy{};
    PresentationPolicy actualPolicy{PresentationPolicy::VSync};
    PresentationPolicyFallbackReason policyFallback{PresentationPolicyFallbackReason::None};
    QueuedFrameLatencyRequest requestedQueuedLatency{};
    QueuedFrameLatency actualQueuedLatency{};
    QueuedFrameLatencyFallbackReason queuedLatencyFallback{QueuedFrameLatencyFallbackReason::None};
    PresentationOutput output{PresentationOutput::OpaqueSdrSrgb};
    platform::PixelSize pixelExtent{};

    [[nodiscard]] friend bool operator==(const SelectedPresentationConfig& lhs,
                                         const SelectedPresentationConfig& rhs) = default;
};

struct TargetRecreationInfo final
{
    TargetRecreationReason reason{TargetRecreationReason::None};
    std::optional<std::uint64_t> previousRevision{};
    std::optional<std::uint64_t> currentRevision{};

    [[nodiscard]] friend constexpr bool operator==(
        const TargetRecreationInfo& lhs, const TargetRecreationInfo& rhs) noexcept = default;
};

struct PresentationRetirementStats final
{
    std::uint64_t pendingResourceSets{};
    std::uint64_t retiredResourceSets{};
    std::uint64_t practicalWaitFallbacks{};
    bool usedExplicitPresentationCompletion{};
    bool usedCoreAcquireHistory{};
    bool surfaceLost{};
    bool deviceLost{};

    [[nodiscard]] friend constexpr bool operator==(
        const PresentationRetirementStats& lhs,
        const PresentationRetirementStats& rhs) noexcept = default;
};

struct BackendDiagnostic final
{
    RenderBackendKind backend{RenderBackendKind::Unknown};
    RenderErrorCode renderCode{RenderErrorCode::BackendFailure};
    std::int64_t nativeCode{};
    std::string symbolicName{};
    std::string operation{};
    std::string validationContext{};
    platform::WindowId windowId{};
    std::string targetLabel{};

    [[nodiscard]] friend bool operator==(const BackendDiagnostic& lhs,
                                         const BackendDiagnostic& rhs) = default;
};

using OptionalBackendDiagnostic = std::optional<BackendDiagnostic>;

struct RenderDebugInstrumentation final
{
    bool objectNames{};
    bool commandLabels{};
    bool timingMarkers{};
    bool captureRegions{};

    [[nodiscard]] friend constexpr bool operator==(const RenderDebugInstrumentation& lhs,
                                                   const RenderDebugInstrumentation& rhs) noexcept =
        default;
};

enum class RenderValidationMessageSeverity : std::uint8_t
{
    Warning = 0,
    Error = 1
};

struct RenderValidationMessage final
{
    static constexpr std::size_t kMaximumMessageIdNameLength = 95U;

    RenderValidationMessageSeverity severity{RenderValidationMessageSeverity::Warning};
    std::int32_t messageIdNumber{};
    std::array<char, kMaximumMessageIdNameLength + 1U> messageIdName{};

    [[nodiscard]] std::string_view GetMessageIdName() const noexcept
    {
        std::size_t length{};
        while (length < messageIdName.size() && messageIdName[length] != '\0')
        {
            ++length;
        }
        return {messageIdName.data(), length};
    }

    [[nodiscard]] friend constexpr bool operator==(
        const RenderValidationMessage& lhs, const RenderValidationMessage& rhs) noexcept = default;
};

struct RenderValidationReport final
{
    static constexpr std::size_t kMaximumCapturedMessages = 32U;

    std::uint64_t warningCount{};
    std::uint64_t errorCount{};
    std::uint64_t droppedMessageCount{};
    std::size_t capturedMessageCount{};
    std::array<RenderValidationMessage, kMaximumCapturedMessages> capturedMessages{};

    [[nodiscard]] constexpr bool IsClean() const noexcept
    {
        return warningCount == 0U && errorCount == 0U;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const RenderValidationReport& lhs, const RenderValidationReport& rhs) noexcept = default;
};

struct RenderBootstrapDiagnostics final
{
    RenderBackendKind backend{RenderBackendKind::Unknown};
    RenderValidationMode requestedValidationMode{RenderValidationMode::Default};
    RenderValidationMode enabledValidationMode{RenderValidationMode::Disabled};
    std::uint32_t negotiatedApiVersion{};
    bool validationEnabled{};
    RenderDebugInstrumentation debugInstrumentation{};
    OptionalBackendDiagnostic lastFailure{};

    [[nodiscard]] friend bool operator==(const RenderBootstrapDiagnostics& lhs,
                                         const RenderBootstrapDiagnostics& rhs) = default;
};

struct RenderDeviceDiagnostics final
{
    RenderAdapterSnapshot selectedAdapter{};
    std::uint64_t targetCreateAttempts{};
    std::uint64_t targetCreateSuccesses{};
    std::uint64_t targetCreateFailures{};
    std::uint64_t waitIdleCalls{};
    std::uint64_t practicalWaitFallbacks{};
    bool deviceLost{};
    OptionalBackendDiagnostic lastFailure{};

    [[nodiscard]] friend bool operator==(const RenderDeviceDiagnostics& lhs,
                                         const RenderDeviceDiagnostics& rhs) = default;
};

struct RenderTargetDiagnostics final
{
    platform::WindowId windowId{};
    std::string targetLabel{};
    RenderTargetSnapshot targetSnapshot{};
    TargetStatus status{TargetStatus::Suspended};
    std::uint64_t swapchainGeneration{};
    std::uint64_t recreationCount{};
    std::uint64_t suspensionCount{};
    std::uint64_t frameAcquireAttempts{};
    std::uint64_t frameAcquireFailures{};
    std::uint64_t framesPresented{};
    std::uint64_t frameTimeouts{};
    std::uint64_t frameFailures{};
    PresentationRetirementStats retirement{};
    std::optional<TargetRecreationInfo> pendingRecreation{};
    std::optional<SelectedPresentationConfig> selectedPresentation{};
    OptionalBackendDiagnostic lastFailure{};

    [[nodiscard]] friend bool operator==(const RenderTargetDiagnostics& lhs,
                                         const RenderTargetDiagnostics& rhs) = default;
};

struct RenderBootstrapDesc final
{
    RenderValidationMode validationMode{RenderValidationMode::Default};

    [[nodiscard]] friend constexpr bool operator==(
        const RenderBootstrapDesc& lhs, const RenderBootstrapDesc& rhs) noexcept = default;
};

struct RenderDeviceDesc final
{
    // Reserved for logical-device policy that is independent of adapter selection.
    [[nodiscard]] friend constexpr bool operator==(const RenderDeviceDesc& lhs,
                                                   const RenderDeviceDesc& rhs) noexcept = default;
};

struct SurfacePreparationDesc final
{
    RenderTargetSnapshot targetSnapshot{};
    SurfacePreparationReason reason{SurfacePreparationReason::Initial};

    [[nodiscard]] friend constexpr bool operator==(
        const SurfacePreparationDesc& lhs, const SurfacePreparationDesc& rhs) noexcept = default;
};

struct RenderTargetDesc final
{
    RenderTargetSnapshot targetSnapshot{};
    PresentationPolicyRequest presentation{};
    QueuedFrameLatencyRequest queuedLatency{};
    ClearColor clearColor{};

    [[nodiscard]] friend constexpr bool operator==(const RenderTargetDesc& lhs,
                                                   const RenderTargetDesc& rhs) noexcept = default;
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
                                                   const RenderFrameResult& rhs) noexcept = default;
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
    case PresentationPolicy::VSync:
    case PresentationPolicy::LowLatencyVSync:
    case PresentationPolicy::Uncapped:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(RequirementStrength value) noexcept
{
    switch (value)
    {
    case RequirementStrength::Preferred:
    case RequirementStrength::Required:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(PresentationPolicyFallbackReason value) noexcept
{
    switch (value)
    {
    case PresentationPolicyFallbackReason::None:
    case PresentationPolicyFallbackReason::UnavailableForTarget:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(QueuedFrameLatencyFallbackReason value) noexcept
{
    switch (value)
    {
    case QueuedFrameLatencyFallbackReason::None:
    case QueuedFrameLatencyFallbackReason::TargetMaximumExceeded:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(PresentationOutput value) noexcept
{
    switch (value)
    {
    case PresentationOutput::OpaqueSdrSrgb:
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
    case TargetStatus::SurfaceLost:
    case TargetStatus::DeviceLost:
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
    case FrameStatus::RecreationPending:
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

[[nodiscard]] constexpr bool IsValid(PresentationPolicyRequest value) noexcept
{
    return IsValid(value.policy) && IsValid(value.strength);
}

[[nodiscard]] constexpr bool IsValid(QueuedFrameLatencyRequest value) noexcept
{
    return IsValid(value.maximumQueuedFrames) && IsValid(value.strength);
}

[[nodiscard]] constexpr bool IsValid(QueuedFrameLatencyRange value) noexcept
{
    return IsValid(value.minimum) && IsValid(value.maximum) &&
           value.minimum.frameCount <= value.maximum.frameCount;
}

[[nodiscard]] inline bool IsValid(const RenderPresentationCapabilities& value) noexcept
{
    if (!value.supportsWindowPresentation || !value.supportsOpaqueSdrSrgbOutput ||
        !IsValid(value.queuedLatency) || value.supportedPolicies.empty())
    {
        return false;
    }

    bool hasVSync{};
    for (std::size_t index = 0; index < value.supportedPolicies.size(); ++index)
    {
        if (!IsValid(value.supportedPolicies[index]))
        {
            return false;
        }
        hasVSync = hasVSync || value.supportedPolicies[index] == PresentationPolicy::VSync;

        for (std::size_t laterIndex = index + 1U; laterIndex < value.supportedPolicies.size();
             ++laterIndex)
        {
            if (value.supportedPolicies[index] == value.supportedPolicies[laterIndex])
            {
                return false;
            }
        }
    }

    return hasVSync;
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

[[nodiscard]] inline bool IsValid(const RenderAdapterSnapshot& value) noexcept
{
    return IsValid(value.adapterId) && IsValid(value.backend) && value.apiVersion != 0U &&
           IsValid(value.identity) && IsValid(value.driver) && IsValid(value.limits) &&
           IsValid(value.presentation);
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

[[nodiscard]] constexpr bool IsValid(ClearColor value) noexcept
{
    const auto isUnitInterval = [](float component) constexpr noexcept
    {
        return core::IsFinite(component) && component >= 0.0F && component <= 1.0F;
    };

    return isUnitInterval(value.red) && isUnitInterval(value.green) && isUnitInterval(value.blue) &&
           isUnitInterval(value.alpha);
}

[[nodiscard]] constexpr bool IsValid(PresentationEnvironmentRevision value) noexcept
{
    return value.GetValue() != 0U;
}

[[nodiscard]] constexpr bool IsValid(RenderTargetSnapshot value) noexcept
{
    if (!value.GetWindowId().IsValid() || value.GetRevision() == 0U ||
        !IsValid(value.GetPresentationEnvironmentRevision()))
    {
        return false;
    }

    switch (value.GetWindowState())
    {
    case platform::WindowState::Normal:
    case platform::WindowState::Minimized:
    case platform::WindowState::Maximized:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(const SelectedPresentationConfig& value) noexcept
{
    if (!IsValid(value.requestedPolicy) || !IsValid(value.actualPolicy) ||
        !IsValid(value.policyFallback) || !IsValid(value.requestedQueuedLatency) ||
        !IsValid(value.actualQueuedLatency) || !IsValid(value.queuedLatencyFallback) ||
        !IsValid(value.output) || value.pixelExtent.width == 0U || value.pixelExtent.height == 0U)
    {
        return false;
    }

    const bool policyChanged = value.requestedPolicy.policy != value.actualPolicy;
    if (value.requestedPolicy.strength == RequirementStrength::Required && policyChanged)
    {
        return false;
    }
    if (policyChanged !=
        (value.policyFallback == PresentationPolicyFallbackReason::UnavailableForTarget))
    {
        return false;
    }

    const bool latencyChanged =
        value.requestedQueuedLatency.maximumQueuedFrames != value.actualQueuedLatency;
    if (value.requestedQueuedLatency.strength == RequirementStrength::Required && latencyChanged)
    {
        return false;
    }
    if (latencyChanged !=
        (value.queuedLatencyFallback == QueuedFrameLatencyFallbackReason::TargetMaximumExceeded))
    {
        return false;
    }

    return !latencyChanged || value.actualQueuedLatency.frameCount <
                                  value.requestedQueuedLatency.maximumQueuedFrames.frameCount;
}

[[nodiscard]] constexpr bool IsValid(TargetRecreationInfo value) noexcept
{
    if (!IsValid(value.reason))
    {
        return false;
    }

    if (value.reason == TargetRecreationReason::None)
    {
        return !value.previousRevision.has_value() && !value.currentRevision.has_value();
    }

    if (value.reason == TargetRecreationReason::SurfaceLost)
    {
        return !value.previousRevision.has_value() && !value.currentRevision.has_value();
    }

    if (value.reason == TargetRecreationReason::PresentationChanged &&
        !value.previousRevision.has_value() && !value.currentRevision.has_value())
    {
        return true;
    }

    return value.previousRevision.has_value() && value.currentRevision.has_value() &&
           *value.currentRevision > *value.previousRevision;
}

[[nodiscard]] inline bool IsValid(const BackendDiagnostic& value) noexcept
{
    return IsValid(value.backend);
}

[[nodiscard]] constexpr bool IsValid(RenderBootstrapDesc value) noexcept
{
    return IsValid(value.validationMode);
}

[[nodiscard]] constexpr bool IsValid(RenderDeviceDesc) noexcept
{
    return true;
}

[[nodiscard]] constexpr bool IsValid(SurfacePreparationDesc value) noexcept
{
    return IsValid(value.targetSnapshot) && IsValid(value.reason);
}

[[nodiscard]] constexpr bool IsValid(RenderTargetDesc value) noexcept
{
    return IsValid(value.targetSnapshot) && IsValid(value.presentation) &&
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

    return value.status == FrameStatus::SkippedSuspended || value.status == FrameStatus::TimedOut ||
           value.status == FrameStatus::RecreationPending;
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
    [[nodiscard]] RenderBootstrapDiagnostics GetDiagnostics() const;
    [[nodiscard]] RenderValidationReport GetValidationReport() const noexcept;

    [[nodiscard]] core::VoidResult Shutdown();

    [[nodiscard]] core::Result<PreparedSurface> PrepareSurface(platform::Window& window,
                                                               const SurfacePreparationDesc& desc);
    [[nodiscard]] core::Result<RenderAdapterSelection> SelectAdapter(
        const PreparedSurface& firstSurface, const RenderAdapterSelectionDesc& desc);
    [[nodiscard]] core::Result<RenderDevice> CreateDevice(
        const PreparedSurface& firstSurface, const RenderAdapterSelection& adapterSelection,
        const RenderDeviceDesc& desc);

private:
    friend class detail::RenderBackendTestAccess;
    friend class detail::RenderBootstrapTestAccess;

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
    [[nodiscard]] bool IsDeviceLost() const noexcept;
    [[nodiscard]] core::VoidResult VerifyRenderThread() const;
    [[nodiscard]] core::VoidResult WaitIdle() const;
    [[nodiscard]] const RenderAdapterSnapshot& GetSelectedAdapter() const noexcept;
    [[nodiscard]] RenderDeviceDiagnostics GetDiagnostics() const;
    [[nodiscard]] std::uint32_t GetActiveTargetCount() const noexcept;
    [[nodiscard]] core::Result<RenderTarget> CreateRenderTarget(PreparedSurface&& preparedSurface,
                                                                const RenderTargetDesc& desc);

private:
    friend class RenderBootstrap;
    friend class RenderTarget;
    friend class RenderFrame;
    friend class detail::RenderBackendTestAccess;

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
    friend class RenderTarget;
    friend class detail::RenderBackendTestAccess;
    friend class detail::RenderBootstrapTestAccess;

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
    [[nodiscard]] bool IsDeviceLost() const noexcept;
    [[nodiscard]] core::VoidResult VerifyRenderThread() const;
    [[nodiscard]] platform::WindowId GetWindowId() const noexcept;
    [[nodiscard]] RenderTargetSnapshot GetTargetSnapshot() const noexcept;
    [[nodiscard]] TargetStatus GetStatus() const noexcept;
    [[nodiscard]] bool IsSuspended() const noexcept;
    [[nodiscard]] bool IsSurfaceLost() const noexcept;
    [[nodiscard]] bool HasSwapchain() const noexcept;
    [[nodiscard]] std::uint64_t GetSwapchainGeneration() const noexcept;
    [[nodiscard]] bool HasPendingRecreation() const noexcept;
    [[nodiscard]] std::optional<TargetRecreationInfo> GetPendingRecreationInfo() const noexcept;
    [[nodiscard]] PresentationRetirementStats GetPresentationRetirementStats() const noexcept;
    [[nodiscard]] std::optional<SelectedPresentationConfig> GetSelectedPresentationConfig()
        const noexcept;
    [[nodiscard]] RenderTargetDiagnostics GetDiagnostics() const;
    [[nodiscard]] core::Result<RenderFrame> AcquireFrame();
    [[nodiscard]] core::VoidResult UpdateTargetSnapshot(RenderTargetSnapshot latestSnapshot);
    [[nodiscard]] core::VoidResult RecoverSurface(PreparedSurface&& preparedSurface);

private:
    friend class RenderDevice;
    friend class RenderFrame;
    friend class detail::RenderBackendTestAccess;

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
    friend class detail::RenderBackendTestAccess;

    struct State;

    explicit RenderFrame(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> m_state;
};

[[nodiscard]] platform::WindowGraphicsCompatibility
GetRequiredWindowGraphicsCompatibility() noexcept;
} // namespace pond::render
