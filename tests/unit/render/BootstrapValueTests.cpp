#include <ponder/render/Bootstrap.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <string>

namespace
{
template <typename Value>
concept ExposesQueueFamilies = requires(Value value) { value.queueFamilies; };

template <typename Value>
concept ExposesNativeSurfaceFormats = requires(Value value) { value.surfaceFormats; };

template <typename Value>
concept ExposesNativePresentModes = requires(Value value) { value.presentModes; };

template <typename Value>
concept ExposesNativeSelectedMode = requires(Value value) { value.selectedMode; };

template <typename Value>
concept ExposesNativeSelectedFormat = requires(Value value) { value.surfaceFormat; };

template <typename Value>
concept ExposesSwapchainMaintenance = requires(Value value) { value.swapchainMaintenance1; };

template <typename Value>
concept ExposesPresentId = requires(Value value) { value.presentId; };

template <typename Value>
concept ExposesPresentWait = requires(Value value) { value.presentWait; };

template <typename Value>
concept ExposesVmaAllocator = requires(Value value) { value.vmaAllocator; };

template <typename Value>
concept ExposesQueuePlan = requires(Value& value) { value.GetQueuePlan(); };

template <typename Value>
concept ExposesOptionalCapabilities = requires(Value& value) { value.GetOptionalCapabilities(); };

static_assert(!ExposesQueueFamilies<pond::render::RenderAdapterSnapshot>);
static_assert(!ExposesNativeSurfaceFormats<pond::render::RenderAdapterSnapshot>);
static_assert(!ExposesNativePresentModes<pond::render::RenderAdapterSnapshot>);
static_assert(!ExposesNativeSelectedMode<pond::render::SelectedPresentationConfig>);
static_assert(!ExposesNativeSelectedFormat<pond::render::SelectedPresentationConfig>);
static_assert(!ExposesSwapchainMaintenance<pond::render::RenderDeviceDiagnostics>);
static_assert(!ExposesPresentId<pond::render::RenderDeviceDiagnostics>);
static_assert(!ExposesPresentWait<pond::render::RenderDeviceDiagnostics>);
static_assert(!ExposesVmaAllocator<pond::render::RenderDeviceDiagnostics>);
static_assert(!ExposesQueuePlan<pond::render::RenderDevice>);
static_assert(!ExposesOptionalCapabilities<pond::render::RenderDevice>);

static_assert(pond::render::IsValid(pond::render::RenderBackendKind::Unknown));
static_assert(pond::render::IsValid(pond::render::RenderBackendKind::Vulkan));
static_assert(!pond::render::IsValid(static_cast<pond::render::RenderBackendKind>(255)));

static_assert(pond::render::IsValid(pond::render::RenderAdapterType::Unknown));
static_assert(pond::render::IsValid(pond::render::RenderAdapterType::IntegratedGpu));
static_assert(pond::render::IsValid(pond::render::RenderAdapterType::DiscreteGpu));
static_assert(pond::render::IsValid(pond::render::RenderAdapterType::VirtualGpu));
static_assert(pond::render::IsValid(pond::render::RenderAdapterType::Cpu));
static_assert(!pond::render::IsValid(static_cast<pond::render::RenderAdapterType>(255)));

static_assert(pond::render::IsValid(pond::render::RenderAdapterPreference::Default));
static_assert(pond::render::IsValid(pond::render::RenderAdapterPreference::HighPerformance));
static_assert(pond::render::IsValid(pond::render::RenderAdapterPreference::LowPower));
static_assert(pond::render::IsValid(pond::render::RenderAdapterPreference::Software));
static_assert(!pond::render::IsValid(static_cast<pond::render::RenderAdapterPreference>(255)));

static_assert(pond::render::IsValid(pond::render::RenderValidationMode::Default));
static_assert(pond::render::IsValid(pond::render::RenderValidationMode::Disabled));
static_assert(pond::render::IsValid(pond::render::RenderValidationMode::Standard));
static_assert(pond::render::IsValid(pond::render::RenderValidationMode::Enabled));
static_assert(pond::render::IsValid(pond::render::RenderValidationMode::Synchronization));
static_assert(pond::render::IsValid(pond::render::RenderValidationMode::BestPractices));
static_assert(pond::render::IsValid(pond::render::RenderValidationMode::GpuAssisted));
static_assert(!pond::render::IsValid(static_cast<pond::render::RenderValidationMode>(255)));

static_assert(pond::render::IsValid(pond::render::PresentationPolicy::VSync));
static_assert(pond::render::IsValid(pond::render::PresentationPolicy::LowLatencyVSync));
static_assert(pond::render::IsValid(pond::render::PresentationPolicy::Uncapped));
static_assert(!pond::render::IsValid(static_cast<pond::render::PresentationPolicy>(255)));

static_assert(pond::render::IsValid(pond::render::RequirementStrength::Preferred));
static_assert(pond::render::IsValid(pond::render::RequirementStrength::Required));
static_assert(!pond::render::IsValid(static_cast<pond::render::RequirementStrength>(255)));
static_assert(
    pond::render::IsValid(pond::render::PresentationPolicyFallbackReason::UnavailableForTarget));
static_assert(
    pond::render::IsValid(pond::render::QueuedFrameLatencyFallbackReason::TargetMaximumExceeded));
static_assert(pond::render::IsValid(pond::render::PresentationOutput::OpaqueSdrSrgb));

static_assert(pond::render::IsValid(pond::render::TargetStatus::Active));
static_assert(pond::render::IsValid(pond::render::TargetStatus::Hidden));
static_assert(pond::render::IsValid(pond::render::TargetStatus::Minimized));
static_assert(pond::render::IsValid(pond::render::TargetStatus::Suspended));
static_assert(pond::render::IsValid(pond::render::TargetStatus::SurfaceLost));
static_assert(pond::render::IsValid(pond::render::TargetStatus::DeviceLost));
static_assert(!pond::render::IsValid(static_cast<pond::render::TargetStatus>(255)));

static_assert(pond::render::IsValid(pond::render::FrameStatus::Ready));
static_assert(pond::render::IsValid(pond::render::FrameStatus::SkippedSuspended));
static_assert(pond::render::IsValid(pond::render::FrameStatus::TimedOut));
static_assert(pond::render::IsValid(pond::render::FrameStatus::Presented));
static_assert(pond::render::IsValid(pond::render::FrameStatus::Suboptimal));
static_assert(pond::render::IsValid(pond::render::FrameStatus::Recreated));
static_assert(pond::render::IsValid(pond::render::FrameStatus::RecreationPending));
static_assert(!pond::render::IsValid(static_cast<pond::render::FrameStatus>(255)));

static_assert(pond::render::IsValid(pond::render::TargetRecreationReason::None));
static_assert(pond::render::IsValid(pond::render::TargetRecreationReason::SizeChanged));
static_assert(pond::render::IsValid(pond::render::TargetRecreationReason::Restored));
static_assert(pond::render::IsValid(pond::render::TargetRecreationReason::SurfaceLost));
static_assert(pond::render::IsValid(pond::render::TargetRecreationReason::PresentationChanged));
static_assert(!pond::render::IsValid(static_cast<pond::render::TargetRecreationReason>(255)));

static_assert(pond::render::IsValid(pond::render::SurfacePreparationReason::Initial));
static_assert(pond::render::IsValid(pond::render::SurfacePreparationReason::SurfaceLossRecovery));
static_assert(!pond::render::IsValid(static_cast<pond::render::SurfacePreparationReason>(255)));

static_assert(pond::render::IsValid(pond::render::QueuedFrameLatency{}));
static_assert(!pond::render::IsValid(pond::render::QueuedFrameLatency{.frameCount = 0}));
static_assert(!pond::render::IsValid(pond::render::QueuedFrameLatency{.frameCount = 4}));
static_assert(pond::render::IsValid(pond::render::PresentationPolicyRequest{}));
static_assert(pond::render::IsValid(pond::render::QueuedFrameLatencyRequest{}));
static_assert(pond::render::IsValid(pond::render::QueuedFrameLatencyRange{}));
static_assert(pond::render::IsValid(pond::render::ClearColor{}));
static_assert(!pond::render::IsValid(pond::render::ClearColor{.red = -0.01F}));
static_assert(!pond::render::IsValid(pond::render::ClearColor{.alpha = 1.01F}));
static_assert(pond::render::IsValid(pond::render::TargetRecreationInfo{}));
static_assert(!pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::None, .currentRevision = 1U}));
static_assert(pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::SizeChanged,
    .previousRevision = 1U,
    .currentRevision = 2U}));
