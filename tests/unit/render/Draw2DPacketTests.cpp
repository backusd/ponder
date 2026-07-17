#include <ponder/render/draw2d/Draw2DPacket.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
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
using pond::render::RenderErrorCode;
using pond::render::draw2d::Draw2DDrawRecord;
using pond::render::draw2d::Draw2DIndex;
using pond::render::draw2d::Draw2DPackedLinearPremultipliedRgba8;
using pond::render::draw2d::Draw2DPacket;
using pond::render::draw2d::Draw2DPacketBuilder;
using pond::render::draw2d::Draw2DPacketBuilderState;
using pond::render::draw2d::Draw2DPacketCounts;
using pond::render::draw2d::Draw2DPacketLimits;
using pond::render::draw2d::Draw2DPacketStats;
using pond::render::draw2d::Draw2DPacketTestFactory;
using pond::render::draw2d::Draw2DPacketValidation;
using pond::render::draw2d::Draw2DPacketValidationIssue;
using pond::render::draw2d::Draw2DPixelExtent;
using pond::render::draw2d::Draw2DScissor;
using pond::render::draw2d::Draw2DVertex;

constexpr Draw2DPixelExtent kExtent{.width = 64U, .height = 48U};
constexpr Draw2DScissor kFullScissor{
    .left = 0U, .top = 0U, .right = kExtent.width, .bottom = kExtent.height};
constexpr Draw2DPackedLinearPremultipliedRgba8 kWhite =
    Draw2DPackedLinearPremultipliedRgba8::FromChannels(255U, 255U, 255U, 255U);
constexpr Draw2DPackedLinearPremultipliedRgba8 kRed =
    Draw2DPackedLinearPremultipliedRgba8::FromChannels(128U, 0U, 0U, 128U);

static_assert(std::same_as<Draw2DIndex, std::uint32_t>);
static_assert(std::is_standard_layout_v<Draw2DVertex>);
static_assert(std::is_trivially_copyable_v<Draw2DVertex>);
static_assert(sizeof(Draw2DVertex) == 12U);
static_assert(alignof(Draw2DVertex) == alignof(float));
static_assert(offsetof(Draw2DVertex, x) == 0U);
static_assert(offsetof(Draw2DVertex, y) == 4U);
static_assert(offsetof(Draw2DVertex, color) == 8U);
static_assert(!std::default_initializable<Draw2DPacket>);
static_assert(!std::copy_constructible<Draw2DPacket>);
static_assert(std::move_constructible<Draw2DPacket>);
static_assert(!std::copy_constructible<Draw2DPacketBuilder>);
static_assert(std::move_constructible<Draw2DPacketBuilder>);
static_assert(std::is_nothrow_move_constructible_v<Draw2DPacket>);
static_assert(std::is_nothrow_move_constructible_v<Draw2DPacketBuilder>);
static_assert(std::same_as<decltype(std::declval<const Draw2DPacket&>().GetVertices()),
                           std::span<const Draw2DVertex>>);
static_assert(std::same_as<decltype(std::declval<const Draw2DPacket&>().GetIndices()),
                           std::span<const Draw2DIndex>>);
static_assert(std::same_as<decltype(std::declval<const Draw2DPacket&>().GetDrawRecords()),
                           std::span<const Draw2DDrawRecord>>);
static_assert(pond::render::draw2d::ComputeDraw2DSchemaFingerprint("hello") ==
              0xa430d84680aabd0bULL);
static_assert(pond::render::draw2d::kDraw2DSchemaFingerprint == 0x05c436a9fda7b4f7ULL);

[[nodiscard]] std::vector<Draw2DVertex> MakeQuadVertices(
    Draw2DPackedLinearPremultipliedRgba8 color = kWhite, float offsetX = 0.0F, float offsetY = 0.0F)
{
    return {{.x = offsetX, .y = offsetY, .color = color},
            {.x = offsetX + 16.0F, .y = offsetY, .color = color},
            {.x = offsetX + 16.0F, .y = offsetY + 12.0F, .color = color},
            {.x = offsetX, .y = offsetY + 12.0F, .color = color}};
}

[[nodiscard]] std::vector<Draw2DIndex> MakeQuadIndices()
{
    return {0U, 1U, 2U, 0U, 2U, 3U};
}

[[nodiscard]] Draw2DDrawRecord MakeQuadDraw(Draw2DScissor scissor = kFullScissor)
{
    return Draw2DDrawRecord{
        .firstIndex = 0U, .indexCount = 6U, .baseVertex = 0, .scissor = scissor};
}

[[nodiscard]] Draw2DPacket MakeValidQuadPacket()
{
    return Draw2DPacketTestFactory::Create(kExtent, MakeQuadVertices(), MakeQuadIndices(),
                                           {MakeQuadDraw()});
}

void ExpectRenderErrorCode(const pond::core::Error& error, RenderErrorCode code)
{
    EXPECT_EQ(error.GetCode(), pond::render::ToErrorCode(code));
}

void ExpectIssue(const Draw2DPacket& packet, Draw2DPacketValidationIssue issue,
                 std::uint64_t elementIndex = pond::render::draw2d::kNoDraw2DElementIndex)
{
    const Draw2DPacketValidation validation = pond::render::draw2d::InspectDraw2DPacket(packet);
    EXPECT_EQ(validation.issue, issue);
    EXPECT_EQ(validation.elementIndex, elementIndex);
}

