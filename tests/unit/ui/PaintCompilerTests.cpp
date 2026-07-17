#include <ponder/render/draw2d/Draw2DPacket.hpp>
#include <ponder/ui/paint/PaintCompiler.hpp>

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
namespace draw2d = pond::render::draw2d;

using pond::ui::paint::CompiledPixelBounds;
using pond::ui::paint::FractionalPixelRect;
using pond::ui::paint::kNoRejectedPaintCommand;
using pond::ui::paint::PaintCompileCountInspection;
using pond::ui::paint::PaintCompileCountIssue;
using pond::ui::paint::PaintCompiler;
using pond::ui::paint::PaintCompileStage;
using pond::ui::paint::PaintCompileStatus;
using pond::ui::paint::SealedPaintList;

static_assert(!std::copy_constructible<PaintCompiler>);
static_assert(std::move_constructible<PaintCompiler>);
static_assert(!std::is_copy_assignable_v<PaintCompiler>);
static_assert(std::is_move_assignable_v<PaintCompiler>);
static_assert(sizeof(FractionalPixelRect) == 32U);
static_assert(sizeof(draw2d::Draw2DVertex) == 12U);
static_assert(sizeof(draw2d::Draw2DDrawRecord) == 28U);

[[nodiscard]] pond::ui::LogicalRect MakeRect(float left, float top, float right, float bottom)
{
    const auto result = pond::ui::MakeLogicalRectFromEdges(left, top, right, bottom);
    if (!result.HasValue())
    {
        ADD_FAILURE() << "Failed to make a logical rectangle: " << result.GetError().GetMessage();
        return {};
    }
    return *result;
}

[[nodiscard]] pond::ui::SrgbStraightAlphaColor MakeColor(float red, float green, float blue,
                                                         float alpha)
{
    const auto result = pond::ui::MakeSrgbStraightAlphaColor(red, green, blue, alpha);
    if (!result.HasValue())
    {
        ADD_FAILURE() << "Failed to make an authored color: " << result.GetError().GetMessage();
        return {};
    }
    return *result;
}

[[nodiscard]] pond::ui::UiTargetMetrics MakeMetrics(float logicalWidth, float logicalHeight,
                                                    std::uint32_t pixelWidth,
                                                    std::uint32_t pixelHeight)
{
    const auto result = pond::ui::MakeUiTargetMetrics(
        pond::ui::UiTargetId{7U}, pond::ui::UiTargetRevision{11U}, pond::ui::UiMetricsRevision{13U},
        pond::ui::LogicalSize{.width = logicalWidth, .height = logicalHeight},
        pond::ui::FramebufferPixelSize{.width = pixelWidth, .height = pixelHeight});
    if (!result.HasValue())
    {
        ADD_FAILURE() << "Failed to make target metrics: " << result.GetError().GetMessage();
        return {};
    }
    return *result;
}

[[nodiscard]] SealedPaintList Seal(pond::ui::paint::PaintRecorder& recorder)
{
    auto result = recorder.Seal();
    if (!result.HasValue())
    {
        ADD_FAILURE() << "Failed to seal paint: " << result.GetError().GetMessage();
        return {};
    }
    return std::move(result).GetValue();
}

[[nodiscard]] SealedPaintList MakeSingleRectanglePaint(
    pond::ui::LogicalRect rectangle = MakeRect(1.0F, 2.0F, 9.0F, 12.0F),
    pond::ui::SrgbStraightAlphaColor color = MakeColor(1.0F, 1.0F, 1.0F, 1.0F))
{
    pond::ui::paint::PaintRecorder recorder;
    const pond::core::VoidResult fill = recorder.FillRectangle(rectangle, color);
    if (!fill.HasValue())
    {
        ADD_FAILURE() << "Failed to record rectangle: " << fill.GetError().GetMessage();
        return {};
    }
    return Seal(recorder);
}

void ExpectUiErrorCode(const pond::core::Error& error, pond::ui::UiErrorCode code)
{
    EXPECT_EQ(error.GetCode(), pond::ui::ToErrorCode(code));
}

void ExpectColor(draw2d::Draw2DPackedLinearPremultipliedRgba8 color, std::uint8_t red,
                 std::uint8_t green, std::uint8_t blue, std::uint8_t alpha)
{
    EXPECT_EQ(color.GetRed(), red);
    EXPECT_EQ(color.GetGreen(), green);
    EXPECT_EQ(color.GetBlue(), blue);
    EXPECT_EQ(color.GetAlpha(), alpha);
}

void ExpectVertex(const draw2d::Draw2DVertex& vertex, float x, float y, std::uint8_t red,
                  std::uint8_t green, std::uint8_t blue, std::uint8_t alpha)
{
    EXPECT_FLOAT_EQ(vertex.x, x);
    EXPECT_FLOAT_EQ(vertex.y, y);
    ExpectColor(vertex.color, red, green, blue, alpha);
}

void ExpectPacketsSemanticallyEqual(const draw2d::Draw2DPacket& lhs,
                                    const draw2d::Draw2DPacket& rhs)
{
    EXPECT_EQ(lhs.GetPixelExtent(), rhs.GetPixelExtent());
    EXPECT_EQ(lhs.GetSchemaFingerprint(), rhs.GetSchemaFingerprint());
    EXPECT_EQ(lhs.GetStats(), rhs.GetStats());

    const auto lhsVertices = lhs.GetVertices();
    const auto rhsVertices = rhs.GetVertices();
    ASSERT_EQ(lhsVertices.size(), rhsVertices.size());
    for (std::size_t index = 0U; index < lhsVertices.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(lhsVertices[index], rhsVertices[index]);
    }

    const auto lhsIndices = lhs.GetIndices();
    const auto rhsIndices = rhs.GetIndices();
    ASSERT_EQ(lhsIndices.size(), rhsIndices.size());
    for (std::size_t index = 0U; index < lhsIndices.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(lhsIndices[index], rhsIndices[index]);
    }

    const auto lhsDraws = lhs.GetDrawRecords();
    const auto rhsDraws = rhs.GetDrawRecords();
    ASSERT_EQ(lhsDraws.size(), rhsDraws.size());
    for (std::size_t index = 0U; index < lhsDraws.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(lhsDraws[index], rhsDraws[index]);
    }
}

