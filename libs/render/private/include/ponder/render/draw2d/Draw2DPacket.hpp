#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/render/RenderError.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pond::render::draw2d
{
using Draw2DIndex = std::uint32_t;
inline constexpr std::uint64_t kNoDraw2DElementIndex{std::numeric_limits<std::uint64_t>::max()};

inline constexpr std::string_view kDraw2DSchemaDescription{
    "ponder.draw2d.packet.v1|extent:u32x2-positive|"
    "vertex:position-f32x2-top-left-pixel-edge@0,rgba8-unorm-linear-premul@8,stride12|"
    "index:u32|draw:first-index-u32,index-count-u32,base-vertex-i32,"
    "scissor-u32-ltrb-half-open|topology:triangle-list"};

[[nodiscard]] consteval std::uint64_t ComputeDraw2DSchemaFingerprint(std::string_view text)
{
    std::uint64_t hash{0xcbf29ce484222325ULL};
    for (const char character : text)
    {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

inline constexpr std::uint64_t kDraw2DSchemaFingerprint{
    ComputeDraw2DSchemaFingerprint(kDraw2DSchemaDescription)};
static_assert(ComputeDraw2DSchemaFingerprint("hello") == 0xa430d84680aabd0bULL);
static_assert(kDraw2DSchemaFingerprint == 0x05c436a9fda7b4f7ULL);

class Draw2DPackedLinearPremultipliedRgba8 final
{
public:
    constexpr Draw2DPackedLinearPremultipliedRgba8() noexcept = default;

    [[nodiscard]] static constexpr Draw2DPackedLinearPremultipliedRgba8 FromChannels(
        std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) noexcept
    {
        return Draw2DPackedLinearPremultipliedRgba8{red, green, blue, alpha};
    }

    [[nodiscard]] constexpr std::uint8_t GetRed() const noexcept
    {
        return m_red;
    }

    [[nodiscard]] constexpr std::uint8_t GetGreen() const noexcept
    {
        return m_green;
    }

    [[nodiscard]] constexpr std::uint8_t GetBlue() const noexcept
    {
        return m_blue;
    }

    [[nodiscard]] constexpr std::uint8_t GetAlpha() const noexcept
    {
        return m_alpha;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DPackedLinearPremultipliedRgba8& lhs,
        const Draw2DPackedLinearPremultipliedRgba8& rhs) noexcept = default;

private:
    constexpr Draw2DPackedLinearPremultipliedRgba8(std::uint8_t red, std::uint8_t green,
                                                   std::uint8_t blue, std::uint8_t alpha) noexcept
        : m_red{red}, m_green{green}, m_blue{blue}, m_alpha{alpha}
    {
    }

    std::uint8_t m_red{};
    std::uint8_t m_green{};
    std::uint8_t m_blue{};
    std::uint8_t m_alpha{};
};

struct Draw2DVertex final
{
    float x{};
    float y{};
    Draw2DPackedLinearPremultipliedRgba8 color{};

    [[nodiscard]] friend constexpr bool operator==(const Draw2DVertex& lhs,
                                                   const Draw2DVertex& rhs) noexcept = default;
};

struct Draw2DPixelExtent final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr bool operator==(const Draw2DPixelExtent& lhs,
                                                   const Draw2DPixelExtent& rhs) noexcept = default;
};

struct Draw2DScissor final
{
    std::uint32_t left{};
    std::uint32_t top{};
    std::uint32_t right{};
    std::uint32_t bottom{};

    [[nodiscard]] friend constexpr bool operator==(const Draw2DScissor& lhs,
                                                   const Draw2DScissor& rhs) noexcept = default;
};

struct Draw2DDrawRecord final
{
    std::uint32_t firstIndex{};
    std::uint32_t indexCount{};
    std::int32_t baseVertex{};
    Draw2DScissor scissor{};

    [[nodiscard]] friend constexpr bool operator==(const Draw2DDrawRecord& lhs,
                                                   const Draw2DDrawRecord& rhs) noexcept = default;
};

static_assert(std::is_standard_layout_v<Draw2DPackedLinearPremultipliedRgba8>);
static_assert(std::is_trivially_copyable_v<Draw2DPackedLinearPremultipliedRgba8>);
static_assert(sizeof(Draw2DPackedLinearPremultipliedRgba8) == 4U);
static_assert(std::is_standard_layout_v<Draw2DVertex>);
static_assert(std::is_trivially_copyable_v<Draw2DVertex>);
static_assert(sizeof(Draw2DVertex) == 12U);
static_assert(alignof(Draw2DVertex) == alignof(float));
static_assert(offsetof(Draw2DVertex, x) == 0U);
static_assert(offsetof(Draw2DVertex, y) == 4U);
static_assert(offsetof(Draw2DVertex, color) == 8U);
static_assert(sizeof(Draw2DIndex) == 4U);

[[nodiscard]] constexpr bool IsValid(Draw2DPackedLinearPremultipliedRgba8 color) noexcept
{
    return color.GetRed() <= color.GetAlpha() && color.GetGreen() <= color.GetAlpha() &&
           color.GetBlue() <= color.GetAlpha();
}

[[nodiscard]] constexpr bool IsValid(Draw2DPixelExtent extent) noexcept
{
    return extent.width > 0U && extent.height > 0U;
}

[[nodiscard]] constexpr bool IsValid(Draw2DScissor scissor, Draw2DPixelExtent extent) noexcept
{
    return scissor.left < scissor.right && scissor.top < scissor.bottom &&
           scissor.right <= extent.width && scissor.bottom <= extent.height;
}

struct Draw2DPacketLimits final
{
    std::uint64_t maxVertexCount{4'194'304U};
    std::uint64_t maxIndexCount{8'388'608U};
    std::uint64_t maxDrawRecordCount{1'048'576U};
    std::uint64_t maxPacketBytes{256U * 1024U * 1024U};
    std::uint64_t maxUploadBytes{256U * 1024U * 1024U};
    std::uint64_t maxBaseVertexValidationIndexCount{8'388'608U};

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DPacketLimits& lhs, const Draw2DPacketLimits& rhs) noexcept = default;
};

inline constexpr Draw2DPacketLimits kDefaultDraw2DPacketLimits{};

[[nodiscard]] constexpr bool IsValid(Draw2DPacketLimits limits) noexcept
{
    return limits.maxVertexCount > 0U && limits.maxIndexCount > 0U &&
           limits.maxDrawRecordCount > 0U && limits.maxPacketBytes > 0U &&
           limits.maxUploadBytes > 0U && limits.maxBaseVertexValidationIndexCount > 0U;
}

struct Draw2DPacketCounts final
{
    std::uint64_t vertexCount{};
    std::uint64_t indexCount{};
    std::uint64_t drawRecordCount{};

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DPacketCounts& lhs, const Draw2DPacketCounts& rhs) noexcept = default;
};