TEST(RenderDraw2DPacketTests, FreezesVertexMemoryAndColorByteOrder)
{
    constexpr Draw2DPackedLinearPremultipliedRgba8 color =
        Draw2DPackedLinearPremultipliedRgba8::FromChannels(0x12U, 0x34U, 0x56U, 0x78U);
    constexpr std::array<std::uint8_t, 4U> bytes =
        std::bit_cast<std::array<std::uint8_t, 4U>>(color);

    EXPECT_EQ(bytes, (std::array<std::uint8_t, 4U>{0x12U, 0x34U, 0x56U, 0x78U}));
    EXPECT_EQ(color.GetRed(), 0x12U);
    EXPECT_EQ(color.GetGreen(), 0x34U);
    EXPECT_EQ(color.GetBlue(), 0x56U);
    EXPECT_EQ(color.GetAlpha(), 0x78U);
}

TEST(RenderDraw2DPacketTests, AcceptsCanonicalEmptyPacketWithExactExtent)
{
    Draw2DPacketBuilder builder;
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());

    auto sealed = builder.Seal();
    ASSERT_TRUE(sealed.HasValue());
    Draw2DPacket packet = std::move(sealed).GetValue();

    EXPECT_TRUE(packet.IsEmpty());
    EXPECT_EQ(packet.GetPixelExtent(), kExtent);
    EXPECT_EQ(packet.GetSchemaFingerprint(), pond::render::draw2d::kDraw2DSchemaFingerprint);
    EXPECT_EQ(packet.GetStats(), Draw2DPacketStats{});
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(packet).IsValid());
    const auto validated = pond::render::draw2d::ValidateDraw2DPacket(packet);
    ASSERT_TRUE(validated.HasValue());
    EXPECT_EQ(*validated, Draw2DPacketStats{});
}

TEST(RenderDraw2DPacketTests, PreservesOrderedDrawsAndPermitsSharedIndexRanges)
{
    std::vector<Draw2DVertex> vertices = MakeQuadVertices(kWhite);
    std::vector<Draw2DVertex> secondVertices = MakeQuadVertices(kRed, 20.0F, 10.0F);
    vertices.insert(vertices.end(), secondVertices.begin(), secondVertices.end());

    const std::vector<Draw2DIndex> indices = MakeQuadIndices();
    const std::vector<Draw2DDrawRecord> draws{
        Draw2DDrawRecord{.firstIndex = 0U,
                         .indexCount = 6U,
                         .baseVertex = 0,
                         .scissor =
                             Draw2DScissor{.left = 0U, .top = 0U, .right = 32U, .bottom = 24U}},
        Draw2DDrawRecord{.firstIndex = 0U,
                         .indexCount = 3U,
                         .baseVertex = 4,
                         .scissor =
                             Draw2DScissor{.left = 8U, .top = 4U, .right = 48U, .bottom = 32U}},
        Draw2DDrawRecord{
            .firstIndex = 3U, .indexCount = 3U, .baseVertex = 4, .scissor = kFullScissor}};
    Draw2DPacket packet =
        Draw2DPacketTestFactory::Create(kExtent, std::move(vertices), indices, draws);

    const Draw2DPacketValidation validation = pond::render::draw2d::InspectDraw2DPacket(packet);
    ASSERT_TRUE(validation.IsValid());
    ASSERT_EQ(packet.GetDrawRecords().size(), draws.size());
    EXPECT_EQ(packet.GetDrawRecords()[0], draws[0]);
    EXPECT_EQ(packet.GetDrawRecords()[1], draws[1]);
    EXPECT_EQ(packet.GetDrawRecords()[2], draws[2]);
    EXPECT_EQ(validation.stats.counts, (Draw2DPacketCounts{8U, 6U, 3U}));
}

TEST(RenderDraw2DPacketTests, AcceptsFiniteGeometryOutsideThePacketExtent)
{
    std::vector<Draw2DVertex> vertices = MakeQuadVertices(kWhite, -100.5F, 200.25F);
    Draw2DPacket packet = Draw2DPacketTestFactory::Create(kExtent, std::move(vertices),
                                                          MakeQuadIndices(), {MakeQuadDraw()});

    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(packet).IsValid());
}

TEST(RenderDraw2DPacketTests, ComputesExactCountsAndByteStatistics)
{
    Draw2DPacket packet = MakeValidQuadPacket();
    const Draw2DPacketValidation validation = pond::render::draw2d::InspectDraw2DPacket(packet);
    ASSERT_TRUE(validation.IsValid());

    const Draw2DPacketStats expected{
        .counts = Draw2DPacketCounts{.vertexCount = 4U, .indexCount = 6U, .drawRecordCount = 1U},
        .vertexBytes = 4U * sizeof(Draw2DVertex),
        .indexBytes = 6U * sizeof(Draw2DIndex),
        .drawRecordBytes = sizeof(Draw2DDrawRecord),
        .packetBytes =
            (4U * sizeof(Draw2DVertex)) + (6U * sizeof(Draw2DIndex)) + sizeof(Draw2DDrawRecord),
        .uploadBytes = (4U * sizeof(Draw2DVertex)) + (6U * sizeof(Draw2DIndex))};
    EXPECT_EQ(validation.stats, expected);
}

TEST(RenderDraw2DPacketTests, RejectsInvalidLimitsBeforeCountArithmetic)
{
    Draw2DPacketLimits limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxVertexCount = 0U;

    const Draw2DPacketValidation validation = pond::render::draw2d::InspectDraw2DPacketCounts(
        Draw2DPacketCounts{.vertexCount = std::numeric_limits<std::uint64_t>::max(),
                           .indexCount = 0U,
                           .drawRecordCount = 0U},
        limits);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::InvalidLimits);
}

