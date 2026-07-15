#include <ponder/core/Log.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace
{
namespace core = pond::core;
namespace platform = pond::platform;
namespace render = pond::render;

inline constexpr std::string_view kLogCategory{"example.render.clear-window"};
inline constexpr render::ClearColor kClearColor{
    .red = 0.035F,
    .green = 0.10F,
    .blue = 0.22F,
    .alpha = 1.0F,
};
inline constexpr auto kSuspendedDelay = std::chrono::milliseconds{50};
inline constexpr auto kNoWorkDelay = std::chrono::milliseconds{16};

struct WindowEventBatch final
{
    platform::WindowId windowId{};
    bool snapshotRequested{};
    bool presentationEnvironmentChanged{};
    bool closeRequested{};
    bool quitRequested{};
    bool captureFailureReported{};
};

struct SnapshotSequence final
{
    std::uint64_t snapshotRevision{1U};
    render::PresentationEnvironmentRevision presentationEnvironmentRevision{1U};
};

enum class FrameAction : std::uint8_t
{
    Presented,
    NoWork,
    RecoverSurface
};

[[nodiscard]] bool IsRenderError(const core::Error& error, render::RenderErrorCode code) noexcept
{
    return error.GetCode() == render::ToErrorCode(code);
}

[[nodiscard]] core::Result<render::RenderTargetSnapshot> CaptureWindowSnapshot(
    const platform::Window& window, std::uint64_t snapshotRevision,
    render::PresentationEnvironmentRevision presentationEnvironmentRevision)
{
    auto pixelSize = window.GetPixelSize();
    if (!pixelSize)
    {
        return core::Result<render::RenderTargetSnapshot>::FromError(
            std::move(pixelSize).GetError());
    }

    auto visible = window.IsVisible();
    if (!visible)
    {
        return core::Result<render::RenderTargetSnapshot>::FromError(std::move(visible).GetError());
    }

    auto windowState = window.GetState();
    if (!windowState)
    {
        return core::Result<render::RenderTargetSnapshot>::FromError(
            std::move(windowState).GetError());
    }

    return render::RenderTargetSnapshot{window.GetId(),
                                        pixelSize.GetValue(),
                                        visible.GetValue(),
                                        windowState.GetValue(),
                                        presentationEnvironmentRevision,
                                        snapshotRevision};
}

struct EventVisitor final
{
    WindowEventBatch& batch;

    void operator()(const platform::QuitRequestedEvent&) const noexcept
    {
        batch.quitRequested = true;
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const noexcept
    {
        if (event.windowId == batch.windowId)
        {
            batch.closeRequested = true;
        }
    }

    void operator()(const platform::WindowLogicalSizeChangedEvent& event) const noexcept
    {
        RequestSnapshot(event.windowId, false);
    }

    void operator()(const platform::WindowPixelSizeChangedEvent& event) const noexcept
    {
        RequestSnapshot(event.windowId, false);
    }

    void operator()(const platform::WindowVisibilityChangedEvent& event) const noexcept
    {
        RequestSnapshot(event.windowId, false);
    }

    void operator()(const platform::WindowStateChangedEvent& event) const noexcept
    {
        RequestSnapshot(event.windowId, false);
    }

    void operator()(const platform::WindowPresentationChangedEvent& event) const noexcept
    {
        RequestSnapshot(event.windowId, true);
    }

    void operator()(const platform::WindowDisplayChangedEvent& event) const noexcept
    {
        RequestSnapshot(event.windowId, true);
    }

    void operator()(const platform::WindowDisplayScaleChangedEvent& event) const noexcept
    {
        RequestSnapshot(event.windowId, true);
    }

    template <typename Event>
    void operator()(const Event&) const noexcept
    {
    }

private:
    void RequestSnapshot(platform::WindowId windowId,
                         bool presentationEnvironmentChanged) const noexcept
    {
        if (windowId != batch.windowId)
        {
            return;
        }

        batch.snapshotRequested = true;
        batch.presentationEnvironmentChanged |= presentationEnvironmentChanged;
    }
};

void DrainEvents(platform::PlatformRuntime& runtime, WindowEventBatch& batch)
{
    while (std::optional<platform::PlatformEvent> event = runtime.PollEvent())
    {
        std::visit(EventVisitor{batch}, *event);
    }
}

[[nodiscard]] core::Result<bool> TryApplyPendingSnapshot(const platform::Window& window,
                                                         render::RenderTarget& target,
                                                         WindowEventBatch& batch,
                                                         SnapshotSequence& sequence)
{
    if (!batch.snapshotRequested)
    {
        return true;
    }

    const std::uint64_t nextSnapshotRevision = sequence.snapshotRevision + 1U;
    const render::PresentationEnvironmentRevision nextPresentationRevision{
        sequence.presentationEnvironmentRevision.GetValue() +
        (batch.presentationEnvironmentChanged ? 1U : 0U)};

    auto snapshotResult =
        CaptureWindowSnapshot(window, nextSnapshotRevision, nextPresentationRevision);
    if (!snapshotResult)
    {
        if (!batch.captureFailureReported)
        {
            LOG_WARNING_CATEGORY(kLogCategory, "window_snapshot_deferred error={}",
                                 core::FormatError(snapshotResult.GetError()));
            batch.captureFailureReported = true;
        }
        return false;
    }

    const render::RenderTargetSnapshot snapshot = std::move(snapshotResult).GetValue();
    if (core::VoidResult update = target.UpdateTargetSnapshot(snapshot); !update)
    {
        return core::Result<bool>::FromError(std::move(update).GetError());
    }

    sequence.snapshotRevision = nextSnapshotRevision;
    sequence.presentationEnvironmentRevision = nextPresentationRevision;
    batch.snapshotRequested = false;
    batch.presentationEnvironmentChanged = false;
    batch.captureFailureReported = false;
    return true;
}

[[nodiscard]] core::Result<FrameAction> RenderOneFrame(render::RenderTarget& target)
{
    auto frameResult = target.AcquireFrame();
    if (!frameResult)
    {
        if (IsRenderError(frameResult.GetError(), render::RenderErrorCode::SurfaceLost))
        {
            return FrameAction::RecoverSurface;
        }
        return core::Result<FrameAction>::FromError(std::move(frameResult).GetError());
    }

    render::RenderFrame frame = std::move(frameResult).GetValue();
    if (core::VoidResult clear = frame.Clear(); !clear)
    {
        core::Error error = std::move(clear).GetError();
        // Releasing an unfinished frame explicitly abandons its acquired work safely.
        frame = render::RenderFrame{};
        return core::Result<FrameAction>::FromError(std::move(error));
    }

    auto presentResult = frame.FinishAndPresent();
    if (!presentResult)
    {
        if (IsRenderError(presentResult.GetError(), render::RenderErrorCode::SurfaceLost))
        {
            return FrameAction::RecoverSurface;
        }
        return core::Result<FrameAction>::FromError(std::move(presentResult).GetError());
    }

    return presentResult.GetValue().presented ? FrameAction::Presented : FrameAction::NoWork;
}

[[nodiscard]] core::VoidResult RecoverSurface(render::RenderBootstrap& bootstrap,
                                              platform::Window& window,
                                              render::RenderTarget& target)
{
    const render::RenderTargetSnapshot snapshot = target.GetTargetSnapshot();
    auto surfaceResult = bootstrap.PrepareSurface(
        window, render::SurfacePreparationDesc{
                    .targetSnapshot = snapshot,
                    .reason = render::SurfacePreparationReason::SurfaceLossRecovery,
                });
    if (!surfaceResult)
    {
        return core::VoidResult::FromError(std::move(surfaceResult).GetError());
    }

    render::PreparedSurface surface = std::move(surfaceResult).GetValue();
    if (core::VoidResult transfer = surface.TransferToCurrentThread(snapshot); !transfer)
    {
        return transfer;
    }

    return target.RecoverSurface(std::move(surface));
}

[[nodiscard]] core::VoidResult RunEventAndRenderLoop(platform::PlatformRuntime& runtime,
                                                     platform::Window& window,
                                                     render::RenderBootstrap& bootstrap,
                                                     render::RenderTarget& target)
{
    WindowEventBatch events{.windowId = window.GetId(), .snapshotRequested = true};
    SnapshotSequence sequence{};

    while (!events.quitRequested && !events.closeRequested)
    {
        DrainEvents(runtime, events);
        if (events.quitRequested || events.closeRequested)
        {
            break;
        }

        auto snapshotApplied = TryApplyPendingSnapshot(window, target, events, sequence);
        if (!snapshotApplied)
        {
            return core::VoidResult::FromError(std::move(snapshotApplied).GetError());
        }
        if (!snapshotApplied.GetValue())
        {
            std::this_thread::sleep_for(kNoWorkDelay);
            continue;
        }

        auto actionResult = RenderOneFrame(target);
        if (!actionResult)
        {
            return core::VoidResult::FromError(std::move(actionResult).GetError());
        }

        switch (actionResult.GetValue())
        {
        case FrameAction::Presented:
            break;
        case FrameAction::NoWork:
            std::this_thread::sleep_for(target.IsSuspended() ? kSuspendedDelay : kNoWorkDelay);
            break;
        case FrameAction::RecoverSurface:
            LOG_WARNING_CATEGORY(kLogCategory, "surface_lost_recovery_requested window={}",
                                 window.GetId().GetValue());
            if (core::VoidResult recovery = RecoverSurface(bootstrap, window, target); !recovery)
            {
                return recovery;
            }
            LOG_INFO_CATEGORY(kLogCategory, "surface_recovered window={}",
                              window.GetId().GetValue());
            std::this_thread::sleep_for(kNoWorkDelay);
            break;
        }
    }

    return {};
}

[[nodiscard]] core::VoidResult ValidateCleanShutdown(const render::RenderBootstrap& bootstrap)
{
    const render::RenderValidationReport report = bootstrap.GetValidationReport();
    if (report.IsClean() && report.droppedMessageCount == 0U)
    {
        return {};
    }

    LOG_ERROR_CATEGORY(kLogCategory,
                       "validation_not_clean warnings={} errors={} dropped={} captured={}",
                       report.warningCount, report.errorCount, report.droppedMessageCount,
                       report.capturedMessageCount);
    for (std::size_t index = 0U; index < report.capturedMessageCount; ++index)
    {
        const render::RenderValidationMessage& message = report.capturedMessages[index];
        LOG_ERROR_CATEGORY(kLogCategory, "validation_message id={} number={}",
                           message.GetMessageIdName(), message.messageIdNumber);
    }

    return core::VoidResult::FromError(
        core::Error{render::ToErrorCode(render::RenderErrorCode::BackendFailure),
                    "Vulkan validation reported an unexpected warning, error, or dropped "
                    "message."});
}

[[nodiscard]] core::VoidResult RunRenderer(platform::PlatformRuntime& runtime,
                                           platform::Window& window)
{
    auto initialSnapshotResult =
        CaptureWindowSnapshot(window, 1U, render::PresentationEnvironmentRevision{1U});
    if (!initialSnapshotResult)
    {
        return core::VoidResult::FromError(std::move(initialSnapshotResult).GetError());
    }
    const render::RenderTargetSnapshot initialSnapshot =
        std::move(initialSnapshotResult).GetValue();

    auto bootstrapResult = render::RenderBootstrap::Create(render::RenderBootstrapDesc{});
    if (!bootstrapResult)
    {
        return core::VoidResult::FromError(std::move(bootstrapResult).GetError());
    }
    render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult =
        bootstrap.PrepareSurface(window, render::SurfacePreparationDesc{
                                             .targetSnapshot = initialSnapshot,
                                             .reason = render::SurfacePreparationReason::Initial,
                                         });
    if (!surfaceResult)
    {
        return core::VoidResult::FromError(std::move(surfaceResult).GetError());
    }
    render::PreparedSurface deviceSelectionSurface = std::move(surfaceResult).GetValue();

    if (core::VoidResult transfer = deviceSelectionSurface.TransferToCurrentThread(initialSnapshot);
        !transfer)
    {
        return transfer;
    }

    auto selectionResult =
        bootstrap.SelectAdapter(deviceSelectionSurface, render::RenderAdapterSelectionDesc{});
    if (!selectionResult)
    {
        return core::VoidResult::FromError(std::move(selectionResult).GetError());
    }
    render::RenderAdapterSelection selection = std::move(selectionResult).GetValue();

    auto deviceResult =
        bootstrap.CreateDevice(deviceSelectionSurface, selection, render::RenderDeviceDesc{});
    if (!deviceResult)
    {
        return core::VoidResult::FromError(std::move(deviceResult).GetError());
    }
    render::RenderDevice device = std::move(deviceResult).GetValue();
    render::PreparedSurface targetSurface = std::move(deviceSelectionSurface);

    auto targetResult =
        device.CreateRenderTarget(std::move(targetSurface), render::RenderTargetDesc{
                                                                .targetSnapshot = initialSnapshot,
                                                                .clearColor = kClearColor,
                                                            });
    if (!targetResult)
    {
        // A failed target creation may leave the prepared surface unconsumed.
        targetSurface = render::PreparedSurface{};
        device = render::RenderDevice{};
        return core::VoidResult::FromError(std::move(targetResult).GetError());
    }
    render::RenderTarget target = std::move(targetResult).GetValue();

    LOG_INFO_CATEGORY(kLogCategory, "renderer_ready adapter={} window={}",
                      selection.selectedAdapter.identity.name, window.GetId().GetValue());

    core::VoidResult runResult = window.Show();
    if (runResult)
    {
        runResult = RunEventAndRenderLoop(runtime, window, bootstrap, target);
    }

    // Renderer children are deliberately retired before their parents and before the window.
    target = render::RenderTarget{};
    device = render::RenderDevice{};

    core::VoidResult validationResult = ValidateCleanShutdown(bootstrap);
    core::VoidResult shutdownResult = bootstrap.Shutdown();

    if (!runResult)
    {
        return runResult;
    }
    if (!validationResult)
    {
        return validationResult;
    }
    return shutdownResult;
}

[[nodiscard]] core::VoidResult RunClearWindow()
{
    const platform::PlatformRuntimeDesc runtimeDesc{
        .applicationName = "Ponder Render Clear Window",
        .applicationVersion = std::string{"0.1.0"},
        .applicationIdentifier = std::string{"org.ponder.examples.render.clear-window"},
    };
    auto runtimeResult = platform::PlatformRuntime::Create(runtimeDesc);
    if (!runtimeResult)
    {
        return core::VoidResult::FromError(std::move(runtimeResult).GetError());
    }
    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    core::VoidResult rendererResult;
    {
        const platform::WindowDesc windowDesc{
            .title = "Ponder Render - Clear Window",
            .logicalSize = {960, 640},
            .visible = false,
            .resizable = true,
            .highPixelDensity = true,
            .minimumLogicalSize = platform::LogicalSize{320, 240},
            .graphicsCompatibility = render::GetRequiredWindowGraphicsCompatibility(),
        };
        auto windowResult = runtime.CreateWindow(windowDesc);
        if (!windowResult)
        {
            return core::VoidResult::FromError(std::move(windowResult).GetError());
        }
        platform::Window window = std::move(windowResult).GetValue();

        rendererResult = RunRenderer(runtime, window);
        // Window destruction happens here, after RunRenderer has destroyed RenderBootstrap.
    }
    // PlatformRuntime is the final owner released when this function returns.
    return rendererResult;
}
} // namespace

int main()
{
    try
    {
        const core::VoidResult result = RunClearWindow();
        if (!result)
        {
            LOG_ERROR_CATEGORY(kLogCategory, "example_failed error={}",
                               core::FormatError(result.GetError()));
            core::FlushLog();
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "example_exception message={}", exception.what());
        core::FlushLog();
        return 1;
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "example_exception message=unknown");
        core::FlushLog();
        return 1;
    }

    LOG_INFO_CATEGORY(kLogCategory, "example_shutdown_complete");
    core::FlushLog();
    return 0;
}
