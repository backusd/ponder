#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>

#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "../LiveRenderTestSupport.hpp"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef CreateWindow
#undef GetMessage
#endif

namespace
{
[[nodiscard]] pond::render::RenderTargetSnapshot MakeSnapshot(
    const pond::platform::Window& window, std::uint64_t revision,
    pond::render::PresentationEnvironmentRevision presentationEnvironmentRevision =
        pond::render::PresentationEnvironmentRevision{1U})
{
    const auto pixelSize = window.GetPixelSize();
    const auto logicalSize = window.GetLogicalSize();
    const auto visible = window.IsVisible();
    const auto windowState = window.GetState();
    if (!pixelSize || !logicalSize || !visible || !windowState)
    {
        return {};
    }

    return pond::render::RenderTargetSnapshot{window.GetId(),
                                              pixelSize.GetValue(),
                                              logicalSize.GetValue(),
                                              visible.GetValue(),
                                              windowState.GetValue(),
                                              presentationEnvironmentRevision,
                                              revision};
}

void ExpectValidationClean(const pond::render::RenderBootstrap& bootstrap)
{
    const pond::render::RenderValidationReport report = bootstrap.GetValidationReport();
    EXPECT_TRUE(report.IsClean()) << pond::render::test::FormatValidationReport(report);
    EXPECT_EQ(report.droppedMessageCount, 0U) << pond::render::test::FormatValidationReport(report);
}

#if defined(_WIN32)
[[nodiscard]] bool RequestNativeClose(pond::platform::Window& window)
{
    auto nativeHandle = window.GetNativeHandle();
    if (!nativeHandle)
    {
        return false;
    }

    const auto* win32 = std::get_if<pond::platform::NativeWin32Window>(&nativeHandle.GetValue());
    return win32 != nullptr && win32->window != nullptr &&
           PostMessageW(static_cast<HWND>(win32->window), WM_CLOSE, 0U, 0) != FALSE;
}
#endif

[[nodiscard]] pond::render::RenderTargetSnapshot MakeWindowStateSnapshot(
    const pond::platform::Window& window, pond::platform::PixelSize pixelSize, bool visible,
    pond::platform::WindowState windowState, std::uint64_t revision,
    pond::render::PresentationEnvironmentRevision presentationEnvironmentRevision =
        pond::render::PresentationEnvironmentRevision{1U})
{
    return pond::render::RenderTargetSnapshot{
        window.GetId(), pixelSize,   pond::platform::LogicalSize{pixelSize.width, pixelSize.height},
        visible,        windowState, presentationEnvironmentRevision,
        revision};
}

[[nodiscard]] constexpr pond::render::ClearColor MakeClearColor(float red, float green,
                                                                float blue) noexcept
{
    return pond::render::ClearColor{.red = red, .green = green, .blue = blue, .alpha = 1.0F};
}

[[nodiscard]] pond::core::Result<pond::render::PreparedSurface> PrepareTransferredSurface(
    pond::render::RenderBootstrap& bootstrap, pond::platform::Window& window,
    pond::render::RenderTargetSnapshot initialSnapshot,
    pond::render::RenderTargetSnapshot transferSnapshot)
{
    auto surfaceResult = bootstrap.PrepareSurface(
        window, pond::render::SurfacePreparationDesc{
                    .targetSnapshot = initialSnapshot,
                    .reason = pond::render::SurfacePreparationReason::Initial});
    if (!surfaceResult)
    {
        return pond::core::Result<pond::render::PreparedSurface>::FromError(
            std::move(surfaceResult).GetError());
    }

    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();
    if (pond::core::VoidResult transfer = surface.TransferToCurrentThread(transferSnapshot);
        !transfer)
    {
        return pond::core::Result<pond::render::PreparedSurface>::FromError(
            std::move(transfer).GetError());
    }

    return pond::core::Result<pond::render::PreparedSurface>::FromValue(std::move(surface));
}

void ExpectPresentedFrame(pond::render::RenderTarget& target, pond::render::ClearColor clearColor,
                          pond::render::RenderFrameResult* observedResult = nullptr)
{
    constexpr std::size_t kMaximumFrameAttempts = 16U;
    for (std::size_t attempt = 0U; attempt < kMaximumFrameAttempts; ++attempt)
    {
        auto frameResult = target.AcquireFrame();
        ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
        pond::render::RenderFrame frame = std::move(frameResult).GetValue();
        ASSERT_TRUE(frame.IsValid());

        const pond::core::VoidResult clearResult = frame.Clear(clearColor);
        ASSERT_TRUE(clearResult) << clearResult.GetError().GetMessage();

        auto presentResult = frame.FinishAndPresent();
        ASSERT_TRUE(presentResult) << presentResult.GetError().GetMessage();
        const pond::render::RenderFrameResult result = presentResult.GetValue();
        ASSERT_TRUE(pond::render::IsValid(result));
        EXPECT_FALSE(frame.IsValid());

        if (result.presented)
        {
            EXPECT_TRUE(result.status == pond::render::FrameStatus::Presented ||
                        result.status == pond::render::FrameStatus::Suboptimal ||
                        result.status == pond::render::FrameStatus::Recreated);
            if (observedResult != nullptr)
            {
                *observedResult = result;
            }
            return;
        }

        ASSERT_TRUE(result.status == pond::render::FrameStatus::TimedOut ||
                    result.status == pond::render::FrameStatus::RecreationPending ||
                    result.status == pond::render::FrameStatus::SkippedSuspended);
    }

    FAIL() << "No live frame was presented after " << kMaximumFrameAttempts << " attempts.";
}

TEST(RenderOwnerThreadSurfaceIntegrationTests, CreatesDestroysAndTransfersOneLiveOwnerThreadSurface)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    if (!runtimeResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                runtimeResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::PlatformRuntime,
                "Platform runtime creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << runtimeResult.GetError().GetMessage();
    }
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc windowDesc;
    windowDesc.title = "Ponder Render Surface Integration";
    windowDesc.visible = true;
    windowDesc.graphicsCompatibility = pond::render::GetRequiredWindowGraphicsCompatibility();

