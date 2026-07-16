#include <ponder/render/Bootstrap.hpp>
#include <ponder/ui/Error.hpp>
#include <ponder/ui/render/FrameMetricsRendezvous.hpp>

#include <array>
#include <cstdint>
#include <gtest/gtest.h>

namespace pond::ui::detail
{
[[nodiscard]] bool IsRenderIntegrationTopologyLinked() noexcept;
} // namespace pond::ui::detail

namespace
{
[[nodiscard]] pond::render::RenderFrameMetrics MakeFrameMetrics()
{
    return pond::render::RenderFrameMetrics{.windowId = pond::platform::WindowId{17U},
                                            .logicalSize = pond::platform::LogicalSize{800U, 600U},
                                            .pixelSize = pond::platform::PixelSize{1200U, 900U},
                                            .metricsRevision =
                                                pond::render::PresentationEnvironmentRevision{23U},
                                            .targetRevision = 29U};
}

void ExpectUiError(const auto& result, pond::ui::UiErrorCode expected)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::ui::ToErrorCode(expected));
}
} // namespace

TEST(UiRenderIntegrationTopologyTests, LinksTheCompleteStaticIntegrationClosure)
{
    EXPECT_TRUE(pond::ui::detail::IsRenderIntegrationTopologyLinked());
}

TEST(UiRenderIntegrationTopologyTests, ConvertsAndMatchesClosedActiveFrameMetricsExactly)
{
    const pond::render::RenderFrameMetrics frameMetrics = MakeFrameMetrics();
    const auto uiMetricsResult = pond::ui::detail::MakeUiTargetMetricsForFrame(frameMetrics);
    ASSERT_TRUE(uiMetricsResult.HasValue()) << uiMetricsResult.GetError().GetMessage();
    const pond::ui::UiTargetMetrics& uiMetrics = uiMetricsResult.GetValue();

    EXPECT_EQ(uiMetrics.GetTargetId(), pond::ui::UiTargetId{17U});
    EXPECT_EQ(uiMetrics.GetTargetRevision(), pond::ui::UiTargetRevision{29U});
    EXPECT_EQ(uiMetrics.GetMetricsRevision(), pond::ui::UiMetricsRevision{23U});
    EXPECT_EQ(uiMetrics.GetLogicalSize(), (pond::ui::LogicalSize{800.0F, 600.0F}));
    EXPECT_EQ(uiMetrics.GetFramebufferPixelSize(), (pond::ui::FramebufferPixelSize{1200U, 900U}));

    const auto validation =
        pond::ui::detail::ValidateUiTargetMetricsForFrame(uiMetrics, frameMetrics);
    EXPECT_TRUE(validation.HasValue()) << validation.GetError().GetMessage();
}

TEST(UiRenderIntegrationTopologyTests, RejectsEveryValidFrameMetricsMismatch)
{
    const pond::render::RenderFrameMetrics frameMetrics = MakeFrameMetrics();
    const auto matchingResult = pond::ui::detail::MakeUiTargetMetricsForFrame(frameMetrics);
    ASSERT_TRUE(matchingResult.HasValue()) << matchingResult.GetError().GetMessage();
    const pond::ui::UiTargetMetrics& matching = matchingResult.GetValue();

    std::array mismatches{frameMetrics, frameMetrics, frameMetrics, frameMetrics, frameMetrics};
    mismatches[0].windowId = pond::platform::WindowId{18U};
    mismatches[1].targetRevision = 30U;
    mismatches[2].metricsRevision = pond::render::PresentationEnvironmentRevision{24U};
    mismatches[3].logicalSize.width = 801U;
    mismatches[4].pixelSize.height = 901U;

    for (const pond::render::RenderFrameMetrics& mismatch : mismatches)
    {
        SCOPED_TRACE(mismatch.targetRevision);
        ExpectUiError(pond::ui::detail::ValidateUiTargetMetricsForFrame(matching, mismatch),
                      pond::ui::UiErrorCode::MetricsMismatch);
    }
}

TEST(UiRenderIntegrationTopologyTests, RejectsInvalidMetricsAndAcceptsSuspendedFrameValues)
{
    ExpectUiError(pond::ui::detail::MakeUiTargetMetricsForFrame({}),
                  pond::ui::UiErrorCode::InvalidMetrics);
    ExpectUiError(pond::ui::detail::ValidateUiTargetMetricsForFrame(pond::ui::UiTargetMetrics{},
                                                                    MakeFrameMetrics()),
                  pond::ui::UiErrorCode::InvalidMetrics);

    pond::render::RenderFrameMetrics suspendedFrame = MakeFrameMetrics();
    suspendedFrame.logicalSize.width = 0U;
    suspendedFrame.pixelSize.width = 0U;
    const auto suspendedUi = pond::ui::detail::MakeUiTargetMetricsForFrame(suspendedFrame);
    ASSERT_TRUE(suspendedUi.HasValue()) << suspendedUi.GetError().GetMessage();
    EXPECT_FALSE(pond::ui::IsDrawable(suspendedUi.GetValue()));
    const auto validation =
        pond::ui::detail::ValidateUiTargetMetricsForFrame(suspendedUi.GetValue(), suspendedFrame);
    EXPECT_TRUE(validation.HasValue()) << validation.GetError().GetMessage();
}