void ExpectCompileLimitFailure(const SealedPaintList& paint,
                               const pond::ui::UiTargetMetrics& metrics,
                               pond::ui::UiHardLimits limits, pond::ui::UiHardLimitKind kind,
                               std::uint64_t requested, std::uint64_t allowed,
                               PaintCompileStage stage)
{
    PaintCompiler compiler{limits};
    const auto result = compiler.Compile(paint, metrics);
    ASSERT_FALSE(result.HasValue());
    ExpectUiErrorCode(result.GetError(), pond::ui::UiErrorCode::LimitExceeded);

    const auto inspection = compiler.GetInspection();
    EXPECT_EQ(inspection.status, PaintCompileStatus::Failed);
    EXPECT_EQ(inspection.stage, stage);
    ASSERT_TRUE(inspection.hasRejectedLimit);
    EXPECT_EQ(inspection.rejectedLimit, (pond::ui::UiLimitExceeded{kind, requested, allowed}));
    EXPECT_EQ(inspection.packetBuilder.state, draw2d::Draw2DPacketBuilderState::Open);
    EXPECT_EQ(inspection.packetBuilder.counts, draw2d::Draw2DPacketCounts{});
}
} // namespace

TEST(UiPaintCompilerTests, EmitsExactIdentityRectanglePacketWindingColorAndMetadata)
{
    const SealedPaintList paint = MakeSingleRectanglePaint(MakeRect(1.5F, 2.25F, 9.75F, 12.5F),
                                                           MakeColor(0.5F, 1.0F, 0.0F, 0.5F));
    const pond::ui::UiTargetMetrics metrics = MakeMetrics(100.0F, 80.0F, 100U, 80U);
    PaintCompiler compiler;

    auto result = compiler.Compile(paint, metrics);
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    draw2d::Draw2DPacket packet = std::move(result).GetValue();

    EXPECT_EQ(packet.GetPixelExtent(), (draw2d::Draw2DPixelExtent{100U, 80U}));
    EXPECT_EQ(packet.GetSchemaFingerprint(), draw2d::kDraw2DSchemaFingerprint);
    ASSERT_EQ(packet.GetVertices().size(), 4U);
    ExpectVertex(packet.GetVertices()[0], 1.5F, 2.25F, 27U, 128U, 0U, 128U);
    ExpectVertex(packet.GetVertices()[1], 9.75F, 2.25F, 27U, 128U, 0U, 128U);
    ExpectVertex(packet.GetVertices()[2], 9.75F, 12.5F, 27U, 128U, 0U, 128U);
    ExpectVertex(packet.GetVertices()[3], 1.5F, 12.5F, 27U, 128U, 0U, 128U);

    constexpr std::array<draw2d::Draw2DIndex, 6U> kExpectedIndices{0U, 1U, 2U, 0U, 2U, 3U};
    ASSERT_EQ(packet.GetIndices().size(), kExpectedIndices.size());
    for (std::size_t index = 0U; index < kExpectedIndices.size(); ++index)
    {
        EXPECT_EQ(packet.GetIndices()[index], kExpectedIndices[index]);
    }

    ASSERT_EQ(packet.GetDrawRecords().size(), 1U);
    EXPECT_EQ(packet.GetDrawRecords()[0],
              (draw2d::Draw2DDrawRecord{0U, 6U, 0, draw2d::Draw2DScissor{0U, 0U, 100U, 80U}}));

    const float signedAreaTwice = (packet.GetVertices()[1].x - packet.GetVertices()[0].x) *
                                      (packet.GetVertices()[2].y - packet.GetVertices()[0].y) -
                                  (packet.GetVertices()[1].y - packet.GetVertices()[0].y) *
                                      (packet.GetVertices()[2].x - packet.GetVertices()[0].x);
    EXPECT_GT(signedAreaTwice, 0.0F);

    const draw2d::Draw2DPacketStats stats = packet.GetStats();
    EXPECT_EQ(stats.counts, (draw2d::Draw2DPacketCounts{4U, 6U, 1U}));
    EXPECT_EQ(stats.vertexBytes, 48U);
    EXPECT_EQ(stats.indexBytes, 24U);
    EXPECT_EQ(stats.drawRecordBytes, 28U);
    EXPECT_EQ(stats.packetBytes, 100U);
    EXPECT_EQ(stats.uploadBytes, 72U);
    EXPECT_TRUE(draw2d::InspectDraw2DPacket(packet).IsValid());

    const auto inspection = compiler.GetInspection();
    EXPECT_EQ(inspection.status, PaintCompileStatus::Succeeded);
    EXPECT_EQ(inspection.stage, PaintCompileStage::None);
    EXPECT_EQ(inspection.lastCommandIndex, 0U);
    EXPECT_EQ(inspection.processedCommandCount, 1U);
    EXPECT_EQ(inspection.maximumClipDepth, 1U);
    EXPECT_EQ(inspection.visibleRectangleCount, 1U);
    EXPECT_EQ(inspection.mergedDrawRecordCount, 0U);
    EXPECT_EQ(inspection.plannedOutput.counts, (draw2d::Draw2DPacketCounts{4U, 6U, 1U}));
    EXPECT_EQ(inspection.outputBounds,
              (pond::ui::paint::CompiledPixelBounds{1.5F, 2.25F, 9.75F, 12.5F, true}));
    EXPECT_EQ(inspection.packetBuilder.state, draw2d::Draw2DPacketBuilderState::Sealed);
}

