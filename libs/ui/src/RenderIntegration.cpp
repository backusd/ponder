#include <ponder/render/draw2d/Draw2DLayer.hpp>
#include <ponder/render/draw2d/Draw2DPacket.hpp>
#include <ponder/ui/Library.hpp>
#include <ponder/ui/render/FrameMetricsRendezvous.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>


namespace pond::ui::detail
{
core::Result<UiTargetMetrics> MakeUiTargetMetricsForFrame(
    const ::pond::render::RenderFrameMetrics& frameMetrics)
{
    if (!::pond::render::IsValid(frameMetrics))
    {
        return MakeUiFailure<UiTargetMetrics>(
            UiErrorCode::InvalidMetrics,
            "UI render integration requires valid active-frame metrics.");
    }

    return MakeUiTargetMetrics(
        UiTargetId{frameMetrics.windowId.GetValue()}, UiTargetRevision{frameMetrics.targetRevision},
        UiMetricsRevision{frameMetrics.metricsRevision.GetValue()},
        LogicalSize{static_cast<float>(frameMetrics.logicalSize.width),
                    static_cast<float>(frameMetrics.logicalSize.height)},
        FramebufferPixelSize{frameMetrics.pixelSize.width, frameMetrics.pixelSize.height});
}

core::VoidResult ValidateUiTargetMetricsForFrame(
    const UiTargetMetrics& uiMetrics, const ::pond::render::RenderFrameMetrics& frameMetrics)
{
    if (!IsValid(uiMetrics))
    {
        return MakeUiFailure(UiErrorCode::InvalidMetrics,
                             "UI render integration received invalid UI target metrics.");
    }

    const core::Result<UiTargetMetrics> expectedMetrics = MakeUiTargetMetricsForFrame(frameMetrics);
    if (!expectedMetrics)
    {
        return MakeUiFailure(UiErrorCode::InvalidMetrics,
                             "UI render integration received invalid active-frame metrics.");
    }

    if (uiMetrics != expectedMetrics.GetValue())
    {
        return MakeUiFailure(
            UiErrorCode::MetricsMismatch,
            "UI target identity, revisions, logical extent, and framebuffer extent must exactly "
            "match the active render frame before paint compilation.");
    }

    return core::VoidResult::Success();
}

} // namespace pond::ui::detail