struct Draw2DPacketStats final
{
    Draw2DPacketCounts counts{};
    std::uint64_t vertexBytes{};
    std::uint64_t indexBytes{};
    std::uint64_t drawRecordBytes{};
    std::uint64_t packetBytes{};
    std::uint64_t uploadBytes{};

    [[nodiscard]] friend constexpr bool operator==(const Draw2DPacketStats& lhs,
                                                   const Draw2DPacketStats& rhs) noexcept = default;
};

enum class Draw2DPacketValidationIssue : std::uint8_t
{
    None,
    InvalidLimits,
    InvalidExtent,
    SchemaFingerprintMismatch,
    VertexByteOverflow,
    IndexByteOverflow,
    DrawRecordByteOverflow,
    UploadByteOverflow,
    PacketByteOverflow,
    VertexCountUnrepresentable,
    IndexCountUnrepresentable,
    VertexCountLimit,
    IndexCountLimit,
    DrawRecordCountLimit,
    PacketByteLimit,
    UploadByteLimit,
    NonCanonicalEmptyPacket,
    NonFiniteVertex,
    InvalidPackedColor,
    IndexOutOfRange,
    EmptyDrawRange,
    NonTriangleDrawRange,
    DrawRangeOutOfRange,
    BaseVertexOutOfRange,
    BaseVertexValidationIndexCountLimit,
    InvalidScissor
};

struct Draw2DPacketValidation final
{
    Draw2DPacketValidationIssue issue{Draw2DPacketValidationIssue::None};
    Draw2DPacketStats stats{};
    std::uint64_t elementIndex{kNoDraw2DElementIndex};
    std::uint64_t requested{};
    std::uint64_t allowed{};

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return issue == Draw2DPacketValidationIssue::None;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DPacketValidation& lhs, const Draw2DPacketValidation& rhs) noexcept = default;
};
[[nodiscard]] constexpr std::string_view GetName(Draw2DPacketValidationIssue issue) noexcept
{
    switch (issue)
    {
    case Draw2DPacketValidationIssue::None:
        return "none";
    case Draw2DPacketValidationIssue::InvalidLimits:
        return "invalid_limits";
    case Draw2DPacketValidationIssue::InvalidExtent:
        return "invalid_extent";
    case Draw2DPacketValidationIssue::SchemaFingerprintMismatch:
        return "schema_fingerprint_mismatch";
    case Draw2DPacketValidationIssue::VertexByteOverflow:
        return "vertex_byte_overflow";
    case Draw2DPacketValidationIssue::IndexByteOverflow:
        return "index_byte_overflow";
    case Draw2DPacketValidationIssue::DrawRecordByteOverflow:
        return "draw_record_byte_overflow";
    case Draw2DPacketValidationIssue::UploadByteOverflow:
        return "upload_byte_overflow";
    case Draw2DPacketValidationIssue::PacketByteOverflow:
        return "packet_byte_overflow";
    case Draw2DPacketValidationIssue::VertexCountUnrepresentable:
        return "vertex_count_unrepresentable";
    case Draw2DPacketValidationIssue::IndexCountUnrepresentable:
        return "index_count_unrepresentable";
    case Draw2DPacketValidationIssue::VertexCountLimit:
        return "vertex_count_limit";
    case Draw2DPacketValidationIssue::IndexCountLimit:
        return "index_count_limit";
    case Draw2DPacketValidationIssue::DrawRecordCountLimit:
        return "draw_record_count_limit";
    case Draw2DPacketValidationIssue::PacketByteLimit:
        return "packet_byte_limit";
    case Draw2DPacketValidationIssue::UploadByteLimit:
        return "upload_byte_limit";
    case Draw2DPacketValidationIssue::NonCanonicalEmptyPacket:
        return "non_canonical_empty_packet";
    case Draw2DPacketValidationIssue::NonFiniteVertex:
        return "non_finite_vertex";
    case Draw2DPacketValidationIssue::InvalidPackedColor:
        return "invalid_packed_color";
    case Draw2DPacketValidationIssue::IndexOutOfRange:
        return "index_out_of_range";
    case Draw2DPacketValidationIssue::EmptyDrawRange:
        return "empty_draw_range";
    case Draw2DPacketValidationIssue::NonTriangleDrawRange:
        return "non_triangle_draw_range";
    case Draw2DPacketValidationIssue::DrawRangeOutOfRange:
        return "draw_range_out_of_range";
    case Draw2DPacketValidationIssue::BaseVertexOutOfRange:
        return "base_vertex_out_of_range";
    case Draw2DPacketValidationIssue::BaseVertexValidationIndexCountLimit:
        return "base_vertex_validation_index_count_limit";
    case Draw2DPacketValidationIssue::InvalidScissor:
        return "invalid_scissor";
    }
    return "unknown";
}