TEST(UiPaintCompilerTests, PreservesFractionalCoordinatesUnderIndependentAxisScales)
{
    struct ScaleCase final
    {
        std::string_view name;
        float logicalWidth;
        float logicalHeight;
        std::uint32_t pixelWidth;
        std::uint32_t pixelHeight;
        pond::ui::LogicalRect rectangle;
        std::array<float, 4U> expectedBounds;
    };

    const std::array cases{ScaleCase{.name = "identity",
                                     .logicalWidth = 20.0F,
                                     .logicalHeight = 20.0F,
                                     .pixelWidth = 20U,
                                     .pixelHeight = 20U,
                                     .rectangle = MakeRect(0.5F, 1.5F, 8.5F, 9.5F),
                                     .expectedBounds = {0.5F, 1.5F, 8.5F, 9.5F}},
                           ScaleCase{.name = "asymmetric",
                                     .logicalWidth = 10.0F,
                                     .logicalHeight = 20.0F,
                                     .pixelWidth = 25U,
                                     .pixelHeight = 30U,
                                     .rectangle = MakeRect(1.25F, 2.5F, 4.5F, 6.75F),
                                     .expectedBounds = {3.125F, 3.75F, 11.25F, 10.125F}},
                           ScaleCase{.name = "fractional-scale",
                                     .logicalWidth = 80.0F,
                                     .logicalHeight = 40.0F,
                                     .pixelWidth = 100U,
                                     .pixelHeight = 50U,
                                     .rectangle = MakeRect(0.5F, 1.5F, 8.5F, 9.5F),
                                     .expectedBounds = {0.625F, 1.875F, 10.625F, 11.875F}},
                           ScaleCase{.name = "double-scale",
                                     .logicalWidth = 16.0F,
                                     .logicalHeight = 12.0F,
                                     .pixelWidth = 32U,
                                     .pixelHeight = 24U,
                                     .rectangle = MakeRect(0.5F, 1.25F, 7.5F, 8.75F),
                                     .expectedBounds = {1.0F, 2.5F, 15.0F, 17.5F}}};

    for (const ScaleCase& testCase : cases)
    {
        SCOPED_TRACE(std::string{testCase.name});
        const SealedPaintList paint = MakeSingleRectanglePaint(testCase.rectangle);
        const pond::ui::UiTargetMetrics metrics =
            MakeMetrics(testCase.logicalWidth, testCase.logicalHeight, testCase.pixelWidth,
                        testCase.pixelHeight);
        PaintCompiler compiler;
        auto result = compiler.Compile(paint, metrics);
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();

        const draw2d::Draw2DPacket& packet = *result;
        ASSERT_EQ(packet.GetVertices().size(), 4U);
        ExpectVertex(packet.GetVertices()[0], testCase.expectedBounds[0],
                     testCase.expectedBounds[1], 255U, 255U, 255U, 255U);
        ExpectVertex(packet.GetVertices()[1], testCase.expectedBounds[2],
                     testCase.expectedBounds[1], 255U, 255U, 255U, 255U);
        ExpectVertex(packet.GetVertices()[2], testCase.expectedBounds[2],
                     testCase.expectedBounds[3], 255U, 255U, 255U, 255U);
        ExpectVertex(packet.GetVertices()[3], testCase.expectedBounds[0],
                     testCase.expectedBounds[3], 255U, 255U, 255U, 255U);
        EXPECT_EQ(packet.GetDrawRecords()[0].scissor,
                  (draw2d::Draw2DScissor{0U, 0U, testCase.pixelWidth, testCase.pixelHeight}));
    }
}

TEST(UiPaintCompilerTests, AcceptsOnePixelAndLargeExactlyRepresentableTargets)
{
    struct ExtentCase final
    {
        std::string_view name;
        float logicalExtent;
        std::uint32_t pixelExtent;
    };
    constexpr std::array cases{
        ExtentCase{.name = "one-pixel", .logicalExtent = 1.0F, .pixelExtent = 1U},
        ExtentCase{
            .name = "large-exact", .logicalExtent = 16'777'216.0F, .pixelExtent = 16'777'216U}};

    for (const ExtentCase& testCase : cases)
    {
        SCOPED_TRACE(std::string{testCase.name});
        const SealedPaintList paint = MakeSingleRectanglePaint(
            MakeRect(0.0F, 0.0F, testCase.logicalExtent, testCase.logicalExtent));
        PaintCompiler compiler;
        auto result =
            compiler.Compile(paint, MakeMetrics(testCase.logicalExtent, testCase.logicalExtent,
                                                testCase.pixelExtent, testCase.pixelExtent));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ASSERT_EQ(result->GetVertices().size(), 4U);
        EXPECT_FLOAT_EQ(result->GetVertices()[0].x, 0.0F);
        EXPECT_FLOAT_EQ(result->GetVertices()[0].y, 0.0F);
        EXPECT_FLOAT_EQ(result->GetVertices()[2].x, testCase.logicalExtent);
        EXPECT_FLOAT_EQ(result->GetVertices()[2].y, testCase.logicalExtent);
        EXPECT_EQ(result->GetDrawRecords()[0].scissor,
                  (draw2d::Draw2DScissor{0U, 0U, testCase.pixelExtent, testCase.pixelExtent}));
        EXPECT_TRUE(draw2d::InspectDraw2DPacket(*result).IsValid());
    }
}

