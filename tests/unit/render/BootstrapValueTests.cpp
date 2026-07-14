#include <ponder/render/Bootstrap.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <string>

namespace
{
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

static_assert(pond::render::IsValid(pond::render::PresentationPolicy::Default));
static_assert(pond::render::IsValid(pond::render::PresentationPolicy::Fifo));
static_assert(pond::render::IsValid(pond::render::PresentationPolicy::PreferMailbox));
static_assert(pond::render::IsValid(pond::render::PresentationPolicy::PreferImmediate));
static_assert(!pond::render::IsValid(static_cast<pond::render::PresentationPolicy>(255)));

static_assert(pond::render::IsValid(pond::render::SelectedPresentMode::Fifo));
static_assert(pond::render::IsValid(pond::render::SelectedPresentMode::Mailbox));
static_assert(pond::render::IsValid(pond::render::SelectedPresentMode::Immediate));
static_assert(!pond::render::IsValid(static_cast<pond::render::SelectedPresentMode>(255)));

static_assert(pond::render::IsValid(pond::render::SelectedCompositeAlpha::Opaque));
static_assert(pond::render::IsValid(pond::render::SelectedCompositeAlpha::PreMultiplied));
static_assert(pond::render::IsValid(pond::render::SelectedCompositeAlpha::PostMultiplied));
static_assert(pond::render::IsValid(pond::render::SelectedCompositeAlpha::Inherit));
static_assert(!pond::render::IsValid(static_cast<pond::render::SelectedCompositeAlpha>(255)));

static_assert(pond::render::IsValid(pond::render::TargetStatus::Active));
static_assert(pond::render::IsValid(pond::render::TargetStatus::Hidden));
static_assert(pond::render::IsValid(pond::render::TargetStatus::Minimized));
static_assert(pond::render::IsValid(pond::render::TargetStatus::Suspended));
static_assert(!pond::render::IsValid(static_cast<pond::render::TargetStatus>(255)));

static_assert(pond::render::IsValid(pond::render::FrameStatus::Ready));
static_assert(pond::render::IsValid(pond::render::FrameStatus::SkippedSuspended));
static_assert(pond::render::IsValid(pond::render::FrameStatus::TimedOut));
static_assert(pond::render::IsValid(pond::render::FrameStatus::Presented));
static_assert(pond::render::IsValid(pond::render::FrameStatus::Suboptimal));
static_assert(pond::render::IsValid(pond::render::FrameStatus::Recreated));
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
static_assert(pond::render::IsValid(pond::render::ClearColor{}));
static_assert(!pond::render::IsValid(pond::render::ClearColor{.red = -0.01F}));
static_assert(!pond::render::IsValid(pond::render::ClearColor{.alpha = 1.01F}));
static_assert(pond::render::IsValid(pond::render::TargetRecreationInfo{}));
static_assert(pond::render::IsValid(pond::render::RenderFrameResult{}));
static_assert(!pond::render::IsValid(pond::render::RenderFrameResult{
    .status = pond::render::FrameStatus::Presented, .presented = true}));
static_assert(!pond::render::IsValid(pond::render::TargetRecreationInfo{
    .reason = pond::render::TargetRecreationReason::SizeChanged,
    .previousRevision = 2,
    .currentRevision = 2}));

[[nodiscard]] constexpr pond::render::RenderTargetSnapshot MakeValidSnapshot(
    std::uint64_t revision = 1)
{
    return pond::render::RenderTargetSnapshot{pond::platform::WindowId{42},
                                              pond::platform::PixelSize{800, 600},
                                              true,
                                              false,
                                              true,
                                              revision};
}

static_assert(pond::render::IsValid(MakeValidSnapshot()));
static_assert(!pond::render::IsValid(pond::render::RenderTargetSnapshot{}));
static_assert(!pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{800, 600}, true, true, true, 1}));
static_assert(pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{}, true, false, true, 1}));
static_assert(pond::render::IsValid(pond::render::RenderTargetSnapshot{
    pond::platform::WindowId{42}, pond::platform::PixelSize{}, false, false, true, 1}));


TEST(RenderBootstrapValueTests, ValidatesFrameResultsWithoutBackendHandles)
{
    const pond::render::RenderFrameResult presented{
        .status = pond::render::FrameStatus::Presented,
        .targetStatus = pond::render::TargetStatus::Active,
        .windowId = pond::platform::WindowId{42},
        .targetRevision = 7U,
        .swapchainGeneration = 3U,
        .presented = true,
        .suboptimal = false};
    EXPECT_TRUE(pond::render::IsValid(presented));

    pond::render::RenderFrameResult invalidPresented = presented;
    invalidPresented.swapchainGeneration = 0U;
    EXPECT_FALSE(pond::render::IsValid(invalidPresented));
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
    EXPECT_EQ(targetDesc.queuedLatency.frameCount,
              pond::render::QueuedFrameLatency::kDefaultFrames);
}