TEST(RenderDraw2DPacketTests, ReportsEveryCountAndByteArithmeticOverflow)
{
    constexpr std::uint64_t kMaximum = std::numeric_limits<std::uint64_t>::max();

    const Draw2DPacketValidation vertexOverflow = pond::render::draw2d::InspectDraw2DPacketCounts(
        Draw2DPacketCounts{.vertexCount = (kMaximum / sizeof(Draw2DVertex)) + 1U,
                           .indexCount = 0U,
                           .drawRecordCount = 0U});
    EXPECT_EQ(vertexOverflow.issue, Draw2DPacketValidationIssue::VertexByteOverflow);

    const Draw2DPacketValidation indexOverflow = pond::render::draw2d::InspectDraw2DPacketCounts(
        Draw2DPacketCounts{.vertexCount = 0U,
                           .indexCount = (kMaximum / sizeof(Draw2DIndex)) + 1U,
                           .drawRecordCount = 0U});
    EXPECT_EQ(indexOverflow.issue, Draw2DPacketValidationIssue::IndexByteOverflow);

    const Draw2DPacketValidation drawOverflow = pond::render::draw2d::InspectDraw2DPacketCounts(
        Draw2DPacketCounts{.vertexCount = 0U,
                           .indexCount = 0U,
                           .drawRecordCount = (kMaximum / sizeof(Draw2DDrawRecord)) + 1U});
    EXPECT_EQ(drawOverflow.issue, Draw2DPacketValidationIssue::DrawRecordByteOverflow);

    const std::uint64_t nearlyMaximumVertexCount = kMaximum / sizeof(Draw2DVertex);
    const Draw2DPacketValidation uploadOverflow =
        pond::render::draw2d::InspectDraw2DPacketCounts(Draw2DPacketCounts{
            .vertexCount = nearlyMaximumVertexCount, .indexCount = 1U, .drawRecordCount = 0U});
    EXPECT_EQ(uploadOverflow.issue, Draw2DPacketValidationIssue::UploadByteOverflow);

    const std::uint64_t nearlyMaximumDrawCount = kMaximum / sizeof(Draw2DDrawRecord);
    const std::uint64_t remainingPacketBytes =
        kMaximum - (nearlyMaximumDrawCount * sizeof(Draw2DDrawRecord));
    const std::uint64_t indicesToOverflowPacket = (remainingPacketBytes / sizeof(Draw2DIndex)) + 1U;
    const Draw2DPacketValidation packetOverflow = pond::render::draw2d::InspectDraw2DPacketCounts(
        Draw2DPacketCounts{.vertexCount = 0U,
                           .indexCount = indicesToOverflowPacket,
                           .drawRecordCount = nearlyMaximumDrawCount});
    EXPECT_EQ(packetOverflow.issue, Draw2DPacketValidationIssue::PacketByteOverflow);
}

TEST(RenderDraw2DPacketTests, RejectsCountsThatCannotBeAddressedByPacketFields)
{
    constexpr std::uint64_t kFirstUnrepresentable =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;

    const Draw2DPacketValidation vertices =
        pond::render::draw2d::InspectDraw2DPacketCounts(Draw2DPacketCounts{
            .vertexCount = kFirstUnrepresentable, .indexCount = 0U, .drawRecordCount = 0U});
    EXPECT_EQ(vertices.issue, Draw2DPacketValidationIssue::VertexCountUnrepresentable);
    EXPECT_EQ(vertices.requested, kFirstUnrepresentable);
    EXPECT_EQ(vertices.allowed, std::numeric_limits<std::uint32_t>::max());

    const Draw2DPacketValidation indices =
        pond::render::draw2d::InspectDraw2DPacketCounts(Draw2DPacketCounts{
            .vertexCount = 0U, .indexCount = kFirstUnrepresentable, .drawRecordCount = 0U});
    EXPECT_EQ(indices.issue, Draw2DPacketValidationIssue::IndexCountUnrepresentable);
    EXPECT_EQ(indices.requested, kFirstUnrepresentable);
    EXPECT_EQ(indices.allowed, std::numeric_limits<std::uint32_t>::max());
}

TEST(RenderDraw2DPacketTests, AcceptsExactConfiguredCountAndByteLimits)
{
    constexpr Draw2DPacketCounts kCounts{
        .vertexCount = 4U, .indexCount = 6U, .drawRecordCount = 2U};
    const Draw2DPacketValidation computed =
        pond::render::draw2d::InspectDraw2DPacketCounts(kCounts);
    ASSERT_TRUE(computed.IsValid());

    const Draw2DPacketLimits exactLimits{.maxVertexCount = kCounts.vertexCount,
                                         .maxIndexCount = kCounts.indexCount,
                                         .maxDrawRecordCount = kCounts.drawRecordCount,
                                         .maxPacketBytes = computed.stats.packetBytes,
                                         .maxUploadBytes = computed.stats.uploadBytes};
    const Draw2DPacketValidation exact =
        pond::render::draw2d::InspectDraw2DPacketCounts(kCounts, exactLimits);
    EXPECT_TRUE(exact.IsValid());
    EXPECT_EQ(exact.stats, computed.stats);
}

TEST(RenderDraw2DPacketTests, ReportsEachConfiguredLimitWithRequestedAndAllowedValues)
{
    constexpr Draw2DPacketCounts kCounts{
        .vertexCount = 4U, .indexCount = 6U, .drawRecordCount = 2U};
    const Draw2DPacketValidation computed =
        pond::render::draw2d::InspectDraw2DPacketCounts(kCounts);
    ASSERT_TRUE(computed.IsValid());

    Draw2DPacketLimits limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxVertexCount = kCounts.vertexCount - 1U;
    Draw2DPacketValidation validation =
        pond::render::draw2d::InspectDraw2DPacketCounts(kCounts, limits);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::VertexCountLimit);
    EXPECT_EQ(validation.requested, kCounts.vertexCount);
    EXPECT_EQ(validation.allowed, limits.maxVertexCount);

    limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxIndexCount = kCounts.indexCount - 1U;
    validation = pond::render::draw2d::InspectDraw2DPacketCounts(kCounts, limits);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::IndexCountLimit);
    EXPECT_EQ(validation.requested, kCounts.indexCount);
    EXPECT_EQ(validation.allowed, limits.maxIndexCount);

    limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxDrawRecordCount = kCounts.drawRecordCount - 1U;
    validation = pond::render::draw2d::InspectDraw2DPacketCounts(kCounts, limits);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::DrawRecordCountLimit);
    EXPECT_EQ(validation.requested, kCounts.drawRecordCount);
    EXPECT_EQ(validation.allowed, limits.maxDrawRecordCount);

    limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxPacketBytes = computed.stats.packetBytes - 1U;
    validation = pond::render::draw2d::InspectDraw2DPacketCounts(kCounts, limits);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::PacketByteLimit);
    EXPECT_EQ(validation.requested, computed.stats.packetBytes);
    EXPECT_EQ(validation.allowed, limits.maxPacketBytes);

    limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxUploadBytes = computed.stats.uploadBytes - 1U;
    validation = pond::render::draw2d::InspectDraw2DPacketCounts(kCounts, limits);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::UploadByteLimit);
    EXPECT_EQ(validation.requested, computed.stats.uploadBytes);
    EXPECT_EQ(validation.allowed, limits.maxUploadBytes);
}