    auto windowResult = runtime.CreateWindow(windowDesc);
    if (!windowResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                windowResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::PlatformWindow,
                "Vulkan-compatible window creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << windowResult.GetError().GetMessage();
    }
    pond::platform::Window window = std::move(windowResult).GetValue();

    const pond::render::RenderTargetSnapshot initialSnapshot = MakeSnapshot(window, 1);
    ASSERT_TRUE(pond::render::IsValid(initialSnapshot));

    auto bootstrapResult =
        pond::render::RenderBootstrap::Create(pond::render::test::MakeLiveRenderBootstrapDesc());
    if (!bootstrapResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                bootstrapResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::Bootstrap,
                "Render bootstrap creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << bootstrapResult.GetError().GetMessage();
    }
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult = bootstrap.PrepareSurface(
        window, pond::render::SurfacePreparationDesc{
                    .targetSnapshot = initialSnapshot,
                    .reason = pond::render::SurfacePreparationReason::Initial});
    if (!surfaceResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                surfaceResult.GetError(), pond::render::test::LivePrerequisiteOperation::Surface,
                "Vulkan surface preparation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << surfaceResult.GetError().GetMessage();
    }

    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();
    EXPECT_TRUE(surface.IsValid());
    EXPECT_EQ(surface.GetWindowId(), window.GetId());
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);

    const pond::render::RenderTargetSnapshot latestSnapshot = initialSnapshot;
    ASSERT_TRUE(pond::render::IsValid(latestSnapshot));
    const pond::core::VoidResult transferResult = surface.TransferToCurrentThread(latestSnapshot);
    ASSERT_TRUE(transferResult) << transferResult.GetError().GetMessage();
    EXPECT_TRUE(surface.IsBoundToRenderThread());
    EXPECT_TRUE(surface.VerifyRenderThread());

    auto selectionResult =
        bootstrap.SelectAdapter(surface, pond::render::RenderAdapterSelectionDesc{});
    if (!selectionResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                selectionResult.GetError(), pond::render::test::LivePrerequisiteOperation::Adapter,
                "Vulkan adapter selection"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << selectionResult.GetError().GetMessage();
    }

    auto deviceResult = bootstrap.CreateDevice(surface, selectionResult.GetValue(),
                                               pond::render::RenderDeviceDesc{});
    if (!deviceResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                deviceResult.GetError(), pond::render::test::LivePrerequisiteOperation::Device,
                "Vulkan logical-device creation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << deviceResult.GetError().GetMessage();
    }

    pond::render::RenderDevice device = std::move(deviceResult).GetValue();
    EXPECT_TRUE(device.IsValid());
    EXPECT_TRUE(device.VerifyRenderThread());
    EXPECT_TRUE(pond::render::IsValid(device.GetSelectedAdapter()));
    const pond::render::RenderDeviceDiagnostics initialDeviceDiagnostics = device.GetDiagnostics();
    EXPECT_EQ(initialDeviceDiagnostics.selectedAdapter, device.GetSelectedAdapter());
    EXPECT_TRUE(initialDeviceDiagnostics.selectedAdapter.presentation.supportsWindowPresentation);
    EXPECT_TRUE(initialDeviceDiagnostics.selectedAdapter.presentation.supportsOpaqueSdrSrgbOutput);
    const pond::core::VoidResult waitIdleResult = device.WaitIdle();
    ASSERT_TRUE(waitIdleResult) << waitIdleResult.GetError().GetMessage();
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 1U);
    EXPECT_EQ(device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);

    auto targetResult = device.CreateRenderTarget(
        std::move(surface), pond::render::RenderTargetDesc{.targetSnapshot = latestSnapshot});
    if (!targetResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                targetResult.GetError(), pond::render::test::LivePrerequisiteOperation::Target,
                "Vulkan render-target creation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << targetResult.GetError().GetMessage();
    }

    pond::render::RenderTarget target = std::move(targetResult).GetValue();
    EXPECT_TRUE(target.IsValid());
    EXPECT_TRUE(target.VerifyRenderThread());
    EXPECT_FALSE(surface.IsValid());
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
    EXPECT_EQ(device.GetActiveTargetCount(), 1U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 1U);

    const auto duplicateTargetResult = device.CreateRenderTarget(
        std::move(surface), pond::render::RenderTargetDesc{.targetSnapshot = latestSnapshot});
    ASSERT_FALSE(duplicateTargetResult);
    EXPECT_EQ(duplicateTargetResult.GetError().GetCode(),
              pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));
    EXPECT_EQ(device.GetActiveTargetCount(), 1U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 1U);

    EXPECT_EQ(target.GetWindowId(), window.GetId());
    EXPECT_EQ(target.GetTargetSnapshot(), latestSnapshot);
    EXPECT_EQ(target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(target.IsSuspended());
    EXPECT_TRUE(target.HasSwapchain());
    EXPECT_EQ(target.GetSwapchainGeneration(), 1U);
    ASSERT_TRUE(target.GetSelectedPresentationConfig().has_value());
    EXPECT_TRUE(pond::render::IsValid(*target.GetSelectedPresentationConfig()));
    EXPECT_EQ(target.GetSelectedPresentationConfig()->pixelExtent, latestSnapshot.GetPixelSize());

    const std::array<pond::render::ClearColor, 3U> clearSequence{
        pond::render::ClearColor{.red = 0.9F, .green = 0.1F, .blue = 0.1F, .alpha = 1.0F},
        pond::render::ClearColor{.red = 0.1F, .green = 0.8F, .blue = 0.2F, .alpha = 1.0F},
        pond::render::ClearColor{.red = 0.1F, .green = 0.2F, .blue = 0.9F, .alpha = 1.0F}};
    for (pond::render::ClearColor clearColor : clearSequence)
    {
        ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(target, clearColor));
    }

    const pond::core::VoidResult postFrameIdle = device.WaitIdle();
    ASSERT_TRUE(postFrameIdle) << postFrameIdle.GetError().GetMessage();

    const pond::render::RenderTargetSnapshot zeroSizedSnapshot{
        window.GetId(),
        pond::platform::PixelSize{},
        pond::platform::LogicalSize{},
        true,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{1U},
        3};
    ASSERT_TRUE(pond::render::IsValid(zeroSizedSnapshot));
    const pond::core::VoidResult zeroSizedUpdate = target.UpdateTargetSnapshot(zeroSizedSnapshot);
    ASSERT_TRUE(zeroSizedUpdate) << zeroSizedUpdate.GetError().GetMessage();
    EXPECT_EQ(target.GetStatus(), pond::render::TargetStatus::Suspended);
    EXPECT_TRUE(target.IsSuspended());
    EXPECT_FALSE(target.HasSwapchain());
    EXPECT_EQ(target.GetSwapchainGeneration(), 0U);
    EXPECT_FALSE(target.GetSelectedPresentationConfig().has_value());
    EXPECT_FALSE(target.HasPendingRecreation());

    const pond::render::RenderTargetSnapshot activeSnapshot{
        window.GetId(),
        latestSnapshot.GetPixelSize(),
        latestSnapshot.GetLogicalSize(),
        true,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{1U},
        4};
    ASSERT_TRUE(pond::render::IsValid(activeSnapshot));
    const pond::core::VoidResult activeUpdate = target.UpdateTargetSnapshot(activeSnapshot);
    ASSERT_TRUE(activeUpdate) << activeUpdate.GetError().GetMessage();
    EXPECT_EQ(target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(target.IsSuspended());
    EXPECT_FALSE(target.HasSwapchain());
    EXPECT_EQ(target.GetSwapchainGeneration(), 0U);
    EXPECT_FALSE(target.GetSelectedPresentationConfig().has_value());
    ASSERT_TRUE(target.HasPendingRecreation());
    ASSERT_TRUE(target.GetPendingRecreationInfo().has_value());
    EXPECT_EQ(target.GetPendingRecreationInfo()->reason,
              pond::render::TargetRecreationReason::Restored);
    EXPECT_EQ(target.GetPendingRecreationInfo()->previousRevision, zeroSizedSnapshot.GetRevision());
    EXPECT_EQ(target.GetPendingRecreationInfo()->currentRevision, activeSnapshot.GetRevision());

    pond::render::RenderFrameResult restoredPresentation{};
    ASSERT_NO_FATAL_FAILURE(
        ExpectPresentedFrame(target, pond::render::ClearColor{}, &restoredPresentation));
    EXPECT_TRUE(target.HasSwapchain());
    EXPECT_GE(target.GetSwapchainGeneration(), 2U);
    ASSERT_TRUE(target.GetSelectedPresentationConfig().has_value());
    EXPECT_TRUE(pond::render::IsValid(*target.GetSelectedPresentationConfig()));
    EXPECT_EQ(target.GetSelectedPresentationConfig()->pixelExtent, activeSnapshot.GetPixelSize());
    EXPECT_TRUE(restoredPresentation.status == pond::render::FrameStatus::Recreated ||
                restoredPresentation.status == pond::render::FrameStatus::Suboptimal);
    if (restoredPresentation.status == pond::render::FrameStatus::Recreated)
    {
        EXPECT_TRUE(restoredPresentation.presented);
        EXPECT_FALSE(target.HasPendingRecreation());
    }
    else
    {
        EXPECT_TRUE(restoredPresentation.presented);
        EXPECT_TRUE(target.HasPendingRecreation());
    }

    const pond::core::VoidResult wrongWindowUpdate =
        target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
            pond::platform::WindowId{9999}, latestSnapshot.GetPixelSize(),
            latestSnapshot.GetLogicalSize(), true, pond::platform::WindowState::Normal,
            pond::render::PresentationEnvironmentRevision{1U}, 5});
    ASSERT_FALSE(wrongWindowUpdate);
    EXPECT_EQ(wrongWindowUpdate.GetError().GetCode(),
              pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidArgument));

    const pond::core::VoidResult staleUpdate = target.UpdateTargetSnapshot(activeSnapshot);
    ASSERT_FALSE(staleUpdate);
    EXPECT_EQ(staleUpdate.GetError().GetCode(),
              pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));

    pond::render::RenderTarget movedTarget{std::move(target)};
    EXPECT_FALSE(target.IsValid());
    EXPECT_TRUE(movedTarget.IsValid());
    EXPECT_EQ(movedTarget.GetWindowId(), window.GetId());
    EXPECT_EQ(device.GetActiveTargetCount(), 1U);

    movedTarget = pond::render::RenderTarget{};
    EXPECT_EQ(device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);

    constexpr std::size_t kRepeatedWindowCycles = 3U;
    for (std::size_t cycle = 0U; cycle < kRepeatedWindowCycles; ++cycle)
    {
        pond::platform::WindowDesc cycleDesc;
        cycleDesc.title = "Ponder Render Repeated Window " + std::to_string(cycle);
        cycleDesc.logicalSize = {240U + static_cast<std::uint32_t>(cycle) * 20U,
                                 180U + static_cast<std::uint32_t>(cycle) * 20U};
        cycleDesc.visible = true;
        cycleDesc.graphicsCompatibility = pond::render::GetRequiredWindowGraphicsCompatibility();

        auto cycleWindowResult = runtime.CreateWindow(cycleDesc);
        ASSERT_TRUE(cycleWindowResult) << cycleWindowResult.GetError().GetMessage();
        pond::platform::Window cycleWindow = std::move(cycleWindowResult).GetValue();
        auto cycleSnapshotResult = pond::render::test::CaptureWindowSnapshot(cycleWindow, 1U);
        ASSERT_TRUE(cycleSnapshotResult) << cycleSnapshotResult.GetError().GetMessage();
        const pond::render::RenderTargetSnapshot cycleSnapshot =
            std::move(cycleSnapshotResult).GetValue();

        auto cycleSurfaceResult =
            PrepareTransferredSurface(bootstrap, cycleWindow, cycleSnapshot, cycleSnapshot);
        ASSERT_TRUE(cycleSurfaceResult) << cycleSurfaceResult.GetError().GetMessage();
        auto cycleTargetResult = device.CreateRenderTarget(
            std::move(cycleSurfaceResult).GetValue(),
            pond::render::RenderTargetDesc{.targetSnapshot = cycleSnapshot});
        ASSERT_TRUE(cycleTargetResult) << cycleTargetResult.GetError().GetMessage();
        pond::render::RenderTarget cycleTarget = std::move(cycleTargetResult).GetValue();
        ASSERT_NO_FATAL_FAILURE(
            ExpectPresentedFrame(cycleTarget, MakeClearColor(0.2F, 0.3F, 0.4F)));
    }
    EXPECT_EQ(device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);

    device = pond::render::RenderDevice{};
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 0U);
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
    ExpectValidationClean(bootstrap);
}