static_assert(!pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::SizeChanged,
    .previousRevision = 2U,
    .currentRevision = 2U}));
static_assert(!pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::Restored, .currentRevision = 2U}));
static_assert(pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::SurfaceLost}));
static_assert(!pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::SurfaceLost,
    .previousRevision = 1U,
    .currentRevision = 2U}));
static_assert(pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::PresentationChanged}));
static_assert(!pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::PresentationChanged, .previousRevision = 1U}));
static_assert(pond::render::IsValid(pond::render::RenderFrameResult{}));
static_assert(pond::render::IsValid(pond::render::RenderFrameResult{
    .status = pond::render::FrameStatus::RecreationPending}));
static_assert(!pond::render::IsValid(pond::render::RenderFrameResult{
    .status = pond::render::FrameStatus::Presented, .presented = true}));
static_assert(!pond::render::IsValid(pond::render::RenderFrameResult{
    .status = pond::render::FrameStatus::Suboptimal}));
static_assert(!pond::render::IsValid(pond::render::RenderFrameResult{
    .status = pond::render::FrameStatus::Recreated}));
static_assert(pond::render::IsValid(pond::render::RenderFrameMetrics{
    .windowId = pond::platform::WindowId{42},
    .logicalSize = pond::platform::LogicalSize{.width = 800, .height = 600},
    .pixelSize = pond::platform::PixelSize{.width = 1600, .height = 1200},
    .metricsRevision = pond::render::PresentationEnvironmentRevision{2U},
    .targetRevision = 3U}));
