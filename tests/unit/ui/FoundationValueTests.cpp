#include <ponder/ui/Color.hpp>
#include <ponder/ui/Error.hpp>
#include <ponder/ui/Geometry.hpp>
#include <ponder/ui/Limits.hpp>
#include <ponder/ui/Metrics.hpp>
#include <ponder/ui/Outcome.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <string_view>
#include <type_traits>

namespace
{
constexpr bool FoundationValuesAreConstexpr()
{
    constexpr pond::ui::LogicalPoint kPoint{.x = -4.5F, .y = 8.25F};
    constexpr pond::ui::LogicalSize kSize{.width = 64.0F, .height = 32.0F};
    constexpr pond::ui::LogicalRect kRect{.origin = kPoint, .size = kSize};
    constexpr pond::ui::FramebufferPixelSize kPixelSize{.width = 1280U, .height = 720U};
    constexpr pond::ui::UiTargetId kTargetId{17U};
    constexpr pond::ui::UiTargetRevision kTargetRevision{19U};
    constexpr pond::ui::UiMetricsRevision kMetricsRevision{23U};
    constexpr pond::ui::PackedLinearPremultipliedRgba8 kPackedColor =
        pond::ui::PackedLinearPremultipliedRgba8::FromChannels(1U, 2U, 3U, 4U);

    return kRect.origin == kPoint && kRect.size == kSize && kPixelSize.width == 1280U &&
           kTargetId.GetValue() == 17U && kTargetRevision.GetValue() == 19U &&
           kMetricsRevision.GetValue() == 23U && kPackedColor.GetValue() == 0x0403'0201U;
}

static_assert(FoundationValuesAreConstexpr());
static_assert(std::is_trivially_copyable_v<pond::ui::LogicalRect>);
static_assert(std::is_trivially_copyable_v<pond::ui::FramebufferPixelSize>);
static_assert(std::is_trivially_copyable_v<pond::ui::UiDrawCounters>);
static_assert(!std::same_as<pond::ui::LogicalSize, pond::ui::FramebufferPixelSize>);
static_assert(pond::ui::IsValid(pond::ui::LogicalPoint{.x = -1.0F, .y = 2.0F}));
static_assert(pond::ui::IsValid(pond::ui::LogicalSize{.width = 0.0F, .height = 2.0F}));
static_assert(!pond::ui::IsValid(pond::ui::LogicalSize{.width = -1.0F, .height = 2.0F}));
static_assert(pond::ui::IsEmpty(pond::ui::LogicalRect{.origin = {.x = 0.0F, .y = 0.0F},
                                                      .size = {.width = 0.0F, .height = 4.0F}}));
static_assert(!pond::ui::HasPositiveArea(pond::ui::FramebufferPixelSize{.width = 0U,
                                                                        .height = 4U}));
static_assert(pond::ui::IsValid(pond::ui::kDefaultUiHardLimits));

void ExpectUiErrorCode(const pond::core::Error& error, pond::ui::UiErrorCode code)
{
    EXPECT_EQ(error.GetCode(), pond::ui::ToErrorCode(code));
}
} // namespace

