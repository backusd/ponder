#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>

#include <gtest/gtest.h>
#include <utility>

namespace
{
[[nodiscard]] pond::render::RenderTargetSnapshot MakeSnapshot(
    const pond::platform::Window& window, std::uint64_t revision)
{
    const auto pixelSize = window.GetPixelSize();
    if (!pixelSize)
    {
        return {};
    }

    return pond::render::RenderTargetSnapshot{window.GetId(),
                                              pixelSize.GetValue(),
                                              true,
                                              false,
                                              true,
                                              revision};
}

[[nodiscard]] bool IsOptionalLiveEnvironmentError(const pond::core::Error& error) noexcept
{
    const pond::core::ErrorCode code = error.GetCode();
    return code == pond::render::ToErrorCode(pond::render::RenderErrorCode::LoaderUnavailable) ||
           code == pond::render::ToErrorCode(pond::render::RenderErrorCode::UnsupportedBackend) ||
           code == pond::render::ToErrorCode(pond::render::RenderErrorCode::UnsupportedCapability) ||
           code == pond::render::ToErrorCode(pond::render::RenderErrorCode::UnsupportedSurface) ||
           code == pond::render::ToErrorCode(pond::render::RenderErrorCode::BackendFailure);
}

TEST(RenderOwnerThreadSurfaceIntegrationTests,
     CreatesDestroysAndTransfersOneLiveOwnerThreadSurface)
{
#if !defined(_WIN32)
    GTEST_SKIP() << "The first live REND-008 surface test is enabled only on Windows for now.";
#else
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    if (!runtimeResult)
    {
        GTEST_SKIP() << "No live platform runtime is available: "
                     << runtimeResult.GetError().GetMessage();
    }
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc windowDesc;
    windowDesc.title = "Ponder Render Surface Integration";
    windowDesc.visible = true;
    windowDesc.graphicsCompatibility = pond::render::GetRequiredWindowGraphicsCompatibility();

    auto windowResult = runtime.CreateWindow(windowDesc);
    if (!windowResult)
    {
        GTEST_SKIP() << "No live Vulkan-compatible platform window is available: "
                     << windowResult.GetError().GetMessage();
    }
    pond::platform::Window window = std::move(windowResult).GetValue();

    const pond::render::RenderTargetSnapshot initialSnapshot = MakeSnapshot(window, 1);
    if (!pond::render::IsValid(initialSnapshot))
    {
        GTEST_SKIP() << "The live window did not report a valid initial target snapshot.";
    }

    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult) << bootstrapResult.GetError().GetMessage();
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult = bootstrap.PrepareSurface(
        window, pond::render::SurfacePreparationDesc{.targetSnapshot = initialSnapshot,
                                                     .reason = pond::render::SurfacePreparationReason::Initial});
    if (!surfaceResult)
    {
        if (IsOptionalLiveEnvironmentError(surfaceResult.GetError()))
        {
            GTEST_SKIP() << "No live Vulkan surface path is available: "
                         << surfaceResult.GetError().GetMessage();
        }

        FAIL() << surfaceResult.GetError().GetMessage();
    }

    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();
    EXPECT_TRUE(surface.IsValid());
    EXPECT_EQ(surface.GetWindowId(), window.GetId());
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);

    const pond::render::RenderTargetSnapshot latestSnapshot = MakeSnapshot(window, 2);
    ASSERT_TRUE(pond::render::IsValid(latestSnapshot));
    const pond::core::VoidResult transferResult = surface.TransferToCurrentThread(latestSnapshot);
    ASSERT_TRUE(transferResult) << transferResult.GetError().GetMessage();
    EXPECT_TRUE(surface.IsBoundToRenderThread());
    EXPECT_TRUE(surface.VerifyRenderThread());

    auto selectionResult = bootstrap.SelectAdapter(surface, pond::render::RenderAdapterSelectionDesc{});
    if (!selectionResult)
    {
        if (IsOptionalLiveEnvironmentError(selectionResult.GetError()))
        {
            GTEST_SKIP() << "No live Vulkan adapter path is available: "
                         << selectionResult.GetError().GetMessage();
        }

        FAIL() << selectionResult.GetError().GetMessage();
    }

    auto deviceResult = bootstrap.CreateDevice(surface, selectionResult.GetValue(),
                                               pond::render::RenderDeviceDesc{});
    if (!deviceResult)
    {
        if (IsOptionalLiveEnvironmentError(deviceResult.GetError()))
        {
            GTEST_SKIP() << "No live Vulkan logical-device path is available: "
                         << deviceResult.GetError().GetMessage();
        }

        FAIL() << deviceResult.GetError().GetMessage();
    }

    pond::render::RenderDevice device = std::move(deviceResult).GetValue();
    EXPECT_TRUE(device.IsValid());
    EXPECT_TRUE(device.VerifyRenderThread());
    EXPECT_TRUE(pond::render::IsValid(device.GetSelectedAdapter()));
    EXPECT_TRUE(pond::render::IsValid(device.GetQueuePlan()));
    EXPECT_TRUE(device.GetOptionalCapabilities().vmaAllocator);
    const pond::core::VoidResult waitIdleResult = device.WaitIdle();
    ASSERT_TRUE(waitIdleResult) << waitIdleResult.GetError().GetMessage();
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 1U);
    EXPECT_EQ(device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(bootstrap.GetActiveTargetCount(), 0U);

    auto targetResult = device.CreateRenderTarget(
        std::move(surface), pond::render::RenderTargetDesc{.targetSnapshot = latestSnapshot});
    if (!targetResult)
    {
        if (IsOptionalLiveEnvironmentError(targetResult.GetError()))
        {
            GTEST_SKIP() << "No live Vulkan target path is available: "
                         << targetResult.GetError().GetMessage();
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
        auto frameResult = target.AcquireFrame();
        ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
        pond::render::RenderFrame frame = std::move(frameResult).GetValue();
        ASSERT_TRUE(frame.IsValid());
        EXPECT_EQ(frame.GetStatus(), pond::render::FrameStatus::Ready);

        const pond::core::VoidResult clearResult = frame.Clear(clearColor);
        ASSERT_TRUE(clearResult) << clearResult.GetError().GetMessage();

        auto presentResult = frame.FinishAndPresent();
        ASSERT_TRUE(presentResult) << presentResult.GetError().GetMessage();
        EXPECT_TRUE(pond::render::IsValid(presentResult.GetValue()));
        EXPECT_TRUE(presentResult.GetValue().presented);
        EXPECT_TRUE(presentResult.GetValue().status == pond::render::FrameStatus::Presented ||
                    presentResult.GetValue().status == pond::render::FrameStatus::Suboptimal);
        EXPECT_FALSE(frame.IsValid());
    }

    const pond::core::VoidResult postFrameIdle = device.WaitIdle();
    ASSERT_TRUE(postFrameIdle) << postFrameIdle.GetError().GetMessage();

    const pond::render::RenderTargetSnapshot zeroSizedSnapshot{
        window.GetId(), pond::platform::PixelSize{}, true, false, true, 3};
    ASSERT_TRUE(pond::render::IsValid(zeroSizedSnapshot));
    const pond::core::VoidResult zeroSizedUpdate =
        target.UpdateTargetSnapshot(zeroSizedSnapshot);
    ASSERT_TRUE(zeroSizedUpdate) << zeroSizedUpdate.GetError().GetMessage();
    EXPECT_EQ(target.GetStatus(), pond::render::TargetStatus::Suspended);
    EXPECT_TRUE(target.IsSuspended());
    EXPECT_FALSE(target.HasSwapchain());
    EXPECT_EQ(target.GetSwapchainGeneration(), 0U);
    EXPECT_FALSE(target.GetSelectedPresentationConfig().has_value());

    const pond::render::RenderTargetSnapshot activeSnapshot{
        window.GetId(), latestSnapshot.GetPixelSize(), true, false, true, 4};
    ASSERT_TRUE(pond::render::IsValid(activeSnapshot));
    const pond::core::VoidResult activeUpdate = target.UpdateTargetSnapshot(activeSnapshot);
    ASSERT_TRUE(activeUpdate) << activeUpdate.GetError().GetMessage();
    EXPECT_EQ(target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(target.IsSuspended());
    EXPECT_TRUE(target.HasSwapchain());
    EXPECT_EQ(target.GetSwapchainGeneration(), 2U);
    ASSERT_TRUE(target.GetSelectedPresentationConfig().has_value());
    EXPECT_TRUE(pond::render::IsValid(*target.GetSelectedPresentationConfig()));

    const pond::core::VoidResult wrongWindowUpdate = target.UpdateTargetSnapshot(
        pond::render::RenderTargetSnapshot{pond::platform::WindowId{9999},
                                           latestSnapshot.GetPixelSize(),
                                           true,
                                           false,
                                           true,
                                           5});
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

    device = pond::render::RenderDevice{};
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 0U);
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
#endif
}
} // namespace

