#include <ponder/render/Bootstrap.hpp>

#include <gtest/gtest.h>
#include <latch>
#include <optional>
#include <thread>

#include "RenderBootstrapTestAccess.hpp"

namespace
{
[[nodiscard]] constexpr pond::render::RenderTargetSnapshot MakeSnapshot(
    std::uint64_t revision = 1, pond::platform::WindowId windowId = pond::platform::WindowId{42},
    pond::render::PresentationEnvironmentRevision presentationEnvironmentRevision =
        pond::render::PresentationEnvironmentRevision{1U},
    pond::platform::PixelSize pixelSize = pond::platform::PixelSize{800, 600}, bool visible = true,
    pond::platform::WindowState windowState = pond::platform::WindowState::Normal)
{
    return pond::render::RenderTargetSnapshot{
        windowId, pixelSize, visible, windowState, presentationEnvironmentRevision, revision};
}

[[nodiscard]] constexpr pond::render::SurfacePreparationDesc MakeSurfaceDesc(
    pond::render::RenderTargetSnapshot snapshot = MakeSnapshot(),
    pond::render::SurfacePreparationReason reason = pond::render::SurfacePreparationReason::Initial)
{
    return pond::render::SurfacePreparationDesc{.targetSnapshot = snapshot, .reason = reason};
}

[[nodiscard]] pond::core::Result<pond::render::PreparedSurface> CreatePreparedSurfaceForTest(
    pond::render::RenderBootstrap& bootstrap, const pond::render::SurfacePreparationDesc& desc)
{
    return pond::render::detail::RenderBootstrapTestAccess::CreatePreparedSurface(bootstrap, desc);
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
    ExpectRenderError(
        pond::render::ValidateSurfacePreparationRequest(
            pond::platform::WindowGraphicsCompatibility::Metal, pond::platform::WindowId{42}, desc),
        pond::render::RenderErrorCode::UnsupportedSurface);
    ExpectRenderError(
        pond::render::ValidateSurfacePreparationRequest(
            pond::platform::WindowGraphicsCompatibility::Vulkan, pond::platform::WindowId{7}, desc),
        pond::render::RenderErrorCode::InvalidArgument);
    ExpectRenderError(pond::render::ValidateSurfacePreparationRequest(
                          pond::platform::WindowGraphicsCompatibility::Vulkan,
                          pond::platform::WindowId{42}, pond::render::SurfacePreparationDesc{}),
                      pond::render::RenderErrorCode::InvalidArgument);
}

TEST(RenderThreadingHandoffTests, ValidatesTargetSnapshotUpdateOrdering)
{
    EXPECT_TRUE(pond::render::ValidateTargetSnapshotUpdate(MakeSnapshot(1), MakeSnapshot(2)));
    EXPECT_TRUE(pond::render::ValidateTargetSnapshotUpdate(
        MakeSnapshot(2), MakeSnapshot(3, pond::platform::WindowId{42},
                                      pond::render::PresentationEnvironmentRevision{2U})));

    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(
                          MakeSnapshot(1), MakeSnapshot(2, pond::platform::WindowId{7})),
                      pond::render::RenderErrorCode::InvalidArgument);
    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(MakeSnapshot(2), MakeSnapshot(2)),
                      pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(MakeSnapshot(2), MakeSnapshot(1)),
                      pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(
                          MakeSnapshot(2, pond::platform::WindowId{42},
                                       pond::render::PresentationEnvironmentRevision{2U}),
                          MakeSnapshot(3, pond::platform::WindowId{42},
                                       pond::render::PresentationEnvironmentRevision{1U})),
                      pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(pond::render::ValidateTargetSnapshotUpdate(
                          MakeSnapshot(2), pond::render::RenderTargetSnapshot{}),
                      pond::render::RenderErrorCode::InvalidArgument);
}