TEST(UiPaintCompilerTests, IntersectsRootAndNestedFractionalClipsWithConservativeScissors)
{
    pond::ui::paint::PaintRecorder recorder;
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(10.25F, 5.5F, 80.75F, 60.25F)).HasValue());
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(20.125F, 0.0F, 90.5F, 40.25F)).HasValue());
    ASSERT_TRUE(recorder
                    .FillRectangle(MakeRect(-10.0F, -10.0F, 95.0F, 70.0F),
                                   MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
                    .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(-5.0F, 70.0F, 10.0F, 90.0F), MakeColor(0.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());
    const SealedPaintList paint = Seal(recorder);

    PaintCompiler compiler;
    auto result = compiler.Compile(paint, MakeMetrics(100.0F, 80.0F, 100U, 80U));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    const draw2d::Draw2DPacket& packet = *result;

    ASSERT_EQ(packet.GetVertices().size(), 8U);
    ExpectVertex(packet.GetVertices()[0], 20.125F, 5.5F, 255U, 0U, 0U, 255U);
    ExpectVertex(packet.GetVertices()[1], 80.75F, 5.5F, 255U, 0U, 0U, 255U);
    ExpectVertex(packet.GetVertices()[2], 80.75F, 40.25F, 255U, 0U, 0U, 255U);
    ExpectVertex(packet.GetVertices()[3], 20.125F, 40.25F, 255U, 0U, 0U, 255U);
    ExpectVertex(packet.GetVertices()[4], 0.0F, 70.0F, 0U, 255U, 0U, 255U);
    ExpectVertex(packet.GetVertices()[5], 10.0F, 70.0F, 0U, 255U, 0U, 255U);
    ExpectVertex(packet.GetVertices()[6], 10.0F, 80.0F, 0U, 255U, 0U, 255U);
    ExpectVertex(packet.GetVertices()[7], 0.0F, 80.0F, 0U, 255U, 0U, 255U);

    constexpr std::array<draw2d::Draw2DIndex, 12U> kExpectedIndices{0U, 1U, 2U, 0U, 2U, 3U,
                                                                    4U, 5U, 6U, 4U, 6U, 7U};
    ASSERT_EQ(packet.GetIndices().size(), kExpectedIndices.size());
    for (std::size_t index = 0U; index < kExpectedIndices.size(); ++index)
    {
        EXPECT_EQ(packet.GetIndices()[index], kExpectedIndices[index]);
    }

    ASSERT_EQ(packet.GetDrawRecords().size(), 2U);
    EXPECT_EQ(packet.GetDrawRecords()[0],
              (draw2d::Draw2DDrawRecord{0U, 6U, 0, draw2d::Draw2DScissor{20U, 5U, 81U, 41U}}));
    EXPECT_EQ(packet.GetDrawRecords()[1],
              (draw2d::Draw2DDrawRecord{6U, 6U, 0, draw2d::Draw2DScissor{0U, 0U, 100U, 80U}}));

    const auto inspection = compiler.GetInspection();
    EXPECT_EQ(inspection.maximumClipDepth, 3U);
    EXPECT_EQ(inspection.visibleRectangleCount, 2U);
    EXPECT_EQ(inspection.outputBounds,
              (pond::ui::paint::CompiledPixelBounds{0.0F, 5.5F, 80.75F, 80.0F, true}));
}

TEST(UiPaintCompilerTests, ElidesDisjointAndOffTargetGeometryThenRestoresClipState)
{
    pond::ui::paint::PaintRecorder recorder;
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(200.0F, 200.0F, 210.0F, 210.0F)).HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 10.0F, 10.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(-20.0F, 1.0F, 0.0F, 5.0F), MakeColor(0.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(100.0F, 1.0F, 110.0F, 5.0F), MakeColor(0.0F, 0.0F, 1.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(-5.0F, -5.0F, 5.0F, 5.0F), MakeColor(1.0F, 1.0F, 1.0F, 1.0F))
            .HasValue());
    const SealedPaintList paint = Seal(recorder);

    PaintCompiler compiler;
    auto result = compiler.Compile(paint, MakeMetrics(100.0F, 80.0F, 100U, 80U));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();

    const draw2d::Draw2DPacket& packet = *result;
    ASSERT_EQ(packet.GetVertices().size(), 4U);
    ExpectVertex(packet.GetVertices()[0], 0.0F, 0.0F, 255U, 255U, 255U, 255U);
    ExpectVertex(packet.GetVertices()[2], 5.0F, 5.0F, 255U, 255U, 255U, 255U);
    ASSERT_EQ(packet.GetDrawRecords().size(), 1U);
    EXPECT_EQ(packet.GetDrawRecords()[0].scissor, (draw2d::Draw2DScissor{0U, 0U, 100U, 80U}));

    const auto inspection = compiler.GetInspection();
    EXPECT_EQ(inspection.processedCommandCount, 6U);
    EXPECT_EQ(inspection.maximumClipDepth, 2U);
    EXPECT_EQ(inspection.visibleRectangleCount, 1U);
    EXPECT_EQ(inspection.clippedElisionCount, 3U);
}

TEST(UiPaintCompilerTests, ProducesCanonicalExtentSpecificEmptyPacketForAllElisionKinds)
{
    pond::ui::paint::PaintRecorder recorder;
    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 0.0F, 10.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 10.0F, 10.0F), MakeColor(0.0F, 1.0F, 0.0F, 0.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(200.0F, 200.0F, 210.0F, 210.0F)).HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 10.0F, 10.0F), MakeColor(0.0F, 0.0F, 1.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    const SealedPaintList paint = Seal(recorder);

    PaintCompiler compiler;
    auto result = compiler.Compile(paint, MakeMetrics(100.0F, 80.0F, 100U, 80U));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    const draw2d::Draw2DPacket& packet = *result;

    EXPECT_TRUE(packet.IsEmpty());
    EXPECT_EQ(packet.GetPixelExtent(), (draw2d::Draw2DPixelExtent{100U, 80U}));
    EXPECT_EQ(packet.GetSchemaFingerprint(), draw2d::kDraw2DSchemaFingerprint);
    EXPECT_EQ(packet.GetStats(), draw2d::Draw2DPacketStats{});
    EXPECT_TRUE(draw2d::InspectDraw2DPacket(packet).IsValid());

    const auto inspection = compiler.GetInspection();
    EXPECT_EQ(inspection.processedCommandCount, 5U);
    EXPECT_EQ(inspection.zeroAreaElisionCount, 1U);
    EXPECT_EQ(inspection.transparentElisionCount, 1U);
    EXPECT_EQ(inspection.clippedElisionCount, 1U);
    EXPECT_EQ(inspection.visibleRectangleCount, 0U);
    EXPECT_EQ(inspection.outputBounds, pond::ui::paint::CompiledPixelBounds{});
}