static_assert(!pond::render::IsValid(pond::render::RenderFrameMetrics{}));
static_assert(!pond::render::IsValid(pond::render::RenderFrameMetrics{
    .windowId = pond::platform::WindowId{42},
    .logicalSize = pond::platform::LogicalSize{},
    .pixelSize = pond::platform::PixelSize{.width = 1600, .height = 1200},
    .metricsRevision = pond::render::PresentationEnvironmentRevision{2U},
    .targetRevision = 3U}));

[[nodiscard]] constexpr pond::render::RenderTargetSnapshot MakeValidSnapshot(
    std::uint64_t revision = 1,
    pond::render::PresentationEnvironmentRevision presentationEnvironmentRevision =
        pond::render::PresentationEnvironmentRevision{1U})
{
    return pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42},
        pond::platform::PixelSize{.width = 800, .height = 600},
        pond::platform::LogicalSize{.width = 800U, .height = 600U},
        true,
        pond::platform::WindowState::Normal,
        presentationEnvironmentRevision,
        revision};
}

static_assert(pond::render::IsValid(pond::render::PresentationEnvironmentRevision{1U}));
static_assert(!pond::render::IsValid(pond::render::PresentationEnvironmentRevision{}));
static_assert(pond::render::IsValid(MakeValidSnapshot()));
static_assert(!pond::render::IsValid(pond::render::RenderTargetSnapshot{}));
static_assert(!pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{.width = 800, .height = 600},
    pond::platform::LogicalSize{.width = 800U, .height = 600U}, true,
    static_cast<pond::platform::WindowState>(255),
    pond::render::PresentationEnvironmentRevision{1U}, 1}));
static_assert(!pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{.width = 800, .height = 600},
    pond::platform::LogicalSize{.width = 800U, .height = 600U}, true,
    pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{}, 1}));
static_assert(!pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{.width = 800, .height = 600},
    pond::platform::LogicalSize{}, true, pond::platform::WindowState::Normal,
    pond::render::PresentationEnvironmentRevision{1U}, 1}));
static_assert(pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{}, pond::platform::LogicalSize{}, true,
    pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U}, 1}));
static_assert(pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{}, pond::platform::LogicalSize{}, false,
    pond::platform::WindowState::Maximized, pond::render::PresentationEnvironmentRevision{1U}, 1}));

