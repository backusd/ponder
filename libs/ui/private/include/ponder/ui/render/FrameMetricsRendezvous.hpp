#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/ui/Metrics.hpp>

namespace pond::ui::detail
{
[[nodiscard]] core::Result<UiTargetMetrics> MakeUiTargetMetricsForFrame(
    const ::pond::render::RenderFrameMetrics& frameMetrics);

[[nodiscard]] core::VoidResult ValidateUiTargetMetricsForFrame(
    const UiTargetMetrics& uiMetrics, const ::pond::render::RenderFrameMetrics& frameMetrics);
} // namespace pond::ui::detail
