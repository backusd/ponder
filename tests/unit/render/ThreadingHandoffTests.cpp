#include <ponder/render/Bootstrap.hpp>

#include <gtest/gtest.h>
#include <optional>
#include <thread>

namespace
{
[[nodiscard]] constexpr pond::render::RenderTargetSnapshot MakeSnapshot(
    std::uint64_t revision = 1, pond::platform::WindowId windowId = pond::platform::WindowId{42})
{
    return pond::render::RenderTargetSnapshot{windowId,
                                              pond::platform::PixelSize{800, 600},
                                              true,
                                              false,
                                              true,
                                              revision};
}

[[nodiscard]] constexpr pond::render::SurfacePreparationDesc MakeSurfaceDesc(
    pond::render::RenderTargetSnapshot snapshot = MakeSnapshot(),
    pond::render::SurfacePreparationReason reason = pond::render::SurfacePreparationReason::Initial)
{
    return pond::render::SurfacePreparationDesc{.targetSnapshot = snapshot, .reason = reason};
}

void ExpectRenderError(const pond::core::VoidResult& result, pond::render::RenderErrorCode code)
{
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().GetCode(), pond::render::ToErrorCode(code));
}

template <typename Value>
void ExpectRenderError(const pond::core::Result<Value>& result, pond::render::RenderErrorCode code)
{
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().GetCode(), pond::render::ToErrorCode(code));
}

TEST(RenderThreadingHandoffTests, ValidatesSurfacePreparationRequestWithoutNativePayload)
{
    const pond::render::SurfacePreparationDesc desc = MakeSurfaceDesc();

    EXPECT_TRUE(pond::render::ValidateSurfacePreparationRequest(
        pond::platform::WindowGraphicsCompatibility::Vulkan, pond::platform::WindowId{42}, desc));

    ExpectRenderError(pond::render::ValidateSurfacePreparationRequest(
                          pond::platform::WindowGraphicsCompatibility::Default,
                          pond::platform::WindowId{42}, desc),
                      pond::render::RenderErrorCode::UnsupportedSurface);
    ExpectRenderError(pond::render::ValidateSurfacePreparationRequest(
                          pond::platform::WindowGraphicsCompatibility::Metal,
                          pond::platform::WindowId{42}, desc),
                      pond::render::RenderErrorCode::UnsupportedSurface);
    ExpectRenderError(pond::render::ValidateSurfacePreparationRequest(
                          pond::platform::WindowGraphicsCompatibility::Vulkan,
                          pond::platform::WindowId{7}, desc),
                      pond::render::RenderErrorCode::InvalidArgument);
    ExpectRenderError(pond::render::ValidateSurfacePreparationRequest(
                          pond::platform::WindowGraphicsCompatibility::Vulkan,
                          pond::platform::WindowId{42}, pond::render::SurfacePreparationDesc{}),
                      pond::render::RenderErrorCode::InvalidArgument);
}

TEST(RenderThreadingHandoffTests, ValidatesTargetSnapshotUpdateOrdering)
{
    EXPECT_TRUE(pond::render::ValidateTargetSnapshotUpdate(MakeSnapshot(1), MakeSnapshot(2)));

    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(
                          MakeSnapshot(1), MakeSnapshot(2, pond::platform::WindowId{7})),
                      pond::render::RenderErrorCode::InvalidArgument);
    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(MakeSnapshot(2), MakeSnapshot(2)),
                      pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(MakeSnapshot(2), MakeSnapshot(1)),
                      pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(
                          MakeSnapshot(2), pond::render::RenderTargetSnapshot{}),
                      pond::render::RenderErrorCode::InvalidArgument);
}

TEST(RenderThreadingHandoffTests, BootstrapTracksPreparedSurfaceChildrenAcrossMoves)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    EXPECT_TRUE(bootstrap.IsValid());
    EXPECT_TRUE(bootstrap.IsOwnerThread());
    EXPECT_FALSE(bootstrap.HasActiveChildren());

    {
        auto surfaceResult = bootstrap.CreatePreparedSurfaceForCompletedSurface(MakeSurfaceDesc());
        ASSERT_TRUE(surfaceResult);
        pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

        EXPECT_TRUE(surface.IsValid());
        EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);
        EXPECT_TRUE(bootstrap.HasActiveChildren());

        pond::render::PreparedSurface movedSurface{std::move(surface)};
        EXPECT_FALSE(surface.IsValid());
        EXPECT_TRUE(movedSurface.IsValid());
        EXPECT_EQ(movedSurface.GetTargetSnapshot(), MakeSnapshot());
        EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);
    }

    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
    EXPECT_FALSE(bootstrap.HasActiveChildren());
}