TEST(RenderBootstrapValueTests, ValidatesFrameLifecycleResultsWithoutBackendHandles)
{
    const pond::render::RenderFrameResult presented{.status = pond::render::FrameStatus::Presented,
                                                    .targetStatus =
                                                        pond::render::TargetStatus::Active,
                                                    .windowId = pond::platform::WindowId{42},
                                                    .targetRevision = 7U,
                                                    .swapchainGeneration = 3U,
                                                    .presented = true,
                                                    .suboptimal = false};
    EXPECT_TRUE(pond::render::IsValid(presented));

    pond::render::RenderFrameResult suboptimal = presented;
    suboptimal.status = pond::render::FrameStatus::Suboptimal;
    suboptimal.suboptimal = true;
    EXPECT_TRUE(pond::render::IsValid(suboptimal));

    pond::render::RenderFrameResult recreated = presented;
    recreated.status = pond::render::FrameStatus::Recreated;
    EXPECT_TRUE(pond::render::IsValid(recreated));

    const pond::render::RenderFrameResult recreationPending{
        .status = pond::render::FrameStatus::RecreationPending,
        .targetStatus = pond::render::TargetStatus::Active};
    EXPECT_TRUE(pond::render::IsValid(recreationPending));

    pond::render::RenderFrameResult invalidPresented = presented;
    invalidPresented.swapchainGeneration = 0U;
    EXPECT_FALSE(pond::render::IsValid(invalidPresented));

    pond::render::RenderFrameResult invalidPendingPresentation = presented;
    invalidPendingPresentation.status = pond::render::FrameStatus::RecreationPending;
    EXPECT_FALSE(pond::render::IsValid(invalidPendingPresentation));

    pond::render::RenderFrameResult invalidNonPresentedSuboptimal = suboptimal;
    invalidNonPresentedSuboptimal.presented = false;
    EXPECT_FALSE(pond::render::IsValid(invalidNonPresentedSuboptimal));

    pond::render::RenderFrameResult invalidNonPresentedRecreated = recreated;
    invalidNonPresentedRecreated.presented = false;
    EXPECT_FALSE(pond::render::IsValid(invalidNonPresentedRecreated));

    EXPECT_FALSE(pond::render::IsValid(
        pond::render::RenderFrameResult{.status = pond::render::FrameStatus::Ready}));
    EXPECT_FALSE(pond::render::IsValid(
        pond::render::RenderFrameResult{.status = pond::render::FrameStatus::Presented}));
}
TEST(RenderBootstrapValueTests, ReportsVulkanWindowCompatibilityForThisBuild)
{
    EXPECT_EQ(pond::render::GetRequiredWindowGraphicsCompatibility(),
              pond::platform::WindowGraphicsCompatibility::Vulkan);
}

TEST(RenderBootstrapValueTests, RecordsExperimentalApiStatus)
{
    EXPECT_NE(pond::render::kExperimentalApiNotice.find("experimental"), std::string::npos);
    EXPECT_NE(pond::render::kExperimentalApiNotice.find("ABI"), std::string::npos);
}

TEST(RenderBootstrapValueTests, ProvidesSafeDescriptorDefaults)
{
    EXPECT_TRUE(pond::render::IsValid(pond::render::RenderBootstrapDesc{}));
    EXPECT_TRUE(pond::render::IsValid(pond::render::RenderDeviceDesc{}));

    const pond::render::SurfacePreparationDesc surfaceDesc;
    EXPECT_FALSE(pond::render::IsValid(surfaceDesc));

    const pond::render::RenderTargetDesc targetDesc;
    EXPECT_FALSE(pond::render::IsValid(targetDesc));
    EXPECT_EQ(targetDesc.clearColor, pond::render::ClearColor{});
    EXPECT_EQ(targetDesc.queuedLatency.maximumQueuedFrames.frameCount,
              pond::render::QueuedFrameLatency::kDefaultFrames);
}

