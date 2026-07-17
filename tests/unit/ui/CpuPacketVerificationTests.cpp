#include <ponder/render/draw2d/Draw2DPacket.hpp>
#include <ponder/ui/Color.hpp>
#include <ponder/ui/Geometry.hpp>
#include <ponder/ui/Metrics.hpp>
#include <ponder/ui/paint/PaintCompiler.hpp>
#include <ponder/ui/paint/PaintRecorder.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pond::render::draw2d
{
struct Draw2DPacketTestFactory final
{
    [[nodiscard]] static Draw2DPacket Create(
        Draw2DPixelExtent extent, std::vector<Draw2DVertex> vertices = {},
        std::vector<Draw2DIndex> indices = {}, std::vector<Draw2DDrawRecord> drawRecords = {},
        std::uint64_t schemaFingerprint = kDraw2DSchemaFingerprint)
    {
        Draw2DPacket packet;
        packet.m_extent = extent;
        packet.m_schemaFingerprint = schemaFingerprint;
        packet.m_vertices = std::move(vertices);
        packet.m_indices = std::move(indices);
        packet.m_drawRecords = std::move(drawRecords);
        packet.m_stats =
            InspectDraw2DPacketCounts(
                Draw2DPacketCounts{
                    .vertexCount = static_cast<std::uint64_t>(packet.m_vertices.size()),
                    .indexCount = static_cast<std::uint64_t>(packet.m_indices.size()),
                    .drawRecordCount = static_cast<std::uint64_t>(packet.m_drawRecords.size())})
                .stats;
        return packet;
    }
};
} // namespace pond::render::draw2d

namespace
{
namespace draw2d = pond::render::draw2d;

using pond::ui::paint::FillRectangleCommand;
using pond::ui::paint::PaintCommandKind;
using pond::ui::paint::PaintListStats;
using pond::ui::paint::PaintRecorder;
using pond::ui::paint::PaintRecorderState;
using pond::ui::paint::PushClipRectangleCommand;
using pond::ui::paint::SealedPaintList;

struct DeterministicRng final
{
    std::uint32_t state;

    [[nodiscard]] std::uint32_t Next() noexcept
    {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        return state;
    }

    [[nodiscard]] std::uint32_t NextBounded(std::uint32_t exclusiveMaximum) noexcept
    {
        return Next() % exclusiveMaximum;
    }