namespace detail
{
[[nodiscard]] constexpr bool CheckedAdd(std::uint64_t lhs, std::uint64_t rhs,
                                        std::uint64_t& result) noexcept
{
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs)
    {
        return false;
    }
    result = lhs + rhs;
    return true;
}

[[nodiscard]] constexpr bool CheckedMultiply(std::uint64_t lhs, std::uint64_t rhs,
                                             std::uint64_t& result) noexcept
{
    if (lhs != 0U && rhs > std::numeric_limits<std::uint64_t>::max() / lhs)
    {
        return false;
    }
    result = lhs * rhs;
    return true;
}

[[nodiscard]] constexpr Draw2DPacketValidation Fail(
    Draw2DPacketValidationIssue issue, Draw2DPacketStats stats,
    std::uint64_t elementIndex = kNoDraw2DElementIndex, std::uint64_t requested = 0U,
    std::uint64_t allowed = 0U) noexcept
{
    return Draw2DPacketValidation{issue, stats, elementIndex, requested, allowed};
}

[[nodiscard]] inline core::Error MakeError(
    RenderErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return core::Error{ToErrorCode(code), std::move(message), location};
}

[[nodiscard]] inline std::string MakeValidationMessage(const Draw2DPacketValidation& validation)
{
    std::string message{"Draw2D packet validation failed: "};
    message += GetName(validation.issue);
    if (validation.elementIndex != kNoDraw2DElementIndex)
    {
        message += ", element ";
        message += std::to_string(validation.elementIndex);
    }
    if (validation.requested != 0U || validation.allowed != 0U)
    {
        message += ", requested ";
        message += std::to_string(validation.requested);
        message += ", allowed ";
        message += std::to_string(validation.allowed);
    }
    message += ".";
    return message;
}
} // namespace detail