TEST(RenderBootstrapValueTests, ValidatesTargetDescriptorTransactionInputs)
{
    const pond::render::RenderTargetDesc validDesc{
        .targetSnapshot = MakeValidSnapshot(),
        .presentation = {.policy = pond::render::PresentationPolicy::VSync,
                         .strength = pond::render::RequirementStrength::Required},
        .queuedLatency = {.maximumQueuedFrames = pond::render::QueuedFrameLatency{3},
                          .strength = pond::render::RequirementStrength::Preferred},
        .clearColor =
            pond::render::ClearColor{.red = 0.1F, .green = 0.2F, .blue = 0.3F, .alpha = 1.0F}};

    EXPECT_TRUE(pond::render::IsValid(validDesc));

    pond::render::RenderTargetDesc invalidLatency = validDesc;
    invalidLatency.queuedLatency.maximumQueuedFrames.frameCount = 0;
    EXPECT_FALSE(pond::render::IsValid(invalidLatency));

    pond::render::RenderTargetDesc invalidPresentation = validDesc;
    invalidPresentation.presentation.policy = static_cast<pond::render::PresentationPolicy>(255);
    EXPECT_FALSE(pond::render::IsValid(invalidPresentation));

    pond::render::RenderTargetDesc invalidClear = validDesc;
    invalidClear.clearColor.green = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(pond::render::IsValid(invalidClear));
}

TEST(RenderBootstrapValueTests, ValidatesSemanticPresentationCapabilities)
{
    const pond::render::RenderPresentationCapabilities capabilities{
        .supportedPolicies = {pond::render::PresentationPolicy::VSync,
                              pond::render::PresentationPolicy::LowLatencyVSync,
                              pond::render::PresentationPolicy::Uncapped},
        .queuedLatency = {},
        .supportsWindowPresentation = true,
        .supportsOpaqueSdrSrgbOutput = true};
    EXPECT_TRUE(pond::render::IsValid(capabilities));

    pond::render::RenderPresentationCapabilities duplicatePolicy = capabilities;
    duplicatePolicy.supportedPolicies.push_back(pond::render::PresentationPolicy::VSync);
    EXPECT_FALSE(pond::render::IsValid(duplicatePolicy));

    pond::render::RenderPresentationCapabilities missingVSync = capabilities;
    missingVSync.supportedPolicies.erase(missingVSync.supportedPolicies.begin());
    EXPECT_FALSE(pond::render::IsValid(missingVSync));
}
TEST(RenderBootstrapValueTests, SnapshotStoresCopiedWindowStateWithoutNativePayload)
{
    const pond::platform::WindowId windowId{9};
    const pond::render::RenderTargetSnapshot minimizedSnapshot{
        windowId,
        pond::platform::PixelSize{},
        pond::platform::LogicalSize{},
        false,
        pond::platform::WindowState::Minimized,
        pond::render::PresentationEnvironmentRevision{2U},
        5};
    const pond::render::RenderTargetSnapshot restoredSnapshot{
        windowId,
        pond::platform::PixelSize{.width = 640, .height = 480},
        pond::platform::LogicalSize{.width = 640U, .height = 480U},
        true,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{2U},
        6};
    const pond::render::RenderTargetSnapshot maximizedSnapshot{
        windowId,
        pond::platform::PixelSize{.width = 640, .height = 480},
        pond::platform::LogicalSize{.width = 640U, .height = 480U},
        true,
        pond::platform::WindowState::Maximized,
        pond::render::PresentationEnvironmentRevision{2U},
        7};

    const pond::render::RenderTargetSnapshot hiddenZeroSizedSnapshot{
        windowId,
        pond::platform::PixelSize{},
        pond::platform::LogicalSize{},
        false,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{2U},
        8};
    const pond::render::RenderTargetSnapshot visibleZeroSizedSnapshot{
        windowId,
        pond::platform::PixelSize{},
        pond::platform::LogicalSize{},
        true,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{2U},
        9};

    EXPECT_TRUE(pond::render::IsValid(minimizedSnapshot));
    EXPECT_TRUE(pond::render::IsValid(restoredSnapshot));
    EXPECT_TRUE(pond::render::IsValid(maximizedSnapshot));
    EXPECT_TRUE(pond::render::IsValid(hiddenZeroSizedSnapshot));
    EXPECT_TRUE(pond::render::IsValid(visibleZeroSizedSnapshot));
    const pond::platform::PixelSize expectedPixelSize{.width = 640, .height = 480};
    const pond::platform::LogicalSize expectedLogicalSize{.width = 640, .height = 480};
    EXPECT_EQ(restoredSnapshot.GetWindowId(), windowId);
    EXPECT_EQ(restoredSnapshot.GetPixelSize(), expectedPixelSize);
    EXPECT_EQ(restoredSnapshot.GetLogicalSize(), expectedLogicalSize);
    EXPECT_TRUE(restoredSnapshot.IsVisible());
    EXPECT_EQ(minimizedSnapshot.GetWindowState(), pond::platform::WindowState::Minimized);
    EXPECT_EQ(restoredSnapshot.GetWindowState(), pond::platform::WindowState::Normal);
    EXPECT_EQ(maximizedSnapshot.GetWindowState(), pond::platform::WindowState::Maximized);
    EXPECT_EQ(restoredSnapshot.GetPresentationEnvironmentRevision(),
              pond::render::PresentationEnvironmentRevision{2U});
}

