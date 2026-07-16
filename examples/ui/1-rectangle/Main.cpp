#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>
#include <ponder/ui/experimental/RectangleRenderer.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
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
namespace ui = pond::ui;
namespace experimental = pond::ui::experimental;

// This facade exists only to exercise the rectangle milestone. It is not the future retained UI
// API, and this example must not be used as evidence that the retained API has been designed.
inline constexpr auto kSmokeDuration = std::chrono::milliseconds{3000};
inline constexpr auto kSmokeResizeAt = std::chrono::milliseconds{500};
inline constexpr auto kSmokeMinimizeAt = std::chrono::milliseconds{1250};
inline constexpr auto kSmokeRestoreAt = std::chrono::milliseconds{2000};
inline constexpr auto kSuspendedDelay = std::chrono::milliseconds{50};
inline constexpr auto kNoWorkDelay = std::chrono::milliseconds{16};
inline constexpr render::ClearColor kClearColor{
    .red = 0.012F,
    .green = 0.018F,
    .blue = 0.032F,
    .alpha = 1.0F,
};

struct Options final
{
    std::optional<platform::Duration> autoCloseAfter;
    render::RenderValidationMode validationMode{render::RenderValidationMode::Default};
    bool exerciseWindowState{};
    bool showHelp{};
};

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

struct WorkbenchStats final
{
    std::uint64_t acquiredFrames{};
    std::uint64_t recordedBatches{};
    std::uint64_t presentedFrames{};
    std::uint64_t uiPresentedFrames{};
    std::uint64_t postRestoreUiPresentedFrames{};
    std::uint64_t suspendedSkips{};
    std::uint64_t emptySkips{};
    std::uint64_t surfaceRecoveries{};
};

enum class FrameAction : std::uint8_t
{
    Presented,
    NoWork,
    RecoverSurface
};

enum class SmokeTourStage : std::uint8_t
{
    Resize,
    Minimize,
    Restore,
    Complete
};

struct FrameStep final
{
    FrameAction action{FrameAction::NoWork};
    std::optional<experimental::RectangleRecordOutcome> rectangleOutcome;
    std::optional<ui::UiTargetMetrics> metrics;
    bool acquired{};
    bool presented{};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0},
                       std::move(message)};
}

[[nodiscard]] core::Error MakeWorkbenchError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::General, 1}, std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::println("Usage: {} [--auto-close-ms <milliseconds> | --smoke]", executableName);
    std::println("\nOptions:");
    std::println("  --auto-close-ms <milliseconds>  Exit after a bounded interactive run.");
    std::println(
        "  --smoke                         Run a validation-disabled {} ms lifecycle smoke.",
        kSmokeDuration.count());
    std::println("  --help                          Print this help text.\n");
}

[[nodiscard]] core::Result<platform::Duration> ParseMilliseconds(std::string_view text)
{
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end)
    {
        return std::unexpected<core::Error>{
            MakeOptionError("Expected a non-negative integer millisecond value.")};
    }

    using Milliseconds = std::chrono::milliseconds;
    constexpr auto kMaximumMilliseconds =
        static_cast<std::uint64_t>(std::numeric_limits<Milliseconds::rep>::max());
    if (value > kMaximumMilliseconds)
    {
        return std::unexpected<core::Error>{
            MakeOptionError("Auto-close duration is too large.")};
    }

    return platform::Duration{Milliseconds{static_cast<Milliseconds::rep>(value)}};
}

[[nodiscard]] core::Result<Options> ParseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
        }
        else if (argument == "--smoke")
        {
            if (options.autoCloseAfter.has_value())
            {
                return std::unexpected<core::Error>{
                    MakeOptionError("Specify only one bounded-run option.")};
            }
            options.autoCloseAfter = platform::Duration{kSmokeDuration};
            options.validationMode = render::RenderValidationMode::Disabled;
            options.exerciseWindowState = true;
        }
        else if (argument == "--auto-close-ms")
        {
            if (options.autoCloseAfter.has_value())
            {
                return std::unexpected<core::Error>{
                    MakeOptionError("Specify only one bounded-run option.")};
            }
            if (index + 1 >= argc)
            {
                return std::unexpected<core::Error>{
                    MakeOptionError("--auto-close-ms requires a value.")};
            }

            ++index;
            auto duration = ParseMilliseconds(argv[index]);
            RETURN_ERROR_IF_FAILED(duration);
            options.autoCloseAfter = std::move(duration).GetValue();
        }
        else
        {
            return std::unexpected<core::Error>{
                MakeOptionError("Unknown option: " + std::string{argument})};
        }
    }

    return options;
}

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