[[nodiscard]] constexpr Draw2DPacketValidation InspectDraw2DPacketCounts(
    Draw2DPacketCounts counts, Draw2DPacketLimits limits = kDefaultDraw2DPacketLimits) noexcept
{
    Draw2DPacketStats stats{.counts = counts};
    if (!IsValid(limits))
    {
        return detail::Fail(Draw2DPacketValidationIssue::InvalidLimits, stats);
    }
    if (!detail::CheckedMultiply(counts.vertexCount, sizeof(Draw2DVertex), stats.vertexBytes))
    {
        return detail::Fail(Draw2DPacketValidationIssue::VertexByteOverflow, stats);
    }
    if (!detail::CheckedMultiply(counts.indexCount, sizeof(Draw2DIndex), stats.indexBytes))
    {
        return detail::Fail(Draw2DPacketValidationIssue::IndexByteOverflow, stats);
    }
    if (!detail::CheckedMultiply(counts.drawRecordCount, sizeof(Draw2DDrawRecord),
                                 stats.drawRecordBytes))
    {
        return detail::Fail(Draw2DPacketValidationIssue::DrawRecordByteOverflow, stats);
    }
    if (!detail::CheckedAdd(stats.vertexBytes, stats.indexBytes, stats.uploadBytes))
    {
        return detail::Fail(Draw2DPacketValidationIssue::UploadByteOverflow, stats);
    }
    if (!detail::CheckedAdd(stats.uploadBytes, stats.drawRecordBytes, stats.packetBytes))
    {
        return detail::Fail(Draw2DPacketValidationIssue::PacketByteOverflow, stats);
    }

    constexpr std::uint64_t kMaximumAddressableCount{std::numeric_limits<std::uint32_t>::max()};
    if (counts.vertexCount > kMaximumAddressableCount)
    {
        return detail::Fail(Draw2DPacketValidationIssue::VertexCountUnrepresentable, stats,
                            kNoDraw2DElementIndex, counts.vertexCount, kMaximumAddressableCount);
    }
    if (counts.indexCount > kMaximumAddressableCount)
    {
        return detail::Fail(Draw2DPacketValidationIssue::IndexCountUnrepresentable, stats,
                            kNoDraw2DElementIndex, counts.indexCount, kMaximumAddressableCount);
    }
    if (counts.vertexCount > limits.maxVertexCount)
    {
        return detail::Fail(Draw2DPacketValidationIssue::VertexCountLimit, stats,
                            kNoDraw2DElementIndex, counts.vertexCount, limits.maxVertexCount);
    }
    if (counts.indexCount > limits.maxIndexCount)
    {
        return detail::Fail(Draw2DPacketValidationIssue::IndexCountLimit, stats,
                            kNoDraw2DElementIndex, counts.indexCount, limits.maxIndexCount);
    }
    if (counts.drawRecordCount > limits.maxDrawRecordCount)
    {
        return detail::Fail(Draw2DPacketValidationIssue::DrawRecordCountLimit, stats,
                            kNoDraw2DElementIndex, counts.drawRecordCount,
                            limits.maxDrawRecordCount);
    }
    if (stats.packetBytes > limits.maxPacketBytes)
    {
        return detail::Fail(Draw2DPacketValidationIssue::PacketByteLimit, stats,
                            kNoDraw2DElementIndex, stats.packetBytes, limits.maxPacketBytes);
    }
    if (stats.uploadBytes > limits.maxUploadBytes)
    {
        return detail::Fail(Draw2DPacketValidationIssue::UploadByteLimit, stats,
                            kNoDraw2DElementIndex, stats.uploadBytes, limits.maxUploadBytes);
    }
    return Draw2DPacketValidation{.stats = stats};
}
[[nodiscard]] inline Draw2DPacketValidation InspectDraw2DPacketData(
    Draw2DPixelExtent extent, std::uint64_t schemaFingerprint,
    std::span<const Draw2DVertex> vertices, std::span<const Draw2DIndex> indices,
    std::span<const Draw2DDrawRecord> drawRecords,
    Draw2DPacketLimits limits = kDefaultDraw2DPacketLimits) noexcept
{
    const Draw2DPacketCounts counts{static_cast<std::uint64_t>(vertices.size()),
                                    static_cast<std::uint64_t>(indices.size()),
                                    static_cast<std::uint64_t>(drawRecords.size())};
    Draw2DPacketValidation validation = InspectDraw2DPacketCounts(counts, limits);
    if (!validation.IsValid())
    {
        return validation;
    }
    if (!IsValid(extent))
    {
        return detail::Fail(Draw2DPacketValidationIssue::InvalidExtent, validation.stats);
    }
    if (schemaFingerprint != kDraw2DSchemaFingerprint)
    {
        return detail::Fail(Draw2DPacketValidationIssue::SchemaFingerprintMismatch,
                            validation.stats, kNoDraw2DElementIndex, schemaFingerprint,
                            kDraw2DSchemaFingerprint);
    }

    const bool emptyVertices = vertices.empty();
    const bool emptyIndices = indices.empty();
    const bool emptyDrawRecords = drawRecords.empty();
    if (emptyVertices && emptyIndices && emptyDrawRecords)
    {
        return validation;
    }
    if (emptyVertices || emptyIndices || emptyDrawRecords)
    {
        return detail::Fail(Draw2DPacketValidationIssue::NonCanonicalEmptyPacket, validation.stats);
    }

    for (std::size_t index = 0U; index < vertices.size(); ++index)
    {
        const Draw2DVertex& vertex = vertices[index];
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y))
        {
            return detail::Fail(Draw2DPacketValidationIssue::NonFiniteVertex, validation.stats,
                                static_cast<std::uint64_t>(index));
        }
        if (!IsValid(vertex.color))
        {
            return detail::Fail(Draw2DPacketValidationIssue::InvalidPackedColor, validation.stats,
                                static_cast<std::uint64_t>(index));
        }
    }
    Draw2DIndex minimumRawIndex{std::numeric_limits<Draw2DIndex>::max()};
    Draw2DIndex maximumRawIndex{};
    for (std::size_t index = 0U; index < indices.size(); ++index)
    {
        if (static_cast<std::uint64_t>(indices[index]) >= counts.vertexCount)
        {
            return detail::Fail(Draw2DPacketValidationIssue::IndexOutOfRange, validation.stats,
                                static_cast<std::uint64_t>(index), indices[index],
                                counts.vertexCount);
        }
        minimumRawIndex = std::min(minimumRawIndex, indices[index]);
        maximumRawIndex = std::max(maximumRawIndex, indices[index]);
    }

    std::uint64_t baseVertexValidationIndexCount{};
    for (std::size_t drawIndex = 0U; drawIndex < drawRecords.size(); ++drawIndex)
    {
        const Draw2DDrawRecord& draw = drawRecords[drawIndex];
        if (draw.indexCount == 0U)
        {
            return detail::Fail(Draw2DPacketValidationIssue::EmptyDrawRange, validation.stats,
                                static_cast<std::uint64_t>(drawIndex));
        }
        if (draw.indexCount % 3U != 0U)
        {
            return detail::Fail(Draw2DPacketValidationIssue::NonTriangleDrawRange, validation.stats,
                                static_cast<std::uint64_t>(drawIndex), draw.indexCount, 3U);
        }
        const std::uint64_t endIndex = static_cast<std::uint64_t>(draw.firstIndex) +
                                       static_cast<std::uint64_t>(draw.indexCount);
        if (endIndex > counts.indexCount)
        {
            return detail::Fail(Draw2DPacketValidationIssue::DrawRangeOutOfRange, validation.stats,
                                static_cast<std::uint64_t>(drawIndex), endIndex, counts.indexCount);
        }
        if (!IsValid(draw.scissor, extent))
        {
            return detail::Fail(Draw2DPacketValidationIssue::InvalidScissor, validation.stats,
                                static_cast<std::uint64_t>(drawIndex));
        }
        if (draw.baseVertex == 0)
        {
            continue;
        }

        const std::int64_t baseVertex = static_cast<std::int64_t>(draw.baseVertex);
        const std::int64_t globalMinimumEffectiveVertex =
            static_cast<std::int64_t>(minimumRawIndex) + baseVertex;
        const std::int64_t globalMaximumEffectiveVertex =
            static_cast<std::int64_t>(maximumRawIndex) + baseVertex;
        if (globalMinimumEffectiveVertex >= 0 &&
            static_cast<std::uint64_t>(globalMaximumEffectiveVertex) < counts.vertexCount)
        {
            continue;
        }

        std::uint64_t requiredValidationIndexCount{};
        const bool countRepresentable = detail::CheckedAdd(
            baseVertexValidationIndexCount, draw.indexCount, requiredValidationIndexCount);
        if (!countRepresentable ||
            requiredValidationIndexCount > limits.maxBaseVertexValidationIndexCount)
        {
            return detail::Fail(Draw2DPacketValidationIssue::BaseVertexValidationIndexCountLimit,
                                validation.stats, static_cast<std::uint64_t>(drawIndex),
                                countRepresentable ? requiredValidationIndexCount
                                                   : std::numeric_limits<std::uint64_t>::max(),
                                limits.maxBaseVertexValidationIndexCount);
        }
        baseVertexValidationIndexCount = requiredValidationIndexCount;

        for (std::uint64_t index = draw.firstIndex; index < endIndex; ++index)
        {
            const std::int64_t effectiveVertex =
                static_cast<std::int64_t>(indices[static_cast<std::size_t>(index)]) +
                static_cast<std::int64_t>(draw.baseVertex);
            if (effectiveVertex < 0 ||
                static_cast<std::uint64_t>(effectiveVertex) >= counts.vertexCount)
            {
                return detail::Fail(Draw2DPacketValidationIssue::BaseVertexOutOfRange,
                                    validation.stats, static_cast<std::uint64_t>(drawIndex));
            }
        }
    }
    return validation;
}