TEST(RenderBootstrapValueTests, RepresentsPresentationPreferencesAsSelectionState)
{
    const pond::render::SelectedPresentationConfig selection{
        .requestedPolicy = {.policy = pond::render::PresentationPolicy::LowLatencyVSync,
                            .strength = pond::render::RequirementStrength::Preferred},
        .actualPolicy = pond::render::PresentationPolicy::VSync,
        .policyFallback = pond::render::PresentationPolicyFallbackReason::UnavailableForTarget,
        .requestedQueuedLatency = {.maximumQueuedFrames = pond::render::QueuedFrameLatency{3},
                                   .strength = pond::render::RequirementStrength::Preferred},
        .actualQueuedLatency = pond::render::QueuedFrameLatency{2},
        .queuedLatencyFallback =
            pond::render::QueuedFrameLatencyFallbackReason::TargetMaximumExceeded,
        .output = pond::render::PresentationOutput::OpaqueSdrSrgb,
        .pixelExtent = pond::platform::PixelSize{.width = 800, .height = 600}};

    EXPECT_TRUE(pond::render::IsValid(selection));
    EXPECT_EQ(selection.actualPolicy, pond::render::PresentationPolicy::VSync);
    EXPECT_EQ(selection.actualQueuedLatency, pond::render::QueuedFrameLatency{2});

    pond::render::SelectedPresentationConfig invalidExtent = selection;
    invalidExtent.pixelExtent = pond::platform::PixelSize{};
    EXPECT_FALSE(pond::render::IsValid(invalidExtent));

    pond::render::SelectedPresentationConfig invalidRequiredFallback = selection;
    invalidRequiredFallback.requestedPolicy.strength = pond::render::RequirementStrength::Required;
    EXPECT_FALSE(pond::render::IsValid(invalidRequiredFallback));
}

TEST(RenderBootstrapValueTests, SeparatesWindowAndBackendRecreationMetadata)
{
    EXPECT_TRUE(pond::render::IsValid(pond::render::TargetRecreationInfo{
        .reason = pond::render::TargetRecreationReason::SizeChanged,
        .previousRevision = 4U,
        .currentRevision = 5U}));
    EXPECT_TRUE(pond::render::IsValid(
        pond::render::TargetRecreationInfo{.reason = pond::render::TargetRecreationReason::Restored,
                                           .previousRevision = 5U,
                                           .currentRevision = 6U}));
    EXPECT_FALSE(pond::render::IsValid(
        pond::render::TargetRecreationInfo{.reason = pond::render::TargetRecreationReason::Restored,
                                           .previousRevision = 6U,
                                           .currentRevision = 5U}));

    const pond::render::TargetRecreationInfo surfaceLost{
        .reason = pond::render::TargetRecreationReason::SurfaceLost};
    const pond::render::TargetRecreationInfo presentationChanged{
        .reason = pond::render::TargetRecreationReason::PresentationChanged};
    const pond::render::TargetRecreationInfo presentationEnvironmentChanged{
        .reason = pond::render::TargetRecreationReason::PresentationChanged,
        .previousRevision = 6U,
        .currentRevision = 7U};
    EXPECT_TRUE(pond::render::IsValid(surfaceLost));
    EXPECT_TRUE(pond::render::IsValid(presentationChanged));
    EXPECT_TRUE(pond::render::IsValid(presentationEnvironmentChanged));
    EXPECT_FALSE(surfaceLost.previousRevision.has_value());
    EXPECT_FALSE(surfaceLost.currentRevision.has_value());
    EXPECT_FALSE(presentationChanged.previousRevision.has_value());
    EXPECT_FALSE(presentationChanged.currentRevision.has_value());
    EXPECT_TRUE(presentationEnvironmentChanged.previousRevision.has_value());
    EXPECT_TRUE(presentationEnvironmentChanged.currentRevision.has_value());
}