[[nodiscard]] core::VoidResult AdvanceSmokeTour(platform::Window& window,
                                                platform::Duration elapsed,
                                                SmokeTourStage& stage)
{
    if (stage == SmokeTourStage::Resize &&
        elapsed >= platform::Duration{kSmokeResizeAt})
    {
        RETURN_ERROR_IF_FAILED(window.SetLogicalSize({800, 520}));
        std::println("Smoke tour requested a logical resize to 800x520.");
        stage = SmokeTourStage::Minimize;
    }
    else if (stage == SmokeTourStage::Minimize &&
             elapsed >= platform::Duration{kSmokeMinimizeAt})
    {
        RETURN_ERROR_IF_FAILED(window.Minimize());
        std::println("Smoke tour requested minimize.");
        stage = SmokeTourStage::Restore;
    }
    else if (stage == SmokeTourStage::Restore &&
             elapsed >= platform::Duration{kSmokeRestoreAt})
    {
        RETURN_ERROR_IF_FAILED(window.Restore());
        std::println("Smoke tour requested restore.");
        stage = SmokeTourStage::Complete;
    }

    return {};
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
            std::println(stderr, "Window snapshot capture deferred: {}",
                         snapshotResult.GetError());
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
    std::println("Target snapshot {} applied: logical={} pixels={} state={} visible={}",
                 snapshot.GetRevision(), snapshot.GetLogicalSize(), snapshot.GetPixelSize(),
                 snapshot.GetWindowState(), snapshot.IsVisible());
    return true;
}

[[nodiscard]] std::array<experimental::RectanglePaint, 3U> MakeRectangleScene(
    const ui::UiTargetMetrics& metrics) noexcept
{
    const ui::LogicalSize extent = metrics.GetLogicalSize();
    return {
        experimental::RectanglePaint{
            .rectangle = {.origin = {.x = extent.width * 0.08F, .y = extent.height * 0.10F},
                          .size = {.width = extent.width * 0.56F,
                                   .height = extent.height * 0.66F}},
            .color = {.red = 0.16F, .green = 0.48F, .blue = 0.92F, .alpha = 1.0F},
        },
        experimental::RectanglePaint{
            .rectangle = {.origin = {.x = extent.width * 0.28F, .y = extent.height * 0.22F},
                          .size = {.width = extent.width * 0.48F,
                                   .height = extent.height * 0.58F}},
            .color = {.red = 1.0F, .green = 0.22F, .blue = 0.06F, .alpha = 0.62F},
        },
        experimental::RectanglePaint{
            .rectangle = {.origin = {.x = extent.width * 0.45F, .y = extent.height * 0.36F},
                          .size = {.width = extent.width * 0.45F,
                                   .height = extent.height * 0.52F}},
            .color = {.red = 0.02F, .green = 0.92F, .blue = 0.65F, .alpha = 0.58F},
        },
    };
}

void PrintMetricsIfChanged(const ui::UiTargetMetrics& metrics,
                           std::optional<ui::UiTargetMetrics>& previous)
{
    if (previous.has_value() && *previous == metrics)
    {
        return;
    }

    const ui::LogicalSize logical = metrics.GetLogicalSize();
    const ui::FramebufferPixelSize pixels = metrics.GetFramebufferPixelSize();
    const ui::LogicalToFramebufferScale scale = metrics.GetLogicalToFramebufferScale();
    std::println("UI metrics revision {}: logical={}x{} pixels={}x{} scale=({}, {})",
                 metrics.GetMetricsRevision().GetValue(), logical.width, logical.height,
                 pixels.width, pixels.height, scale.x, scale.y);
    previous = metrics;
}