struct Draw2DPacketTestFactory;

class Draw2DPacket final
{
public:
    Draw2DPacket(const Draw2DPacket&) = delete;
    Draw2DPacket(Draw2DPacket&& other) noexcept
        : m_extent{other.m_extent}, m_schemaFingerprint{other.m_schemaFingerprint},
          m_vertices{std::move(other.m_vertices)}, m_indices{std::move(other.m_indices)},
          m_drawRecords{std::move(other.m_drawRecords)}, m_stats{other.m_stats}
    {
        other.NormalizeAfterMove();
    }
    Draw2DPacket& operator=(const Draw2DPacket&) = delete;
    Draw2DPacket& operator=(Draw2DPacket&& other) noexcept
    {
        if (this != &other)
        {
            m_extent = other.m_extent;
            m_schemaFingerprint = other.m_schemaFingerprint;
            m_vertices = std::move(other.m_vertices);
            m_indices = std::move(other.m_indices);
            m_drawRecords = std::move(other.m_drawRecords);
            m_stats = other.m_stats;
            other.NormalizeAfterMove();
        }
        return *this;
    }
    ~Draw2DPacket() = default;

    [[nodiscard]] Draw2DPixelExtent GetPixelExtent() const noexcept
    {
        return m_extent;
    }

    [[nodiscard]] std::uint64_t GetSchemaFingerprint() const noexcept
    {
        return m_schemaFingerprint;
    }

    [[nodiscard]] std::span<const Draw2DVertex> GetVertices() const noexcept
    {
        return m_vertices;
    }

    [[nodiscard]] std::span<const Draw2DIndex> GetIndices() const noexcept
    {
        return m_indices;
    }

    [[nodiscard]] std::span<const Draw2DDrawRecord> GetDrawRecords() const noexcept
    {
        return m_drawRecords;
    }

    [[nodiscard]] Draw2DPacketStats GetStats() const noexcept
    {
        return m_stats;
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return m_vertices.empty() && m_indices.empty() && m_drawRecords.empty();
    }

private:
    friend class Draw2DPacketBuilder;
    friend struct Draw2DPacketTestFactory;

    Draw2DPacket() = default;

