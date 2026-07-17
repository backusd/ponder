#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>
#include <ponder/render/draw2d/Draw2DLayer.hpp>
#include <ponder/ui/Error.hpp>
#include <ponder/ui/experimental/RectangleRenderer.hpp>
#include <ponder/ui/paint/PaintCompiler.hpp>
#include <ponder/ui/paint/PaintRecorder.hpp>
#include <ponder/ui/render/FrameMetricsRendezvous.hpp>

#include <new>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace pond::ui::experimental
{
namespace
{
[[nodiscard]] bool ShouldSkipFrame(render::FrameStatus status) noexcept
{
    return status == render::FrameStatus::SkippedSuspended ||
           status == render::FrameStatus::TimedOut ||
           status == render::FrameStatus::RecreationPending;
}

[[nodiscard]] core::Error MakeRenderError(render::RenderErrorCode code, std::string message)
{
    return core::Error{render::ToErrorCode(code), std::move(message)};
}

template <typename Value>
[[nodiscard]] core::Result<Value> MakeRenderFailure(render::RenderErrorCode code,
                                                    std::string message)
{
    return core::Result<Value>::FromError(MakeRenderError(code, std::move(message)));
}

template <typename Value>
[[nodiscard]] core::Result<Value> PropagateError(core::Error error)
{
    return core::Result<Value>::FromError(std::move(error));
}
} // namespace

struct RectangleRenderer::State final
{
    explicit State(render::draw2d::Draw2DLayer layer, UiHardLimits limits)
        : drawLayer{std::move(layer)}, recorder{limits}, compiler{limits}
    {
    }

    render::draw2d::Draw2DLayer drawLayer;
    paint::PaintRecorder recorder;
    paint::SealedPaintList paintList;
    paint::PaintCompiler compiler;
    std::optional<render::draw2d::Draw2DPacket> packet;
};

core::Result<UiTargetMetrics> MakeUiTargetMetricsForFrame(const render::RenderFrame& frame)
{
    if (core::VoidResult renderThread = frame.VerifyRenderThread(); !renderThread)
    {
        return PropagateError<UiTargetMetrics>(std::move(renderThread).GetError());
    }

    return detail::MakeUiTargetMetricsForFrame(frame.GetMetrics());
}

RectangleRenderer::RectangleRenderer() noexcept = default;

RectangleRenderer::RectangleRenderer(std::unique_ptr<State> state) noexcept
    : m_state{std::move(state)}
{
}

RectangleRenderer::RectangleRenderer(RectangleRenderer&& other) noexcept = default;

RectangleRenderer& RectangleRenderer::operator=(RectangleRenderer&& other) noexcept = default;

RectangleRenderer::~RectangleRenderer() = default;

core::Result<RectangleRenderer> RectangleRenderer::Create(render::RenderDevice& device)
{
    auto layerResult = render::draw2d::Draw2DLayer::Create(device);
    if (!layerResult)
    {
        return PropagateError<RectangleRenderer>(std::move(layerResult).GetError());
    }

    try
    {
        auto state =
            std::make_unique<State>(std::move(layerResult).GetValue(), kDefaultUiHardLimits);
        return core::Result<RectangleRenderer>::FromValue(RectangleRenderer{std::move(state)});
    }
    catch (const std::bad_alloc&)
    {
        return MakeRenderFailure<RectangleRenderer>(
            render::RenderErrorCode::OutOfMemory,
            "Experimental rectangle renderer could not allocate its private state.");
    }
}

bool RectangleRenderer::IsValid() const noexcept
{
    return m_state != nullptr && m_state->drawLayer.IsValid();
}

core::Result<RectangleRecordOutcome> RectangleRenderer::Record(render::RenderFrame& frame,
                                                               const UiTargetMetrics& metrics,
                                                               RectanglePaint rectangle)
{
    return Record(frame, metrics, std::span<const RectanglePaint>{&rectangle, 1U});
}

core::Result<RectangleRecordOutcome> RectangleRenderer::Record(
    render::RenderFrame& frame, const UiTargetMetrics& metrics,
    std::span<const RectanglePaint> rectangles)
{
    if (m_state == nullptr)
    {
        return MakeRenderFailure<RectangleRecordOutcome>(
            render::RenderErrorCode::InvalidState,
            "Experimental rectangle renderer is moved-from or empty.");
    }

    if (core::VoidResult access = m_state->drawLayer.ValidateFrameAccess(frame); !access)
    {
        return PropagateError<RectangleRecordOutcome>(std::move(access).GetError());
    }

    for (const RectanglePaint& rectangle : rectangles)
    {
        if (!pond::ui::IsValid(rectangle.rectangle) || !pond::ui::IsValid(rectangle.color))
        {
            return MakeUiFailure<RectangleRecordOutcome>(
                UiErrorCode::InvalidPaintValue,
                "Experimental rectangle renderer received an invalid rectangle paint value.");
        }
    }

    if (core::VoidResult validatedMetrics =
            detail::ValidateUiTargetMetricsForFrame(metrics, frame.GetMetrics());
        !validatedMetrics)
    {
        return PropagateError<RectangleRecordOutcome>(std::move(validatedMetrics).GetError());
    }

    if (ShouldSkipFrame(frame.GetStatus()) || !IsDrawable(metrics))
    {
        return core::Result<RectangleRecordOutcome>::FromValue(
            RectangleRecordOutcome::SkippedSuspended);
    }

    m_state->recorder.Reset();
    if (m_state->packet.has_value())
    {
        m_state->compiler.Reset(std::move(*m_state->packet));
        m_state->packet.reset();
    }
    else
    {
        m_state->compiler.Reset();
    }

    for (const RectanglePaint& rectangle : rectangles)
    {
        if (core::VoidResult fill =
                m_state->recorder.FillRectangle(rectangle.rectangle, rectangle.color);
            !fill)
        {
            m_state->recorder.Reset();
            return PropagateError<RectangleRecordOutcome>(std::move(fill).GetError());
        }
    }

    if (core::VoidResult sealed = m_state->recorder.SealInto(m_state->paintList); !sealed)
    {
        m_state->recorder.Reset();
        return PropagateError<RectangleRecordOutcome>(std::move(sealed).GetError());
    }

    auto packetResult = m_state->compiler.Compile(m_state->paintList, metrics);
    if (!packetResult)
    {
        m_state->recorder.Reset();
        m_state->compiler.Reset();
        return PropagateError<RectangleRecordOutcome>(std::move(packetResult).GetError());
    }

    m_state->packet.emplace(std::move(packetResult).GetValue());
    const render::draw2d::Draw2DPacket& packet = *m_state->packet;
    if (packet.IsEmpty())
    {
        m_state->recorder.Reset();
        return core::Result<RectangleRecordOutcome>::FromValue(
            RectangleRecordOutcome::SkippedEmpty);
    }

    if (core::VoidResult recorded = m_state->drawLayer.Record(frame, packet); !recorded)
    {
        m_state->recorder.Reset();
        return PropagateError<RectangleRecordOutcome>(std::move(recorded).GetError());
    }

    m_state->recorder.Reset();
    return core::Result<RectangleRecordOutcome>::FromValue(RectangleRecordOutcome::Recorded);
}
} // namespace pond::ui::experimental