TEST(RenderThreadingHandoffTests, BootstrapTracksPreparedSurfaceChildrenAcrossMoves)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create(
        {.validationMode = pond::render::RenderValidationMode::Synchronization});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    EXPECT_TRUE(bootstrap.IsValid());
    EXPECT_TRUE(bootstrap.IsOwnerThread());
    EXPECT_FALSE(bootstrap.HasActiveChildren());
    const pond::render::RenderBootstrapDiagnostics diagnostics = bootstrap.GetDiagnostics();
    EXPECT_EQ(diagnostics.backend, pond::render::RenderBackendKind::Vulkan);
    EXPECT_EQ(diagnostics.requestedValidationMode,
              pond::render::RenderValidationMode::Synchronization);
    EXPECT_EQ(diagnostics.enabledValidationMode, pond::render::RenderValidationMode::Disabled);
    EXPECT_EQ(diagnostics.negotiatedApiVersion, 0U);
    EXPECT_FALSE(diagnostics.validationEnabled);
    EXPECT_EQ(diagnostics.debugInstrumentation, pond::render::RenderDebugInstrumentation{});
    EXPECT_FALSE(diagnostics.lastFailure.has_value());

    {
        auto surfaceResult = CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc());
        ASSERT_TRUE(surfaceResult);
        pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

        EXPECT_TRUE(surface.IsValid());
        EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);
        EXPECT_TRUE(bootstrap.HasActiveChildren());
        ASSERT_TRUE(surface.TransferToCurrentThread(surface.GetTargetSnapshot()));

        const auto failedSelection =
            bootstrap.SelectAdapter(surface, pond::render::RenderAdapterSelectionDesc{});
        ExpectRenderError(failedSelection, pond::render::RenderErrorCode::InvalidState);
        const pond::render::RenderBootstrapDiagnostics failedDiagnostics =
            bootstrap.GetDiagnostics();
        ASSERT_TRUE(failedDiagnostics.lastFailure.has_value());
        EXPECT_EQ(failedDiagnostics.lastFailure->renderCode,
                  pond::render::RenderErrorCode::InvalidState);
        EXPECT_EQ(failedDiagnostics.lastFailure->operation, "SelectAdapter");
        EXPECT_EQ(failedDiagnostics.lastFailure->windowId, surface.GetWindowId());

        pond::render::PreparedSurface movedSurface{std::move(surface)};
        EXPECT_FALSE(surface.IsValid());
        EXPECT_TRUE(movedSurface.IsValid());
        EXPECT_EQ(movedSurface.GetTargetSnapshot(), MakeSnapshot());
        EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);
    }

    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
    EXPECT_FALSE(bootstrap.HasActiveChildren());
}

TEST(RenderThreadingHandoffTests, PreparedSurfaceTransfersOnceToCallerSelectedRenderThread)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult = CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc());
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    EXPECT_FALSE(surface.IsBoundToRenderThread());
    ExpectRenderError(surface.VerifyRenderThread(), pond::render::RenderErrorCode::InvalidState);

    bool transferSucceeded{};
    bool verifySucceededOnRenderThread{};
    std::optional<pond::core::ErrorCode> transferError;
    std::optional<pond::core::ErrorCode> verifyError;
    std::latch transferComplete{1};
    std::latch releaseSurface{1};
    std::thread renderThread{[&surface, &transferSucceeded, &verifySucceededOnRenderThread,
                              &transferError, &verifyError, &transferComplete, &releaseSurface]()
                             {
                                 pond::core::VoidResult transfer =
                                     surface.TransferToCurrentThread(MakeSnapshot());
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

                                 transferComplete.count_down();
                                 releaseSurface.wait();
                                 if (transferSucceeded)
                                 {
                                     surface = pond::render::PreparedSurface{};
                                 }
                             }};
    transferComplete.wait();

    EXPECT_TRUE(transferSucceeded);
    EXPECT_FALSE(transferError.has_value());
    EXPECT_TRUE(verifySucceededOnRenderThread);
    EXPECT_FALSE(verifyError.has_value());
    EXPECT_TRUE(surface.IsBoundToRenderThread());
    EXPECT_EQ(surface.GetTargetSnapshot(), MakeSnapshot());

    ExpectRenderError(surface.VerifyRenderThread(), pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(surface.TransferToCurrentThread(MakeSnapshot(3)),
                      pond::render::RenderErrorCode::InvalidState);

    releaseSurface.count_down();
    renderThread.join();
    EXPECT_FALSE(surface.IsValid());
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);
}