    void NormalizeAfterMove() noexcept
    {
        m_schemaFingerprint = kDraw2DSchemaFingerprint;
        m_vertices.clear();
        m_indices.clear();
        m_drawRecords.clear();
        m_stats = {};
    }

    Draw2DPixelExtent m_extent{};
    std::uint64_t m_schemaFingerprint{kDraw2DSchemaFingerprint};
    std::vector<Draw2DVertex> m_vertices;
    std::vector<Draw2DIndex> m_indices;
    std::vector<Draw2DDrawRecord> m_drawRecords;
    Draw2DPacketStats m_stats{};
};

[[nodiscard]] inline Draw2DPacketValidation InspectDraw2DPacket(
    const Draw2DPacket& packet, Draw2DPacketLimits limits = kDefaultDraw2DPacketLimits) noexcept
{
    return InspectDraw2DPacketData(packet.GetPixelExtent(), packet.GetSchemaFingerprint(),
                                   packet.GetVertices(), packet.GetIndices(),
                                   packet.GetDrawRecords(), limits);
}

[[nodiscard]] inline core::Result<Draw2DPacketStats> ValidateDraw2DPacket(
    const Draw2DPacket& packet, Draw2DPacketLimits limits = kDefaultDraw2DPacketLimits)
{
    const Draw2DPacketValidation validation = InspectDraw2DPacket(packet, limits);
    if (!validation.IsValid())
    {
        return core::Result<Draw2DPacketStats>::FromError(detail::MakeError(
            RenderErrorCode::InvalidArgument, detail::MakeValidationMessage(validation)));
    }
    return validation.stats;
}

enum class Draw2DPacketBuilderState : std::uint8_t
{
    Open,
    Sealed
};

struct Draw2DPacketBuilderSnapshot final
{
    Draw2DPacketBuilderState state{Draw2DPacketBuilderState::Open};
    Draw2DPixelExtent extent{};
    Draw2DPacketCounts counts{};
    Draw2DPacketCounts capacities{};

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DPacketBuilderSnapshot& lhs,
        const Draw2DPacketBuilderSnapshot& rhs) noexcept = default;
};
class Draw2DPacketBuilder final
{
public:
    explicit Draw2DPacketBuilder(Draw2DPacketLimits limits = kDefaultDraw2DPacketLimits) noexcept
        : m_limits{limits}
    {
    }

    Draw2DPacketBuilder(const Draw2DPacketBuilder&) = delete;
    Draw2DPacketBuilder(Draw2DPacketBuilder&& other) noexcept
        : m_limits{other.m_limits}, m_state{other.m_state}, m_extent{other.m_extent},
          m_vertices{std::move(other.m_vertices)}, m_indices{std::move(other.m_indices)},
          m_drawRecords{std::move(other.m_drawRecords)},
          m_reusablePacket{std::move(other.m_reusablePacket)}
    {
        other.m_reusablePacket.reset();
        other.Reset();
    }
    Draw2DPacketBuilder& operator=(const Draw2DPacketBuilder&) = delete;
    Draw2DPacketBuilder& operator=(Draw2DPacketBuilder&& other) noexcept
    {
        if (this != &other)
        {
            m_limits = other.m_limits;
            m_state = other.m_state;
            m_extent = other.m_extent;
            m_vertices = std::move(other.m_vertices);
            m_indices = std::move(other.m_indices);
            m_drawRecords = std::move(other.m_drawRecords);
            m_reusablePacket = std::move(other.m_reusablePacket);
            other.m_reusablePacket.reset();
            other.Reset();
        }
        return *this;
    }
    ~Draw2DPacketBuilder() = default;

    [[nodiscard]] core::VoidResult SetPixelExtent(Draw2DPixelExtent extent)
    {
        if (core::VoidResult open = EnsureOpen(); !open.HasValue())
        {
            return open;
        }
        if (!IsEmpty())
        {
            return MakeFailure(RenderErrorCode::InvalidState,
                               "Draw2D packet extent must be set before packet data is appended.");
        }
        if (!IsValid(extent))
        {
            return MakeFailure(RenderErrorCode::InvalidArgument,
                               "Draw2D packet extent must have positive width and height.");
        }
        m_extent = extent;
        return core::VoidResult::Success();
    }

    [[nodiscard]] core::VoidResult Reserve(Draw2DPacketCounts counts)
    {
        if (core::VoidResult open = EnsureOpen(); !open.HasValue())
        {
            return open;
        }
        const Draw2DPacketCounts current = GetCounts();
        if (counts.vertexCount < current.vertexCount || counts.indexCount < current.indexCount ||
            counts.drawRecordCount < current.drawRecordCount)
        {
            return MakeFailure(RenderErrorCode::InvalidArgument,
                               "Draw2D reserve counts cannot be smaller than current storage.");
        }
        const Draw2DPacketValidation validation = InspectDraw2DPacketCounts(counts, m_limits);
        if (!validation.IsValid())
        {
            return MakeFailure(RenderErrorCode::InvalidArgument,
                               detail::MakeValidationMessage(validation));
        }
        if (core::VoidResult result = ReserveVector(m_vertices, counts.vertexCount, "vertex");
            !result.HasValue())
        {
            return result;
        }
        if (core::VoidResult result = ReserveVector(m_indices, counts.indexCount, "index");
            !result.HasValue())
        {
            return result;
        }
        return ReserveVector(m_drawRecords, counts.drawRecordCount, "draw record");
    }