TEST(RenderBootstrapValueTests, OwnsBackendDiagnosticStringsWithoutBackendDeclarations)
{
    const pond::render::BackendDiagnostic diagnostic{
        .backend = pond::render::RenderBackendKind::Vulkan,
        .renderCode = pond::render::RenderErrorCode::DeviceLost,
        .nativeCode = -4,
        .symbolicName = "VK_ERROR_DEVICE_LOST",
        .operation = "present",
        .validationContext = "frame end",
        .windowId = pond::platform::WindowId{42U},
        .targetLabel = "target/window:42"};

    const pond::render::OptionalBackendDiagnostic optionalDiagnostic{diagnostic};

    EXPECT_TRUE(pond::render::IsValid(diagnostic));
    ASSERT_TRUE(optionalDiagnostic.has_value());
    EXPECT_EQ(optionalDiagnostic->renderCode, pond::render::RenderErrorCode::DeviceLost);
    EXPECT_EQ(optionalDiagnostic->nativeCode, -4);
    EXPECT_EQ(optionalDiagnostic->symbolicName, "VK_ERROR_DEVICE_LOST");
    EXPECT_EQ(optionalDiagnostic->operation, "present");
    EXPECT_EQ(optionalDiagnostic->windowId, pond::platform::WindowId{42U});
    EXPECT_EQ(optionalDiagnostic->targetLabel, "target/window:42");
}