TEST(RenderDraw2DPacketTests, RejectsZeroExtentAndSchemaFingerprintMismatch)
{
    Draw2DPacket zeroWidth =
        Draw2DPacketTestFactory::Create({.width = 0U, .height = kExtent.height});
    ExpectIssue(zeroWidth, Draw2DPacketValidationIssue::InvalidExtent);

    Draw2DPacket zeroHeight =
        Draw2DPacketTestFactory::Create({.width = kExtent.width, .height = 0U});
    ExpectIssue(zeroHeight, Draw2DPacketValidationIssue::InvalidExtent);

    constexpr std::uint64_t kWrongFingerprint =
        pond::render::draw2d::kDraw2DSchemaFingerprint ^ 0x1U;
    Draw2DPacket wrongFingerprint =
        Draw2DPacketTestFactory::Create(kExtent, {}, {}, {}, kWrongFingerprint);
    const Draw2DPacketValidation validation =
        pond::render::draw2d::InspectDraw2DPacket(wrongFingerprint);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::SchemaFingerprintMismatch);
    EXPECT_EQ(validation.requested, kWrongFingerprint);
    EXPECT_EQ(validation.allowed, pond::render::draw2d::kDraw2DSchemaFingerprint);
}

TEST(RenderDraw2DPacketTests, RejectsEveryPartiallyEmptyPacketShape)
{
    Draw2DPacket verticesOnly = Draw2DPacketTestFactory::Create(kExtent, MakeQuadVertices());
    ExpectIssue(verticesOnly, Draw2DPacketValidationIssue::NonCanonicalEmptyPacket);

    Draw2DPacket indicesOnly = Draw2DPacketTestFactory::Create(kExtent, {}, MakeQuadIndices());
    ExpectIssue(indicesOnly, Draw2DPacketValidationIssue::NonCanonicalEmptyPacket);

    Draw2DPacket drawsOnly = Draw2DPacketTestFactory::Create(kExtent, {}, {}, {MakeQuadDraw()});
    ExpectIssue(drawsOnly, Draw2DPacketValidationIssue::NonCanonicalEmptyPacket);

    Draw2DPacket missingDraws =
        Draw2DPacketTestFactory::Create(kExtent, MakeQuadVertices(), MakeQuadIndices());
    ExpectIssue(missingDraws, Draw2DPacketValidationIssue::NonCanonicalEmptyPacket);
}

TEST(RenderDraw2DPacketTests, RejectsEveryNonFiniteVertexComponentAtItsIndex)
{
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
    constexpr std::array<std::pair<float, float>, 4U> kInvalidPositions{{
        {kInfinity, 0.0F},
        {-kInfinity, 0.0F},
        {0.0F, kInfinity},
        {0.0F, kNan},
    }};

    for (std::size_t caseIndex = 0U; caseIndex < kInvalidPositions.size(); ++caseIndex)
    {
        SCOPED_TRACE(caseIndex);
        std::vector<Draw2DVertex> vertices = MakeQuadVertices();
        vertices[2].x = kInvalidPositions[caseIndex].first;
        vertices[2].y = kInvalidPositions[caseIndex].second;
        Draw2DPacket packet = Draw2DPacketTestFactory::Create(kExtent, std::move(vertices),
                                                              MakeQuadIndices(), {MakeQuadDraw()});
        ExpectIssue(packet, Draw2DPacketValidationIssue::NonFiniteVertex, 2U);
    }
}

TEST(RenderDraw2DPacketTests, RejectsColorThatIsNotLinearPremultiplied)
{
    std::vector<Draw2DVertex> vertices = MakeQuadVertices();
    vertices[1].color = Draw2DPackedLinearPremultipliedRgba8::FromChannels(129U, 0U, 0U, 128U);
    Draw2DPacket packet = Draw2DPacketTestFactory::Create(kExtent, std::move(vertices),
                                                          MakeQuadIndices(), {MakeQuadDraw()});

    ExpectIssue(packet, Draw2DPacketValidationIssue::InvalidPackedColor, 1U);
}

TEST(RenderDraw2DPacketTests, RejectsOutOfRangeIndexBeforeInspectingDraws)
{
    std::vector<Draw2DIndex> indices = MakeQuadIndices();
    indices[2] = 4U;
    Draw2DPacket packet = Draw2DPacketTestFactory::Create(kExtent, MakeQuadVertices(),
                                                          std::move(indices), {MakeQuadDraw()});

    const Draw2DPacketValidation validation = pond::render::draw2d::InspectDraw2DPacket(packet);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::IndexOutOfRange);
    EXPECT_EQ(validation.elementIndex, 2U);
    EXPECT_EQ(validation.requested, 4U);
    EXPECT_EQ(validation.allowed, 4U);
}