TEST(UiPaintCompilerTests, MergesOnlyAdjacentContiguousDrawsWithEqualScissors)
{
    pond::ui::paint::PaintRecorder recorder;
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 10.0F, 10.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(10.0F, 0.0F, 10.0F, 10.0F), MakeColor(1.0F, 1.0F, 1.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(10.0F, 0.0F, 20.0F, 10.0F), MakeColor(0.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 50.0F, 50.0F)).HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(20.0F, 0.0F, 30.0F, 10.0F), MakeColor(0.0F, 0.0F, 1.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(30.0F, 0.0F, 40.0F, 10.0F), MakeColor(1.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(40.0F, 0.0F, 50.0F, 10.0F), MakeColor(1.0F, 1.0F, 1.0F, 1.0F))
            .HasValue());
    const SealedPaintList paint = Seal(recorder);

    PaintCompiler compiler;
    auto result = compiler.Compile(paint, MakeMetrics(100.0F, 80.0F, 100U, 80U));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    const draw2d::Draw2DPacket& packet = *result;

    EXPECT_EQ(packet.GetVertices().size(), 20U);
    EXPECT_EQ(packet.GetIndices().size(), 30U);
    ASSERT_EQ(packet.GetDrawRecords().size(), 3U);
    EXPECT_EQ(packet.GetDrawRecords()[0],
              (draw2d::Draw2DDrawRecord{0U, 12U, 0, draw2d::Draw2DScissor{0U, 0U, 100U, 80U}}));
    EXPECT_EQ(packet.GetDrawRecords()[1],
              (draw2d::Draw2DDrawRecord{12U, 12U, 0, draw2d::Draw2DScissor{0U, 0U, 50U, 50U}}));
    EXPECT_EQ(packet.GetDrawRecords()[2],
              (draw2d::Draw2DDrawRecord{24U, 6U, 0, draw2d::Draw2DScissor{0U, 0U, 100U, 80U}}));

    constexpr std::array<draw2d::Draw2DIndex, 30U> kExpectedIndices{
        0U, 1U,  2U,  0U,  2U,  3U,  4U,  5U,  6U,  4U,  6U,  7U,  8U,  9U,  10U,
        8U, 10U, 11U, 12U, 13U, 14U, 12U, 14U, 15U, 16U, 17U, 18U, 16U, 18U, 19U};
    ASSERT_EQ(packet.GetIndices().size(), kExpectedIndices.size());
    for (std::size_t index = 0U; index < kExpectedIndices.size(); ++index)
    {
        EXPECT_EQ(packet.GetIndices()[index], kExpectedIndices[index]);
    }

    ExpectColor(packet.GetVertices()[0].color, 255U, 0U, 0U, 255U);
    ExpectColor(packet.GetVertices()[4].color, 0U, 255U, 0U, 255U);
    ExpectColor(packet.GetVertices()[8].color, 0U, 0U, 255U, 255U);
    ExpectColor(packet.GetVertices()[12].color, 255U, 255U, 0U, 255U);
    ExpectColor(packet.GetVertices()[16].color, 255U, 255U, 255U, 255U);

    const auto inspection = compiler.GetInspection();
    EXPECT_EQ(inspection.visibleRectangleCount, 5U);
    EXPECT_EQ(inspection.zeroAreaElisionCount, 1U);
    EXPECT_EQ(inspection.mergedDrawRecordCount, 2U);
    EXPECT_EQ(inspection.plannedOutput.counts, (draw2d::Draw2DPacketCounts{20U, 30U, 3U}));
}

TEST(UiPaintCompilerTests, RejectsGeometryThatCollapsesDuringPacketFloatNarrowing)
{
    const float left = std::nextafter(1.0F, 0.0F);
    SealedPaintList paint = MakeSingleRectanglePaint(MakeRect(left, 0.0F, 1.0F, 1.0F));
    PaintCompiler compiler;

    const auto result = compiler.Compile(paint, MakeMetrics(1.0F, 1.0F, 16'777'217U, 1U));

    ASSERT_FALSE(result.HasValue());
    ExpectUiErrorCode(result.GetError(), pond::ui::UiErrorCode::CompilationFailure);
    EXPECT_EQ(compiler.GetInspection().status, PaintCompileStatus::Failed);
    EXPECT_EQ(compiler.GetInspection().stage, PaintCompileStage::GeometryPreflight);
    EXPECT_EQ(compiler.GetInspection().lastCommandIndex, 0U);
    EXPECT_EQ(compiler.GetInspection().rejectedCommandIndex, 0U);
    EXPECT_EQ(compiler.GetInspection().processedCommandCount, 1U);
    EXPECT_FALSE(paint.IsEmpty());
}

TEST(UiPaintCompilerTests, RejectsInvalidAndSuspendedMetricsTransactionally)
{
    const SealedPaintList paint;
    PaintCompiler compiler;

    const auto invalid = compiler.Compile(paint, pond::ui::UiTargetMetrics{});
    ASSERT_FALSE(invalid.HasValue());
    ExpectUiErrorCode(invalid.GetError(), pond::ui::UiErrorCode::InvalidMetrics);
    EXPECT_EQ(compiler.GetInspection().stage, PaintCompileStage::MetricsValidation);
    EXPECT_FALSE(compiler.IsReady());

    compiler.Reset();
    const pond::ui::UiTargetMetrics suspended = MakeMetrics(0.0F, 80.0F, 0U, 80U);
    ASSERT_TRUE(pond::ui::IsValid(suspended));
    ASSERT_FALSE(pond::ui::IsDrawable(suspended));
    const auto suspendedResult = compiler.Compile(paint, suspended);
    ASSERT_FALSE(suspendedResult.HasValue());
    ExpectUiErrorCode(suspendedResult.GetError(), pond::ui::UiErrorCode::InvalidMetrics);
    EXPECT_EQ(compiler.GetInspection().stage, PaintCompileStage::MetricsValidation);

    compiler.Reset();
    const auto recovered = compiler.Compile(paint, MakeMetrics(100.0F, 80.0F, 100U, 80U));
    ASSERT_TRUE(recovered.HasValue()) << recovered.GetError().GetMessage();
    EXPECT_TRUE(recovered->IsEmpty());
}