    [[nodiscard]] float NextFloat(float minimum, float maximum) noexcept
    {
        const float unit = static_cast<float>(NextBounded(10'001U)) / 10'000.0F;
        return minimum + ((maximum - minimum) * unit);
    }
};

[[nodiscard]] constexpr std::uint32_t MakeCaseSeed(std::uint32_t baseSeed,
                                                   std::uint32_t caseIndex) noexcept
{
    std::uint32_t value = baseSeed + 0x9E37'79B9U + caseIndex;
    value ^= value >> 16U;
    value *= 0x7FEB'352DU;
    value ^= value >> 15U;
    value *= 0x846C'A68BU;
    value ^= value >> 16U;
    return value == 0U ? 0xA341'316CU : value;
}

struct RecorderModel final
{
    PaintRecorderState state{PaintRecorderState::Open};
    std::uint64_t commandCount{};
    std::uint64_t fillRectangleCount{};
    std::uint64_t pushClipRectangleCount{};
    std::uint64_t popClipCount{};
    std::uint64_t payloadBytes{};
    std::uint64_t clipDepth{1U};
    std::uint64_t maxClipDepthObserved{1U};
};

struct ReferenceRect final
{
    double left{};
    double top{};
    double right{};
    double bottom{};
};

[[nodiscard]] pond::ui::LogicalRect MakeRect(float left, float top, float right, float bottom)
{
    auto rect = pond::ui::MakeLogicalRectFromEdges(left, top, right, bottom);
    EXPECT_TRUE(rect.HasValue()) << (rect.HasValue() ? "" : rect.GetError().GetMessage());
    return rect.HasValue() ? *rect : pond::ui::LogicalRect{};
}

[[nodiscard]] pond::ui::SrgbStraightAlphaColor MakeColor(float red, float green, float blue,
                                                         float alpha)
{
    auto color = pond::ui::MakeSrgbStraightAlphaColor(red, green, blue, alpha);
    EXPECT_TRUE(color.HasValue()) << (color.HasValue() ? "" : color.GetError().GetMessage());
    return color.HasValue() ? *color : pond::ui::SrgbStraightAlphaColor{};
}

[[nodiscard]] pond::ui::UiTargetMetrics MakeMetrics(float logicalWidth, float logicalHeight,
                                                    std::uint32_t pixelWidth,
                                                    std::uint32_t pixelHeight)
{
    auto metrics = pond::ui::MakeUiTargetMetrics(
        pond::ui::UiTargetId{101U}, pond::ui::UiTargetRevision{103U},
        pond::ui::UiMetricsRevision{107U},
        pond::ui::LogicalSize{.width = logicalWidth, .height = logicalHeight},
        pond::ui::FramebufferPixelSize{.width = pixelWidth, .height = pixelHeight});
    EXPECT_TRUE(metrics.HasValue()) << (metrics.HasValue() ? "" : metrics.GetError().GetMessage());
    return metrics.HasValue() ? *metrics : pond::ui::UiTargetMetrics{};
}

[[nodiscard]] pond::ui::LogicalRect MakeFuzzRect(DeterministicRng& rng)
{
    const float left = rng.NextFloat(-6.0F, 14.0F);
    const float top = rng.NextFloat(-4.0F, 12.0F);
    const float width = rng.NextBounded(5U) == 0U ? 0.0F : rng.NextFloat(0.125F, 10.0F);
    const float height = rng.NextBounded(5U) == 0U ? 0.0F : rng.NextFloat(0.125F, 10.0F);
    return MakeRect(left, top, left + width, top + height);
}

[[nodiscard]] pond::ui::SrgbStraightAlphaColor MakeFuzzColor(DeterministicRng& rng)
{
    const auto channel = [&]() noexcept
    {
        return static_cast<float>(rng.NextBounded(257U)) / 256.0F;
    };
    const float red = channel();
    const float green = channel();
    const float blue = channel();
    const float alpha = channel();
    return MakeColor(red, green, blue, alpha);
}

[[nodiscard]] PaintListStats MakeExpectedStats(const RecorderModel& model) noexcept
{
    return PaintListStats{.state = model.state,
                          .commandCount = model.commandCount,
                          .fillRectangleCount = model.fillRectangleCount,
                          .pushClipRectangleCount = model.pushClipRectangleCount,
                          .popClipCount = model.popClipCount,
                          .payloadBytes = model.payloadBytes,
                          .clipDepth = model.clipDepth,
                          .maxClipDepthObserved = model.maxClipDepthObserved};
}

void ExpectRecorderMatchesModel(const PaintRecorder& recorder, const RecorderModel& model)
{
    EXPECT_EQ(recorder.GetSnapshot().stats, MakeExpectedStats(model));
}

[[nodiscard]] bool CanAppendCommand(const RecorderModel& model, pond::ui::UiHardLimits limits,
                                    std::uint64_t payloadBytesToAdd) noexcept
{
    return model.state == PaintRecorderState::Open &&
           model.commandCount + 1U <= limits.maxPaintCommandCount &&
           model.payloadBytes + payloadBytesToAdd <= limits.maxPaintCommandPayloadBytes;
}

[[nodiscard]] ReferenceRect ToReferenceRect(pond::ui::LogicalRect rectangle, double scaleX,
                                            double scaleY) noexcept
{
    return ReferenceRect{.left = static_cast<double>(pond::ui::GetLeft(rectangle)) * scaleX,
                         .top = static_cast<double>(pond::ui::GetTop(rectangle)) * scaleY,
                         .right = static_cast<double>(pond::ui::GetRight(rectangle)) * scaleX,
                         .bottom = static_cast<double>(pond::ui::GetBottom(rectangle)) * scaleY};
}

[[nodiscard]] bool IsEmpty(ReferenceRect rectangle) noexcept
{
    return rectangle.left >= rectangle.right || rectangle.top >= rectangle.bottom;
}

[[nodiscard]] ReferenceRect Intersect(ReferenceRect lhs, ReferenceRect rhs) noexcept
{
    const ReferenceRect result{.left = std::max(lhs.left, rhs.left),
                               .top = std::max(lhs.top, rhs.top),
                               .right = std::min(lhs.right, rhs.right),
                               .bottom = std::min(lhs.bottom, rhs.bottom)};
    return IsEmpty(result) ? ReferenceRect{} : result;
}

[[nodiscard]] draw2d::Draw2DScissor MakeReferenceScissor(ReferenceRect clip,
                                                         draw2d::Draw2DPixelExtent extent) noexcept
{
    const double maxX = static_cast<double>(extent.width);
    const double maxY = static_cast<double>(extent.height);
    return draw2d::Draw2DScissor{
        .left = static_cast<std::uint32_t>(std::clamp(std::floor(clip.left), 0.0, maxX)),
        .top = static_cast<std::uint32_t>(std::clamp(std::floor(clip.top), 0.0, maxY)),
        .right = static_cast<std::uint32_t>(std::clamp(std::ceil(clip.right), 0.0, maxX)),
        .bottom = static_cast<std::uint32_t>(std::clamp(std::ceil(clip.bottom), 0.0, maxY))};
}

[[nodiscard]] SealedPaintList Seal(PaintRecorder& recorder)
{
    auto sealed = recorder.Seal();
    EXPECT_TRUE(sealed.HasValue()) << (sealed.HasValue() ? "" : sealed.GetError().GetMessage());
    return sealed.HasValue() ? std::move(sealed).GetValue() : SealedPaintList{};
}

[[nodiscard]] std::vector<draw2d::Draw2DVertex> MakePacketVertices(
    draw2d::Draw2DPackedLinearPremultipliedRgba8 color =
        draw2d::Draw2DPackedLinearPremultipliedRgba8::FromChannels(255U, 255U, 255U, 255U))
{
    return {{.x = 0.0F, .y = 0.0F, .color = color},
            {.x = 8.0F, .y = 0.0F, .color = color},
            {.x = 8.0F, .y = 8.0F, .color = color},
            {.x = 0.0F, .y = 8.0F, .color = color}};
}

[[nodiscard]] std::vector<draw2d::Draw2DIndex> MakePacketIndices()
{
    return {0U, 1U, 2U, 0U, 2U, 3U};
}

[[nodiscard]] draw2d::Draw2DDrawRecord MakePacketDraw(
    draw2d::Draw2DScissor scissor = draw2d::Draw2DScissor{
        .left = 0U, .top = 0U, .right = 64U, .bottom = 48U}) noexcept
{
    return draw2d::Draw2DDrawRecord{
        .firstIndex = 0U, .indexCount = 6U, .baseVertex = 0, .scissor = scissor};
}

void ExpectPacketsEqual(const draw2d::Draw2DPacket& lhs, const draw2d::Draw2DPacket& rhs)
{
    EXPECT_EQ(lhs.GetPixelExtent(), rhs.GetPixelExtent());
    EXPECT_EQ(lhs.GetSchemaFingerprint(), rhs.GetSchemaFingerprint());
    EXPECT_EQ(lhs.GetStats(), rhs.GetStats());
    EXPECT_TRUE(std::ranges::equal(lhs.GetVertices(), rhs.GetVertices()));
    EXPECT_TRUE(std::ranges::equal(lhs.GetIndices(), rhs.GetIndices()));
    EXPECT_TRUE(std::ranges::equal(lhs.GetDrawRecords(), rhs.GetDrawRecords()));
}
} // namespace

TEST(UiCpuPacketVerificationTests, BoundedRecorderStateSequencesRemainTransactional)
{
    constexpr std::uint32_t kSeed{0x5EED'1601U};
    pond::ui::UiHardLimits limits = pond::ui::kDefaultUiHardLimits;
    limits.maxPaintCommandCount = 12U;
    limits.maxPaintCommandPayloadBytes =
        (3U * sizeof(FillRectangleCommand)) + (3U * sizeof(PushClipRectangleCommand));
    limits.maxClipDepth = 4U;

    for (std::uint32_t sequenceIndex = 0U; sequenceIndex < 48U; ++sequenceIndex)
    {
        const std::uint32_t caseSeed = MakeCaseSeed(kSeed, sequenceIndex);
        DeterministicRng rng{caseSeed};
        SCOPED_TRACE("seed=0x5EED1601 sequence=" + std::to_string(sequenceIndex) +
                     " case_seed=" + std::to_string(caseSeed));
        PaintRecorder recorder{limits};
        RecorderModel model;

        for (std::uint32_t step = 0U; step < 40U; ++step)
        {
            const std::uint32_t stepSeed = rng.state;
            const auto before = recorder.GetSnapshot();
            const std::uint32_t operation = rng.NextBounded(6U);
            SCOPED_TRACE("step=" + std::to_string(step) + " step_seed=" + std::to_string(stepSeed) +
                         " operation=" + std::to_string(operation));

            if (operation == 0U)
            {
                const pond::ui::LogicalRect rectangle = MakeFuzzRect(rng);
                const pond::ui::SrgbStraightAlphaColor color = MakeFuzzColor(rng);
                SCOPED_TRACE(::testing::Message()
                             << "rectangle=" << rectangle.origin.x << ',' << rectangle.origin.y
                             << ',' << rectangle.size.width << ',' << rectangle.size.height
                             << " color=" << color.red << ',' << color.green << ',' << color.blue
                             << ',' << color.alpha);
                const auto result = recorder.FillRectangle(rectangle, color);
                const bool expected = CanAppendCommand(model, limits, sizeof(FillRectangleCommand));
                EXPECT_EQ(result.HasValue(), expected);
                if (expected)
                {
                    ++model.commandCount;
                    ++model.fillRectangleCount;
                    model.payloadBytes += sizeof(FillRectangleCommand);
                }
                else
                {
                    EXPECT_EQ(recorder.GetSnapshot(), before);
                }
            }
            else if (operation == 1U)
            {
                const auto result = recorder.PushClipRectangle(MakeFuzzRect(rng));
                const bool expected =
                    CanAppendCommand(model, limits, sizeof(PushClipRectangleCommand)) &&
                    model.clipDepth + 1U <= limits.maxClipDepth;
                EXPECT_EQ(result.HasValue(), expected);
                if (expected)
                {
                    ++model.commandCount;
                    ++model.pushClipRectangleCount;
                    model.payloadBytes += sizeof(PushClipRectangleCommand);
                    ++model.clipDepth;
                    model.maxClipDepthObserved =
                        std::max(model.maxClipDepthObserved, model.clipDepth);
                }
                else
                {
                    EXPECT_EQ(recorder.GetSnapshot(), before);
                }
            }
            else if (operation == 2U)
            {
                const auto result = recorder.PopClip();
                const bool expected = model.state == PaintRecorderState::Open &&
                                      model.clipDepth > 1U &&
                                      model.commandCount + 1U <= limits.maxPaintCommandCount;
                EXPECT_EQ(result.HasValue(), expected);
                if (expected)
                {
                    ++model.commandCount;
                    ++model.popClipCount;
                    --model.clipDepth;
                }
                else
                {
                    EXPECT_EQ(recorder.GetSnapshot(), before);
                }
            }
            else if (operation == 3U)
            {
                auto sealed = recorder.Seal();
                const bool expected =
                    model.state == PaintRecorderState::Open && model.clipDepth == 1U;
                EXPECT_EQ(sealed.HasValue(), expected);
                if (expected)
                {
                    model.state = PaintRecorderState::Sealed;
                    ASSERT_TRUE(sealed.HasValue());
                    EXPECT_EQ(sealed->GetStats(), MakeExpectedStats(model));
                }
                else
                {
                    EXPECT_EQ(recorder.GetSnapshot(), before);
                }
            }
            else if (operation == 4U)
            {
                recorder.Reset();
                model = RecorderModel{};
            }
            else
            {
                const auto result = recorder.FillRectangle(
                    pond::ui::LogicalRect{
                        .origin = pond::ui::LogicalPoint{.x = 0.0F, .y = 0.0F},
                        .size = pond::ui::LogicalSize{.width = -1.0F, .height = 1.0F}},
                    MakeFuzzColor(rng));
                EXPECT_FALSE(result.HasValue());
                EXPECT_EQ(recorder.GetSnapshot(), before);
            }

            ExpectRecorderMatchesModel(recorder, model);
        }
    }
}

TEST(UiCpuPacketVerificationTests, FractionalClipAndScalePropertyMatchesIndependentReference)
{
    constexpr std::uint32_t kSeed{0xC11A'1602U};

    for (std::uint32_t caseIndex = 0U; caseIndex < 96U; ++caseIndex)
    {
        const std::uint32_t caseSeed = MakeCaseSeed(kSeed, caseIndex);
        DeterministicRng rng{caseSeed};
        SCOPED_TRACE("seed=0xC11A1602 case=" + std::to_string(caseIndex) +
                     " case_seed=" + std::to_string(caseSeed));
        const std::uint32_t pixelWidth = 23U + rng.NextBounded(101U);
        const std::uint32_t pixelHeight = 17U + rng.NextBounded(89U);
        const float logicalWidth = 10.0F + (static_cast<float>(rng.NextBounded(33U)) * 0.25F);
        const float logicalHeight = 8.0F + (static_cast<float>(rng.NextBounded(29U)) * 0.25F);
        const auto metrics = MakeMetrics(logicalWidth, logicalHeight, pixelWidth, pixelHeight);
        const double scaleX = static_cast<double>(pixelWidth) / logicalWidth;
        const double scaleY = static_cast<double>(pixelHeight) / logicalHeight;

        const float clipLeft = rng.NextFloat(0.0F, logicalWidth * 0.35F);
        const float clipTop = rng.NextFloat(0.0F, logicalHeight * 0.35F);
        const float clipRight = rng.NextFloat(logicalWidth * 0.65F, logicalWidth);
        const float clipBottom = rng.NextFloat(logicalHeight * 0.65F, logicalHeight);
        const auto clip = MakeRect(clipLeft, clipTop, clipRight, clipBottom);

        const float rectLeft = rng.NextFloat(-1.0F, logicalWidth * 0.45F);
        const float rectTop = rng.NextFloat(-1.0F, logicalHeight * 0.45F);
        const float rectRight = rng.NextFloat(logicalWidth * 0.55F, logicalWidth + 1.0F);
        const float rectBottom = rng.NextFloat(logicalHeight * 0.55F, logicalHeight + 1.0F);
        const auto rect = MakeRect(rectLeft, rectTop, rectRight, rectBottom);
        SCOPED_TRACE(::testing::Message()
                     << "metrics=" << logicalWidth << 'x' << logicalHeight << " -> " << pixelWidth
                     << 'x' << pixelHeight << " clip=" << clipLeft << ',' << clipTop << ','
                     << clipRight << ',' << clipBottom << " rect=" << rectLeft << ',' << rectTop
                     << ',' << rectRight << ',' << rectBottom);

        const draw2d::Draw2DPixelExtent extent{.width = pixelWidth, .height = pixelHeight};
        const ReferenceRect root{.left = 0.0,
                                 .top = 0.0,
                                 .right = static_cast<double>(pixelWidth),
                                 .bottom = static_cast<double>(pixelHeight)};
        const ReferenceRect effectiveClip = Intersect(root, ToReferenceRect(clip, scaleX, scaleY));
        const ReferenceRect expectedGeometry =
            Intersect(ToReferenceRect(rect, scaleX, scaleY), effectiveClip);
        ASSERT_FALSE(IsEmpty(expectedGeometry));

        PaintRecorder recorder;
        ASSERT_TRUE(recorder.PushClipRectangle(clip).HasValue());
        ASSERT_TRUE(recorder.FillRectangle(rect, MakeColor(1.0F, 0.5F, 0.0F, 1.0F)).HasValue());
        ASSERT_TRUE(recorder.PopClip().HasValue());

        pond::ui::paint::PaintCompiler compiler;
        auto packetResult = compiler.Compile(Seal(recorder), metrics);
        ASSERT_TRUE(packetResult.HasValue()) << packetResult.GetError().GetMessage();
        const draw2d::Draw2DPacket& packet = *packetResult;
        ASSERT_EQ(packet.GetVertices().size(), 4U);
        ASSERT_EQ(packet.GetDrawRecords().size(), 1U);

        EXPECT_FLOAT_EQ(packet.GetVertices()[0].x, static_cast<float>(expectedGeometry.left));
        EXPECT_FLOAT_EQ(packet.GetVertices()[0].y, static_cast<float>(expectedGeometry.top));
        EXPECT_FLOAT_EQ(packet.GetVertices()[2].x, static_cast<float>(expectedGeometry.right));
        EXPECT_FLOAT_EQ(packet.GetVertices()[2].y, static_cast<float>(expectedGeometry.bottom));
        EXPECT_EQ(packet.GetDrawRecords()[0].scissor, MakeReferenceScissor(effectiveClip, extent));
        EXPECT_TRUE(draw2d::InspectDraw2DPacket(packet).IsValid());
    }
}

TEST(UiCpuPacketVerificationTests, CompilerOutputInvariantsAreDeterministicAcrossBoundedInputs)
{
    constexpr std::uint32_t kSeed{0xC0DE'1603U};
    const auto metrics = MakeMetrics(16.0F, 12.0F, 80U, 72U);

    for (std::uint32_t caseIndex = 0U; caseIndex < 80U; ++caseIndex)
    {
        const std::uint32_t caseSeed = MakeCaseSeed(kSeed, caseIndex);
        DeterministicRng rng{caseSeed};
        SCOPED_TRACE("seed=0xC0DE1603 case=" + std::to_string(caseIndex) +
                     " case_seed=" + std::to_string(caseSeed));
        PaintRecorder recorder;
        std::uint32_t openClips = 0U;
        for (std::uint32_t step = 0U; step < 18U; ++step)
        {
            const std::uint32_t stepSeed = rng.state;
            const std::uint32_t operation = rng.NextBounded(5U);
            SCOPED_TRACE("step=" + std::to_string(step) + " step_seed=" + std::to_string(stepSeed) +
                         " operation=" + std::to_string(operation));
            if (operation == 0U && openClips < 4U)
            {
                ASSERT_TRUE(recorder.PushClipRectangle(MakeFuzzRect(rng)).HasValue());
                ++openClips;
            }
            else if (operation == 1U && openClips > 0U)
            {
                ASSERT_TRUE(recorder.PopClip().HasValue());
                --openClips;
            }
            else
            {
                const pond::ui::LogicalRect rectangle = MakeFuzzRect(rng);
                const pond::ui::SrgbStraightAlphaColor color = MakeFuzzColor(rng);
                SCOPED_TRACE(::testing::Message()
                             << "rectangle=" << rectangle.origin.x << ',' << rectangle.origin.y
                             << ',' << rectangle.size.width << ',' << rectangle.size.height
                             << " color=" << color.red << ',' << color.green << ',' << color.blue
                             << ',' << color.alpha);
                ASSERT_TRUE(recorder.FillRectangle(rectangle, color).HasValue());
            }
        }
        while (openClips > 0U)
        {
            ASSERT_TRUE(recorder.PopClip().HasValue());
            --openClips;
        }
        const SealedPaintList paint = Seal(recorder);

        pond::ui::paint::PaintCompiler firstCompiler;
        auto first = firstCompiler.Compile(paint, metrics);
        ASSERT_TRUE(first.HasValue()) << first.GetError().GetMessage();
        const auto firstInspection = firstCompiler.GetInspection();

        pond::ui::paint::PaintCompiler secondCompiler;
        auto second = secondCompiler.Compile(paint, metrics);
        ASSERT_TRUE(second.HasValue()) << second.GetError().GetMessage();
        EXPECT_EQ(firstInspection, secondCompiler.GetInspection());
        ExpectPacketsEqual(*first, *second);

        const draw2d::Draw2DPacket& packet = *first;
        EXPECT_TRUE(draw2d::InspectDraw2DPacket(packet).IsValid());
        EXPECT_EQ(packet.GetStats().counts.vertexCount, packet.GetVertices().size());
        EXPECT_EQ(packet.GetStats().counts.indexCount, packet.GetIndices().size());
        EXPECT_EQ(packet.GetStats().counts.drawRecordCount, packet.GetDrawRecords().size());

        for (std::size_t drawIndex = 0U; drawIndex < packet.GetDrawRecords().size(); ++drawIndex)
        {
            const draw2d::Draw2DDrawRecord& draw = packet.GetDrawRecords()[drawIndex];
            SCOPED_TRACE("draw=" + std::to_string(drawIndex));
            EXPECT_EQ(draw.indexCount % 3U, 0U);
            EXPECT_LE(static_cast<std::uint64_t>(draw.firstIndex) + draw.indexCount,
                      packet.GetIndices().size());
            EXPECT_TRUE(draw2d::IsValid(draw.scissor, packet.GetPixelExtent()));
        }
    }
}

TEST(UiCpuPacketVerificationTests, HostilePacketMutationsProjectStableDiagnostics)
{
    constexpr std::uint32_t kSeed{0xBAD0'1604U};
    constexpr draw2d::Draw2DPixelExtent kExtent{.width = 64U, .height = 48U};
    const auto white =
        draw2d::Draw2DPackedLinearPremultipliedRgba8::FromChannels(255U, 255U, 255U, 255U);

    for (std::uint32_t caseIndex = 0U; caseIndex < 120U; ++caseIndex)
    {
        const std::uint32_t caseSeed = MakeCaseSeed(kSeed, caseIndex);
        DeterministicRng rng{caseSeed};
        SCOPED_TRACE("seed=0xBAD01604 case=" + std::to_string(caseIndex) +
                     " case_seed=" + std::to_string(caseSeed));
        std::vector<draw2d::Draw2DVertex> vertices = MakePacketVertices(white);
        std::vector<draw2d::Draw2DIndex> indices = MakePacketIndices();
        std::vector<draw2d::Draw2DDrawRecord> draws{MakePacketDraw()};
        draw2d::Draw2DPixelExtent extent = kExtent;
        std::uint64_t fingerprint = draw2d::kDraw2DSchemaFingerprint;
        draw2d::Draw2DPacketValidationIssue expectedIssue{
            draw2d::Draw2DPacketValidationIssue::None};
        std::uint64_t expectedElement = draw2d::kNoDraw2DElementIndex;

        const std::uint32_t mutation = rng.NextBounded(8U);
        SCOPED_TRACE("mutation=" + std::to_string(mutation));
        switch (mutation)
        {
        case 0U:
            extent.width = 0U;
            expectedIssue = draw2d::Draw2DPacketValidationIssue::InvalidExtent;
            break;
        case 1U:
            fingerprint ^= 0x10U;
            expectedIssue = draw2d::Draw2DPacketValidationIssue::SchemaFingerprintMismatch;
            break;
        case 2U:
            vertices[1].x = std::numeric_limits<float>::infinity();
            expectedIssue = draw2d::Draw2DPacketValidationIssue::NonFiniteVertex;
            expectedElement = 1U;
            break;
        case 3U:
            vertices[2].color =
                draw2d::Draw2DPackedLinearPremultipliedRgba8::FromChannels(2U, 0U, 0U, 1U);
            expectedIssue = draw2d::Draw2DPacketValidationIssue::InvalidPackedColor;
            expectedElement = 2U;
            break;
        case 4U:
            indices[3] = 4U;
            expectedIssue = draw2d::Draw2DPacketValidationIssue::IndexOutOfRange;
            expectedElement = 3U;
            break;
        case 5U:
            draws[0].indexCount = 5U;
            expectedIssue = draw2d::Draw2DPacketValidationIssue::NonTriangleDrawRange;
            expectedElement = 0U;
            break;
        case 6U:
            draws[0].firstIndex = 4U;
            expectedIssue = draw2d::Draw2DPacketValidationIssue::DrawRangeOutOfRange;
            expectedElement = 0U;
            break;
        default:
            draws[0].scissor.right = kExtent.width + 1U;
            expectedIssue = draw2d::Draw2DPacketValidationIssue::InvalidScissor;
            expectedElement = 0U;
            break;
        }

        const draw2d::Draw2DPacket packet = draw2d::Draw2DPacketTestFactory::Create(
            extent, std::move(vertices), std::move(indices), std::move(draws), fingerprint);
        const draw2d::Draw2DPacketValidation validation = draw2d::InspectDraw2DPacket(packet);
        EXPECT_EQ(validation.issue, expectedIssue);
        EXPECT_EQ(validation.elementIndex, expectedElement);
    }
}

TEST(UiCpuPacketVerificationTests, PacketValidationIssueNamesAreStableDiagnosticProjections)
{
    struct NameCase final
    {
        draw2d::Draw2DPacketValidationIssue issue;
        std::string_view name;
    };

    constexpr std::array kCases{
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::None, .name = "none"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::InvalidLimits,
                 .name = "invalid_limits"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::InvalidExtent,
                 .name = "invalid_extent"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::SchemaFingerprintMismatch,
                 .name = "schema_fingerprint_mismatch"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::VertexByteOverflow,
                 .name = "vertex_byte_overflow"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::IndexByteOverflow,
                 .name = "index_byte_overflow"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::DrawRecordByteOverflow,
                 .name = "draw_record_byte_overflow"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::UploadByteOverflow,
                 .name = "upload_byte_overflow"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::PacketByteOverflow,
                 .name = "packet_byte_overflow"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::VertexCountUnrepresentable,
                 .name = "vertex_count_unrepresentable"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::IndexCountUnrepresentable,
                 .name = "index_count_unrepresentable"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::VertexCountLimit,
                 .name = "vertex_count_limit"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::IndexCountLimit,
                 .name = "index_count_limit"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::DrawRecordCountLimit,
                 .name = "draw_record_count_limit"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::PacketByteLimit,
                 .name = "packet_byte_limit"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::UploadByteLimit,
                 .name = "upload_byte_limit"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::NonCanonicalEmptyPacket,
                 .name = "non_canonical_empty_packet"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::NonFiniteVertex,
                 .name = "non_finite_vertex"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::InvalidPackedColor,
                 .name = "invalid_packed_color"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::IndexOutOfRange,
                 .name = "index_out_of_range"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::EmptyDrawRange,
                 .name = "empty_draw_range"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::NonTriangleDrawRange,
                 .name = "non_triangle_draw_range"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::DrawRangeOutOfRange,
                 .name = "draw_range_out_of_range"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::BaseVertexOutOfRange,
                 .name = "base_vertex_out_of_range"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::BaseVertexValidationIndexCountLimit,
                 .name = "base_vertex_validation_index_count_limit"},
        NameCase{.issue = draw2d::Draw2DPacketValidationIssue::InvalidScissor,
                 .name = "invalid_scissor"}};

    for (const NameCase& testCase : kCases)
    {
        EXPECT_EQ(draw2d::GetName(testCase.issue), testCase.name);
    }
}