TEST(RenderDraw2DPacketTests, RejectsEmptyNonTriangleAndTruncatedDrawRanges)
{
    Draw2DPacket emptyRange = Draw2DPacketTestFactory::Create(
        kExtent, MakeQuadVertices(), MakeQuadIndices(),
        {Draw2DDrawRecord{
            .firstIndex = 0U, .indexCount = 0U, .baseVertex = 0, .scissor = kFullScissor}});
    ExpectIssue(emptyRange, Draw2DPacketValidationIssue::EmptyDrawRange, 0U);

    Draw2DPacket nonTriangle = Draw2DPacketTestFactory::Create(
        kExtent, MakeQuadVertices(), MakeQuadIndices(),
        {Draw2DDrawRecord{
            .firstIndex = 0U, .indexCount = 4U, .baseVertex = 0, .scissor = kFullScissor}});
    ExpectIssue(nonTriangle, Draw2DPacketValidationIssue::NonTriangleDrawRange, 0U);

    Draw2DPacket truncated = Draw2DPacketTestFactory::Create(
        kExtent, MakeQuadVertices(), MakeQuadIndices(),
        {Draw2DDrawRecord{
            .firstIndex = 4U, .indexCount = 3U, .baseVertex = 0, .scissor = kFullScissor}});
    const Draw2DPacketValidation validation = pond::render::draw2d::InspectDraw2DPacket(truncated);
    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::DrawRangeOutOfRange);
    EXPECT_EQ(validation.elementIndex, 0U);
    EXPECT_EQ(validation.requested, 7U);
    EXPECT_EQ(validation.allowed, 6U);
}

TEST(RenderDraw2DPacketTests, ChecksDrawRangeArithmeticInPromotedWidth)
{
    Draw2DPacket packet = Draw2DPacketTestFactory::Create(
        kExtent, MakeQuadVertices(), MakeQuadIndices(),
        {Draw2DDrawRecord{.firstIndex = std::numeric_limits<std::uint32_t>::max(),
                          .indexCount = std::numeric_limits<std::uint32_t>::max(),
                          .baseVertex = 0,
                          .scissor = kFullScissor}});
    const Draw2DPacketValidation validation = pond::render::draw2d::InspectDraw2DPacket(packet);

    EXPECT_EQ(validation.issue, Draw2DPacketValidationIssue::DrawRangeOutOfRange);
    EXPECT_EQ(validation.requested,
              2U * static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));

    Draw2DPacket promotedRange = Draw2DPacketTestFactory::Create(
        kExtent, MakeQuadVertices(), MakeQuadIndices(),
        {Draw2DDrawRecord{.firstIndex = std::numeric_limits<std::uint32_t>::max() - 1U,
                          .indexCount = 3U,
                          .baseVertex = 0,
                          .scissor = kFullScissor}});
    const Draw2DPacketValidation promotedValidation =
        pond::render::draw2d::InspectDraw2DPacket(promotedRange);
    EXPECT_EQ(promotedValidation.issue, Draw2DPacketValidationIssue::DrawRangeOutOfRange);
    EXPECT_EQ(promotedValidation.requested,
              static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 2U);
}

TEST(RenderDraw2DPacketTests, RejectsPositiveAndNegativeBaseVertexEscapes)
{
    Draw2DPacket positive = Draw2DPacketTestFactory::Create(
        kExtent, MakeQuadVertices(), MakeQuadIndices(),
        {Draw2DDrawRecord{
            .firstIndex = 0U, .indexCount = 6U, .baseVertex = 1, .scissor = kFullScissor}});
    ExpectIssue(positive, Draw2DPacketValidationIssue::BaseVertexOutOfRange, 0U);

    Draw2DPacket negative = Draw2DPacketTestFactory::Create(
        kExtent, MakeQuadVertices(), MakeQuadIndices(),
        {Draw2DDrawRecord{
            .firstIndex = 0U, .indexCount = 6U, .baseVertex = -1, .scissor = kFullScissor}});
    ExpectIssue(negative, Draw2DPacketValidationIssue::BaseVertexOutOfRange, 0U);
}

TEST(RenderDraw2DPacketTests, BoundsBaseVertexValidationWorkAndUsesSafeShortcuts)
{
    Draw2DPacketLimits shortcutLimits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    shortcutLimits.maxBaseVertexValidationIndexCount = 1U;

    Draw2DPacket zeroBase = MakeValidQuadPacket();
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(zeroBase, shortcutLimits).IsValid());

    std::vector<Draw2DVertex> shortcutVertices = MakeQuadVertices();
    std::vector<Draw2DVertex> secondVertices = MakeQuadVertices(kRed, 20.0F, 10.0F);
    shortcutVertices.insert(shortcutVertices.end(), secondVertices.begin(), secondVertices.end());
    Draw2DPacket globallySafe = Draw2DPacketTestFactory::Create(
        kExtent, std::move(shortcutVertices), MakeQuadIndices(),
        {Draw2DDrawRecord{
            .firstIndex = 0U, .indexCount = 6U, .baseVertex = 4, .scissor = kFullScissor}});
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(globallySafe, shortcutLimits).IsValid());

    std::vector<Draw2DVertex> scannedVertices = MakeQuadVertices();
    secondVertices = MakeQuadVertices(kRed, 20.0F, 10.0F);
    scannedVertices.insert(scannedVertices.end(), secondVertices.begin(), secondVertices.end());
    const std::vector<Draw2DIndex> scannedIndices{0U, 1U, 2U, 7U, 7U, 7U};
    const std::vector<Draw2DDrawRecord> scannedDraws{
        Draw2DDrawRecord{
            .firstIndex = 0U, .indexCount = 3U, .baseVertex = 1, .scissor = kFullScissor},
        Draw2DDrawRecord{
            .firstIndex = 0U, .indexCount = 3U, .baseVertex = 1, .scissor = kFullScissor}};
    Draw2DPacket requiresScans = Draw2DPacketTestFactory::Create(
        kExtent, std::move(scannedVertices), scannedIndices, scannedDraws);

    Draw2DPacketLimits exactLimits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    exactLimits.maxBaseVertexValidationIndexCount = 6U;
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(requiresScans, exactLimits).IsValid());

    Draw2DPacketLimits boundedLimits = exactLimits;
    boundedLimits.maxBaseVertexValidationIndexCount = 5U;
    const Draw2DPacketValidation bounded =
        pond::render::draw2d::InspectDraw2DPacket(requiresScans, boundedLimits);
    EXPECT_EQ(bounded.issue, Draw2DPacketValidationIssue::BaseVertexValidationIndexCountLimit);
    EXPECT_EQ(bounded.elementIndex, 1U);
    EXPECT_EQ(bounded.requested, 6U);
    EXPECT_EQ(bounded.allowed, 5U);
}