TEST(UiPaintCompilerTests, EnforcesInputAndScratchLimitsAtExactBoundaries)
{
    pond::ui::paint::PaintRecorder recorder;
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 50.0F, 50.0F)).HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(1.0F, 1.0F, 10.0F, 10.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(10.0F, 10.0F, 20.0F, 20.0F), MakeColor(0.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    const SealedPaintList paint = Seal(recorder);
    const auto stats = paint.GetStats();
    const pond::ui::UiTargetMetrics metrics = MakeMetrics(100.0F, 80.0F, 100U, 80U);

    pond::ui::UiHardLimits exact = pond::ui::kDefaultUiHardLimits;
    exact.maxPaintCommandCount = stats.commandCount;
    exact.maxPaintCommandPayloadBytes = stats.payloadBytes;
    exact.maxClipDepth = stats.maxClipDepthObserved;
    exact.maxCompilerScratchBytes = 2U * sizeof(FractionalPixelRect);
    PaintCompiler exactCompiler{exact};
    const auto exactResult = exactCompiler.Compile(paint, metrics);
    ASSERT_TRUE(exactResult.HasValue()) << exactResult.GetError().GetMessage();
    EXPECT_EQ(exactCompiler.GetInspection().scratchBytes, 64U);

    pond::ui::UiHardLimits commandLimited = exact;
    --commandLimited.maxPaintCommandCount;
    ExpectCompileLimitFailure(paint, metrics, commandLimited,
                              pond::ui::UiHardLimitKind::PaintCommandCount, stats.commandCount,
                              stats.commandCount - 1U, PaintCompileStage::PaintValidation);

    pond::ui::UiHardLimits payloadLimited = exact;
    --payloadLimited.maxPaintCommandPayloadBytes;
    ExpectCompileLimitFailure(
        paint, metrics, payloadLimited, pond::ui::UiHardLimitKind::PaintCommandPayloadBytes,
        stats.payloadBytes, stats.payloadBytes - 1U, PaintCompileStage::PaintValidation);

    pond::ui::UiHardLimits clipLimited = exact;
    --clipLimited.maxClipDepth;
    ExpectCompileLimitFailure(paint, metrics, clipLimited, pond::ui::UiHardLimitKind::ClipDepth,
                              stats.maxClipDepthObserved, stats.maxClipDepthObserved - 1U,
                              PaintCompileStage::PaintValidation);

    pond::ui::UiHardLimits scratchLimited = exact;
    --scratchLimited.maxCompilerScratchBytes;
    ExpectCompileLimitFailure(paint, metrics, scratchLimited,
                              pond::ui::UiHardLimitKind::CompilerScratchBytes, 64U, 63U,
                              PaintCompileStage::ScratchPreflight);
}

TEST(UiPaintCompilerTests, EnforcesOutputLimitsAtExactBoundaries)
{
    const SealedPaintList paint = MakeSingleRectanglePaint();
    const pond::ui::UiTargetMetrics metrics = MakeMetrics(100.0F, 80.0F, 100U, 80U);

    pond::ui::UiHardLimits exact = pond::ui::kDefaultUiHardLimits;
    exact.maxCompiledVertexCount = 4U;
    exact.maxCompiledIndexCount = 6U;
    exact.maxDrawRecordCount = 1U;
    exact.maxDrawPacketBytes = 100U;
    exact.maxUploadBytes = 72U;
    PaintCompiler exactCompiler{exact};
    const auto exactResult = exactCompiler.Compile(paint, metrics);
    ASSERT_TRUE(exactResult.HasValue()) << exactResult.GetError().GetMessage();
    EXPECT_EQ(exactResult->GetStats().packetBytes, 100U);
    EXPECT_EQ(exactResult->GetStats().uploadBytes, 72U);

    pond::ui::UiHardLimits vertexLimited = exact;
    vertexLimited.maxCompiledVertexCount = 3U;
    ExpectCompileLimitFailure(paint, metrics, vertexLimited,
                              pond::ui::UiHardLimitKind::CompiledVertexCount, 4U, 3U,
                              PaintCompileStage::GeometryPreflight);

    pond::ui::UiHardLimits indexLimited = exact;
    indexLimited.maxCompiledIndexCount = 5U;
    ExpectCompileLimitFailure(paint, metrics, indexLimited,
                              pond::ui::UiHardLimitKind::CompiledIndexCount, 6U, 5U,
                              PaintCompileStage::GeometryPreflight);

    pond::ui::UiHardLimits packetLimited = exact;
    packetLimited.maxDrawPacketBytes = 99U;
    ExpectCompileLimitFailure(paint, metrics, packetLimited,
                              pond::ui::UiHardLimitKind::DrawPacketBytes, 100U, 99U,
                              PaintCompileStage::GeometryPreflight);

    pond::ui::UiHardLimits uploadLimited = exact;
    uploadLimited.maxUploadBytes = 71U;
    ExpectCompileLimitFailure(paint, metrics, uploadLimited, pond::ui::UiHardLimitKind::UploadBytes,
                              72U, 71U, PaintCompileStage::GeometryPreflight);

    pond::ui::paint::PaintRecorder twoDrawRecorder;
    ASSERT_TRUE(
        twoDrawRecorder
            .FillRectangle(MakeRect(1.0F, 1.0F, 10.0F, 10.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(twoDrawRecorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 50.0F, 50.0F)).HasValue());
    ASSERT_TRUE(
        twoDrawRecorder
            .FillRectangle(MakeRect(10.0F, 10.0F, 20.0F, 20.0F), MakeColor(0.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(twoDrawRecorder.PopClip().HasValue());
    const SealedPaintList twoDrawPaint = Seal(twoDrawRecorder);

    pond::ui::UiHardLimits twoDrawExact = pond::ui::kDefaultUiHardLimits;
    twoDrawExact.maxDrawRecordCount = 2U;
    PaintCompiler twoDrawCompiler{twoDrawExact};
    const auto twoDrawResult = twoDrawCompiler.Compile(twoDrawPaint, metrics);
    ASSERT_TRUE(twoDrawResult.HasValue()) << twoDrawResult.GetError().GetMessage();
    ASSERT_EQ(twoDrawResult->GetDrawRecords().size(), 2U);

    pond::ui::UiHardLimits drawLimited = twoDrawExact;
    drawLimited.maxDrawRecordCount = 1U;
    PaintCompiler drawLimitedCompiler{drawLimited};
    const auto drawLimitedResult = drawLimitedCompiler.Compile(twoDrawPaint, metrics);
    ASSERT_FALSE(drawLimitedResult.HasValue());
    ExpectUiErrorCode(drawLimitedResult.GetError(), pond::ui::UiErrorCode::LimitExceeded);
    const auto drawLimitedInspection = drawLimitedCompiler.GetInspection();
    EXPECT_EQ(drawLimitedInspection.status, PaintCompileStatus::Failed);
    EXPECT_EQ(drawLimitedInspection.stage, PaintCompileStage::GeometryPreflight);
    EXPECT_EQ(drawLimitedInspection.lastCommandIndex, 2U);
    EXPECT_EQ(drawLimitedInspection.rejectedCommandIndex, 2U);
    EXPECT_EQ(drawLimitedInspection.processedCommandCount, 3U);
    ASSERT_TRUE(drawLimitedInspection.hasRejectedLimit);
    EXPECT_EQ(drawLimitedInspection.rejectedLimit,
              (pond::ui::UiLimitExceeded{pond::ui::UiHardLimitKind::DrawRecordCount, 2U, 1U}));
    EXPECT_EQ(drawLimitedInspection.packetBuilder.state, draw2d::Draw2DPacketBuilderState::Open);
    EXPECT_EQ(drawLimitedInspection.packetBuilder.counts, draw2d::Draw2DPacketCounts{});
}

TEST(UiPaintCompilerTests, CountInspectionChecksExactStatisticsLimitsAndOverflow)
{
    const auto oneRectangle = pond::ui::paint::InspectPaintCompileCounts(
        1U, 1U, pond::ui::kDefaultUiHardLimits, draw2d::kDefaultDraw2DPacketLimits);
    ASSERT_TRUE(oneRectangle.IsValid());
    EXPECT_EQ(oneRectangle.counts, (draw2d::Draw2DPacketCounts{4U, 6U, 1U}));
    EXPECT_EQ(oneRectangle.packetStats.vertexBytes, 48U);
    EXPECT_EQ(oneRectangle.packetStats.indexBytes, 24U);
    EXPECT_EQ(oneRectangle.packetStats.drawRecordBytes, 28U);
    EXPECT_EQ(oneRectangle.packetStats.packetBytes, 100U);
    EXPECT_EQ(oneRectangle.packetStats.uploadBytes, 72U);

    pond::ui::UiHardLimits invalidLimits = pond::ui::kDefaultUiHardLimits;
    invalidLimits.maxCompiledVertexCount = 0U;
    const auto invalid = pond::ui::paint::InspectPaintCompileCounts(
        1U, 1U, invalidLimits, draw2d::kDefaultDraw2DPacketLimits);
    EXPECT_EQ(invalid.issue, PaintCompileCountIssue::InvalidLimits);

    constexpr std::uint64_t kMaximum = std::numeric_limits<std::uint64_t>::max();
    const auto vertexOverflow = pond::ui::paint::InspectPaintCompileCounts(
        (kMaximum / 4U) + 1U, 1U, pond::ui::kDefaultUiHardLimits,
        draw2d::kDefaultDraw2DPacketLimits);
    EXPECT_EQ(vertexOverflow.issue, PaintCompileCountIssue::VertexCountOverflow);
    ASSERT_TRUE(vertexOverflow.hasRejectedLimit);
    EXPECT_EQ(vertexOverflow.rejectedLimit.kind, pond::ui::UiHardLimitKind::CompiledVertexCount);
    EXPECT_EQ(vertexOverflow.rejectedLimit.requested, kMaximum);

    const auto indexOverflow = pond::ui::paint::InspectPaintCompileCounts(
        (kMaximum / 6U) + 1U, 1U, pond::ui::kDefaultUiHardLimits,
        draw2d::kDefaultDraw2DPacketLimits);
    EXPECT_EQ(indexOverflow.issue, PaintCompileCountIssue::IndexCountOverflow);
    ASSERT_TRUE(indexOverflow.hasRejectedLimit);
    EXPECT_EQ(indexOverflow.rejectedLimit.kind, pond::ui::UiHardLimitKind::CompiledIndexCount);
    EXPECT_EQ(indexOverflow.rejectedLimit.requested, kMaximum);
}

TEST(UiPaintCompilerTests, FailureAndResetPreserveInputPreviousPacketAndWarmCapacityAcrossMove)
{
    const SealedPaintList paint = MakeSingleRectanglePaint();
    const pond::ui::UiTargetMetrics metrics = MakeMetrics(100.0F, 80.0F, 100U, 80U);
    const auto sourceStats = paint.GetStats();
    PaintCompiler compiler;

    auto firstResult = compiler.Compile(paint, metrics);
    ASSERT_TRUE(firstResult.HasValue()) << firstResult.GetError().GetMessage();
    draw2d::Draw2DPacket firstPacket = std::move(firstResult).GetValue();
    const auto successfulInspection = compiler.GetInspection();
    ASSERT_EQ(successfulInspection.status, PaintCompileStatus::Succeeded);
    EXPECT_FALSE(compiler.IsReady());

    const auto withoutReset = compiler.Compile(paint, metrics);
    ASSERT_FALSE(withoutReset.HasValue());
    ExpectUiErrorCode(withoutReset.GetError(), pond::ui::UiErrorCode::InvalidPaintState);
    const auto withoutResetInspection = compiler.GetInspection();
    EXPECT_EQ(withoutResetInspection.status, PaintCompileStatus::Failed);
    EXPECT_EQ(withoutResetInspection.stage, PaintCompileStage::StateValidation);
    EXPECT_EQ(withoutResetInspection.lastCommandIndex, kNoRejectedPaintCommand);
    EXPECT_EQ(withoutResetInspection.rejectedCommandIndex, kNoRejectedPaintCommand);
    EXPECT_EQ(withoutResetInspection.processedCommandCount, 0U);
    EXPECT_EQ(withoutResetInspection.visibleRectangleCount, 0U);
    EXPECT_EQ(withoutResetInspection.plannedOutput, PaintCompileCountInspection{});
    EXPECT_EQ(withoutResetInspection.outputBounds, CompiledPixelBounds{});
    EXPECT_EQ(withoutResetInspection.packetBuilder.state, draw2d::Draw2DPacketBuilderState::Sealed);
    EXPECT_EQ(firstPacket.GetVertices().size(), 4U);
    EXPECT_EQ(paint.GetStats(), sourceStats);

    compiler.Reset();
    const auto resetInspection = compiler.GetInspection();
    EXPECT_TRUE(compiler.IsReady());
    EXPECT_EQ(resetInspection.status, PaintCompileStatus::Ready);
    EXPECT_GE(resetInspection.clipStackCapacity, successfulInspection.clipStackCapacity);
    EXPECT_GE(resetInspection.packetBuilder.capacities.vertexCount,
              successfulInspection.packetBuilder.capacities.vertexCount);
    EXPECT_GE(resetInspection.packetBuilder.capacities.indexCount,
              successfulInspection.packetBuilder.capacities.indexCount);
    EXPECT_GE(resetInspection.packetBuilder.capacities.drawRecordCount,
              successfulInspection.packetBuilder.capacities.drawRecordCount);

    const auto failedMetrics = compiler.Compile(paint, pond::ui::UiTargetMetrics{});
    ASSERT_FALSE(failedMetrics.HasValue());
    ExpectUiErrorCode(failedMetrics.GetError(), pond::ui::UiErrorCode::InvalidMetrics);
    EXPECT_EQ(firstPacket.GetVertices().size(), 4U);
    ExpectVertex(firstPacket.GetVertices()[0], 1.0F, 2.0F, 255U, 255U, 255U, 255U);
    EXPECT_EQ(paint.GetStats(), sourceStats);

    compiler.Reset();
    PaintCompiler movedCompiler = std::move(compiler);
    EXPECT_TRUE(compiler.IsReady());
    EXPECT_TRUE(movedCompiler.IsReady());
    const auto movedFromCompilerResult = compiler.Compile(paint, metrics);
    ASSERT_TRUE(movedFromCompilerResult.HasValue())
        << movedFromCompilerResult.GetError().GetMessage();
    auto secondResult = movedCompiler.Compile(paint, metrics);
    ASSERT_TRUE(secondResult.HasValue()) << secondResult.GetError().GetMessage();
    draw2d::Draw2DPacket secondPacket = std::move(secondResult).GetValue();
    ExpectPacketsSemanticallyEqual(firstPacket, secondPacket);
}

TEST(UiPaintCompilerTests, RepeatedCompilationProducesIdenticalSemanticPacketAndInspection)
{
    pond::ui::paint::PaintRecorder recorder;
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(0.5F, 1.25F, 30.75F, 20.5F)).HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(-2.0F, 0.0F, 10.25F, 12.5F), MakeColor(0.5F, 0.25F, 1.0F, 0.5F))
            .HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(12.0F, 2.0F, 22.0F, 15.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    const SealedPaintList paint = Seal(recorder);
    const pond::ui::UiTargetMetrics metrics = MakeMetrics(40.0F, 30.0F, 50U, 45U);
    PaintCompiler compiler;

    auto firstResult = compiler.Compile(paint, metrics);
    ASSERT_TRUE(firstResult.HasValue()) << firstResult.GetError().GetMessage();
    draw2d::Draw2DPacket firstPacket = std::move(firstResult).GetValue();
    const auto firstInspection = compiler.GetInspection();

    compiler.Reset();
    auto secondResult = compiler.Compile(paint, metrics);
    ASSERT_TRUE(secondResult.HasValue()) << secondResult.GetError().GetMessage();
    draw2d::Draw2DPacket secondPacket = std::move(secondResult).GetValue();
    const auto secondInspection = compiler.GetInspection();

    ExpectPacketsSemanticallyEqual(firstPacket, secondPacket);
    EXPECT_EQ(firstInspection, secondInspection);
}

TEST(UiPaintCompilerTests, RecycledPacketReusesEveryWarmedOutputStream)
{
    const SealedPaintList paint = MakeSingleRectanglePaint();
    const pond::ui::UiTargetMetrics metrics = MakeMetrics(100.0F, 80.0F, 200U, 160U);
    PaintCompiler compiler;

    auto firstResult = compiler.Compile(paint, metrics);
    ASSERT_TRUE(firstResult.HasValue()) << firstResult.GetError().GetMessage();
    draw2d::Draw2DPacket firstPacket = std::move(firstResult).GetValue();
    const draw2d::Draw2DVertex* const vertices = firstPacket.GetVertices().data();
    const draw2d::Draw2DIndex* const indices = firstPacket.GetIndices().data();
    const draw2d::Draw2DDrawRecord* const draws = firstPacket.GetDrawRecords().data();
    ASSERT_NE(vertices, nullptr);
    ASSERT_NE(indices, nullptr);
    ASSERT_NE(draws, nullptr);

    compiler.Reset(std::move(firstPacket));
    auto secondResult = compiler.Compile(paint, metrics);
    ASSERT_TRUE(secondResult.HasValue()) << secondResult.GetError().GetMessage();
    const draw2d::Draw2DPacket& secondPacket = secondResult.GetValue();
    EXPECT_EQ(secondPacket.GetVertices().data(), vertices);
    EXPECT_EQ(secondPacket.GetIndices().data(), indices);
    EXPECT_EQ(secondPacket.GetDrawRecords().data(), draws);
    EXPECT_TRUE(draw2d::InspectDraw2DPacket(secondPacket).IsValid());
}