TEST(RenderBootstrapValueTests, OwnsDiagnosticSnapshotsWithoutBackendHandles)
{
    const pond::render::BackendDiagnostic backendFailure{
        .backend = pond::render::RenderBackendKind::Vulkan,
        .renderCode = pond::render::RenderErrorCode::BackendFailure,
        .nativeCode = -3,
        .symbolicName = "VK_ERROR_INITIALIZATION_FAILED",
        .operation = "vkCreateInstance",
        .validationContext = "instance creation failed"};
    const pond::render::RenderBootstrapDiagnostics bootstrapDiagnostics{
        .backend = pond::render::RenderBackendKind::Vulkan,
        .requestedValidationMode = pond::render::RenderValidationMode::Synchronization,
        .enabledValidationMode = pond::render::RenderValidationMode::Standard,
        .negotiatedApiVersion = 4'202'496U,
        .validationEnabled = true,
        .debugInstrumentation =
            pond::render::RenderDebugInstrumentation{.objectNames = true, .commandLabels = true},
        .lastFailure = backendFailure};
    const pond::render::RenderDeviceDiagnostics deviceDiagnostics{.targetCreateAttempts = 3U,
                                                                  .targetCreateSuccesses = 2U,
                                                                  .targetCreateFailures = 1U,
                                                                  .waitIdleCalls = 4U,
                                                                  .practicalWaitFallbacks = 5U,
                                                                  .deviceLost = true,
                                                                  .lastFailure = backendFailure};

    EXPECT_EQ(bootstrapDiagnostics.requestedValidationMode,
              pond::render::RenderValidationMode::Synchronization);
    EXPECT_EQ(bootstrapDiagnostics.enabledValidationMode,
              pond::render::RenderValidationMode::Standard);
    EXPECT_TRUE(bootstrapDiagnostics.validationEnabled);
    EXPECT_TRUE(bootstrapDiagnostics.debugInstrumentation.objectNames);
    EXPECT_TRUE(bootstrapDiagnostics.debugInstrumentation.commandLabels);
    EXPECT_FALSE(bootstrapDiagnostics.debugInstrumentation.timingMarkers);
    EXPECT_FALSE(bootstrapDiagnostics.debugInstrumentation.captureRegions);
    ASSERT_TRUE(bootstrapDiagnostics.lastFailure.has_value());
    EXPECT_EQ(bootstrapDiagnostics.lastFailure->nativeCode, -3);
    EXPECT_EQ(deviceDiagnostics.targetCreateAttempts, 3U);
    EXPECT_EQ(deviceDiagnostics.targetCreateSuccesses, 2U);
    EXPECT_EQ(deviceDiagnostics.targetCreateFailures, 1U);
    EXPECT_EQ(deviceDiagnostics.waitIdleCalls, 4U);
    EXPECT_EQ(deviceDiagnostics.practicalWaitFallbacks, 5U);
    EXPECT_TRUE(deviceDiagnostics.deviceLost);
    ASSERT_TRUE(deviceDiagnostics.lastFailure.has_value());
    EXPECT_EQ(deviceDiagnostics.lastFailure->symbolicName, "VK_ERROR_INITIALIZATION_FAILED");

    const pond::render::RenderTargetDiagnostics targetDiagnostics{
        .windowId = pond::platform::WindowId{7U},
        .targetLabel = "target/window:7",
        .status = pond::render::TargetStatus::Active,
        .swapchainGeneration = 9U,
        .recreationCount = 2U,
        .suspensionCount = 1U,
        .frameAcquireAttempts = 8U,
        .frameAcquireFailures = 3U,
        .framesPresented = 5U,
        .frameTimeouts = 1U,
        .frameFailures = 4U,
        .lastFailure = pond::render::BackendDiagnostic{
            .backend = pond::render::RenderBackendKind::Vulkan,
            .renderCode = pond::render::RenderErrorCode::SurfaceLost,
            .windowId = pond::platform::WindowId{7U},
            .targetLabel = "target/window:7"}};

    EXPECT_EQ(targetDiagnostics.windowId, pond::platform::WindowId{7U});
    EXPECT_EQ(targetDiagnostics.targetLabel, "target/window:7");
    EXPECT_EQ(targetDiagnostics.frameAcquireAttempts, 8U);
    EXPECT_EQ(targetDiagnostics.framesPresented, 5U);
    ASSERT_TRUE(targetDiagnostics.lastFailure.has_value());
    EXPECT_EQ(targetDiagnostics.lastFailure->renderCode,
              pond::render::RenderErrorCode::SurfaceLost);
}
TEST(RenderBootstrapValueTests, ValidatesAdapterSelectionValuesWithoutBackendHandles)
{
    pond::render::RenderAdapterId adapterId;
    adapterId.deviceUuid[0] = 1U;
    adapterId.deviceUuid[1] = 2U;

    const pond::render::RenderAdapterSnapshot snapshot{
        .adapterId = adapterId,
        .backend = pond::render::RenderBackendKind::Vulkan,
        .apiVersion = 1U,
        .identity =
            pond::render::RenderAdapterIdentity{.vendorId = 10U,
                                                .deviceId = 20U,
                                                .adapterType =
                                                    pond::render::RenderAdapterType::DiscreteGpu,
                                                .name = "public adapter snapshot"},
        .driver = pond::render::RenderAdapterDriverIdentity{.driverVersion = 30U,
                                                            .driverName = "driver",
                                                            .driverInfo = "driver info"},
        .limits = pond::render::RenderAdapterLimits{.maxImageDimension2D = 16384U,
                                                    .maxFramebufferWidth = 16384U,
                                                    .maxFramebufferHeight = 16384U,
                                                    .maxColorAttachments = 8U},
        .memoryHeaps = {pond::render::RenderAdapterMemoryHeap{.sizeBytes = 1024U,
                                                              .deviceLocal = true}},
        .presentation = pond::render::RenderPresentationCapabilities{
            .supportedPolicies = {pond::render::PresentationPolicy::VSync},
            .queuedLatency = {},
            .supportsWindowPresentation = true,
            .supportsOpaqueSdrSrgbOutput = true}};

    EXPECT_TRUE(pond::render::IsValid(adapterId));
    EXPECT_TRUE(pond::render::IsValid(pond::render::RenderAdapterSelectionDesc{
        .adapterPreference = pond::render::RenderAdapterPreference::Software}));
    EXPECT_TRUE(pond::render::IsValid(snapshot));
    EXPECT_TRUE(pond::render::IsValid(
        pond::render::RenderAdapterSelection{.request = pond::render::RenderAdapterSelectionDesc{},
                                             .selectedAdapter = snapshot,
                                             .compatibleAdapters = {snapshot}}));
    EXPECT_FALSE(pond::render::IsValid(pond::render::RenderAdapterSelectionDesc{
        .explicitAdapterId = pond::render::RenderAdapterId{}}));
}
} // namespace