    [[nodiscard]] core::VoidResult AppendVertices(std::span<const Draw2DVertex> vertices)
    {
        return AppendRange(m_vertices, vertices, StorageKind::Vertex);
    }

    [[nodiscard]] core::VoidResult AppendIndices(std::span<const Draw2DIndex> indices)
    {
        return AppendRange(m_indices, indices, StorageKind::Index);
    }

    [[nodiscard]] core::VoidResult AppendDrawRecords(std::span<const Draw2DDrawRecord> drawRecords)
    {
        return AppendRange(m_drawRecords, drawRecords, StorageKind::DrawRecord);
    }

    [[nodiscard]] core::VoidResult AppendDrawRecord(const Draw2DDrawRecord& drawRecord)
    {
        return AppendDrawRecords(std::span<const Draw2DDrawRecord>{&drawRecord, 1U});
    }

    [[nodiscard]] core::Result<Draw2DPacket> Seal()
    {
        if (core::VoidResult open = EnsureOpen(); !open.HasValue())
        {
            return core::Result<Draw2DPacket>::FromError(std::move(open).GetError());
        }

        const Draw2DPacketValidation validation = InspectDraw2DPacketData(
            m_extent, kDraw2DSchemaFingerprint, m_vertices, m_indices, m_drawRecords, m_limits);
        if (!validation.IsValid())
        {
            return core::Result<Draw2DPacket>::FromError(detail::MakeError(
                RenderErrorCode::InvalidArgument, detail::MakeValidationMessage(validation)));
        }

        Draw2DPacket packet = m_reusablePacket.has_value()
                                  ? std::move(*m_reusablePacket)
                                  : Draw2DPacket{};
        m_reusablePacket.reset();
        try
        {
            // Grow every stream before publishing new contents. Recycled packet capacity makes
            // this branch allocation-free for a warmed packet of the same or smaller shape.
            packet.m_vertices.reserve(m_vertices.size());
            packet.m_indices.reserve(m_indices.size());
            packet.m_drawRecords.reserve(m_drawRecords.size());
            packet.m_extent = m_extent;
            packet.m_schemaFingerprint = kDraw2DSchemaFingerprint;
            packet.m_vertices.assign(m_vertices.begin(), m_vertices.end());
            packet.m_indices.assign(m_indices.begin(), m_indices.end());
            packet.m_drawRecords.assign(m_drawRecords.begin(), m_drawRecords.end());
            packet.m_stats = validation.stats;
        }
        catch (const std::bad_alloc&)
        {
            return core::Result<Draw2DPacket>::FromError(detail::MakeError(
                RenderErrorCode::OutOfMemory,
                "Draw2D packet builder could not allocate sealed packet storage."));
        }
        catch (const std::length_error&)
        {
            return core::Result<Draw2DPacket>::FromError(detail::MakeError(
                RenderErrorCode::OutOfMemory,
                "Draw2D packet sealed storage exceeds the host container maximum."));
        }

        m_state = Draw2DPacketBuilderState::Sealed;
        return core::Result<Draw2DPacket>::FromValue(std::move(packet));
    }

    void Reset() noexcept
    {
        m_vertices.clear();
        m_indices.clear();
        m_drawRecords.clear();
        m_extent = {};
        m_state = Draw2DPacketBuilderState::Open;
    }

    void Reset(Draw2DPacket&& reusablePacket) noexcept
    {
        Reset();
        m_reusablePacket.emplace(std::move(reusablePacket));
    }

    [[nodiscard]] Draw2DPacketBuilderState GetState() const noexcept
    {
        return m_state;
    }

    [[nodiscard]] bool IsOpen() const noexcept
    {
        return m_state == Draw2DPacketBuilderState::Open;
    }

    [[nodiscard]] bool IsSealed() const noexcept
    {
        return m_state == Draw2DPacketBuilderState::Sealed;
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return m_vertices.empty() && m_indices.empty() && m_drawRecords.empty();
    }

    [[nodiscard]] Draw2DPacketLimits GetLimits() const noexcept
    {
        return m_limits;
    }

    [[nodiscard]] Draw2DPacketBuilderSnapshot GetSnapshot() const noexcept
    {
        return Draw2DPacketBuilderSnapshot{
            m_state, m_extent, GetCounts(),
            Draw2DPacketCounts{static_cast<std::uint64_t>(m_vertices.capacity()),
                               static_cast<std::uint64_t>(m_indices.capacity()),
                               static_cast<std::uint64_t>(m_drawRecords.capacity())}};
    }

private:
    enum class StorageKind : std::uint8_t
    {
        Vertex,
        Index,
        DrawRecord
    };

    [[nodiscard]] Draw2DPacketCounts GetCounts() const noexcept
    {
        return Draw2DPacketCounts{static_cast<std::uint64_t>(m_vertices.size()),
                                  static_cast<std::uint64_t>(m_indices.size()),
                                  static_cast<std::uint64_t>(m_drawRecords.size())};
    }