[[nodiscard]] core::Result<FrameStep> RenderOneFrame(
    render::RenderTarget& target, experimental::RectangleRenderer& rectangleRenderer)
{
    auto frameResult = target.AcquireFrame();
    if (!frameResult && frameResult.GetError() == render::RenderErrorCode::SurfaceLost)
    {
        return FrameStep{.action = FrameAction::RecoverSurface};
    }
    RETURN_ERROR_IF_FAILED(frameResult);

    render::RenderFrame frame = std::move(frameResult).GetValue();
    core::VoidResult clear = frame.Clear(kClearColor);
    RETURN_ERROR_IF_FAILED_FN(
        clear,
        [&frame](const core::Error&)
        {
            // Explicitly abandon acquired work before propagating the frame failure.
            frame = render::RenderFrame{};
        });

    auto metricsResult = experimental::MakeUiTargetMetricsForFrame(frame);
    RETURN_ERROR_IF_FAILED_FN(
        metricsResult,
        [&frame](const core::Error&)
        {
            frame = render::RenderFrame{};
        });
    const ui::UiTargetMetrics metrics = std::move(metricsResult).GetValue();
    const std::array rectangles = MakeRectangleScene(metrics);

    auto rectangleResult = rectangleRenderer.Record(frame, metrics, rectangles);
    RETURN_ERROR_IF_FAILED_FN(
        rectangleResult,
        [&frame](const core::Error&)
        {
            frame = render::RenderFrame{};
        });
    const experimental::RectangleRecordOutcome outcome = rectangleResult.GetValue();

    auto presentResult = frame.FinishAndPresent();
    if (!presentResult && presentResult.GetError() == render::RenderErrorCode::SurfaceLost)
    {
        return FrameStep{.action = FrameAction::RecoverSurface,
                         .rectangleOutcome = outcome,
                         .metrics = metrics,
                         .acquired = true};
    }
    RETURN_ERROR_IF_FAILED(presentResult);

    const bool presented = presentResult->presented;
    return FrameStep{.action = presented ? FrameAction::Presented : FrameAction::NoWork,
                     .rectangleOutcome = outcome,
                     .metrics = metrics,
                     .acquired = true,
                     .presented = presented};
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

void AccumulateFrameStep(const FrameStep& step, bool afterSmokeRestore, WorkbenchStats& stats)
{
    if (step.acquired)
    {
        ++stats.acquiredFrames;
    }
    bool recordedThisFrame{};
    if (step.rectangleOutcome.has_value())
    {
        switch (*step.rectangleOutcome)
        {
        case experimental::RectangleRecordOutcome::Recorded:
            ++stats.recordedBatches;
            recordedThisFrame = true;
            break;
        case experimental::RectangleRecordOutcome::SkippedSuspended:
            ++stats.suspendedSkips;
            break;
        case experimental::RectangleRecordOutcome::SkippedEmpty:
            ++stats.emptySkips;
            break;
        }
    }

    if (step.presented)
    {
        ++stats.presentedFrames;
        if (recordedThisFrame)
        {
            ++stats.uiPresentedFrames;
            if (afterSmokeRestore)
            {
                ++stats.postRestoreUiPresentedFrames;
            }
        }
    }
}

void PrintSummary(const WorkbenchStats& stats)
{
    std::println(
        "Workbench summary: acquired={} batches={} presented={} ui_presented={} "
        "post_restore_ui_presented={} "
        "suspended_skips={} empty_skips={} surface_recoveries={}",
        stats.acquiredFrames, stats.recordedBatches, stats.presentedFrames,
        stats.uiPresentedFrames, stats.postRestoreUiPresentedFrames,
        stats.suspendedSkips, stats.emptySkips, stats.surfaceRecoveries);
}

[[nodiscard]] core::Result<WorkbenchStats> RunEventAndRenderLoop(
    platform::PlatformRuntime& runtime, platform::Window& window,
    render::RenderBootstrap& bootstrap, render::RenderTarget& target,
    experimental::RectangleRenderer& rectangleRenderer, const Options& options)
{
    WindowEventBatch events{.windowId = window.GetId(), .snapshotRequested = true};
    SnapshotSequence sequence;
    WorkbenchStats stats;
    std::optional<ui::UiTargetMetrics> previousMetrics;
    const platform::Timestamp start = runtime.Now();
    SmokeTourStage smokeTourStage =
        options.exerciseWindowState ? SmokeTourStage::Resize : SmokeTourStage::Complete;
    bool suspensionReported{};

    while (!events.quitRequested && !events.closeRequested)
    {
        DrainEvents(runtime, events);
        if (events.quitRequested || events.closeRequested)
        {
            break;
        }

        if (options.autoCloseAfter.has_value() &&
            runtime.Now() - start >= *options.autoCloseAfter)
        {
            std::println("Bounded run duration reached after {}.", runtime.Now() - start);
            break;
        }

        RETURN_ERROR_IF_FAILED(AdvanceSmokeTour(window, runtime.Now() - start, smokeTourStage));

        auto snapshotApplied = TryApplyPendingSnapshot(window, target, events, sequence);
        RETURN_ERROR_IF_FAILED(snapshotApplied);
        if (!snapshotApplied.GetValue())
        {
            std::this_thread::sleep_for(kNoWorkDelay);
            continue;
        }

        auto stepResult = RenderOneFrame(target, rectangleRenderer);
        RETURN_ERROR_IF_FAILED(stepResult);
        const FrameStep step = std::move(stepResult).GetValue();
        const bool afterSmokeRestore =
            options.exerciseWindowState && smokeTourStage == SmokeTourStage::Complete;
        AccumulateFrameStep(step, afterSmokeRestore, stats);
        if (step.metrics.has_value())
        {
            PrintMetricsIfChanged(*step.metrics, previousMetrics);
        }

        switch (step.action)
        {
        case FrameAction::Presented:
            if (suspensionReported)
            {
                std::println("Drawable presentation resumed.");
                suspensionReported = false;
            }
            break;
        case FrameAction::NoWork:
        {
            const bool suspended = target.IsSuspended();
            if (suspended && !suspensionReported)
            {
                std::println("Target suspended; polling events with a {} ms backoff.",
                             kSuspendedDelay.count());
                suspensionReported = true;
            }
            std::this_thread::sleep_for(suspended ? kSuspendedDelay : kNoWorkDelay);
            break;
        }
        case FrameAction::RecoverSurface:
            std::println("Surface loss detected; rebuilding from the latest target snapshot.");
            RETURN_ERROR_IF_FAILED(RecoverSurface(bootstrap, window, target));
            ++stats.surfaceRecoveries;
            std::println("Surface recovery completed.");
            std::this_thread::sleep_for(kNoWorkDelay);
            break;
        }
    }

    if (options.autoCloseAfter.has_value() && stats.uiPresentedFrames == 0U)
    {
        return std::unexpected<core::Error>{MakeWorkbenchError(
            "Bounded mode ended before any rectangle batch was recorded and presented.")};
    }
    if (options.exerciseWindowState && stats.postRestoreUiPresentedFrames == 0U)
    {
        return std::unexpected<core::Error>{MakeWorkbenchError(
            "Smoke mode ended before rectangle presentation resumed after restore.")};
    }
    return stats;
}

[[nodiscard]] core::VoidResult ValidateCleanShutdown(const render::RenderBootstrap& bootstrap)
{
    const render::RenderValidationReport report = bootstrap.GetValidationReport();
    std::println("Validation summary: warnings={} errors={} dropped={} captured={}",
                 report.warningCount, report.errorCount, report.droppedMessageCount,
                 report.capturedMessageCount);
    if (report.IsClean() && report.droppedMessageCount == 0U)
    {
        return {};
    }

    for (std::size_t index = 0U; index < report.capturedMessageCount; ++index)
    {
        const render::RenderValidationMessage& message = report.capturedMessages[index];
        std::println(stderr, "Validation message: id={} number={}", message.GetMessageIdName(),
                     message.messageIdNumber);
    }

    return std::unexpected<core::Error>{core::Error{
        render::ToErrorCode(render::RenderErrorCode::BackendFailure),
        "Vulkan validation reported an unexpected warning, error, or dropped message."}};
}

[[nodiscard]] core::VoidResult RunRenderer(platform::PlatformRuntime& runtime,
                                           platform::Window& window, const Options& options)
{
    auto initialSnapshotResult =
        CaptureWindowSnapshot(window, 1U, render::PresentationEnvironmentRevision{1U});
    RETURN_ERROR_IF_FAILED(initialSnapshotResult);
    const render::RenderTargetSnapshot initialSnapshot =
        std::move(initialSnapshotResult).GetValue();

    auto bootstrapResult = render::RenderBootstrap::Create(
        render::RenderBootstrapDesc{.validationMode = options.validationMode});
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
            targetSurface = render::PreparedSurface{};
            device = render::RenderDevice{};
        });
    render::RenderTarget target = std::move(targetResult).GetValue();

    auto rectangleRendererResult = experimental::RectangleRenderer::Create(device);
    RETURN_ERROR_IF_FAILED(rectangleRendererResult);
    experimental::RectangleRenderer rectangleRenderer =
        std::move(rectangleRendererResult).GetValue();

    std::println("Rectangle workbench ready: adapter=\"{}\" window={} validation={}",
                 selection.selectedAdapter.identity.name, window.GetId(),
                 bootstrap.GetDiagnostics().validationEnabled);
    std::println("Experimental API warning: {}", experimental::kRectangleFacadeExperimentalNotice);
    std::println("Resize, minimize, and restore the window; close it to exit.");

    WorkbenchStats stats;
    const auto runWorkbench = [&]() -> core::VoidResult
    {
        RETURN_ERROR_IF_FAILED(window.Show());
        auto loopResult =
            RunEventAndRenderLoop(runtime, window, bootstrap, target, rectangleRenderer, options);
        RETURN_ERROR_IF_FAILED(loopResult);
        stats = std::move(loopResult).GetValue();
        PrintSummary(stats);
        return {};
    };
    const core::VoidResult runResult = runWorkbench();

    // Device-scoped UI state is retired before the target and device; all are gone before the
    // bootstrap, platform window, and runtime.
    rectangleRenderer = experimental::RectangleRenderer{};
    target = render::RenderTarget{};
    device = render::RenderDevice{};

    std::println("Renderer children retired: surfaces={} devices={} targets={}",
                 bootstrap.GetActivePreparedSurfaceCount(), bootstrap.GetActiveDeviceCount(),
                 bootstrap.GetActiveTargetCount());
    const core::VoidResult validationResult = ValidateCleanShutdown(bootstrap);
    const core::VoidResult shutdownResult = bootstrap.Shutdown();

    if (!runResult)
    {
        if (!validationResult)
        {
            std::println(stderr, "Secondary validation failure during teardown: {}",
                         validationResult.GetError());
        }
        if (!shutdownResult)
        {
            std::println(stderr, "Secondary bootstrap shutdown failure: {}",
                         shutdownResult.GetError());
        }
        return std::unexpected<core::Error>{runResult.GetError()};
    }
    if (!validationResult)
    {
        if (!shutdownResult)
        {
            std::println(stderr, "Secondary bootstrap shutdown failure: {}",
                         shutdownResult.GetError());
        }
        return std::unexpected<core::Error>{validationResult.GetError()};
    }
    return shutdownResult;
}