TEST(RenderBootstrapValueTests, ValidatesTargetDescriptorTransactionInputs)
{
    const pond::render::RenderTargetDesc validDesc{.targetSnapshot = MakeValidSnapshot(),
                                                   .presentationPolicy =
                                                       pond::render::PresentationPolicy::Fifo,
                                                   .queuedLatency =
                                                       pond::render::QueuedFrameLatency{3},
                                                   .clearColor = pond::render::ClearColor{
                                                       .red = 0.1F,
                                                       .green = 0.2F,
                                                       .blue = 0.3F,
                                                       .alpha = 1.0F}};

    EXPECT_TRUE(pond::render::IsValid(validDesc));

    pond::render::RenderTargetDesc invalidLatency = validDesc;
    invalidLatency.queuedLatency.frameCount = 0;
    EXPECT_FALSE(pond::render::IsValid(invalidLatency));

    pond::render::RenderTargetDesc invalidPresentation = validDesc;
    invalidPresentation.presentationPolicy = static_cast<pond::render::PresentationPolicy>(255);
    EXPECT_FALSE(pond::render::IsValid(invalidPresentation));

    pond::render::RenderTargetDesc invalidClear = validDesc;
    invalidClear.clearColor.green = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(pond::render::IsValid(invalidClear));
}

TEST(RenderBootstrapValueTests, ValidatesDeviceQueuePlanSnapshotsWithoutBackendHandles)
{
    EXPECT_TRUE(pond::render::IsValid(pond::render::RenderDeviceOptionalCapabilities{}));
    EXPECT_FALSE(pond::render::IsValid(pond::render::RenderDeviceQueuePlan{}));

    const pond::render::RenderDeviceQueuePlan sharedQueuePlan{
        .graphicsQueueFamilyIndex = 0U,
        .presentationQueueFamilyIndex = 0U,
        .provisionedQueueFamilyIndices = {0U},
        .usesDistinctPresentationQueue = false};
    EXPECT_TRUE(pond::render::IsValid(sharedQueuePlan));

    const pond::render::RenderDeviceQueuePlan distinctQueuePlan{
        .graphicsQueueFamilyIndex = 0U,
        .presentationQueueFamilyIndex = 2U,
        .provisionedQueueFamilyIndices = {0U, 1U, 2U},
        .usesDistinctPresentationQueue = true};
    EXPECT_TRUE(pond::render::IsValid(distinctQueuePlan));

    pond::render::RenderDeviceQueuePlan duplicateFamilyPlan = distinctQueuePlan;
    duplicateFamilyPlan.provisionedQueueFamilyIndices.push_back(1U);
    EXPECT_FALSE(pond::render::IsValid(duplicateFamilyPlan));
}
TEST(RenderBootstrapValueTests, SnapshotStoresCopiedWindowStateWithoutNativePayload)
{
    const pond::platform::WindowId windowId{9};
    const pond::render::RenderTargetSnapshot minimizedSnapshot{
        windowId, pond::platform::PixelSize{}, false, true, false, 5};
    const pond::render::RenderTargetSnapshot restoredSnapshot{
        windowId, pond::platform::PixelSize{640, 480}, true, false, true, 6};

    const pond::render::RenderTargetSnapshot hiddenZeroSizedSnapshot{
        windowId, pond::platform::PixelSize{}, false, false, true, 7};
    const pond::render::RenderTargetSnapshot visibleZeroSizedSnapshot{
        windowId, pond::platform::PixelSize{}, true, false, true, 8};

    EXPECT_TRUE(pond::render::IsValid(minimizedSnapshot));
    EXPECT_TRUE(pond::render::IsValid(restoredSnapshot));
    EXPECT_TRUE(pond::render::IsValid(hiddenZeroSizedSnapshot));
    EXPECT_TRUE(pond::render::IsValid(visibleZeroSizedSnapshot));
    const pond::platform::PixelSize expectedPixelSize{640, 480};
    EXPECT_EQ(restoredSnapshot.GetWindowId(), windowId);
    EXPECT_EQ(restoredSnapshot.GetPixelSize(), expectedPixelSize);
    EXPECT_TRUE(restoredSnapshot.IsVisible());
    EXPECT_FALSE(restoredSnapshot.IsMinimized());
    EXPECT_TRUE(restoredSnapshot.IsRestored());
}