TEST(RenderDraw2DPacketTests, RejectsEveryMalformedOrUnclampedScissor)
{
    constexpr std::array<Draw2DScissor, 6U> kInvalidScissors{{
        Draw2DScissor{.left = 4U, .top = 0U, .right = 4U, .bottom = 10U},
        Draw2DScissor{.left = 5U, .top = 0U, .right = 4U, .bottom = 10U},
        Draw2DScissor{.left = 0U, .top = 7U, .right = 10U, .bottom = 7U},
        Draw2DScissor{.left = 0U, .top = 8U, .right = 10U, .bottom = 7U},
        Draw2DScissor{.left = 0U, .top = 0U, .right = kExtent.width + 1U, .bottom = kExtent.height},
        Draw2DScissor{.left = 0U, .top = 0U, .right = kExtent.width, .bottom = kExtent.height + 1U},
    }};

    for (std::size_t caseIndex = 0U; caseIndex < kInvalidScissors.size(); ++caseIndex)
    {
        SCOPED_TRACE(caseIndex);
        Draw2DPacket packet =
            Draw2DPacketTestFactory::Create(kExtent, MakeQuadVertices(), MakeQuadIndices(),
                                            {MakeQuadDraw(kInvalidScissors[caseIndex])});
        ExpectIssue(packet, Draw2DPacketValidationIssue::InvalidScissor, 0U);
    }
}

