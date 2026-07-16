#include <ponder/render/Bootstrap.hpp>

#include <cstdlib>
#include <gtest/gtest.h>
#include <optional>
#include <thread>
#include <utility>

#include "RenderBootstrapTestAccess.hpp"
#include "VulkanBootstrap.hpp"

#if !defined(GTEST_HAS_DEATH_TEST) || !GTEST_HAS_DEATH_TEST
#error "Render lifetime contracts require GoogleTest death-test support on Windows and Linux."
#endif

namespace
{
[[nodiscard]] constexpr pond::render::RenderTargetSnapshot MakeSnapshot(std::uint64_t revision = 1)
{
    return pond::render::RenderTargetSnapshot{pond::platform::WindowId{42},
                                              pond::platform::PixelSize{800, 600},
                                              pond::platform::LogicalSize{800U, 600U},
                                              true,
                                              pond::platform::WindowState::Normal,
                                              pond::render::PresentationEnvironmentRevision{1U},
                                              revision};
}

[[nodiscard]] constexpr pond::render::SurfacePreparationDesc MakeSurfaceDesc()
{
    return pond::render::SurfacePreparationDesc{
        .targetSnapshot = MakeSnapshot(),
        .reason = pond::render::SurfacePreparationReason::Initial};
}

[[nodiscard]] pond::render::RenderBootstrap CreateBootstrapOrAbort()
{
    auto result = pond::render::RenderBootstrap::Create({});
    if (!result)
    {
        std::abort();
    }

    return std::move(result).GetValue();
}

[[nodiscard]] pond::render::PreparedSurface CreateSurfaceOrAbort(
    pond::render::RenderBootstrap& bootstrap)
{
    auto result = pond::render::detail::RenderBootstrapTestAccess::CreatePreparedSurface(
        bootstrap, MakeSurfaceDesc());
    if (!result)
    {
        std::abort();
    }

    return std::move(result).GetValue();
}

[[nodiscard]] constexpr pond::render::RenderTargetDesc MakeSuspendedTargetDesc()
{
    return pond::render::RenderTargetDesc{
        .targetSnapshot = pond::render::RenderTargetSnapshot{
            pond::platform::WindowId{42}, pond::platform::PixelSize{800, 600},
            pond::platform::LogicalSize{800U, 600U}, false, pond::platform::WindowState::Normal,
            pond::render::PresentationEnvironmentRevision{1U}, 1U}};
}

[[nodiscard]] pond::render::detail::RenderLifetimeContractOwners CreateDeviceAndTargetOrAbort()
{
    auto result = pond::render::detail::RenderBackendTestAccess::CreateLifetimeContractOwners(
        MakeSuspendedTargetDesc());
    if (!result)
    {
        std::abort();
    }

    return std::move(result).GetValue();
}

class RenderLifetimeContractDeathTests : public testing::Test
{
protected:
    void SetUp() override
    {
        GTEST_FLAG_SET(death_test_style, "threadsafe");
    }
};

TEST_F(RenderLifetimeContractDeathTests, BootstrapDestructorTerminatesWithLivePreparedSurface)
{
    EXPECT_DEATH(
        {
            std::optional<pond::render::RenderBootstrap> bootstrap;
            bootstrap.emplace(CreateBootstrapOrAbort());
            [[maybe_unused]] pond::render::PreparedSurface surface =
                CreateSurfaceOrAbort(*bootstrap);
            bootstrap.reset();
        },
        "Cannot destroy RenderBootstrap");
}

TEST_F(RenderLifetimeContractDeathTests, BootstrapMoveAssignmentTerminatesWithLivePreparedSurface)
{
    EXPECT_DEATH(
        {
            pond::render::RenderBootstrap bootstrap = CreateBootstrapOrAbort();
            [[maybe_unused]] pond::render::PreparedSurface surface =
                CreateSurfaceOrAbort(bootstrap);
            bootstrap = pond::render::RenderBootstrap{};
        },
        "Cannot destroy RenderBootstrap");
}

TEST_F(RenderLifetimeContractDeathTests, BootstrapDestructorTerminatesOffOwnerThread)
{
    EXPECT_DEATH(
        {
            std::optional<pond::render::RenderBootstrap> bootstrap;
            bootstrap.emplace(CreateBootstrapOrAbort());
            std::thread worker{[bootstrap = std::move(bootstrap)]() mutable
                               {
                                   bootstrap.reset();
                               }};
            worker.join();
        },
        "Cannot destroy RenderBootstrap");
}

TEST_F(RenderLifetimeContractDeathTests, UnboundPreparedSurfaceDestructorTerminatesOffOwnerThread)
{
    EXPECT_DEATH(
        {
            pond::render::RenderBootstrap bootstrap = CreateBootstrapOrAbort();
            std::optional<pond::render::PreparedSurface> surface;
            surface.emplace(CreateSurfaceOrAbort(bootstrap));
            std::thread worker{[surface = std::move(surface)]() mutable
                               {
                                   surface.reset();
                               }};
            worker.join();
        },
        "PreparedSurface destruction");
}

TEST_F(RenderLifetimeContractDeathTests, BoundPreparedSurfaceDestructorTerminatesOffRenderThread)
{
    EXPECT_DEATH(
        {
            pond::render::RenderBootstrap bootstrap = CreateBootstrapOrAbort();
            std::optional<pond::render::PreparedSurface> surface;
            surface.emplace(CreateSurfaceOrAbort(bootstrap));
            bool transferSucceeded{};
            std::thread renderThread{[&]()
                                     {
                                         transferSucceeded = static_cast<bool>(
                                             surface->TransferToCurrentThread(MakeSnapshot(2)));
                                     }};
            renderThread.join();
            if (!transferSucceeded)
            {
                std::abort();
            }

            surface.reset();
        },
        "PreparedSurface destruction");
}
TEST_F(RenderLifetimeContractDeathTests, RenderDeviceDestructorTerminatesWithLiveTarget)
{
    EXPECT_DEATH(
        {
            auto owners = CreateDeviceAndTargetOrAbort();
            std::optional<pond::render::RenderDevice> device;
            device.emplace(std::move(owners.device));
            device.reset();
        },
        "RenderDevice destruction");
}

TEST_F(RenderLifetimeContractDeathTests,
       RenderDeviceMoveAssignmentTerminatesWithLiveDestinationTarget)
{
    EXPECT_DEATH(
        {
            auto destination = CreateDeviceAndTargetOrAbort();
            auto source = CreateDeviceAndTargetOrAbort();
            destination.device = std::move(source.device);
        },
        "RenderDevice destruction");
}

TEST_F(RenderLifetimeContractDeathTests, RenderDeviceDestructorTerminatesOffRenderThread)
{
    EXPECT_DEATH(
        {
            auto owners = CreateDeviceAndTargetOrAbort();
            owners.target = pond::render::RenderTarget{};
            std::optional<pond::render::RenderDevice> device;
            device.emplace(std::move(owners.device));
            std::thread worker{[device = std::move(device)]() mutable
                               {
                                   device.reset();
                               }};
            worker.join();
        },
        "RenderDevice destruction");
}

TEST_F(RenderLifetimeContractDeathTests, RenderTargetDestructorTerminatesWithLiveFrame)
{
    EXPECT_DEATH(
        {
            auto owners = CreateDeviceAndTargetOrAbort();
            auto frameResult = owners.target.AcquireFrame();
            if (!frameResult)
            {
                std::abort();
            }
            [[maybe_unused]] pond::render::RenderFrame frame = std::move(frameResult).GetValue();
            std::optional<pond::render::RenderTarget> target;
            target.emplace(std::move(owners.target));
            target.reset();
        },
        "RenderTarget destruction");
}

TEST_F(RenderLifetimeContractDeathTests,
       RenderTargetMoveAssignmentTerminatesWithLiveDestinationFrame)
{
    EXPECT_DEATH(
        {
            auto destination = CreateDeviceAndTargetOrAbort();
            auto source = CreateDeviceAndTargetOrAbort();
            auto frameResult = destination.target.AcquireFrame();
            if (!frameResult)
            {
                std::abort();
            }
            [[maybe_unused]] pond::render::RenderFrame frame = std::move(frameResult).GetValue();
            destination.target = std::move(source.target);
        },
        "RenderTarget destruction");
}

TEST_F(RenderLifetimeContractDeathTests, RenderTargetDestructorTerminatesOffRenderThread)
{
    EXPECT_DEATH(
        {
            auto owners = CreateDeviceAndTargetOrAbort();
            std::optional<pond::render::RenderTarget> target;
            target.emplace(std::move(owners.target));
            std::thread worker{[target = std::move(target)]() mutable
                               {
                                   target.reset();
                               }};
            worker.join();
        },
        "RenderTarget destruction");
}

TEST_F(RenderLifetimeContractDeathTests, RenderFrameDestructorTerminatesOffRenderThread)
{
    EXPECT_DEATH(
        {
            auto owners = CreateDeviceAndTargetOrAbort();
            auto frameResult = owners.target.AcquireFrame();
            if (!frameResult)
            {
                std::abort();
            }
            std::optional<pond::render::RenderFrame> frame;
            frame.emplace(std::move(frameResult).GetValue());
            std::thread worker{[frame = std::move(frame)]() mutable
                               {
                                   frame.reset();
                               }};
            worker.join();
        },
        "RenderFrame destruction");
}

} // namespace