TEST(UiFoundationValueTests, ReservesStableUiErrorRange)
{
    struct ErrorMapping final
    {
        pond::ui::UiErrorCode uiCode;
        pond::core::ErrorCategory category;
        pond::core::ErrorCodeValue value;
    };

    constexpr std::array kMappings{
        ErrorMapping{.uiCode = pond::ui::UiErrorCode::InvalidPaintValue,
                     .category = pond::core::ErrorCategory::InvalidArgument,
                     .value = 0x0004'0001},
        ErrorMapping{.uiCode = pond::ui::UiErrorCode::UnbalancedPaintState,
                     .category = pond::core::ErrorCategory::InvalidArgument,
                     .value = 0x0004'0002},
        ErrorMapping{.uiCode = pond::ui::UiErrorCode::InvalidMetrics,
                     .category = pond::core::ErrorCategory::InvalidArgument,
                     .value = 0x0004'0003},
        ErrorMapping{.uiCode = pond::ui::UiErrorCode::MetricsMismatch,
                     .category = pond::core::ErrorCategory::InvalidArgument,
                     .value = 0x0004'0004},
        ErrorMapping{.uiCode = pond::ui::UiErrorCode::CompilationFailure,
                     .category = pond::core::ErrorCategory::General,
                     .value = 0x0004'0005},
        ErrorMapping{.uiCode = pond::ui::UiErrorCode::LimitExceeded,
                     .category = pond::core::ErrorCategory::InvalidArgument,
                     .value = 0x0004'0006},
        ErrorMapping{.uiCode = pond::ui::UiErrorCode::InvalidPaintState,
                     .category = pond::core::ErrorCategory::InvalidArgument,
                     .value = 0x0004'0007}};

    for (const ErrorMapping& mapping : kMappings)
    {
        const pond::core::ErrorCode coreCode = pond::ui::ToErrorCode(mapping.uiCode);
        EXPECT_EQ(coreCode.GetCategory(), mapping.category);
        EXPECT_EQ(coreCode.GetValue(), mapping.value);
        EXPECT_GE(coreCode.GetValue(), pond::ui::kUiErrorCodeFirst);
        EXPECT_LE(coreCode.GetValue(), pond::ui::kUiErrorCodeLast);
    }

    const std::uint_least32_t expectedLine = static_cast<std::uint_least32_t>(__LINE__ + 1);
    const pond::core::VoidResult failure = pond::ui::MakeUiFailure(
        pond::ui::UiErrorCode::InvalidPaintValue, "source location regression");
    ASSERT_FALSE(failure.HasValue());
    EXPECT_EQ(failure.GetError().GetLocation().line(), expectedLine);
    EXPECT_NE(std::string_view{failure.GetError().GetLocation().file_name()}.find(
                  "FoundationValueTests.cpp"),
              std::string_view::npos);
}
TEST(UiFoundationValueTests, ValidatesLogicalGeometryWithoutPixelConflation)
{
    const auto rect = pond::ui::MakeLogicalRectFromEdges(-2.5F, 4.0F, 5.5F, 10.0F);
    ASSERT_TRUE(rect.HasValue());
    EXPECT_EQ(rect->origin, (pond::ui::LogicalPoint{-2.5F, 4.0F}));
    EXPECT_EQ(rect->size, (pond::ui::LogicalSize{8.0F, 6.0F}));
    EXPECT_FLOAT_EQ(pond::ui::GetRight(*rect), 5.5F);
    EXPECT_FLOAT_EQ(pond::ui::GetBottom(*rect), 10.0F);
    EXPECT_TRUE(pond::ui::HasPositiveArea(*rect));

    const auto empty = pond::ui::MakeLogicalRectFromEdges(1.0F, 2.0F, 1.0F, 5.0F);
    ASSERT_TRUE(empty.HasValue());
    EXPECT_TRUE(pond::ui::IsEmpty(*empty));
    EXPECT_FALSE(pond::ui::HasPositiveArea(*empty));

    const auto reversed = pond::ui::MakeLogicalRectFromEdges(3.0F, 2.0F, 1.0F, 5.0F);
    ASSERT_FALSE(reversed.HasValue());
    ExpectUiErrorCode(reversed.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto nonFinite = pond::ui::MakeLogicalRectFromEdges(
        0.0F, 0.0F, std::numeric_limits<float>::infinity(), 1.0F);
    ASSERT_FALSE(nonFinite.HasValue());
    ExpectUiErrorCode(nonFinite.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto overflowingExtent = pond::ui::MakeLogicalRectFromEdges(
        -std::numeric_limits<float>::max(), 0.0F, std::numeric_limits<float>::max(), 1.0F);
    ASSERT_FALSE(overflowingExtent.HasValue());
    ExpectUiErrorCode(overflowingExtent.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    constexpr pond::ui::LogicalRect kCollapsedPositiveExtent{
        .origin = pond::ui::LogicalPoint{.x = 16'777'216.0F, .y = 0.0F},
        .size = pond::ui::LogicalSize{.width = 1.0F, .height = 1.0F}};
    static_assert(pond::ui::GetRight(kCollapsedPositiveExtent) ==
                  kCollapsedPositiveExtent.origin.x);
    EXPECT_FALSE(pond::ui::IsValid(kCollapsedPositiveExtent));
    EXPECT_FALSE(pond::ui::HasPositiveArea(kCollapsedPositiveExtent));
}

TEST(UiFoundationValueTests, DerivesCopiedTargetMetricsFromExactExtents)
{
    const auto metrics = pond::ui::MakeUiTargetMetrics(
        pond::ui::UiTargetId{7U}, pond::ui::UiTargetRevision{11U}, pond::ui::UiMetricsRevision{13U},
        pond::ui::LogicalSize{.width = 800.0F, .height = 400.0F},
        pond::ui::FramebufferPixelSize{.width = 1200U, .height = 1000U});

    ASSERT_TRUE(metrics.HasValue());
    EXPECT_EQ(metrics->GetTargetId().GetValue(), 7U);
    EXPECT_EQ(metrics->GetTargetRevision().GetValue(), 11U);
    EXPECT_EQ(metrics->GetMetricsRevision().GetValue(), 13U);
    EXPECT_FLOAT_EQ(metrics->GetLogicalToFramebufferScale().x, 1.5F);
    EXPECT_FLOAT_EQ(metrics->GetLogicalToFramebufferScale().y, 2.5F);
    EXPECT_TRUE(pond::ui::IsDrawable(*metrics));

    const auto suspendedMetrics = pond::ui::MakeUiTargetMetrics(
        pond::ui::UiTargetId{7U}, pond::ui::UiTargetRevision{12U}, pond::ui::UiMetricsRevision{14U},
        pond::ui::LogicalSize{.width = 0.0F, .height = 400.0F},
        pond::ui::FramebufferPixelSize{.width = 0U, .height = 1000U});
    ASSERT_TRUE(suspendedMetrics.HasValue());
    EXPECT_FLOAT_EQ(suspendedMetrics->GetLogicalToFramebufferScale().x, 0.0F);
    EXPECT_FLOAT_EQ(suspendedMetrics->GetLogicalToFramebufferScale().y, 2.5F);
    EXPECT_FALSE(pond::ui::IsDrawable(*suspendedMetrics));

    const auto invalidIdentity = pond::ui::MakeUiTargetMetrics(
        pond::ui::UiTargetId{}, pond::ui::UiTargetRevision{12U}, pond::ui::UiMetricsRevision{14U},
        pond::ui::LogicalSize{.width = 800.0F, .height = 400.0F},
        pond::ui::FramebufferPixelSize{.width = 1200U, .height = 1000U});
    ASSERT_FALSE(invalidIdentity.HasValue());
    ExpectUiErrorCode(invalidIdentity.GetError(), pond::ui::UiErrorCode::InvalidMetrics);
}

TEST(UiFoundationValueTests, RejectsInvalidAuthoredColorsWithoutClamping)
{
    const auto color = pond::ui::MakeSrgbStraightAlphaColor(1.0F, 0.5F, 0.0F, 0.25F);
    ASSERT_TRUE(color.HasValue());
    EXPECT_FALSE(pond::ui::IsTransparent(*color));

    const auto transparent = pond::ui::MakeSrgbStraightAlphaColor(1.0F, 0.5F, 0.0F, 0.0F);
    ASSERT_TRUE(transparent.HasValue());
    EXPECT_TRUE(pond::ui::IsTransparent(*transparent));

    const auto outOfRange = pond::ui::MakeSrgbStraightAlphaColor(1.01F, 0.0F, 0.0F, 1.0F);
    ASSERT_FALSE(outOfRange.HasValue());
    ExpectUiErrorCode(outOfRange.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto nonFinite = pond::ui::MakeSrgbStraightAlphaColor(
        0.0F, std::numeric_limits<float>::quiet_NaN(), 0.0F, 1.0F);
    ASSERT_FALSE(nonFinite.HasValue());
    ExpectUiErrorCode(nonFinite.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto invalidQuantization =
        pond::ui::QuantizeUnitFloatToUnorm8(std::numeric_limits<float>::quiet_NaN());
    ASSERT_FALSE(invalidQuantization.HasValue());
    ExpectUiErrorCode(invalidQuantization.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);
}

TEST(UiFoundationValueTests, MatchesIndependentColorReferenceVectors)
{
    EXPECT_NEAR(pond::ui::ConvertSrgbChannelToLinear(0.5F), 0.21404114F, 0.000001F);
    EXPECT_NEAR(pond::ui::ConvertLinearChannelToSrgb(0.21404114F), 0.5F, 0.000001F);

    const auto authored = pond::ui::MakeSrgbStraightAlphaColor(0.5F, 1.0F, 0.0F, 0.25F);
    ASSERT_TRUE(authored.HasValue());

    const pond::ui::LinearPremultipliedColor semantic = pond::ui::ToLinearPremultiplied(*authored);
    EXPECT_NEAR(semantic.red, 0.05351029F, 0.000001F);
    EXPECT_NEAR(semantic.green, 0.25F, 0.000001F);
    EXPECT_NEAR(semantic.blue, 0.0F, 0.000001F);
    EXPECT_NEAR(semantic.alpha, 0.25F, 0.000001F);

    const auto packed = pond::ui::PackLinearPremultipliedRgba8(semantic);
    ASSERT_TRUE(packed.HasValue());
    EXPECT_EQ(packed->GetRed(), 14U);
    EXPECT_EQ(packed->GetGreen(), 64U);
    EXPECT_EQ(packed->GetBlue(), 0U);
    EXPECT_EQ(packed->GetAlpha(), 64U);
    EXPECT_EQ(packed->GetValue(), 0x4000'400EU);

    const pond::ui::LinearPremultipliedColor blended = pond::ui::BlendSourceOver(
        pond::ui::LinearPremultipliedColor{
            .red = 0.25F, .green = 0.0F, .blue = 0.0F, .alpha = 0.5F},
        pond::ui::LinearPremultipliedColor{
            .red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F});
    EXPECT_NEAR(blended.red, 0.25F, 0.000001F);
    EXPECT_NEAR(blended.green, 0.0F, 0.000001F);
    EXPECT_NEAR(blended.blue, 0.5F, 0.000001F);
    EXPECT_NEAR(blended.alpha, 1.0F, 0.000001F);
    EXPECT_NEAR(pond::ui::ConvertLinearChannelToSrgb(blended.blue), 0.73535698F, 0.000001F);

    struct ColorCase final
    {
        pond::ui::SrgbStraightAlphaColor authored;
        pond::ui::LinearPremultipliedColor semantic;
        std::uint32_t packed;
    };
    constexpr std::array colorCases{
        ColorCase{.authored = {.red = 1.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F},
                  .semantic = {.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F},
                  .packed = 0x0000'0000U},
        ColorCase{.authored = {.red = 1.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F},
                  .semantic = {.red = 1.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F},
                  .packed = 0xFF00'00FFU},
        ColorCase{.authored = {.red = 0.0F, .green = 1.0F, .blue = 0.0F, .alpha = 1.0F},
                  .semantic = {.red = 0.0F, .green = 1.0F, .blue = 0.0F, .alpha = 1.0F},
                  .packed = 0xFF00'FF00U},
        ColorCase{.authored = {.red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F},
                  .semantic = {.red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F},
                  .packed = 0xFFFF'0000U},
        ColorCase{.authored = {.red = 0.5F, .green = 0.5F, .blue = 0.5F, .alpha = 1.0F},
                  .semantic = {.red = 0.21404114F,
                               .green = 0.21404114F,
                               .blue = 0.21404114F,
                               .alpha = 1.0F},
                  .packed = 0xFF37'3737U},
        ColorCase{.authored = {.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 0.5F},
                  .semantic = {.red = 0.5F, .green = 0.5F, .blue = 0.5F, .alpha = 0.5F},
                  .packed = 0x8080'8080U}};

    for (const ColorCase& testCase : colorCases)
    {
        const pond::ui::LinearPremultipliedColor converted =
            pond::ui::ToLinearPremultiplied(testCase.authored);
        EXPECT_NEAR(converted.red, testCase.semantic.red, 0.000001F);
        EXPECT_NEAR(converted.green, testCase.semantic.green, 0.000001F);
        EXPECT_NEAR(converted.blue, testCase.semantic.blue, 0.000001F);
        EXPECT_NEAR(converted.alpha, testCase.semantic.alpha, 0.000001F);
        const auto packedCase = pond::ui::PackLinearPremultipliedRgba8(converted);
        ASSERT_TRUE(packedCase.HasValue()) << packedCase.GetError().GetMessage();
        EXPECT_EQ(packedCase->GetValue(), testCase.packed);
    }
}

TEST(UiFoundationValueTests, DefinesCheckedHardLimits)
{
    constexpr pond::ui::UiHardLimits kLimits{.maxPaintCommandCount = 4U,
                                             .maxPaintCommandPayloadBytes = 16U,
                                             .maxClipDepth = 2U,
                                             .maxCompilerScratchBytes = 32U,
                                             .maxCompiledVertexCount = 8U,
                                             .maxCompiledIndexCount = 12U,
                                             .maxDrawRecordCount = 3U,
                                             .maxDrawPacketBytes = 64U,
                                             .maxUploadBytes = 128U};

    static_assert(pond::ui::IsValid(kLimits));
    EXPECT_EQ(pond::ui::GetLimitValue(kLimits, pond::ui::UiHardLimitKind::PaintCommandCount), 4U);
    EXPECT_EQ(pond::ui::GetLimitValue(kLimits, pond::ui::UiHardLimitKind::UploadBytes), 128U);

    EXPECT_TRUE(
        pond::ui::CheckUiHardLimit(pond::ui::UiHardLimitKind::PaintCommandCount, 4U, kLimits)
            .HasValue());

    const pond::core::VoidResult tooManyCommands =
        pond::ui::CheckUiHardLimit(pond::ui::UiHardLimitKind::PaintCommandCount, 5U, kLimits);
    ASSERT_FALSE(tooManyCommands.HasValue());
    ExpectUiErrorCode(tooManyCommands.GetError(), pond::ui::UiErrorCode::LimitExceeded);

    constexpr pond::ui::UiLimitExceeded kExceeded =
        pond::ui::MakeUiLimitExceeded(pond::ui::UiHardLimitKind::DrawPacketBytes, 65U, 64U);
    EXPECT_EQ(kExceeded.kind, pond::ui::UiHardLimitKind::DrawPacketBytes);
    EXPECT_EQ(kExceeded.requested, 65U);
    EXPECT_EQ(kExceeded.allowed, 64U);
}

TEST(UiFoundationValueTests, DefinesDrawOutcomesAndDiagnostics)
{
    constexpr pond::ui::UiDrawCounters kCounters{.recorded = 1U,
                                                 .skippedSuspended = 2U,
                                                 .skippedEmpty = 3U,
                                                 .paintCommands = 4U,
                                                 .compiledVertices = 5U,
                                                 .compiledIndices = 6U,
                                                 .drawRecords = 7U,
                                                 .packetBytes = 8U,
                                                 .uploadBytes = 9U};
    constexpr pond::ui::UiHighWaterMarks kHighWater{.paintCommands = 4U,
                                                    .paintCommandPayloadBytes = 16U,
                                                    .clipDepth = 2U,
                                                    .compilerScratchBytes = 32U,
                                                    .compiledVertices = 8U,
                                                    .compiledIndices = 12U,
                                                    .drawRecords = 3U,
                                                    .packetBytes = 64U,
                                                    .uploadBytes = 128U};

    EXPECT_EQ(pond::ui::UiDrawOutcome::Recorded, pond::ui::UiDrawOutcome::Recorded);
    EXPECT_NE(pond::ui::UiDrawOutcome::Recorded, pond::ui::UiDrawOutcome::SkippedSuspended);
    EXPECT_NE(pond::ui::UiDrawOutcome::SkippedSuspended, pond::ui::UiDrawOutcome::SkippedEmpty);
    EXPECT_EQ(kCounters.uploadBytes, 9U);
    EXPECT_EQ(kHighWater.packetBytes, 64U);
}