TEST(RenderThreadingHandoffTests, LegacyDeviceCreationRequiresFirstSurfaceAndAdapterSelection)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto deviceResult = bootstrap.CreateDevice({});

    ExpectRenderError(deviceResult, pond::render::RenderErrorCode::InvalidState);
    EXPECT_EQ(bootstrap.GetActiveDeviceCount(), 0U);
    EXPECT_FALSE(bootstrap.HasActiveChildren());
}
TEST(RenderThreadingHandoffTests, PreparedSurfaceTransfersOnceToCallerSelectedRenderThread)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult = bootstrap.CreatePreparedSurfaceForCompletedSurface(MakeSurfaceDesc());
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    EXPECT_FALSE(surface.IsBoundToRenderThread());
    ExpectRenderError(surface.VerifyRenderThread(), pond::render::RenderErrorCode::InvalidState);

    bool transferSucceeded{};
    bool verifySucceededOnRenderThread{};
    std::optional<pond::core::ErrorCode> transferError;
    std::optional<pond::core::ErrorCode> verifyError;
    std::thread renderThread{[&surface, &transferSucceeded, &verifySucceededOnRenderThread,
                              &transferError, &verifyError]()
                             {
                                 pond::core::VoidResult transfer =
                                     surface.TransferToCurrentThread(MakeSnapshot(2));
                                 transferSucceeded = static_cast<bool>(transfer);
                                 if (!transfer)
                                 {
                                     transferError = transfer.GetError().GetCode();
                                 }

                                 pond::core::VoidResult verify = surface.VerifyRenderThread();
                                 verifySucceededOnRenderThread = static_cast<bool>(verify);
                                 if (!verify)
                                 {
                                     verifyError = verify.GetError().GetCode();
                                 }
                             }};
    renderThread.join();

    EXPECT_TRUE(transferSucceeded);
    EXPECT_FALSE(transferError.has_value());
    EXPECT_TRUE(verifySucceededOnRenderThread);
    EXPECT_FALSE(verifyError.has_value());
    EXPECT_TRUE(surface.IsBoundToRenderThread());
    EXPECT_EQ(surface.GetTargetSnapshot(), MakeSnapshot(2));

    ExpectRenderError(surface.VerifyRenderThread(), pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(surface.TransferToCurrentThread(MakeSnapshot(3)),
                      pond::render::RenderErrorCode::InvalidState);
}

TEST(RenderThreadingHandoffTests, PreparedSurfaceRejectsWrongWindowAndStaleTransferState)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto wrongWindowSurfaceResult =
        bootstrap.CreatePreparedSurfaceForCompletedSurface(MakeSurfaceDesc(MakeSnapshot(1)));
    ASSERT_TRUE(wrongWindowSurfaceResult);
    pond::render::PreparedSurface wrongWindowSurface =
        std::move(wrongWindowSurfaceResult).GetValue();
    ExpectRenderError(wrongWindowSurface.TransferToCurrentThread(
                          MakeSnapshot(2, pond::platform::WindowId{7})),
                      pond::render::RenderErrorCode::InvalidArgument);

    auto staleSurfaceResult =
        bootstrap.CreatePreparedSurfaceForCompletedSurface(MakeSurfaceDesc(MakeSnapshot(3)));
    ASSERT_TRUE(staleSurfaceResult);
    pond::render::PreparedSurface staleSurface = std::move(staleSurfaceResult).GetValue();
    ExpectRenderError(staleSurface.TransferToCurrentThread(MakeSnapshot(3)),
                      pond::render::RenderErrorCode::InvalidState);
}

TEST(RenderThreadingHandoffTests, PreparedSurfaceCreationIsOwnerThreadOnly)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    bool createdOnWrongThread{};
    std::optional<pond::core::ErrorCode> wrongThreadCode;
    std::thread worker{[&bootstrap, &createdOnWrongThread, &wrongThreadCode]()
                       {
                           auto surfaceResult =
                               bootstrap.CreatePreparedSurfaceForCompletedSurface(MakeSurfaceDesc());
                           createdOnWrongThread = static_cast<bool>(surfaceResult);
                           if (!surfaceResult)
                           {
                               wrongThreadCode = surfaceResult.GetError().GetCode();
                           }
                       }};
    worker.join();

    EXPECT_FALSE(createdOnWrongThread);
    ASSERT_TRUE(wrongThreadCode.has_value());
    EXPECT_EQ(*wrongThreadCode,
              pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
}

TEST(RenderThreadingHandoffTests, SurfaceLossRecoveryUsesAFreshOwnerThreadPreparation)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    const pond::render::SurfacePreparationDesc recoveryDesc = MakeSurfaceDesc(
        MakeSnapshot(10), pond::render::SurfacePreparationReason::SurfaceLossRecovery);
    ASSERT_TRUE(pond::render::IsValid(recoveryDesc));

    auto surfaceResult = bootstrap.CreatePreparedSurfaceForCompletedSurface(recoveryDesc);
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    EXPECT_EQ(surface.GetTargetSnapshot(), MakeSnapshot(10));
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);
}

TEST(RenderThreadingHandoffTests, ShutdownInvalidatesChildrenAndRejectsNewWork)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult = bootstrap.CreatePreparedSurfaceForCompletedSurface(MakeSurfaceDesc());
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    EXPECT_TRUE(bootstrap.HasActiveChildren());
    bootstrap.Shutdown();

    EXPECT_TRUE(bootstrap.IsShutdown());
    EXPECT_FALSE(bootstrap.IsValid());
    EXPECT_FALSE(surface.IsValid());

    ExpectRenderError(surface.TransferToCurrentThread(MakeSnapshot(2)),
                      pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(bootstrap.CreateDevice({}), pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(bootstrap.CreatePreparedSurfaceForCompletedSurface(MakeSurfaceDesc()),
                      pond::render::RenderErrorCode::InvalidState);
}

TEST(RenderThreadingHandoffTests, PreparedSurfaceCopiesDescriptorStateAtCreation)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    pond::render::SurfacePreparationDesc desc = MakeSurfaceDesc(MakeSnapshot(1));
    auto surfaceResult = bootstrap.CreatePreparedSurfaceForCompletedSurface(desc);
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    desc = MakeSurfaceDesc(MakeSnapshot(99, pond::platform::WindowId{77}));

    EXPECT_EQ(surface.GetTargetSnapshot(), MakeSnapshot(1));
    EXPECT_EQ(surface.GetWindowId(), pond::platform::WindowId{42});
}
} // namespace