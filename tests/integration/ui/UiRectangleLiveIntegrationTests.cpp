#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>
#include <ponder/ui/Metrics.hpp>
#include <ponder/ui/experimental/RectangleRenderer.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <utility>

#include "../LiveRenderTestSupport.hpp"
#include "RenderLiveTestAccess.hpp"

namespace
{
namespace core = pond::core;
namespace experimental = pond::ui::experimental;
namespace platform = pond::platform;
namespace render = pond::render;
namespace ui = pond::ui;

inline constexpr render::ClearColor kContrastingClearColor{
    .red = 0.025F, .green = 0.045F, .blue = 0.085F, .alpha = 1.0F};
inline constexpr auto kPlatformEventTimeout = std::chrono::seconds{3};

struct PresentedRectangleFrame final
{
    ui::UiTargetMetrics metrics{};
    render::RenderFrameResult presentation{};
};

[[nodiscard]] core::Error MakeLiveTestError(std::string message)
{
    return core::Error{render::ToErrorCode(render::RenderErrorCode::BackendFailure),
                       std::move(message)};
}

template <typename Value>
[[nodiscard]] core::Result<Value> MakeLiveTestFailure(std::string message)
{
    return core::Result<Value>::FromError(MakeLiveTestError(std::move(message)));
}

[[nodiscard]] std::array<experimental::RectanglePaint, 3U> MakeRectangleScene(
    const ui::UiTargetMetrics& metrics) noexcept
{
    const ui::LogicalSize extent = metrics.GetLogicalSize();
    return {
        experimental::RectanglePaint{
            .rectangle = {.origin = {.x = extent.width * 0.08F, .y = extent.height * 0.10F},
                          .size = {.width = extent.width * 0.56F, .height = extent.height * 0.66F}},
            .color = {.red = 0.16F, .green = 0.48F, .blue = 0.92F, .alpha = 1.0F}},
        experimental::RectanglePaint{
            .rectangle = {.origin = {.x = extent.width * 0.28F, .y = extent.height * 0.22F},
                          .size = {.width = extent.width * 0.48F, .height = extent.height * 0.58F}},
            .color = {.red = 1.0F, .green = 0.22F, .blue = 0.06F, .alpha = 0.62F}},
        experimental::RectanglePaint{
            .rectangle = {.origin = {.x = extent.width * 0.45F, .y = extent.height * 0.36F},
                          .size = {.width = extent.width * 0.45F, .height = extent.height * 0.52F}},
            .color = {.red = 0.02F, .green = 0.92F, .blue = 0.65F, .alpha = 0.58F}}};
}

[[nodiscard]] core::Result<PresentedRectangleFrame> PresentRectangleFrame(
    render::RenderTarget& target, experimental::RectangleRenderer& rectangleRenderer)
{
    constexpr std::size_t kMaximumFrameAttempts = 32U;
    for (std::size_t attempt = 0U; attempt < kMaximumFrameAttempts; ++attempt)
    {
        auto frameResult = target.AcquireFrame();
        if (!frameResult)
        {
            return core::Result<PresentedRectangleFrame>::FromError(
                std::move(frameResult).GetError());
        }
        render::RenderFrame frame = std::move(frameResult).GetValue();

        if (core::VoidResult clear = frame.Clear(kContrastingClearColor); !clear)
        {
            return core::Result<PresentedRectangleFrame>::FromError(std::move(clear).GetError());
        }

        auto metricsResult = experimental::MakeUiTargetMetricsForFrame(frame);
        if (!metricsResult)
        {
            return core::Result<PresentedRectangleFrame>::FromError(
                std::move(metricsResult).GetError());
        }
        const ui::UiTargetMetrics metrics = std::move(metricsResult).GetValue();
        const std::array rectangles = MakeRectangleScene(metrics);

        auto recordResult = rectangleRenderer.Record(frame, metrics, rectangles);
        if (!recordResult)
        {
            return core::Result<PresentedRectangleFrame>::FromError(
                std::move(recordResult).GetError());
        }

        auto presentationResult = frame.FinishAndPresent();
        if (!presentationResult)
        {
            return core::Result<PresentedRectangleFrame>::FromError(
                std::move(presentationResult).GetError());
        }
        const render::RenderFrameResult presentation = presentationResult.GetValue();
        if (presentation.presented)
        {
            if (recordResult.GetValue() != experimental::RectangleRecordOutcome::Recorded)
            {
                return MakeLiveTestFailure<PresentedRectangleFrame>(
                    "A presented rectangle frame did not record its production Draw2D stage.");
            }
            return PresentedRectangleFrame{.metrics = metrics, .presentation = presentation};
        }

        if (presentation.status != render::FrameStatus::TimedOut &&
            presentation.status != render::FrameStatus::RecreationPending &&
            presentation.status != render::FrameStatus::SkippedSuspended)
        {
            return MakeLiveTestFailure<PresentedRectangleFrame>(
                "A live rectangle frame completed with an unexpected non-presented status.");
        }
    }

    return MakeLiveTestFailure<PresentedRectangleFrame>(
        "No live rectangle frame was presented within the bounded attempt count.");
}

[[nodiscard]] core::VoidResult ExerciseSuspendedRectangleFrame(
    render::RenderTarget& target, experimental::RectangleRenderer& rectangleRenderer)
{
    auto frameResult = target.AcquireFrame();
    if (!frameResult)
    {
        return core::VoidResult::FromError(std::move(frameResult).GetError());
    }
    render::RenderFrame frame = std::move(frameResult).GetValue();

    if (core::VoidResult clear = frame.Clear(kContrastingClearColor); !clear)
    {
        return clear;
    }

    auto metricsResult = experimental::MakeUiTargetMetricsForFrame(frame);
    if (!metricsResult)
    {
        return core::VoidResult::FromError(std::move(metricsResult).GetError());
    }
    const ui::UiTargetMetrics metrics = std::move(metricsResult).GetValue();
    const std::array rectangles = MakeRectangleScene(metrics);
    auto recordResult = rectangleRenderer.Record(frame, metrics, rectangles);
    if (!recordResult)
    {
        return core::VoidResult::FromError(std::move(recordResult).GetError());
    }
    if (recordResult.GetValue() != experimental::RectangleRecordOutcome::SkippedSuspended)
    {
        return core::VoidResult::FromError(
            MakeLiveTestError("A minimized rectangle frame did not report a suspended skip."));
    }

    auto presentationResult = frame.FinishAndPresent();
    if (!presentationResult)
    {
        return core::VoidResult::FromError(std::move(presentationResult).GetError());
    }
    if (presentationResult.GetValue().presented ||
        presentationResult.GetValue().status != render::FrameStatus::SkippedSuspended)
    {
        return core::VoidResult::FromError(MakeLiveTestError(
            "A minimized rectangle frame unexpectedly presented or returned the wrong status."));
    }

    return core::VoidResult::Success();
}

void ExpectMetricsMatchSnapshot(const ui::UiTargetMetrics& metrics,
                                const render::RenderTargetSnapshot& snapshot)
{
    const ui::LogicalSize logicalSize = metrics.GetLogicalSize();
    const ui::FramebufferPixelSize pixelSize = metrics.GetFramebufferPixelSize();
    EXPECT_EQ(metrics.GetTargetId().GetValue(), snapshot.GetWindowId().GetValue());
    EXPECT_EQ(metrics.GetTargetRevision().GetValue(), snapshot.GetRevision());
    EXPECT_EQ(metrics.GetMetricsRevision().GetValue(),
              snapshot.GetPresentationEnvironmentRevision().GetValue());
    EXPECT_EQ(logicalSize.width, static_cast<float>(snapshot.GetLogicalSize().width));
    EXPECT_EQ(logicalSize.height, static_cast<float>(snapshot.GetLogicalSize().height));
    EXPECT_EQ(pixelSize.width, snapshot.GetPixelSize().width);
    EXPECT_EQ(pixelSize.height, snapshot.GetPixelSize().height);
}

template <typename TargetStats>
void ExpectUploadArenaConsistent(const TargetStats& stats)
{
    EXPECT_TRUE(stats.hasUploadArena);
    EXPECT_GT(stats.slotCount, 0U);
    EXPECT_EQ(stats.idleSlotCount + stats.reservedSlotCount + stats.submittedSlotCount,
              stats.slotCount);
    EXPECT_GT(stats.currentCapacityBytes, 0U);
    EXPECT_GT(stats.allocationCount, 0U);
    EXPECT_GT(stats.generationCount, 0U);
}

void ExpectValidationConfiguration(const render::RenderBootstrap& bootstrap)
{
    const render::RenderValidationMode requested = render::test::GetLiveRenderValidationMode();
    const render::RenderBootstrapDiagnostics diagnostics = bootstrap.GetDiagnostics();
    EXPECT_EQ(diagnostics.requestedValidationMode, requested);
    EXPECT_FALSE(diagnostics.lastFailure.has_value());

    if (requested == render::RenderValidationMode::Default)
    {
        EXPECT_TRUE(diagnostics.enabledValidationMode == render::RenderValidationMode::Standard ||
                    diagnostics.enabledValidationMode == render::RenderValidationMode::Disabled);
        EXPECT_EQ(diagnostics.validationEnabled,
                  diagnostics.enabledValidationMode == render::RenderValidationMode::Standard);
    }
    else if (requested == render::RenderValidationMode::Disabled)
    {
        EXPECT_FALSE(diagnostics.validationEnabled);
        EXPECT_EQ(diagnostics.enabledValidationMode, render::RenderValidationMode::Disabled);
    }
    else
    {
        EXPECT_TRUE(diagnostics.validationEnabled);
        EXPECT_EQ(diagnostics.enabledValidationMode, requested);
    }
}

TEST(UiRectangleLiveIntegrationTests, ExercisesProductionPathLifecycleAndSteadyState)
{
    auto runtimeResult = platform::PlatformRuntime::Create(platform::PlatformRuntimeDesc{});
    if (!runtimeResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                runtimeResult.GetError(), render::test::LivePrerequisiteOperation::PlatformRuntime,
                "UI live platform runtime creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << runtimeResult.GetError().GetMessage();
    }
    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const platform::WindowDesc firstWindowDesc{
        .title = "Ponder UI Live Rectangle A",
        .logicalSize = {.width = 480U, .height = 320U},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .graphicsCompatibility = render::GetRequiredWindowGraphicsCompatibility()};
    const platform::WindowDesc secondWindowDesc{
        .title = "Ponder UI Live Rectangle B",
        .logicalSize = {.width = 360U, .height = 260U},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .graphicsCompatibility = render::GetRequiredWindowGraphicsCompatibility()};

    auto firstWindowResult = runtime.CreateWindow(firstWindowDesc);
    if (!firstWindowResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                firstWindowResult.GetError(),
                render::test::LivePrerequisiteOperation::PlatformWindow,
                "First UI live Vulkan window creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << firstWindowResult.GetError().GetMessage();
    }
    std::optional<platform::Window> firstWindow{std::move(firstWindowResult).GetValue()};

    auto secondWindowResult = runtime.CreateWindow(secondWindowDesc);
    if (!secondWindowResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                secondWindowResult.GetError(),
                render::test::LivePrerequisiteOperation::PlatformWindow,
                "Second UI live Vulkan window creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << secondWindowResult.GetError().GetMessage();
    }
    platform::Window secondWindow = std::move(secondWindowResult).GetValue();

    auto firstSnapshotResult = render::test::CaptureWindowSnapshot(*firstWindow, 1U);
    ASSERT_TRUE(firstSnapshotResult) << firstSnapshotResult.GetError().GetMessage();
    render::RenderTargetSnapshot firstSnapshot = std::move(firstSnapshotResult).GetValue();
    auto secondSnapshotResult = render::test::CaptureWindowSnapshot(secondWindow, 1U);
    ASSERT_TRUE(secondSnapshotResult) << secondSnapshotResult.GetError().GetMessage();
    const render::RenderTargetSnapshot secondSnapshot = std::move(secondSnapshotResult).GetValue();

    auto bootstrapResult =
        render::RenderBootstrap::Create(render::test::MakeLiveRenderBootstrapDesc());
    if (!bootstrapResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                bootstrapResult.GetError(), render::test::LivePrerequisiteOperation::Bootstrap,
                "UI live Vulkan bootstrap creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << bootstrapResult.GetError().GetMessage();
    }
    render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto firstSurfaceResult = bootstrap.PrepareSurface(
        *firstWindow,
        render::SurfacePreparationDesc{.targetSnapshot = firstSnapshot,
                                       .reason = render::SurfacePreparationReason::Initial});
    if (!firstSurfaceResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                firstSurfaceResult.GetError(), render::test::LivePrerequisiteOperation::Surface,
                "First UI live Vulkan surface preparation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << firstSurfaceResult.GetError().GetMessage();
    }
    render::PreparedSurface firstSurface = std::move(firstSurfaceResult).GetValue();
    ExpectValidationConfiguration(bootstrap);

    auto secondSurfaceResult = bootstrap.PrepareSurface(
        secondWindow,
        render::SurfacePreparationDesc{.targetSnapshot = secondSnapshot,
                                       .reason = render::SurfacePreparationReason::Initial});
    if (!secondSurfaceResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                secondSurfaceResult.GetError(), render::test::LivePrerequisiteOperation::Surface,
                "Second UI live Vulkan surface preparation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << secondSurfaceResult.GetError().GetMessage();
    }
    render::PreparedSurface secondSurface = std::move(secondSurfaceResult).GetValue();

    ASSERT_TRUE(firstSurface.TransferToCurrentThread(firstSnapshot));
    ASSERT_TRUE(secondSurface.TransferToCurrentThread(secondSnapshot));
    auto selectionResult =
        bootstrap.SelectAdapter(firstSurface, render::RenderAdapterSelectionDesc{});
    if (!selectionResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                selectionResult.GetError(), render::test::LivePrerequisiteOperation::Adapter,
                "UI live Vulkan adapter selection"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << selectionResult.GetError().GetMessage();
    }

    auto deviceResult = bootstrap.CreateDevice(firstSurface, selectionResult.GetValue(),
                                               render::RenderDeviceDesc{});
    if (!deviceResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                deviceResult.GetError(), render::test::LivePrerequisiteOperation::Device,
                "UI live Vulkan logical-device creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << deviceResult.GetError().GetMessage();
    }
    render::RenderDevice device = std::move(deviceResult).GetValue();

    auto firstTargetResult = device.CreateRenderTarget(
        std::move(firstSurface), render::RenderTargetDesc{.targetSnapshot = firstSnapshot,
                                                          .clearColor = kContrastingClearColor});
    if (!firstTargetResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                firstTargetResult.GetError(), render::test::LivePrerequisiteOperation::Target,
                "First UI live render-target creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << firstTargetResult.GetError().GetMessage();
    }
    render::RenderTarget firstTarget = std::move(firstTargetResult).GetValue();

    auto secondTargetResult = device.CreateRenderTarget(
        std::move(secondSurface), render::RenderTargetDesc{.targetSnapshot = secondSnapshot,
                                                           .clearColor = kContrastingClearColor});
    if (!secondTargetResult)
    {
        if (auto reason = render::test::MakeOptionalLiveSkipReason(
                secondTargetResult.GetError(), render::test::LivePrerequisiteOperation::Target,
                "Second UI live render-target creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << secondTargetResult.GetError().GetMessage();
    }
    render::RenderTarget secondTarget = std::move(secondTargetResult).GetValue();

    auto rectangleRendererResult = experimental::RectangleRenderer::Create(device);
    ASSERT_TRUE(rectangleRendererResult) << rectangleRendererResult.GetError().GetMessage();
    experimental::RectangleRenderer rectangleRenderer =
        std::move(rectangleRendererResult).GetValue();
    EXPECT_EQ(render::detail::RenderLiveTestAccess::GetDraw2DDeviceStats(device).activeLayerCount,
              1U);

    constexpr std::size_t kWarmFramesPerTarget = 8U;
    for (std::size_t frameIndex = 0U; frameIndex < kWarmFramesPerTarget; ++frameIndex)
    {
        auto firstFrame = PresentRectangleFrame(firstTarget, rectangleRenderer);
        ASSERT_TRUE(firstFrame) << firstFrame.GetError().GetMessage();
        ExpectMetricsMatchSnapshot(firstFrame.GetValue().metrics, firstSnapshot);

        auto secondFrame = PresentRectangleFrame(secondTarget, rectangleRenderer);
        ASSERT_TRUE(secondFrame) << secondFrame.GetError().GetMessage();
        ExpectMetricsMatchSnapshot(secondFrame.GetValue().metrics, secondSnapshot);
    }

    const auto warmedDeviceStats =
        render::detail::RenderLiveTestAccess::GetDraw2DDeviceStats(device);
    const auto warmedFirstTargetStats =
        render::detail::RenderLiveTestAccess::GetDraw2DTargetStats(firstTarget);
    const auto warmedSecondTargetStats =
        render::detail::RenderLiveTestAccess::GetDraw2DTargetStats(secondTarget);
    EXPECT_TRUE(warmedDeviceStats.hasPipeline);
    EXPECT_EQ(warmedDeviceStats.pipelineCreationCount, 1U);
    EXPECT_EQ(warmedDeviceStats.pipelineReplacementCount, 0U);
    EXPECT_GT(warmedDeviceStats.pipelineReuseCount, 0U);
    ExpectUploadArenaConsistent(warmedFirstTargetStats);
    ExpectUploadArenaConsistent(warmedSecondTargetStats);

    constexpr std::size_t kSteadyFramesPerTarget = 6U;
    for (std::size_t frameIndex = 0U; frameIndex < kSteadyFramesPerTarget; ++frameIndex)
    {
        auto firstFrame = PresentRectangleFrame(firstTarget, rectangleRenderer);
        ASSERT_TRUE(firstFrame) << firstFrame.GetError().GetMessage();
        auto secondFrame = PresentRectangleFrame(secondTarget, rectangleRenderer);
        ASSERT_TRUE(secondFrame) << secondFrame.GetError().GetMessage();
    }

    const auto steadyDeviceStats =
        render::detail::RenderLiveTestAccess::GetDraw2DDeviceStats(device);
    const auto steadyFirstTargetStats =
        render::detail::RenderLiveTestAccess::GetDraw2DTargetStats(firstTarget);
    const auto steadySecondTargetStats =
        render::detail::RenderLiveTestAccess::GetDraw2DTargetStats(secondTarget);
    EXPECT_EQ(steadyDeviceStats.pipelineCreationCount, warmedDeviceStats.pipelineCreationCount);
    EXPECT_EQ(steadyDeviceStats.pipelineReplacementCount,
              warmedDeviceStats.pipelineReplacementCount);
    EXPECT_GE(steadyDeviceStats.pipelineReuseCount,
              warmedDeviceStats.pipelineReuseCount + (2U * kSteadyFramesPerTarget));
    EXPECT_EQ(steadyFirstTargetStats.currentCapacityBytes,
              warmedFirstTargetStats.currentCapacityBytes);
    EXPECT_EQ(steadyFirstTargetStats.allocationCount, warmedFirstTargetStats.allocationCount);
    EXPECT_EQ(steadyFirstTargetStats.growthCount, warmedFirstTargetStats.growthCount);
    EXPECT_EQ(steadyFirstTargetStats.generationCount, warmedFirstTargetStats.generationCount);
    EXPECT_EQ(steadySecondTargetStats.currentCapacityBytes,
              warmedSecondTargetStats.currentCapacityBytes);
    EXPECT_EQ(steadySecondTargetStats.allocationCount, warmedSecondTargetStats.allocationCount);
    EXPECT_EQ(steadySecondTargetStats.growthCount, warmedSecondTargetStats.growthCount);
    EXPECT_EQ(steadySecondTargetStats.generationCount, warmedSecondTargetStats.generationCount);
    EXPECT_EQ(device.GetDiagnostics().waitIdleCalls, 0U);
    EXPECT_EQ(device.GetDiagnostics().practicalWaitFallbacks, 0U);

    render::test::CoalescedWindowEvents firstEvents{.windowId = firstWindow->GetId()};
    render::test::DrainPlatformEvents(runtime, firstEvents);
    constexpr std::array<platform::LogicalSize, 4U> kResizeStorm{
        platform::LogicalSize{.width = 520U, .height = 340U},
        platform::LogicalSize{.width = 560U, .height = 360U},
        platform::LogicalSize{.width = 540U, .height = 350U},
        platform::LogicalSize{.width = 600U, .height = 400U}};
    for (const platform::LogicalSize logicalSize : kResizeStorm)
    {
        ASSERT_TRUE(firstWindow->SetLogicalSize(logicalSize));
    }
    ASSERT_TRUE(render::test::PumpPlatformEventsUntil(
        runtime, firstEvents,
        [&firstEvents, &firstWindow]()
        {
            const auto logicalSize = firstWindow->GetLogicalSize();
            const auto pixelSize = firstWindow->GetPixelSize();
            return firstEvents.GetResizeEventCount() > 0U && logicalSize &&
                   logicalSize.GetValue() == platform::LogicalSize{.width = 600U, .height = 400U} &&
                   pixelSize && pixelSize.GetValue().width > 0U && pixelSize.GetValue().height > 0U;
        },
        kPlatformEventTimeout));
    EXPECT_GT(firstEvents.GetResizeEventCount(), 0U);

    const std::uint64_t firstGenerationBeforeResize = firstTarget.GetSwapchainGeneration();
    const std::uint64_t firstRecreationsBeforeResize = firstTarget.GetDiagnostics().recreationCount;
    auto resizedSnapshotResult = render::test::CaptureWindowSnapshot(*firstWindow, 2U);
    ASSERT_TRUE(resizedSnapshotResult) << resizedSnapshotResult.GetError().GetMessage();
    firstSnapshot = std::move(resizedSnapshotResult).GetValue();
    ASSERT_TRUE(firstTarget.UpdateTargetSnapshot(firstSnapshot));
    const std::uint64_t secondGenerationBeforeFirstResize = secondTarget.GetSwapchainGeneration();
    auto resizedFrame = PresentRectangleFrame(firstTarget, rectangleRenderer);
    ASSERT_TRUE(resizedFrame) << resizedFrame.GetError().GetMessage();
    ExpectMetricsMatchSnapshot(resizedFrame.GetValue().metrics, firstSnapshot);
    EXPECT_GT(firstTarget.GetSwapchainGeneration(), firstGenerationBeforeResize);
    EXPECT_GT(firstTarget.GetDiagnostics().recreationCount, firstRecreationsBeforeResize);
    EXPECT_EQ(secondTarget.GetSwapchainGeneration(), secondGenerationBeforeFirstResize);
    auto isolatedSecondFrame = PresentRectangleFrame(secondTarget, rectangleRenderer);
    ASSERT_TRUE(isolatedSecondFrame) << isolatedSecondFrame.GetError().GetMessage();

    const std::size_t stateChangesBeforeMinimize = firstEvents.stateChanges;
    ASSERT_TRUE(firstWindow->Minimize());
    ASSERT_TRUE(render::test::PumpPlatformEventsUntil(
        runtime, firstEvents,
        [&firstEvents, &firstWindow, stateChangesBeforeMinimize]()
        {
            const auto state = firstWindow->GetState();
            return firstEvents.stateChanges > stateChangesBeforeMinimize && state &&
                   state.GetValue() == platform::WindowState::Minimized;
        },
        kPlatformEventTimeout));
    auto minimizedSnapshotResult = render::test::CaptureWindowSnapshot(*firstWindow, 3U);
    ASSERT_TRUE(minimizedSnapshotResult) << minimizedSnapshotResult.GetError().GetMessage();
    firstSnapshot = std::move(minimizedSnapshotResult).GetValue();
    ASSERT_TRUE(firstTarget.UpdateTargetSnapshot(firstSnapshot));
    EXPECT_TRUE(firstTarget.IsSuspended());
    EXPECT_FALSE(firstTarget.HasSwapchain());
    const core::VoidResult suspendedFrame =
        ExerciseSuspendedRectangleFrame(firstTarget, rectangleRenderer);
    ASSERT_TRUE(suspendedFrame) << suspendedFrame.GetError().GetMessage();
    auto secondWhileFirstMinimized = PresentRectangleFrame(secondTarget, rectangleRenderer);
    ASSERT_TRUE(secondWhileFirstMinimized) << secondWhileFirstMinimized.GetError().GetMessage();

    const std::uint64_t firstGenerationBeforeRestore = firstTarget.GetSwapchainGeneration();
    const std::size_t stateChangesBeforeRestore = firstEvents.stateChanges;
    ASSERT_TRUE(firstWindow->Restore());
    ASSERT_TRUE(render::test::PumpPlatformEventsUntil(
        runtime, firstEvents,
        [&firstEvents, &firstWindow, stateChangesBeforeRestore]()
        {
            const auto state = firstWindow->GetState();
            const auto pixelSize = firstWindow->GetPixelSize();
            return firstEvents.stateChanges > stateChangesBeforeRestore && state &&
                   state.GetValue() != platform::WindowState::Minimized && pixelSize &&
                   pixelSize.GetValue().width > 0U && pixelSize.GetValue().height > 0U;
        },
        kPlatformEventTimeout));
    auto restoredSnapshotResult = render::test::CaptureWindowSnapshot(*firstWindow, 4U);
    ASSERT_TRUE(restoredSnapshotResult) << restoredSnapshotResult.GetError().GetMessage();
    firstSnapshot = std::move(restoredSnapshotResult).GetValue();
    ASSERT_TRUE(firstTarget.UpdateTargetSnapshot(firstSnapshot));
    auto restoredFrame = PresentRectangleFrame(firstTarget, rectangleRenderer);
    ASSERT_TRUE(restoredFrame) << restoredFrame.GetError().GetMessage();
    ExpectMetricsMatchSnapshot(restoredFrame.GetValue().metrics, firstSnapshot);
    EXPECT_GT(firstTarget.GetSwapchainGeneration(), firstGenerationBeforeRestore);

    const render::RenderTargetDiagnostics firstDiagnostics = firstTarget.GetDiagnostics();
    const render::RenderTargetDiagnostics secondDiagnostics = secondTarget.GetDiagnostics();
    EXPECT_FALSE(firstDiagnostics.lastFailure.has_value());
    EXPECT_FALSE(secondDiagnostics.lastFailure.has_value());
    EXPECT_EQ(device.GetDiagnostics().waitIdleCalls, 0U);
    EXPECT_EQ(device.GetDiagnostics().practicalWaitFallbacks, 0U);

    firstTarget = render::RenderTarget{};
    firstWindow.reset();
    EXPECT_EQ(device.GetActiveTargetCount(), 1U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 1U);
    auto survivingTargetFrame = PresentRectangleFrame(secondTarget, rectangleRenderer);
    ASSERT_TRUE(survivingTargetFrame) << survivingTargetFrame.GetError().GetMessage();
    EXPECT_FALSE(secondTarget.GetDiagnostics().lastFailure.has_value());

    rectangleRenderer = experimental::RectangleRenderer{};
    EXPECT_EQ(render::detail::RenderLiveTestAccess::GetDraw2DDeviceStats(device).activeLayerCount,
              0U);
    secondTarget = render::RenderTarget{};
    EXPECT_EQ(device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);
    EXPECT_EQ(device.GetDiagnostics().waitIdleCalls, 0U);
    EXPECT_FALSE(device.GetDiagnostics().lastFailure.has_value());

    device = render::RenderDevice{};
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);
    EXPECT_FALSE(bootstrap.GetDiagnostics().lastFailure.has_value());

    const render::RenderValidationReport validationReport = bootstrap.GetValidationReport();
    EXPECT_TRUE(validationReport.IsClean())
        << render::test::FormatValidationReport(validationReport);
    EXPECT_EQ(validationReport.droppedMessageCount, 0U)
        << render::test::FormatValidationReport(validationReport);

    const core::VoidResult shutdown = bootstrap.Shutdown();
    ASSERT_TRUE(shutdown) << shutdown.GetError().GetMessage();
    EXPECT_FALSE(bootstrap.IsValid());
}
} // namespace
