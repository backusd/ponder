#include <ponder/core/BuildInfo.hpp>
#include <ponder/ui/Color.hpp>
#include <ponder/ui/Error.hpp>
#include <ponder/ui/Geometry.hpp>
#include <ponder/ui/Library.hpp>
#include <ponder/ui/Limits.hpp>
#include <ponder/ui/Metrics.hpp>
#include <ponder/ui/Outcome.hpp>

#include <string_view>

int main()
{
    if (pond::ui::GetLibraryName() != std::string_view{"ui"})
    {
        return 1;
    }

    const pond::core::BuildInfo buildInfo = pond::core::GetBuildInfo();
    if (buildInfo.GetProjectName() != "ponder")
    {
        return 2;
    }

    const auto metrics = pond::ui::MakeUiTargetMetrics(
        pond::ui::UiTargetId{1U}, pond::ui::UiTargetRevision{1U}, pond::ui::UiMetricsRevision{1U},
        pond::ui::LogicalSize{.width = 100.0F, .height = 50.0F},
        pond::ui::FramebufferPixelSize{.width = 200U, .height = 100U});
    if (!metrics.HasValue() || !pond::ui::IsDrawable(*metrics))
    {
        return 3;
    }

    const auto color = pond::ui::MakeSrgbStraightAlphaColor(1.0F, 0.0F, 0.0F, 1.0F);
    if (!color.HasValue())
    {
        return 4;
    }

    const pond::ui::LinearPremultipliedColor linear = pond::ui::ToLinearPremultiplied(*color);
    const auto packed = pond::ui::PackLinearPremultipliedRgba8(linear);
    if (!packed.HasValue() || packed->GetRed() != 255U || packed->GetAlpha() != 255U)
    {
        return 5;
    }

    if (!pond::ui::CheckUiHardLimit(pond::ui::UiHardLimitKind::PaintCommandCount, 1U).HasValue())
    {
        return 6;
    }

    constexpr pond::ui::UiDrawCounters kCounters{.recorded = 1U};
    if (kCounters.recorded != 1U ||
        pond::ui::UiDrawOutcome::Recorded == pond::ui::UiDrawOutcome::SkippedEmpty)
    {
        return 7;
    }

    return 0;
}