TEST(RenderBootstrapValueTests, RepresentsPresentationPreferencesAsSelectionState)
{
    const pond::render::SelectedPresentationConfig selection{
        .requestedPolicy = pond::render::PresentationPolicy::PreferMailbox,
        .selectedMode = pond::render::SelectedPresentMode::Fifo,
        .surfaceFormat = pond::render::RenderSurfaceFormatSnapshot{.formatCode = 50,
                                                                   .formatName = "format",
                                                                   .colorSpaceCode = 0,
                                                                   .colorSpaceName = "colorspace"},
        .compositeAlpha = pond::render::SelectedCompositeAlpha::Opaque,
        .pixelExtent = pond::platform::PixelSize{800, 600},
        .queuedLatency = pond::render::QueuedFrameLatency{2},
        .optionalPreferenceUnavailable = true,
        .queuedLatencyLimitedBySurface = true};

    EXPECT_TRUE(pond::render::IsValid(selection));
    EXPECT_TRUE(selection.optionalPreferenceUnavailable);
    EXPECT_TRUE(selection.queuedLatencyLimitedBySurface);

    pond::render::SelectedPresentationConfig invalidExtent = selection;
    invalidExtent.pixelExtent = pond::platform::PixelSize{};
    EXPECT_FALSE(pond::render::IsValid(invalidExtent));
}

TEST(RenderBootstrapValueTests, ValidatesRecreationMetadataOrdering)
{
    EXPECT_TRUE(pond::render::IsValid(pond::render::TargetRecreationInfo{
        .reason = pond::render::TargetRecreationReason::SurfaceLost,
        .previousRevision = 4,
        .currentRevision = 5}));
    EXPECT_FALSE(pond::render::IsValid(pond::render::TargetRecreationInfo{
        .reason = pond::render::TargetRecreationReason::SurfaceLost,
        .previousRevision = 5,
        .currentRevision = 4}));
}

TEST(RenderBootstrapValueTests, OwnsBackendDiagnosticStringsWithoutBackendDeclarations)
{
    const pond::render::BackendDiagnostic diagnostic{.backend = pond::render::RenderBackendKind::Vulkan,
                                                     .nativeCode = -4,
                                                     .symbolicName = "VK_ERROR_DEVICE_LOST",
                                                     .operation = "present",
                                                     .validationContext = "frame end"};

    const pond::render::OptionalBackendDiagnostic optionalDiagnostic{diagnostic};

    EXPECT_TRUE(pond::render::IsValid(diagnostic));
    ASSERT_TRUE(optionalDiagnostic.has_value());
    EXPECT_EQ(optionalDiagnostic->symbolicName, "VK_ERROR_DEVICE_LOST");
    EXPECT_EQ(optionalDiagnostic->operation, "present");
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
        .identity = pond::render::RenderAdapterIdentity{.vendorId = 10U,
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
        .queueFamilies = {pond::render::RenderQueueFamilySnapshot{.familyIndex = 0U,
                                                                  .queueCount = 1U,
                                                                  .supportsGraphics = true,
                                                                  .supportsPresentation = true}},
        .surfaceFormats = {pond::render::RenderSurfaceFormatSnapshot{.formatCode = 44,
                                                                     .formatName = "format",
                                                                     .colorSpaceCode = 0,
                                                                     .colorSpaceName = "colorspace"}},
        .presentModes = {pond::render::SelectedPresentMode::Fifo},
        .supportsSwapchain = true,
        .supportsSurfacePresentation = true};

    EXPECT_TRUE(pond::render::IsValid(adapterId));
    EXPECT_TRUE(pond::render::IsValid(pond::render::RenderAdapterSelectionDesc{
        .adapterPreference = pond::render::RenderAdapterPreference::Software}));
    EXPECT_TRUE(pond::render::IsValid(snapshot));
    EXPECT_TRUE(pond::render::IsValid(pond::render::RenderAdapterSelection{
        .request = pond::render::RenderAdapterSelectionDesc{},
        .selectedAdapter = snapshot,
        .compatibleAdapters = {snapshot}}));
    EXPECT_FALSE(pond::render::IsValid(pond::render::RenderAdapterSelectionDesc{
        .explicitAdapterId = pond::render::RenderAdapterId{}}));
}
} // namespace