TEST(RenderDraw2DPacketTests, ConvertsValidationFailureToRenderInvalidArgument)
{
    Draw2DPacket malformed =
        Draw2DPacketTestFactory::Create({.width = 0U, .height = kExtent.height});
    const auto result = pond::render::draw2d::ValidateDraw2DPacket(malformed);
    ASSERT_FALSE(result.HasValue());
    ExpectRenderErrorCode(result.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_NE(std::string{result.GetError().GetMessage()}.find("invalid_extent"),
              std::string::npos);
}

TEST(RenderDraw2DPacketTests, BuilderRequiresPositiveExtentBeforeAppendingData)
{
    Draw2DPacketBuilder builder;
    const auto initial = builder.GetSnapshot();

    const auto zeroWidth = builder.SetPixelExtent({.width = 0U, .height = kExtent.height});
    ASSERT_FALSE(zeroWidth.HasValue());
    ExpectRenderErrorCode(zeroWidth.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_EQ(builder.GetSnapshot(), initial);

    const auto zeroHeight = builder.SetPixelExtent({.width = kExtent.width, .height = 0U});
    ASSERT_FALSE(zeroHeight.HasValue());
    ExpectRenderErrorCode(zeroHeight.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_EQ(builder.GetSnapshot(), initial);

    const std::vector<Draw2DVertex> vertices = MakeQuadVertices();
    const std::vector<Draw2DIndex> indices = MakeQuadIndices();
    const Draw2DDrawRecord draw = MakeQuadDraw();
    const auto verticesWithoutExtent = builder.AppendVertices(vertices);
    const auto indicesWithoutExtent = builder.AppendIndices(indices);
    const auto drawWithoutExtent = builder.AppendDrawRecord(draw);

    for (const pond::core::VoidResult* result :
         std::array{&verticesWithoutExtent, &indicesWithoutExtent, &drawWithoutExtent})
    {
        ASSERT_FALSE(result->HasValue());
        ExpectRenderErrorCode(result->GetError(), RenderErrorCode::InvalidState);
    }
    EXPECT_EQ(builder.GetSnapshot(), initial);

    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    EXPECT_TRUE(builder.AppendVertices(vertices).HasValue());
}

TEST(RenderDraw2DPacketTests, BuilderPreflightsExactReservationAndRejectsGrowthPastLimits)
{
    constexpr Draw2DPacketCounts kCounts{
        .vertexCount = 4U, .indexCount = 6U, .drawRecordCount = 1U};
    const Draw2DPacketValidation computed =
        pond::render::draw2d::InspectDraw2DPacketCounts(kCounts);
    ASSERT_TRUE(computed.IsValid());
    const Draw2DPacketLimits limits{.maxVertexCount = kCounts.vertexCount,
                                    .maxIndexCount = kCounts.indexCount,
                                    .maxDrawRecordCount = kCounts.drawRecordCount,
                                    .maxPacketBytes = computed.stats.packetBytes,
                                    .maxUploadBytes = computed.stats.uploadBytes};
    Draw2DPacketBuilder builder{limits};
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    ASSERT_TRUE(builder.Reserve(kCounts).HasValue());

    const auto reserved = builder.GetSnapshot();
    EXPECT_GE(reserved.capacities.vertexCount, kCounts.vertexCount);
    EXPECT_GE(reserved.capacities.indexCount, kCounts.indexCount);
    EXPECT_GE(reserved.capacities.drawRecordCount, kCounts.drawRecordCount);

    const auto tooManyVertices = builder.Reserve(
        Draw2DPacketCounts{.vertexCount = 5U, .indexCount = 6U, .drawRecordCount = 1U});
    ASSERT_FALSE(tooManyVertices.HasValue());
    ExpectRenderErrorCode(tooManyVertices.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_EQ(builder.GetSnapshot(), reserved);

    ASSERT_TRUE(builder.AppendVertices(MakeQuadVertices()).HasValue());
    ASSERT_TRUE(builder.AppendIndices(MakeQuadIndices()).HasValue());
    const Draw2DDrawRecord draw = MakeQuadDraw();
    ASSERT_TRUE(builder.AppendDrawRecord(draw).HasValue());
    const auto full = builder.GetSnapshot();

    const Draw2DVertex extraVertex{.x = 1.0F, .y = 1.0F, .color = kWhite};
    const auto appendPastLimit =
        builder.AppendVertices(std::span<const Draw2DVertex>{&extraVertex, 1U});
    ASSERT_FALSE(appendPastLimit.HasValue());
    ExpectRenderErrorCode(appendPastLimit.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_EQ(builder.GetSnapshot(), full);

    const auto reserveBelowCurrent = builder.Reserve(
        Draw2DPacketCounts{.vertexCount = 3U, .indexCount = 6U, .drawRecordCount = 1U});
    ASSERT_FALSE(reserveBelowCurrent.HasValue());
    ExpectRenderErrorCode(reserveBelowCurrent.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_EQ(builder.GetSnapshot(), full);
}

TEST(RenderDraw2DPacketTests, BuilderRejectsArithmeticOverflowWithoutGrowingStorage)
{
    Draw2DPacketBuilder builder;
    const auto before = builder.GetSnapshot();
    const auto result =
        builder.Reserve(Draw2DPacketCounts{.vertexCount = std::numeric_limits<std::uint64_t>::max(),
                                           .indexCount = 0U,
                                           .drawRecordCount = 0U});

    ASSERT_FALSE(result.HasValue());
    ExpectRenderErrorCode(result.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_EQ(builder.GetSnapshot(), before);
}

TEST(RenderDraw2DPacketTests, BuilderRejectsInvalidConfiguredLimits)
{
    Draw2DPacketLimits limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxUploadBytes = 0U;
    Draw2DPacketBuilder builder{limits};

    const auto extent = builder.SetPixelExtent(kExtent);
    ASSERT_FALSE(extent.HasValue());
    ExpectRenderErrorCode(extent.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_TRUE(builder.IsOpen());
    EXPECT_TRUE(builder.IsEmpty());

    limits = pond::render::draw2d::kDefaultDraw2DPacketLimits;
    limits.maxBaseVertexValidationIndexCount = 0U;
    Draw2DPacketBuilder invalidValidationBudgetBuilder{limits};
    const auto invalidValidationBudget = invalidValidationBudgetBuilder.SetPixelExtent(kExtent);
    ASSERT_FALSE(invalidValidationBudget.HasValue());
    ExpectRenderErrorCode(invalidValidationBudget.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_TRUE(invalidValidationBudgetBuilder.IsOpen());
    EXPECT_TRUE(invalidValidationBudgetBuilder.IsEmpty());
}

TEST(RenderDraw2DPacketTests, BuilderSealFailureIsTransactionalAndRecoverable)
{
    Draw2DPacketBuilder builder;
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    ASSERT_TRUE(builder.AppendVertices(MakeQuadVertices()).HasValue());
    const auto beforeSeal = builder.GetSnapshot();

    auto failedSeal = builder.Seal();
    ASSERT_FALSE(failedSeal.HasValue());
    ExpectRenderErrorCode(failedSeal.GetError(), RenderErrorCode::InvalidArgument);
    EXPECT_TRUE(builder.IsOpen());
    EXPECT_EQ(builder.GetSnapshot(), beforeSeal);

    builder.Reset();
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    ASSERT_TRUE(builder.AppendVertices(MakeQuadVertices()).HasValue());
    ASSERT_TRUE(builder.AppendIndices(MakeQuadIndices()).HasValue());
    const Draw2DDrawRecord draw = MakeQuadDraw();
    ASSERT_TRUE(builder.AppendDrawRecord(draw).HasValue());
    auto recovered = builder.Seal();
    ASSERT_TRUE(recovered.HasValue());
    EXPECT_TRUE(builder.IsSealed());
}

TEST(RenderDraw2DPacketTests, BuilderRejectsExtentChangesAfterAppendingData)
{
    Draw2DPacketBuilder builder;
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    ASSERT_TRUE(builder.AppendVertices(MakeQuadVertices()).HasValue());
    const auto before = builder.GetSnapshot();

    const auto result = builder.SetPixelExtent({.width = 128U, .height = 96U});
    ASSERT_FALSE(result.HasValue());
    ExpectRenderErrorCode(result.GetError(), RenderErrorCode::InvalidState);
    EXPECT_EQ(builder.GetSnapshot(), before);
}

TEST(RenderDraw2DPacketTests, BuilderRejectsEveryMutationAfterSealUntilReset)
{
    Draw2DPacketBuilder builder;
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    auto sealed = builder.Seal();
    ASSERT_TRUE(sealed.HasValue());
    EXPECT_EQ(builder.GetState(), Draw2DPacketBuilderState::Sealed);

    const Draw2DVertex vertex{.x = 0.0F, .y = 0.0F, .color = kWhite};
    const Draw2DIndex index{0U};
    const Draw2DDrawRecord draw = MakeQuadDraw();
    const auto extent = builder.SetPixelExtent(kExtent);
    const auto reserve =
        builder.Reserve({.vertexCount = 4U, .indexCount = 6U, .drawRecordCount = 1U});
    const auto vertices = builder.AppendVertices(std::span<const Draw2DVertex>{&vertex, 1U});
    const auto indices = builder.AppendIndices(std::span<const Draw2DIndex>{&index, 1U});
    const auto draws = builder.AppendDrawRecord(draw);
    auto reseal = builder.Seal();

    for (const pond::core::VoidResult* result :
         std::array{&extent, &reserve, &vertices, &indices, &draws})
    {
        ASSERT_FALSE(result->HasValue());
        ExpectRenderErrorCode(result->GetError(), RenderErrorCode::InvalidState);
    }
    ASSERT_FALSE(reseal.HasValue());
    ExpectRenderErrorCode(reseal.GetError(), RenderErrorCode::InvalidState);

    builder.Reset();
    EXPECT_TRUE(builder.IsOpen());
    EXPECT_TRUE(builder.IsEmpty());
    EXPECT_EQ(builder.GetSnapshot().extent, Draw2DPixelExtent{});
    EXPECT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
}

TEST(RenderDraw2DPacketTests, ResetPreservesCapacityAndSealedPacketOwnsItsStorage)
{
    Draw2DPacketBuilder builder;
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    ASSERT_TRUE(builder.Reserve({4U, 6U, 1U}).HasValue());
    ASSERT_TRUE(builder.AppendVertices(MakeQuadVertices(kWhite)).HasValue());
    ASSERT_TRUE(builder.AppendIndices(MakeQuadIndices()).HasValue());
    const Draw2DDrawRecord firstDraw = MakeQuadDraw();
    ASSERT_TRUE(builder.AppendDrawRecord(firstDraw).HasValue());
    const auto beforeSeal = builder.GetSnapshot();

    auto sealedResult = builder.Seal();
    ASSERT_TRUE(sealedResult.HasValue());
    Draw2DPacket sealed = std::move(sealedResult).GetValue();
    builder.Reset();
    const auto afterReset = builder.GetSnapshot();
    EXPECT_EQ(afterReset.state, Draw2DPacketBuilderState::Open);
    EXPECT_EQ(afterReset.extent, Draw2DPixelExtent{});
    EXPECT_EQ(afterReset.counts, Draw2DPacketCounts{});
    EXPECT_GE(afterReset.capacities.vertexCount, beforeSeal.capacities.vertexCount);
    EXPECT_GE(afterReset.capacities.indexCount, beforeSeal.capacities.indexCount);
    EXPECT_GE(afterReset.capacities.drawRecordCount, beforeSeal.capacities.drawRecordCount);

    ASSERT_TRUE(builder.SetPixelExtent({32U, 24U}).HasValue());
    ASSERT_TRUE(builder.AppendVertices(MakeQuadVertices(kRed, 4.0F, 5.0F)).HasValue());
    ASSERT_TRUE(builder.AppendIndices(MakeQuadIndices()).HasValue());
    const Draw2DDrawRecord secondDraw{
        .firstIndex = 0U,
        .indexCount = 6U,
        .baseVertex = 0,
        .scissor = Draw2DScissor{.left = 0U, .top = 0U, .right = 32U, .bottom = 24U}};
    ASSERT_TRUE(builder.AppendDrawRecord(secondDraw).HasValue());

    ASSERT_EQ(sealed.GetVertices().size(), 4U);
    EXPECT_EQ(sealed.GetVertices()[0].color, kWhite);
    EXPECT_EQ(sealed.GetPixelExtent(), kExtent);
    EXPECT_EQ(sealed.GetDrawRecords()[0], firstDraw);
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(sealed).IsValid());
}

TEST(RenderDraw2DPacketTests, MovesOpenBuilderAndSealedPacketWithoutBorrowingSourceStorage)
{
    Draw2DPacketBuilder builder;
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    ASSERT_TRUE(builder.AppendVertices(MakeQuadVertices()).HasValue());
    ASSERT_TRUE(builder.AppendIndices(MakeQuadIndices()).HasValue());
    const Draw2DDrawRecord draw = MakeQuadDraw();
    ASSERT_TRUE(builder.AppendDrawRecord(draw).HasValue());

    Draw2DPacketBuilder movedBuilder = std::move(builder);
    EXPECT_TRUE(builder.IsOpen());
    EXPECT_TRUE(builder.IsEmpty());
    EXPECT_EQ(builder.GetSnapshot().extent, Draw2DPixelExtent{});
    ASSERT_TRUE(builder.SetPixelExtent(kExtent).HasValue());
    auto movedFromBuilderPacket = builder.Seal();
    ASSERT_TRUE(movedFromBuilderPacket.HasValue());
    EXPECT_TRUE(movedFromBuilderPacket->IsEmpty());
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(*movedFromBuilderPacket).IsValid());

    auto sealedResult = movedBuilder.Seal();
    ASSERT_TRUE(sealedResult.HasValue());
    Draw2DPacket packet = std::move(sealedResult).GetValue();
    Draw2DPacket movedPacket = std::move(packet);

    EXPECT_TRUE(packet.IsEmpty());
    EXPECT_EQ(packet.GetPixelExtent(), kExtent);
    EXPECT_EQ(packet.GetStats(), Draw2DPacketStats{});
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(packet).IsValid());
    EXPECT_EQ(movedPacket.GetPixelExtent(), kExtent);
    EXPECT_EQ(movedPacket.GetVertices().size(), 4U);
    ASSERT_EQ(movedPacket.GetIndices().size(), 6U);
    EXPECT_EQ(movedPacket.GetIndices()[0], 0U);
    EXPECT_EQ(movedPacket.GetIndices()[5], 3U);
    EXPECT_EQ(movedPacket.GetDrawRecords()[0], draw);
    EXPECT_TRUE(pond::render::draw2d::InspectDraw2DPacket(movedPacket).IsValid());
}
} // namespace
