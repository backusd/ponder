#include <ponder/core/Log.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace std
{
template <>
struct formatter<pond::platform::WindowId> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowId id, FormatContext& context) const
    {
        if (!id.IsValid())
        {
            return formatter<string>::format("invalid", context);
        }

        return formatter<string>::format(std::to_string(id.GetValue()), context);
    }
};
} // namespace std

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


[[nodiscard]] core::Result<render::RenderTargetSnapshot> CaptureWindowSnapshot(
    const platform::Window& window, std::uint64_t snapshotRevision,
    render::PresentationEnvironmentRevision presentationEnvironmentRevision)
{
    auto pixelSize = window.GetPixelSize();
    RETURN_ERROR_IF_FAILED(pixelSize);

    auto logicalSize = window.GetLogicalSize();
    RETURN_ERROR_IF_FAILED(logicalSize);

    auto visible = window.IsVisible();
    RETURN_ERROR_IF_FAILED(visible);

    auto windowState = window.GetState();
    RETURN_ERROR_IF_FAILED(windowState);

    return render::RenderTargetSnapshot{window.GetId(),         pixelSize.GetValue(),
                                        logicalSize.GetValue(), visible.GetValue(),
                                        windowState.GetValue(), presentationEnvironmentRevision,
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
                                 std::format("{}", snapshotResult.GetError()));
            batch.captureFailureReported = true;
        }
        return false;
    }

    const render::RenderTargetSnapshot snapshot = std::move(snapshotResult).GetValue();
    RETURN_ERROR_IF_FAILED(target.UpdateTargetSnapshot(snapshot));

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
    if (!frameResult && frameResult.GetError() == render::RenderErrorCode::SurfaceLost)
    {
        return FrameAction::RecoverSurface;
    }
    RETURN_ERROR_IF_FAILED(frameResult);

    render::RenderFrame frame = std::move(frameResult).GetValue();
    core::VoidResult clear = frame.Clear();
    RETURN_ERROR_IF_FAILED_FN(
        clear,
        [&frame](const core::Error&)
        {
            // Releasing an unfinished frame explicitly abandons its acquired work safely.
            frame = render::RenderFrame{};
        });

    auto presentResult = frame.FinishAndPresent();
    if (!presentResult && presentResult.GetError() == render::RenderErrorCode::SurfaceLost)
    {
        return FrameAction::RecoverSurface;
    }
    RETURN_ERROR_IF_FAILED(presentResult);

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
    RETURN_ERROR_IF_FAILED(surfaceResult);

    render::PreparedSurface surface = std::move(surfaceResult).GetValue();
    RETURN_ERROR_IF_FAILED(surface.TransferToCurrentThread(snapshot));

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
        RETURN_ERROR_IF_FAILED(snapshotApplied);
        if (!snapshotApplied.GetValue())
        {
            std::this_thread::sleep_for(kNoWorkDelay);
            continue;
        }

        auto actionResult = RenderOneFrame(target);
        RETURN_ERROR_IF_FAILED(actionResult);

        switch (actionResult.GetValue())
        {
        case FrameAction::Presented:
            break;
        case FrameAction::NoWork:
            std::this_thread::sleep_for(target.IsSuspended() ? kSuspendedDelay : kNoWorkDelay);
            break;
        case FrameAction::RecoverSurface:
            LOG_WARNING_CATEGORY(kLogCategory, "surface_lost_recovery_requested window={}",
                                 std::format("{}", window.GetId()));
            auto recovery = RecoverSurface(bootstrap, window, target);
            RETURN_ERROR_IF_FAILED(recovery);
            LOG_INFO_CATEGORY(kLogCategory, "surface_recovered window={}",
                              std::format("{}", window.GetId()));
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
    RETURN_ERROR_IF_FAILED(initialSnapshotResult);
    const render::RenderTargetSnapshot initialSnapshot =
        std::move(initialSnapshotResult).GetValue();

    auto bootstrapResult = render::RenderBootstrap::Create(render::RenderBootstrapDesc{});
    RETURN_ERROR_IF_FAILED(bootstrapResult);
    render::RenderBootstrap bootstrap = std::move(bootstrapResult).GetValue();

    auto surfaceResult =
        bootstrap.PrepareSurface(window, render::SurfacePreparationDesc{
                                             .targetSnapshot = initialSnapshot,
                                             .reason = render::SurfacePreparationReason::Initial,
                                         });
    RETURN_ERROR_IF_FAILED(surfaceResult);
    render::PreparedSurface deviceSelectionSurface = std::move(surfaceResult).GetValue();

    RETURN_ERROR_IF_FAILED(deviceSelectionSurface.TransferToCurrentThread(initialSnapshot));

    auto selectionResult =
        bootstrap.SelectAdapter(deviceSelectionSurface, render::RenderAdapterSelectionDesc{});
    RETURN_ERROR_IF_FAILED(selectionResult);
    render::RenderAdapterSelection selection = std::move(selectionResult).GetValue();

    auto deviceResult =
        bootstrap.CreateDevice(deviceSelectionSurface, selection, render::RenderDeviceDesc{});
    RETURN_ERROR_IF_FAILED(deviceResult);
    render::RenderDevice device = std::move(deviceResult).GetValue();
    render::PreparedSurface targetSurface = std::move(deviceSelectionSurface);

    auto targetResult =
        device.CreateRenderTarget(std::move(targetSurface), render::RenderTargetDesc{
                                                                .targetSnapshot = initialSnapshot,
                                                                .clearColor = kClearColor,
                                                            });
    RETURN_ERROR_IF_FAILED_FN(
        targetResult,
        [&targetSurface, &device](const core::Error&)
        {
            // A failed target creation may leave the prepared surface unconsumed.
            targetSurface = render::PreparedSurface{};
            device = render::RenderDevice{};
        });
    render::RenderTarget target = std::move(targetResult).GetValue();

    LOG_INFO_CATEGORY(kLogCategory, "renderer_ready adapter={} window={}",
                      selection.selectedAdapter.identity.name, std::format("{}", window.GetId()));

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
    RETURN_ERROR_IF_FAILED(runtimeResult);
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
        RETURN_ERROR_IF_FAILED(windowResult);
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
                               std::format("{}", result.GetError()));
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