TEST(RenderThreadingHandoffTests,
     PreparedSurfaceRejectsOlderAndConflictingStateButAcceptsNewerState)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto wrongWindowSurfaceResult =
        CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc(MakeSnapshot(1)));
    ASSERT_TRUE(wrongWindowSurfaceResult);
    pond::render::PreparedSurface wrongWindowSurface =
        std::move(wrongWindowSurfaceResult).GetValue();
    ExpectRenderError(
        wrongWindowSurface.TransferToCurrentThread(MakeSnapshot(2, pond::platform::WindowId{7})),
        pond::render::RenderErrorCode::InvalidArgument);

    auto staleSurfaceResult =
        CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc(MakeSnapshot(3)));
    ASSERT_TRUE(staleSurfaceResult);
    pond::render::PreparedSurface staleSurface = std::move(staleSurfaceResult).GetValue();
    ExpectRenderError(staleSurface.TransferToCurrentThread(MakeSnapshot(2)),
                      pond::render::RenderErrorCode::InvalidState);

    auto conflictingSurfaceResult =
        CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc(MakeSnapshot(3)));
    ASSERT_TRUE(conflictingSurfaceResult);
    pond::render::PreparedSurface conflictingSurface =
        std::move(conflictingSurfaceResult).GetValue();
    ExpectRenderError(conflictingSurface.TransferToCurrentThread(
                          MakeSnapshot(3, pond::platform::WindowId{42},
                                       pond::render::PresentationEnvironmentRevision{1U},
                                       pond::platform::PixelSize{1024, 768})),
                      pond::render::RenderErrorCode::InvalidState);

    auto regressingSurfaceResult = CreatePreparedSurfaceForTest(
        bootstrap,
        MakeSurfaceDesc(MakeSnapshot(3, pond::platform::WindowId{42},
                                     pond::render::PresentationEnvironmentRevision{2U})));
    ASSERT_TRUE(regressingSurfaceResult);
    pond::render::PreparedSurface regressingSurface = std::move(regressingSurfaceResult).GetValue();
    ExpectRenderError(
        regressingSurface.TransferToCurrentThread(MakeSnapshot(
            4, pond::platform::WindowId{42}, pond::render::PresentationEnvironmentRevision{1U})),
        pond::render::RenderErrorCode::InvalidState);

    auto newerSurfaceResult =
        CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc(MakeSnapshot(3)));
    ASSERT_TRUE(newerSurfaceResult);
    pond::render::PreparedSurface newerSurface = std::move(newerSurfaceResult).GetValue();
    EXPECT_TRUE(newerSurface.TransferToCurrentThread(MakeSnapshot(
        4, pond::platform::WindowId{42}, pond::render::PresentationEnvironmentRevision{2U})));
    EXPECT_EQ(newerSurface.GetTargetSnapshot(),
              MakeSnapshot(4, pond::platform::WindowId{42},
                           pond::render::PresentationEnvironmentRevision{2U}));
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
                               CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc());
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

    auto surfaceResult = CreatePreparedSurfaceForTest(bootstrap, recoveryDesc);
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    EXPECT_EQ(surface.GetTargetSnapshot(), MakeSnapshot(10));
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);
}

TEST(RenderThreadingHandoffTests, ShutdownRejectsLivePreparedSurfaceWithoutMutationThenRetries)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult = CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc());
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    EXPECT_TRUE(bootstrap.HasActiveChildren());
    ExpectRenderError(bootstrap.Shutdown(), pond::render::RenderErrorCode::InvalidState);

    EXPECT_FALSE(bootstrap.IsShutdown());
    EXPECT_TRUE(bootstrap.IsValid());
    EXPECT_TRUE(surface.IsValid());
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 1U);

    surface = pond::render::PreparedSurface{};
    EXPECT_FALSE(bootstrap.HasActiveChildren());
    EXPECT_EQ(bootstrap.GetActivePreparedSurfaceCount(), 0U);

    ASSERT_TRUE(bootstrap.Shutdown());
    EXPECT_TRUE(bootstrap.IsShutdown());
    EXPECT_FALSE(bootstrap.IsValid());
    ExpectRenderError(CreatePreparedSurfaceForTest(bootstrap, MakeSurfaceDesc()),
                      pond::render::RenderErrorCode::InvalidState);
}

TEST(RenderThreadingHandoffTests, ShutdownRejectsWrongOwnerThreadWithoutMutation)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    bool shutdownSucceeded{};
    std::optional<pond::core::ErrorCode> shutdownError;
    std::thread worker{[&bootstrap, &shutdownSucceeded, &shutdownError]()
                       {
                           pond::core::VoidResult shutdown = bootstrap.Shutdown();
                           shutdownSucceeded = static_cast<bool>(shutdown);
                           if (!shutdown)
                           {
                               shutdownError = shutdown.GetError().GetCode();
                           }
                       }};
    worker.join();

    EXPECT_FALSE(shutdownSucceeded);
    ASSERT_TRUE(shutdownError.has_value());
    EXPECT_EQ(*shutdownError,
              pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));
    EXPECT_TRUE(bootstrap.IsValid());
    EXPECT_FALSE(bootstrap.IsShutdown());
    EXPECT_TRUE(bootstrap.IsOwnerThread());

    ASSERT_TRUE(bootstrap.Shutdown());
    EXPECT_TRUE(bootstrap.IsShutdown());
}

TEST(RenderThreadingHandoffTests, PreparedSurfaceCopiesDescriptorStateAtCreation)
{
    auto bootstrapResult = pond::render::RenderBootstrap::Create({});
    ASSERT_TRUE(bootstrapResult);
    pond::render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    pond::render::SurfacePreparationDesc desc = MakeSurfaceDesc(MakeSnapshot(1));
    auto surfaceResult = CreatePreparedSurfaceForTest(bootstrap, desc);
    ASSERT_TRUE(surfaceResult);
    pond::render::PreparedSurface surface = std::move(surfaceResult).GetValue();

    desc = MakeSurfaceDesc(MakeSnapshot(99, pond::platform::WindowId{77}));

    EXPECT_EQ(surface.GetTargetSnapshot(), MakeSnapshot(1));
    EXPECT_EQ(surface.GetWindowId(), pond::platform::WindowId{42});
}
} // namespace