TEST(RenderOwnerThreadSurfaceIntegrationTests, KeepsTwoLiveWindowsIndependentOnOneRenderDevice)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    if (!runtimeResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                runtimeResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::PlatformRuntime,
                "Platform runtime creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << runtimeResult.GetError().GetMessage();
    }
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc firstDesc;
    firstDesc.title = "Ponder Render Multi Window A";
    firstDesc.logicalSize = {320, 240};
    firstDesc.visible = true;
    firstDesc.graphicsCompatibility = pond::render::GetRequiredWindowGraphicsCompatibility();

    pond::platform::WindowDesc secondDesc;
    secondDesc.title = "Ponder Render Multi Window B";
    secondDesc.logicalSize = {480, 360};
    secondDesc.visible = true;
    secondDesc.graphicsCompatibility = pond::render::GetRequiredWindowGraphicsCompatibility();

    auto firstWindowResult = runtime.CreateWindow(firstDesc);
    if (!firstWindowResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                firstWindowResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::PlatformWindow,
                "First Vulkan-compatible window creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << firstWindowResult.GetError().GetMessage();
    }
    auto firstWindow =
        std::make_unique<pond::platform::Window>(std::move(firstWindowResult).GetValue());

    auto secondWindowResult = runtime.CreateWindow(secondDesc);
    if (!secondWindowResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                secondWindowResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::PlatformWindow,
                "Second Vulkan-compatible window creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << secondWindowResult.GetError().GetMessage();
    }
    pond::platform::Window secondWindow = std::move(secondWindowResult).GetValue();
    ASSERT_NE(firstWindow->GetId(), secondWindow.GetId());

    const pond::render::RenderTargetSnapshot firstInitial = MakeSnapshot(*firstWindow, 1);
    const pond::render::RenderTargetSnapshot secondInitial = MakeSnapshot(secondWindow, 1);
    const pond::render::RenderTargetSnapshot firstActive = MakeSnapshot(*firstWindow, 2);
    const pond::render::RenderTargetSnapshot secondActive = MakeSnapshot(secondWindow, 2);
    ASSERT_TRUE(pond::render::IsValid(firstInitial));
    ASSERT_TRUE(pond::render::IsValid(secondInitial));
    ASSERT_TRUE(pond::render::IsValid(firstActive));
    ASSERT_TRUE(pond::render::IsValid(secondActive));
    ASSERT_NE(firstActive.GetPixelSize(), secondActive.GetPixelSize());

    auto bootstrapResult =
        pond::render::RenderBootstrap::Create(pond::render::test::MakeLiveRenderBootstrapDesc());
    if (!bootstrapResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                bootstrapResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::Bootstrap,
                "Render bootstrap creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << bootstrapResult.GetError().GetMessage();
    }
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto firstSurfaceResult =
        PrepareTransferredSurface(bootstrap, *firstWindow, firstInitial, firstActive);
    if (!firstSurfaceResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                firstSurfaceResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::Surface,
                "First Vulkan surface preparation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << firstSurfaceResult.GetError().GetMessage();
    }
    pond::render::PreparedSurface firstSurface = std::move(firstSurfaceResult).GetValue();

    auto secondSurfaceResult =
        PrepareTransferredSurface(bootstrap, secondWindow, secondInitial, secondActive);
    if (!secondSurfaceResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                secondSurfaceResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::Surface,
                "Second Vulkan surface preparation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << secondSurfaceResult.GetError().GetMessage();
    }
    pond::render::PreparedSurface secondSurface = std::move(secondSurfaceResult).GetValue();
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 2U);

    auto selectionResult =
        bootstrap.SelectAdapter(firstSurface, pond::render::RenderAdapterSelectionDesc{});
    if (!selectionResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                selectionResult.GetError(), pond::render::test::LivePrerequisiteOperation::Adapter,
                "Vulkan adapter selection"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << selectionResult.GetError().GetMessage();
    }

    auto deviceResult = bootstrap.CreateDevice(firstSurface, selectionResult.GetValue(),
                                               pond::render::RenderDeviceDesc{});
    if (!deviceResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                deviceResult.GetError(), pond::render::test::LivePrerequisiteOperation::Device,
                "Vulkan logical-device creation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << deviceResult.GetError().GetMessage();
    }
    pond::render::RenderDevice device = std::move(deviceResult).GetValue();

    auto firstTargetResult = device.CreateRenderTarget(
        std::move(firstSurface), pond::render::RenderTargetDesc{.targetSnapshot = firstActive});
    if (!firstTargetResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                firstTargetResult.GetError(), pond::render::test::LivePrerequisiteOperation::Target,
                "First Vulkan render-target creation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << firstTargetResult.GetError().GetMessage();
    }
    pond::render::RenderTarget firstTarget = std::move(firstTargetResult).GetValue();

    auto secondTargetResult = device.CreateRenderTarget(
        std::move(secondSurface), pond::render::RenderTargetDesc{.targetSnapshot = secondActive});
    if (!secondTargetResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                secondTargetResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::Target,
                "Second Vulkan render-target creation"))
        {
            GTEST_SKIP() << *reason;
        }

        FAIL() << secondTargetResult.GetError().GetMessage();
    }
    pond::render::RenderTarget secondTarget = std::move(secondTargetResult).GetValue();

    pond::render::RenderDevice movedDevice{std::move(device)};
    EXPECT_FALSE(device.IsValid());
    ASSERT_TRUE(movedDevice.IsValid());
    EXPECT_EQ(movedDevice.GetActiveTargetCount(), 2U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 2U);
    const pond::render::RenderDeviceDiagnostics deviceDiagnostics = movedDevice.GetDiagnostics();
    EXPECT_EQ(deviceDiagnostics.targetCreateAttempts, 2U);
    EXPECT_EQ(deviceDiagnostics.targetCreateSuccesses, 2U);
    EXPECT_EQ(deviceDiagnostics.targetCreateFailures, 0U);
    EXPECT_FALSE(deviceDiagnostics.deviceLost);
    EXPECT_TRUE(firstTarget.VerifyRenderThread());
    EXPECT_TRUE(secondTarget.VerifyRenderThread());
    EXPECT_EQ(firstTarget.GetWindowId(), firstWindow->GetId());
    EXPECT_EQ(secondTarget.GetWindowId(), secondWindow.GetId());
    ASSERT_TRUE(firstTarget.GetSelectedPresentationConfig().has_value());
    ASSERT_TRUE(secondTarget.GetSelectedPresentationConfig().has_value());
    EXPECT_EQ(firstTarget.GetSelectedPresentationConfig()->pixelExtent, firstActive.GetPixelSize());
    EXPECT_EQ(secondTarget.GetSelectedPresentationConfig()->pixelExtent,
              secondActive.GetPixelSize());
    EXPECT_NE(firstTarget.GetSelectedPresentationConfig()->pixelExtent,
              secondTarget.GetSelectedPresentationConfig()->pixelExtent);

    auto firstFrameResult = firstTarget.AcquireFrame();
    ASSERT_TRUE(firstFrameResult) << firstFrameResult.GetError().GetMessage();
    pond::render::RenderFrame firstFrame = std::move(firstFrameResult).GetValue();
    auto secondFrameResult = secondTarget.AcquireFrame();
    ASSERT_TRUE(secondFrameResult) << secondFrameResult.GetError().GetMessage();
    pond::render::RenderFrame secondFrame = std::move(secondFrameResult).GetValue();

    const auto duplicateFirstFrame = firstTarget.AcquireFrame();
    ASSERT_FALSE(duplicateFirstFrame);
    EXPECT_EQ(duplicateFirstFrame.GetError().GetCode(),
              pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));
    const pond::render::RenderTargetDiagnostics firstDiagnosticsAfterMisuse =
        firstTarget.GetDiagnostics();
    EXPECT_EQ(firstDiagnosticsAfterMisuse.windowId, firstWindow->GetId());
    EXPECT_EQ(firstDiagnosticsAfterMisuse.targetLabel,
              "target/window:" + std::to_string(firstWindow->GetId().GetValue()));
    EXPECT_EQ(firstDiagnosticsAfterMisuse.frameAcquireAttempts, 2U);
    EXPECT_EQ(firstDiagnosticsAfterMisuse.frameAcquireFailures, 1U);
    ASSERT_TRUE(firstDiagnosticsAfterMisuse.lastFailure.has_value());
    EXPECT_EQ(firstDiagnosticsAfterMisuse.lastFailure->renderCode,
              pond::render::RenderErrorCode::InvalidState);
    EXPECT_EQ(firstDiagnosticsAfterMisuse.lastFailure->windowId, firstWindow->GetId());

    ASSERT_TRUE(firstFrame.Clear(MakeClearColor(0.9F, 0.1F, 0.1F)));
    ASSERT_TRUE(secondFrame.Clear(MakeClearColor(0.1F, 0.2F, 0.9F)));
    auto firstPresent = firstFrame.FinishAndPresent();
    ASSERT_TRUE(firstPresent) << firstPresent.GetError().GetMessage();
    EXPECT_TRUE(firstPresent.GetValue().presented);
    auto secondPresent = secondFrame.FinishAndPresent();
    ASSERT_TRUE(secondPresent) << secondPresent.GetError().GetMessage();
    EXPECT_TRUE(secondPresent.GetValue().presented);

    pond::render::test::CoalescedWindowEvents firstEvents{.windowId = firstWindow->GetId()};
    pond::render::test::DrainPlatformEvents(runtime, firstEvents);
    constexpr std::array<pond::platform::LogicalSize, 4U> kResizeStorm{
        pond::platform::LogicalSize{360, 260}, pond::platform::LogicalSize{420, 300},
        pond::platform::LogicalSize{380, 270}, pond::platform::LogicalSize{400, 280}};
    for (const pond::platform::LogicalSize size : kResizeStorm)
    {
        ASSERT_TRUE(firstWindow->SetLogicalSize(size));
    }
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(
        runtime, firstEvents,
        [&firstWindow]()
        {
            const auto logicalSize = firstWindow->GetLogicalSize();
            return logicalSize && logicalSize.GetValue() == pond::platform::LogicalSize{400, 280};
        }));
    EXPECT_GT(firstEvents.GetResizeEventCount(), 0U);

    auto firstResizedResult = pond::render::test::CaptureWindowSnapshot(*firstWindow, 3U);
    ASSERT_TRUE(firstResizedResult) << firstResizedResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot firstResized =
        std::move(firstResizedResult).GetValue();
    ASSERT_TRUE(pond::render::IsValid(firstResized));
    ASSERT_NE(firstResized.GetPixelSize(), firstActive.GetPixelSize());
    const std::uint64_t firstGenerationBeforeResize = firstTarget.GetSwapchainGeneration();
    const pond::core::VoidResult resizeFirst = firstTarget.UpdateTargetSnapshot(firstResized);
    ASSERT_TRUE(resizeFirst) << resizeFirst.GetError().GetMessage();
    ASSERT_TRUE(firstTarget.HasPendingRecreation());
    ASSERT_TRUE(firstTarget.GetPendingRecreationInfo().has_value());
    EXPECT_EQ(firstTarget.GetPendingRecreationInfo()->reason,
              pond::render::TargetRecreationReason::SizeChanged);
    EXPECT_FALSE(secondTarget.HasPendingRecreation());
    EXPECT_EQ(secondTarget.GetTargetSnapshot(), secondActive);

    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(secondTarget, MakeClearColor(0.1F, 0.7F, 0.2F)));
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(firstTarget, MakeClearColor(0.8F, 0.6F, 0.1F)));
    EXPECT_GT(firstTarget.GetSwapchainGeneration(), firstGenerationBeforeResize);
    ASSERT_TRUE(firstTarget.GetSelectedPresentationConfig().has_value());
    EXPECT_EQ(firstTarget.GetSelectedPresentationConfig()->pixelExtent,
              firstResized.GetPixelSize());
    EXPECT_EQ(secondTarget.GetSwapchainGeneration(), 1U);

    ASSERT_TRUE(firstWindow->Hide());
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(runtime, firstEvents,
                                                            [&firstWindow]()
                                                            {
                                                                const auto visible =
                                                                    firstWindow->IsVisible();
                                                                return visible &&
                                                                       !visible.GetValue();
                                                            }));
    auto firstHiddenResult = pond::render::test::CaptureWindowSnapshot(*firstWindow, 4U);
    ASSERT_TRUE(firstHiddenResult) << firstHiddenResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot firstHidden = std::move(firstHiddenResult).GetValue();
    ASSERT_TRUE(firstTarget.UpdateTargetSnapshot(firstHidden));
    EXPECT_EQ(firstTarget.GetStatus(), pond::render::TargetStatus::Hidden);
    EXPECT_FALSE(firstTarget.HasSwapchain());
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(secondTarget, MakeClearColor(0.0F, 0.8F, 0.8F)));

    ASSERT_TRUE(firstWindow->Show());
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(runtime, firstEvents,
                                                            [&firstWindow]()
                                                            {
                                                                const auto visible =
                                                                    firstWindow->IsVisible();
                                                                return visible &&
                                                                       visible.GetValue();
                                                            }));
    auto firstShownResult = pond::render::test::CaptureWindowSnapshot(*firstWindow, 5U);
    ASSERT_TRUE(firstShownResult) << firstShownResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot firstShown = std::move(firstShownResult).GetValue();
    ASSERT_TRUE(firstTarget.UpdateTargetSnapshot(firstShown));
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(firstTarget, MakeClearColor(0.7F, 0.2F, 0.7F)));