[[nodiscard]] core::VoidResult RunRectangleWorkbench(int argc, char** argv)
{
    auto optionsResult = ParseOptions(argc, argv);
    RETURN_ERROR_IF_FAILED(optionsResult);
    const Options options = std::move(optionsResult).GetValue();
    if (options.showHelp)
    {
        PrintUsage(argc > 0 ? argv[0] : "ponder-ui-1-rectangle");
        return {};
    }

    const platform::PlatformRuntimeDesc runtimeDesc{
        .applicationName = "Ponder UI Rectangle Workbench",
        .applicationVersion = std::string{"0.1.0"},
        .applicationIdentifier = std::string{"org.ponder.examples.ui.rectangle"},
    };
    auto runtimeResult = platform::PlatformRuntime::Create(runtimeDesc);
    RETURN_ERROR_IF_FAILED(runtimeResult);
    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    core::VoidResult rendererResult;
    {
        const platform::WindowDesc windowDesc{
            .title = "Ponder UI - Experimental Rectangle Workbench",
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

        rendererResult = RunRenderer(runtime, window, options);
        // The window is released here after every renderer owner has been retired.
    }
    // PlatformRuntime remains the final owner released when this function returns.
    return rendererResult;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const core::VoidResult result = RunRectangleWorkbench(argc, argv);
        if (!result)
        {
            std::println(stderr, "ponder-ui-1-rectangle failed: {}", result.GetError());
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "ponder-ui-1-rectangle terminated with an exception: {}",
                     exception.what());
        return 1;
    }
    catch (...)
    {
        std::println(stderr, "ponder-ui-1-rectangle terminated with an unknown exception.");
        return 1;
    }

    return 0;
}
