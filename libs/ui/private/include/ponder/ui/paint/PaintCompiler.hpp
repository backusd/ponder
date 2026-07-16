#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/render/draw2d/Draw2DPacket.hpp>
#include <ponder/ui/Limits.hpp>
#include <ponder/ui/Metrics.hpp>
#include <ponder/ui/paint/PaintRecorder.hpp>

#include <cstdint>
#include <limits>
#include <vector>

namespace pond::ui::paint
{
inline constexpr PaintCommandIndex kNoRejectedPaintCommand{
    std::numeric_limits<PaintCommandIndex>::max()};

enum class PaintCompileStatus : std::uint8_t
{
    Ready,
    Succeeded,
    Failed
};

enum class PaintCompileStage : std::uint8_t
{
    None,
    StateValidation,
    PaintValidation,
    MetricsValidation,
    ScratchPreflight,
    GeometryPreflight,
    PacketBuild,
    PacketSeal
};

enum class PaintCompileCountIssue : std::uint8_t
{
    None,
    InvalidLimits,
    VertexCountOverflow,
    IndexCountOverflow,
    LimitExceeded
};

struct PaintCompileCountInspection final
{
    PaintCompileCountIssue issue{PaintCompileCountIssue::None};
    render::draw2d::Draw2DPacketCounts counts{};
    render::draw2d::Draw2DPacketStats packetStats{};
    bool hasRejectedLimit{};
    UiLimitExceeded rejectedLimit{};

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return issue == PaintCompileCountIssue::None;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const PaintCompileCountInspection& lhs,
        const PaintCompileCountInspection& rhs) noexcept = default;
};

[[nodiscard]] PaintCompileCountInspection InspectPaintCompileCounts(
    std::uint64_t visibleRectangleCount, std::uint64_t drawRecordCount, UiHardLimits uiLimits,
    render::draw2d::Draw2DPacketLimits packetLimits) noexcept;

struct CompiledPixelBounds final
{
    float left{};
    float top{};
    float right{};
    float bottom{};
    bool hasValue{};

    [[nodiscard]] friend constexpr bool operator==(
        const CompiledPixelBounds& lhs, const CompiledPixelBounds& rhs) noexcept = default;
};

struct PaintCompilerInspection final
{
    PaintCompileStatus status{PaintCompileStatus::Ready};
    PaintCompileStage stage{PaintCompileStage::None};
    PaintCommandIndex lastCommandIndex{kNoRejectedPaintCommand};
    PaintCommandIndex rejectedCommandIndex{kNoRejectedPaintCommand};
    std::uint64_t processedCommandCount{};
    std::uint64_t maximumClipDepth{};
    std::uint64_t visibleRectangleCount{};
    std::uint64_t zeroAreaElisionCount{};
    std::uint64_t transparentElisionCount{};
    std::uint64_t clippedElisionCount{};
    std::uint64_t mergedDrawRecordCount{};
    std::uint64_t scratchBytes{};
    PaintCompileCountInspection plannedOutput{};
    CompiledPixelBounds outputBounds{};
    bool hasRejectedLimit{};
    UiLimitExceeded rejectedLimit{};
    std::uint64_t clipStackCapacity{};
    render::draw2d::Draw2DPacketBuilderSnapshot packetBuilder{};

    [[nodiscard]] friend constexpr bool operator==(
        const PaintCompilerInspection& lhs, const PaintCompilerInspection& rhs) noexcept = default;
};

struct FractionalPixelRect final
{
    double left{};
    double top{};
    double right{};
    double bottom{};

    [[nodiscard]] friend constexpr bool operator==(
        const FractionalPixelRect& lhs, const FractionalPixelRect& rhs) noexcept = default;
};

class PaintCompiler final
{
public:
    explicit PaintCompiler(UiHardLimits limits = kDefaultUiHardLimits) noexcept;
    PaintCompiler(const PaintCompiler&) = delete;
    PaintCompiler(PaintCompiler&& other) noexcept;
    PaintCompiler& operator=(const PaintCompiler&) = delete;
    PaintCompiler& operator=(PaintCompiler&& other) noexcept;
    ~PaintCompiler() = default;

    [[nodiscard]] core::Result<render::draw2d::Draw2DPacket> Compile(
        const SealedPaintList& paintList, const UiTargetMetrics& metrics);

    void Reset() noexcept;
    void Reset(render::draw2d::Draw2DPacket&& reusablePacket) noexcept;

    [[nodiscard]] PaintCompilerInspection GetInspection() const noexcept;
    [[nodiscard]] UiHardLimits GetLimits() const noexcept;
    [[nodiscard]] bool IsReady() const noexcept;

private:
    UiHardLimits m_limits{};
    render::draw2d::Draw2DPacketBuilder m_packetBuilder;
    std::vector<FractionalPixelRect> m_clipStack;
    PaintCompilerInspection m_inspection{};
};
} // namespace pond::ui::paint