#if defined(_WIN32)
    ASSERT_TRUE(RequestNativeClose(*firstWindow));
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(runtime, firstEvents,
                                                            [&firstEvents]()
                                                            {
                                                                return firstEvents.closeRequested;
                                                            }));
    EXPECT_TRUE(firstTarget.IsValid());
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(firstTarget, MakeClearColor(0.6F, 0.3F, 0.8F)));
#endif

    ASSERT_TRUE(firstWindow->Minimize());
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(
        runtime, firstEvents,
        [&firstWindow]()
        {
            const auto state = firstWindow->GetState();
            return state && state.GetValue() == pond::platform::WindowState::Minimized;
        }));
    auto firstMinimizedResult = pond::render::test::CaptureWindowSnapshot(*firstWindow, 6U);
    ASSERT_TRUE(firstMinimizedResult) << firstMinimizedResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot firstMinimized =
        std::move(firstMinimizedResult).GetValue();
    ASSERT_TRUE(pond::render::IsValid(firstMinimized));
    const pond::core::VoidResult minimizeFirst = firstTarget.UpdateTargetSnapshot(firstMinimized);
    ASSERT_TRUE(minimizeFirst) << minimizeFirst.GetError().GetMessage();
    EXPECT_TRUE(firstTarget.IsSuspended());
    EXPECT_FALSE(firstTarget.HasSwapchain());
    EXPECT_FALSE(secondTarget.IsSuspended());
    EXPECT_TRUE(secondTarget.HasSwapchain());
    const pond::render::PresentationRetirementStats firstRetirement =
        firstTarget.GetPresentationRetirementStats();
    EXPECT_GE(firstRetirement.pendingResourceSets + firstRetirement.retiredResourceSets, 1U);
    EXPECT_EQ(firstRetirement.practicalWaitFallbacks, 0U);
    EXPECT_EQ(secondTarget.GetPresentationRetirementStats().pendingResourceSets, 0U);
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(secondTarget, MakeClearColor(0.0F, 0.8F, 0.8F)));
    const pond::render::RenderTargetDiagnostics firstDiagnosticsAfterMinimize =
        firstTarget.GetDiagnostics();
    EXPECT_EQ(firstDiagnosticsAfterMinimize.suspensionCount, 2U);
    EXPECT_EQ(firstDiagnosticsAfterMinimize.status, pond::render::TargetStatus::Minimized);

    ASSERT_TRUE(firstWindow->Restore());
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(
        runtime, firstEvents,
        [&firstWindow]()
        {
            const auto state = firstWindow->GetState();
            return state && state.GetValue() != pond::platform::WindowState::Minimized;
        }));
    auto firstRestoredResult = pond::render::test::CaptureWindowSnapshot(*firstWindow, 7U);
    ASSERT_TRUE(firstRestoredResult) << firstRestoredResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot firstRestored =
        std::move(firstRestoredResult).GetValue();
    ASSERT_TRUE(pond::render::IsValid(firstRestored));
    const pond::core::VoidResult restoreFirst = firstTarget.UpdateTargetSnapshot(firstRestored);
    ASSERT_TRUE(restoreFirst) << restoreFirst.GetError().GetMessage();
    EXPECT_FALSE(firstTarget.IsSuspended());
    EXPECT_TRUE(firstTarget.HasPendingRecreation());
    EXPECT_FALSE(secondTarget.HasPendingRecreation());
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(firstTarget, MakeClearColor(0.7F, 0.2F, 0.7F)));

    pond::render::test::CoalescedWindowEvents secondEvents{.windowId = secondWindow.GetId()};
    pond::render::test::DrainPlatformEvents(runtime, secondEvents);
    ASSERT_TRUE(secondWindow.Minimize());
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(
        runtime, secondEvents,
        [&secondWindow]()
        {
            const auto state = secondWindow.GetState();
            return state && state.GetValue() == pond::platform::WindowState::Minimized;
        }));
    auto secondMinimizedResult = pond::render::test::CaptureWindowSnapshot(secondWindow, 3U);
    ASSERT_TRUE(secondMinimizedResult) << secondMinimizedResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot secondMinimized =
        std::move(secondMinimizedResult).GetValue();
    ASSERT_TRUE(pond::render::IsValid(secondMinimized));
    const pond::core::VoidResult minimizeSecond =
        secondTarget.UpdateTargetSnapshot(secondMinimized);
    ASSERT_TRUE(minimizeSecond) << minimizeSecond.GetError().GetMessage();
    EXPECT_TRUE(secondTarget.IsSuspended());
    EXPECT_FALSE(secondTarget.HasSwapchain());
    EXPECT_FALSE(firstTarget.IsSuspended());
    EXPECT_TRUE(firstTarget.HasSwapchain());
    const pond::render::PresentationRetirementStats secondRetirement =
        secondTarget.GetPresentationRetirementStats();
    EXPECT_GE(secondRetirement.pendingResourceSets + secondRetirement.retiredResourceSets, 1U);
    EXPECT_EQ(secondRetirement.practicalWaitFallbacks, 0U);
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(firstTarget, MakeClearColor(0.2F, 0.4F, 0.9F)));

    ASSERT_TRUE(secondWindow.Restore());
    ASSERT_TRUE(pond::render::test::PumpPlatformEventsUntil(
        runtime, secondEvents,
        [&secondWindow]()
        {
            const auto state = secondWindow.GetState();
            return state && state.GetValue() != pond::platform::WindowState::Minimized;
        }));
    auto secondRestoredResult = pond::render::test::CaptureWindowSnapshot(secondWindow, 4U);
    ASSERT_TRUE(secondRestoredResult) << secondRestoredResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot secondRestored =
        std::move(secondRestoredResult).GetValue();
    ASSERT_TRUE(pond::render::IsValid(secondRestored));
    const pond::core::VoidResult restoreSecond = secondTarget.UpdateTargetSnapshot(secondRestored);
    ASSERT_TRUE(restoreSecond) << restoreSecond.GetError().GetMessage();
    EXPECT_FALSE(secondTarget.IsSuspended());
    EXPECT_TRUE(secondTarget.HasPendingRecreation());
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(secondTarget, MakeClearColor(0.9F, 0.4F, 0.2F)));
    EXPECT_EQ(movedDevice.GetDiagnostics().practicalWaitFallbacks, 0U);

    firstTarget = pond::render::RenderTarget{};
    EXPECT_FALSE(firstTarget.IsValid());
    firstWindow.reset();
    EXPECT_EQ(movedDevice.GetActiveTargetCount(), 1U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 1U);
    EXPECT_TRUE(secondTarget.IsValid());
    ASSERT_NO_FATAL_FAILURE(ExpectPresentedFrame(secondTarget, MakeClearColor(0.3F, 0.9F, 0.3F)));

    secondTarget = pond::render::RenderTarget{};
    EXPECT_EQ(movedDevice.GetActiveTargetCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);
    movedDevice = pond::render::RenderDevice{};
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 0U);
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
    ExpectValidationClean(bootstrap);
}