    [[nodiscard]] core::VoidResult EnsureOpen() const
    {
        if (!IsValid(m_limits))
        {
            return MakeFailure(RenderErrorCode::InvalidArgument,
                               "Draw2D packet builder limits must all be non-zero.");
        }
        if (!IsOpen())
        {
            return MakeFailure(RenderErrorCode::InvalidState,
                               "Draw2D packet builder is sealed; call Reset before reuse.");
        }
        return core::VoidResult::Success();
    }

    [[nodiscard]] static core::VoidResult MakeFailure(
        RenderErrorCode code, std::string message,
        std::source_location location = std::source_location::current())
    {
        return core::VoidResult::FromError(detail::MakeError(code, std::move(message), location));
    }

    template <typename Value>
    [[nodiscard]] core::VoidResult ReserveVector(std::vector<Value>& storage,
                                                 std::uint64_t requested,
                                                 std::string_view storageName)
    {
        if (requested <= static_cast<std::uint64_t>(storage.capacity()))
        {
            return core::VoidResult::Success();
        }
        if (requested > static_cast<std::uint64_t>(storage.max_size()) ||
            requested > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            return MakeFailure(RenderErrorCode::InvalidArgument,
                               "Draw2D " + std::string{storageName} +
                                   " capacity is not representable by the host container.");
        }
        try
        {
            storage.reserve(static_cast<std::size_t>(requested));
        }
        catch (const std::bad_alloc&)
        {
            return MakeFailure(RenderErrorCode::OutOfMemory,
                               "Draw2D packet builder could not reserve " +
                                   std::string{storageName} + " storage.");
        }
        catch (const std::length_error&)
        {
            return MakeFailure(RenderErrorCode::OutOfMemory,
                               "Draw2D " + std::string{storageName} +
                                   " storage exceeds the host container maximum.");
        }
        return core::VoidResult::Success();
    }

    template <typename Value>
    [[nodiscard]] core::VoidResult AppendRange(std::vector<Value>& storage,
                                               std::span<const Value> values, StorageKind kind)
    {
        if (core::VoidResult open = EnsureOpen(); !open.HasValue())
        {
            return open;
        }
        if (!IsValid(m_extent))
        {
            return MakeFailure(RenderErrorCode::InvalidState,
                               "Draw2D packet extent must be set before appending packet data.");
        }
        std::uint64_t required{};
        if (!detail::CheckedAdd(static_cast<std::uint64_t>(storage.size()),
                                static_cast<std::uint64_t>(values.size()), required))
        {
            return MakeFailure(RenderErrorCode::InvalidArgument,
                               "Draw2D packet append count overflowed uint64.");
        }

        Draw2DPacketCounts counts = GetCounts();
        std::uint64_t hardMaximum{};
        std::string_view storageName;
        switch (kind)
        {
        case StorageKind::Vertex:
            counts.vertexCount = required;
            hardMaximum = m_limits.maxVertexCount;
            storageName = "vertex";
            break;
        case StorageKind::Index:
            counts.indexCount = required;
            hardMaximum = m_limits.maxIndexCount;
            storageName = "index";
            break;
        case StorageKind::DrawRecord:
            counts.drawRecordCount = required;
            hardMaximum = m_limits.maxDrawRecordCount;
            storageName = "draw record";
            break;
        }

        const Draw2DPacketValidation validation = InspectDraw2DPacketCounts(counts, m_limits);
        if (!validation.IsValid())
        {
            return MakeFailure(RenderErrorCode::InvalidArgument,
                               detail::MakeValidationMessage(validation));
        }

        std::uint64_t desiredCapacity = required;
        const std::uint64_t currentCapacity = static_cast<std::uint64_t>(storage.capacity());
        if (currentCapacity < required)
        {
            std::uint64_t grownCapacity{};
            if (!detail::CheckedMultiply(std::max<std::uint64_t>(currentCapacity, 4U), 2U,
                                         grownCapacity))
            {
                grownCapacity = hardMaximum;
            }
            desiredCapacity = std::min(hardMaximum, std::max(required, grownCapacity));
        }
        if (core::VoidResult reserved = ReserveVector(storage, desiredCapacity, storageName);
            !reserved.HasValue())
        {
            return reserved;
        }

        try
        {
            storage.insert(storage.end(), values.begin(), values.end());
        }
        catch (const std::bad_alloc&)
        {
            return MakeFailure(RenderErrorCode::OutOfMemory,
                               "Draw2D packet append could not allocate storage.");
        }
        catch (const std::length_error&)
        {
            return MakeFailure(RenderErrorCode::OutOfMemory,
                               "Draw2D packet append exceeds the host container maximum.");
        }
        return core::VoidResult::Success();
    }

    Draw2DPacketLimits m_limits{};
    Draw2DPacketBuilderState m_state{Draw2DPacketBuilderState::Open};
    Draw2DPixelExtent m_extent{};
    std::vector<Draw2DVertex> m_vertices;
    std::vector<Draw2DIndex> m_indices;
    std::vector<Draw2DDrawRecord> m_drawRecords;
    std::optional<Draw2DPacket> m_reusablePacket;
};
} // namespace pond::render::draw2d