TEST(RenderOwnerThreadSurfaceIntegrationTests, ShutsDownRendererChildrenOnDedicatedRenderThread)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    if (!runtimeResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                runtimeResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::PlatformRuntime,
                "Platform runtime creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << runtimeResult.GetError().GetMessage();
    }
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc windowDesc;
    windowDesc.title = "Ponder Dedicated Render Thread";
    windowDesc.logicalSize = {360, 240};
    windowDesc.visible = true;
    windowDesc.graphicsCompatibility = pond::render::GetRequiredWindowGraphicsCompatibility();
    auto windowResult = runtime.CreateWindow(windowDesc);
    if (!windowResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                windowResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::PlatformWindow,
                "Dedicated-thread Vulkan window creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << windowResult.GetError().GetMessage();
    }
    pond::platform::Window window = std::move(windowResult).GetValue();

    auto snapshotResult = pond::render::test::CaptureWindowSnapshot(window, 1U);
    ASSERT_TRUE(snapshotResult) << snapshotResult.GetError().GetMessage();
    const pond::render::RenderTargetSnapshot snapshot = std::move(snapshotResult).GetValue();

    auto bootstrapResult =
        pond::render::RenderBootstrap::Create(pond::render::test::MakeLiveRenderBootstrapDesc());
    if (!bootstrapResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                bootstrapResult.GetError(),
                pond::render::test::LivePrerequisiteOperation::Bootstrap,
                "Dedicated-thread render bootstrap creation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << bootstrapResult.GetError().GetMessage();
    }
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();
    auto surfaceResult = bootstrap.PrepareSurface(
        window,
        pond::render::SurfacePreparationDesc{
            .targetSnapshot = snapshot, .reason = pond::render::SurfacePreparationReason::Initial});
    if (!surfaceResult)
    {
        if (auto reason = pond::render::test::MakeOptionalLiveSkipReason(
                surfaceResult.GetError(), pond::render::test::LivePrerequisiteOperation::Surface,
                "Dedicated-thread Vulkan surface preparation"))
        {
            GTEST_SKIP() << *reason;
        }
        FAIL() << surfaceResult.GetError().GetMessage();
    }

    struct ThreadOutcome final
    {
        std::string failure{};
        std::optional<std::string> optionalSkip{};
        bool presented{};
    };
    ThreadOutcome outcome;
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();
    std::thread renderThread{
        [&bootstrap, snapshot, surface = std::move(surface), &outcome]() mutable
        {
            const auto recordFailure =
                [&outcome](const pond::core::Error& error,
                           pond::render::test::LivePrerequisiteOperation operation,
                           std::string_view context)
            {
                outcome.optionalSkip =
                    pond::render::test::MakeOptionalLiveSkipReason(error, operation, context);
                if (!outcome.optionalSkip.has_value())
                {
                    outcome.failure = context;
                    outcome.failure += ": ";
                    outcome.failure += error.GetMessage();
                }
            };

            if (pond::core::VoidResult transfer = surface.TransferToCurrentThread(snapshot);
                !transfer)
            {
                recordFailure(transfer.GetError(),
                              pond::render::test::LivePrerequisiteOperation::Surface,
                              "Prepared-surface render-thread transfer");
                return;
            }

            auto selection =
                bootstrap.SelectAdapter(surface, pond::render::RenderAdapterSelectionDesc{});
            if (!selection)
            {
                recordFailure(selection.GetError(),
                              pond::render::test::LivePrerequisiteOperation::Adapter,
                              "Dedicated-thread adapter selection");
                return;
            }

            auto deviceResult = bootstrap.CreateDevice(surface, selection.GetValue(),
                                                       pond::render::RenderDeviceDesc{});
            if (!deviceResult)
            {
                recordFailure(deviceResult.GetError(),
                              pond::render::test::LivePrerequisiteOperation::Device,
                              "Dedicated-thread device creation");
                return;
            }
            pond::render::RenderDevice device = std::move(deviceResult).GetValue();

            auto targetResult = device.CreateRenderTarget(
                std::move(surface), pond::render::RenderTargetDesc{.targetSnapshot = snapshot});
            if (!targetResult)
            {
                recordFailure(targetResult.GetError(),
                              pond::render::test::LivePrerequisiteOperation::Target,
                              "Dedicated-thread target creation");
                return;
            }
            pond::render::RenderTarget target = std::move(targetResult).GetValue();

            constexpr std::size_t kMaximumAttempts = 16U;
            for (std::size_t attempt = 0U; attempt < kMaximumAttempts; ++attempt)
            {
                auto frameResult = target.AcquireFrame();
                if (!frameResult)
                {
                    outcome.failure = "Dedicated-thread frame acquisition: " +
                                      std::string{frameResult.GetError().GetMessage()};
                    return;
                }
                pond::render::RenderFrame frame = std::move(frameResult).GetValue();
                if (pond::core::VoidResult clear = frame.Clear(MakeClearColor(0.2F, 0.5F, 0.8F));
                    !clear)
                {
                    outcome.failure =
                        "Dedicated-thread clear: " + std::string{clear.GetError().GetMessage()};
                    return;
                }
                auto presentation = frame.FinishAndPresent();
                if (!presentation)
                {
                    outcome.failure = "Dedicated-thread presentation: " +
                                      std::string{presentation.GetError().GetMessage()};
                    return;
                }
                if (presentation.GetValue().presented)
                {
                    outcome.presented = true;
                    break;
                }
            }

            target = pond::render::RenderTarget{};
            device = pond::render::RenderDevice{};
        }};
    renderThread.join();

    if (outcome.optionalSkip.has_value())
    {
        GTEST_SKIP() << *outcome.optionalSkip;
    }
    ASSERT_TRUE(outcome.failure.empty()) << outcome.failure;
    EXPECT_TRUE(outcome.presented);
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);
    ExpectValidationClean(bootstrap);

    const pond::core::VoidResult shutdown = bootstrap.Shutdown();
    ASSERT_TRUE(shutdown) << shutdown.GetError().GetMessage();
    EXPECT_FALSE(bootstrap.IsValid());
}
} // namespace
