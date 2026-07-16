#include <ponder/ui/paint/PaintCompiler.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace pond::ui::paint
{
namespace
{
using render::draw2d::Draw2DDrawRecord;
using render::draw2d::Draw2DIndex;
using render::draw2d::Draw2DPackedLinearPremultipliedRgba8;
using render::draw2d::Draw2DPacketBuilder;
using render::draw2d::Draw2DPacketCounts;
using render::draw2d::Draw2DPacketLimits;
using render::draw2d::Draw2DPixelExtent;
using render::draw2d::Draw2DScissor;
using render::draw2d::Draw2DVertex;

[[nodiscard]] bool CheckedAdd(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& result) noexcept
{
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs)
    {
        return false;
    }
    result = lhs + rhs;
    return true;
}

[[nodiscard]] bool CheckedMultiply(std::uint64_t lhs, std::uint64_t rhs,
                                   std::uint64_t& result) noexcept
{
    if (lhs != 0U && rhs > std::numeric_limits<std::uint64_t>::max() / lhs)
    {
        return false;
    }
    result = lhs * rhs;
    return true;
}

[[nodiscard]] Draw2DPacketLimits MakePacketLimits(UiHardLimits limits) noexcept
{
    return Draw2DPacketLimits{.maxVertexCount = limits.maxCompiledVertexCount,
                              .maxIndexCount = limits.maxCompiledIndexCount,
                              .maxDrawRecordCount = limits.maxDrawRecordCount,
                              .maxPacketBytes = limits.maxDrawPacketBytes,
                              .maxUploadBytes = limits.maxUploadBytes,
                              .maxBaseVertexValidationIndexCount = limits.maxCompiledIndexCount};
}

[[nodiscard]] core::VoidResult MakeCompilerFailure(
    UiErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return MakeUiFailure(code, std::move(message), location);
}

template <typename Value>
[[nodiscard]] core::Result<Value> MakeCompilerFailureValue(
    UiErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return MakeUiFailure<Value>(code, std::move(message), location);
}

[[nodiscard]] bool IsEmpty(FractionalPixelRect rectangle) noexcept
{
    return rectangle.left >= rectangle.right || rectangle.top >= rectangle.bottom;
}

[[nodiscard]] FractionalPixelRect Intersect(FractionalPixelRect lhs,
                                            FractionalPixelRect rhs) noexcept
{
    const FractionalPixelRect intersection{std::max(lhs.left, rhs.left), std::max(lhs.top, rhs.top),
                                           std::min(lhs.right, rhs.right),
                                           std::min(lhs.bottom, rhs.bottom)};
    if (IsEmpty(intersection))
    {
        return FractionalPixelRect{};
    }
    return intersection;
}

[[nodiscard]] core::Result<FractionalPixelRect> Transform(LogicalRect rectangle, double scaleX,
                                                          double scaleY)
{
    const FractionalPixelRect transformed{static_cast<double>(GetLeft(rectangle)) * scaleX,
                                          static_cast<double>(GetTop(rectangle)) * scaleY,
                                          static_cast<double>(GetRight(rectangle)) * scaleX,
                                          static_cast<double>(GetBottom(rectangle)) * scaleY};
    if (!std::isfinite(transformed.left) || !std::isfinite(transformed.top) ||
        !std::isfinite(transformed.right) || !std::isfinite(transformed.bottom))
    {
        return MakeCompilerFailureValue<FractionalPixelRect>(
            UiErrorCode::CompilationFailure,
            "UI paint geometry became non-finite while scaling to framebuffer pixels.");
    }
    return transformed;
}

[[nodiscard]] float ToPacketFloat(double value) noexcept
{
    const float narrowed = static_cast<float>(value);
    return narrowed == 0.0F ? 0.0F : narrowed;
}

[[nodiscard]] Draw2DScissor MakeScissor(FractionalPixelRect clip, Draw2DPixelExtent extent) noexcept
{
    const double maximumX = static_cast<double>(extent.width);
    const double maximumY = static_cast<double>(extent.height);
    const double left = std::clamp(std::floor(clip.left), 0.0, maximumX);
    const double top = std::clamp(std::floor(clip.top), 0.0, maximumY);
    const double right = std::clamp(std::ceil(clip.right), 0.0, maximumX);
    const double bottom = std::clamp(std::ceil(clip.bottom), 0.0, maximumY);
    return Draw2DScissor{static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(top),
                         static_cast<std::uint32_t>(right), static_cast<std::uint32_t>(bottom)};
}

[[nodiscard]] bool IsSameScissor(Draw2DScissor lhs, Draw2DScissor rhs) noexcept
{
    return lhs == rhs;
}

[[nodiscard]] core::VoidResult ValidatePaintListStructure(const SealedPaintList& paintList,
                                                          const PaintListStats& stats,
                                                          PaintCommandIndex& rejectedCommand)
{
    const std::span<const PaintCommandRecord> commands = paintList.GetCommands();
    const std::span<const FillRectangleCommand> fills = paintList.GetFillRectangles();
    const std::span<const PushClipRectangleCommand> clips = paintList.GetPushClipRectangles();

    if (stats.state != PaintRecorderState::Sealed ||
        stats.commandCount != static_cast<std::uint64_t>(commands.size()) ||
        stats.fillRectangleCount != static_cast<std::uint64_t>(fills.size()) ||
        stats.pushClipRectangleCount != static_cast<std::uint64_t>(clips.size()) ||
        stats.clipDepth != 1U)
    {
        return MakeCompilerFailure(
            UiErrorCode::InvalidPaintValue,
            "UI paint compiler received inconsistent sealed paint-list statistics.");
    }

    std::uint64_t fillBytes{};
    std::uint64_t clipBytes{};
    std::uint64_t payloadBytes{};
    if (!CheckedMultiply(static_cast<std::uint64_t>(fills.size()), sizeof(FillRectangleCommand),
                         fillBytes) ||
        !CheckedMultiply(static_cast<std::uint64_t>(clips.size()), sizeof(PushClipRectangleCommand),
                         clipBytes) ||
        !CheckedAdd(fillBytes, clipBytes, payloadBytes) || payloadBytes != stats.payloadBytes)
    {
        return MakeCompilerFailure(
            UiErrorCode::InvalidPaintValue,
            "UI paint compiler received inconsistent paint payload byte counts.");
    }

    std::uint64_t fillIndex{};
    std::uint64_t clipIndex{};
    std::uint64_t popCount{};
    std::uint64_t clipDepth{1U};
    std::uint64_t maximumClipDepth{1U};
    for (std::size_t index = 0U; index < commands.size(); ++index)
    {
        const PaintCommandRecord& command = commands[index];
        rejectedCommand = static_cast<PaintCommandIndex>(index);
        if (command.index != static_cast<PaintCommandIndex>(index))
        {
            return MakeCompilerFailure(
                UiErrorCode::InvalidPaintValue,
                "UI paint command indices must be contiguous and preserve insertion order.");
        }

        switch (command.kind)
        {
        case PaintCommandKind::FillRectangle:
        {
            if (command.payloadIndex != fillIndex || fillIndex >= fills.size())
            {
                return MakeCompilerFailure(
                    UiErrorCode::InvalidPaintValue,
                    "UI fill-rectangle command has an invalid payload index.");
            }
            const FillRectangleCommand& fill = fills[static_cast<std::size_t>(fillIndex)];
            if (!IsValid(fill.rectangle) || !IsValid(fill.color) ||
                fill.isZeroArea != IsEmpty(fill.rectangle) ||
                fill.isTransparent != IsTransparent(fill.color))
            {
                return MakeCompilerFailure(
                    UiErrorCode::InvalidPaintValue,
                    "UI fill-rectangle payload failed compiler-boundary validation.");
            }
            ++fillIndex;
            break;
        }
        case PaintCommandKind::PushClipRectangle:
        {
            if (command.payloadIndex != clipIndex || clipIndex >= clips.size())
            {
                return MakeCompilerFailure(UiErrorCode::InvalidPaintValue,
                                           "UI push-clip command has an invalid payload index.");
            }
            const PushClipRectangleCommand& clip = clips[static_cast<std::size_t>(clipIndex)];
            if (!IsValid(clip.rectangle) || clip.isZeroArea != IsEmpty(clip.rectangle))
            {
                return MakeCompilerFailure(
                    UiErrorCode::InvalidPaintValue,
                    "UI clip-rectangle payload failed compiler-boundary validation.");
            }
            if (!CheckedAdd(clipDepth, 1U, clipDepth))
            {
                return MakeCompilerFailure(UiErrorCode::CompilationFailure,
                                           "UI paint clip depth overflowed during validation.");
            }
            maximumClipDepth = std::max(maximumClipDepth, clipDepth);
            ++clipIndex;
            break;
        }
        case PaintCommandKind::PopClip:
            if (command.payloadIndex != kNoPaintPayloadIndex || clipDepth == 1U)
            {
                return MakeCompilerFailure(
                    UiErrorCode::UnbalancedPaintState,
                    "UI pop-clip command would underflow the compiler clip sentinel.");
            }
            --clipDepth;
            ++popCount;
            break;
        default:
            return MakeCompilerFailure(UiErrorCode::InvalidPaintValue,
                                       "UI paint list contains an unknown command kind.");
        }
    }

    rejectedCommand = kNoRejectedPaintCommand;
    if (fillIndex != fills.size() || clipIndex != clips.size() || popCount != stats.popClipCount ||
        clipDepth != 1U || maximumClipDepth != stats.maxClipDepthObserved)
    {
        return MakeCompilerFailure(
            UiErrorCode::UnbalancedPaintState,
            "UI sealed paint list has inconsistent payload or clip-stack final state.");
    }
    return core::VoidResult::Success();
}

struct PassSummary final
{
    PaintCommandIndex lastCommandIndex{kNoRejectedPaintCommand};
    std::uint64_t processedCommandCount{};
    std::uint64_t maximumClipDepth{1U};
    std::uint64_t visibleRectangleCount{};
    std::uint64_t zeroAreaElisionCount{};
    std::uint64_t transparentElisionCount{};
    std::uint64_t clippedElisionCount{};
    std::uint64_t drawRecordCount{};
    std::uint64_t mergedDrawRecordCount{};
    PaintCompileCountInspection plannedOutput{};
    CompiledPixelBounds bounds{};

    [[nodiscard]] friend bool operator==(const PassSummary&, const PassSummary&) noexcept = default;
};
[[nodiscard]] core::Result<PassSummary> RunCompilationPass(
    const SealedPaintList& paintList, double scaleX, double scaleY, Draw2DPixelExtent extent,
    std::vector<FractionalPixelRect>& clipStack, UiHardLimits uiLimits,
    Draw2DPacketLimits packetLimits, Draw2DPacketBuilder* packetBuilder, PassSummary& progress)
{
    const std::span<const PaintCommandRecord> commands = paintList.GetCommands();
    const std::span<const FillRectangleCommand> fills = paintList.GetFillRectangles();
    const std::span<const PushClipRectangleCommand> clips = paintList.GetPushClipRectangles();

    progress = PassSummary{};
    PassSummary& summary = progress;
    clipStack.clear();
    clipStack.push_back(FractionalPixelRect{0.0, 0.0, static_cast<double>(extent.width),
                                            static_cast<double>(extent.height)});

    std::optional<Draw2DDrawRecord> pendingDraw;
    const auto flushPending = [&]() -> core::VoidResult
    {
        if (!pendingDraw.has_value() || packetBuilder == nullptr)
        {
            return core::VoidResult::Success();
        }
        const core::VoidResult appended = packetBuilder->AppendDrawRecord(*pendingDraw);
        if (!appended.HasValue())
        {
            return MakeCompilerFailure(UiErrorCode::CompilationFailure,
                                       "UI paint compiler could not append a Draw2D record: " +
                                           std::string{appended.GetError().GetMessage()});
        }
        return core::VoidResult::Success();
    };

    for (const PaintCommandRecord& command : commands)
    {
        summary.lastCommandIndex = command.index;
        ++summary.processedCommandCount;

        if (command.kind == PaintCommandKind::PushClipRectangle)
        {
            const PushClipRectangleCommand& clip =
                clips[static_cast<std::size_t>(command.payloadIndex)];
            const core::Result<FractionalPixelRect> transformed =
                Transform(clip.rectangle, scaleX, scaleY);
            if (!transformed.HasValue())
            {
                return core::Result<PassSummary>::FromError(transformed.GetError());
            }
            clipStack.push_back(Intersect(clipStack.back(), *transformed));
            summary.maximumClipDepth =
                std::max(summary.maximumClipDepth, static_cast<std::uint64_t>(clipStack.size()));
            continue;
        }
        if (command.kind == PaintCommandKind::PopClip)
        {
            clipStack.pop_back();
            continue;
        }

        const FillRectangleCommand& fill = fills[static_cast<std::size_t>(command.payloadIndex)];
        if (fill.isZeroArea)
        {
            ++summary.zeroAreaElisionCount;
            continue;
        }
        if (fill.isTransparent)
        {
            ++summary.transparentElisionCount;
            continue;
        }
        const FractionalPixelRect effectiveClip = clipStack.back();
        if (IsEmpty(effectiveClip))
        {
            ++summary.clippedElisionCount;
            continue;
        }

        const core::Result<FractionalPixelRect> transformed =
            Transform(fill.rectangle, scaleX, scaleY);
        if (!transformed.HasValue())
        {
            return core::Result<PassSummary>::FromError(transformed.GetError());
        }
        const FractionalPixelRect geometry = Intersect(*transformed, effectiveClip);
        if (IsEmpty(geometry))
        {
            ++summary.clippedElisionCount;
            continue;
        }

        const core::Result<PackedLinearPremultipliedRgba8> packedColor =
            PackLinearPremultipliedRgba8(fill.color);
        if (!packedColor.HasValue())
        {
            return MakeCompilerFailureValue<PassSummary>(
                UiErrorCode::InvalidPaintValue,
                "UI paint color failed RGBA8 quantization during compilation.");
        }
        const Draw2DPackedLinearPremultipliedRgba8 packetColor =
            Draw2DPackedLinearPremultipliedRgba8::FromChannels(
                packedColor->GetRed(), packedColor->GetGreen(), packedColor->GetBlue(),
                packedColor->GetAlpha());

        std::uint64_t vertexBaseWide{};
        std::uint64_t firstIndexWide{};
        if (!CheckedMultiply(summary.visibleRectangleCount, 4U, vertexBaseWide) ||
            !CheckedMultiply(summary.visibleRectangleCount, 6U, firstIndexWide) ||
            vertexBaseWide > std::numeric_limits<std::uint32_t>::max() ||
            firstIndexWide > std::numeric_limits<std::uint32_t>::max())
        {
            return MakeCompilerFailureValue<PassSummary>(
                UiErrorCode::LimitExceeded,
                "UI visible rectangle offsets exceed the Draw2D index representation.");
        }

        const float left = ToPacketFloat(geometry.left);
        const float top = ToPacketFloat(geometry.top);
        const float right = ToPacketFloat(geometry.right);
        const float bottom = ToPacketFloat(geometry.bottom);
        if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) ||
            !std::isfinite(bottom) || left >= right || top >= bottom)
        {
            return MakeCompilerFailureValue<PassSummary>(
                UiErrorCode::CompilationFailure, "UI clipped geometry cannot be represented "
                                                 "without collapsing in Draw2D pixel floats.");
        }

        const std::uint32_t vertexBase = static_cast<std::uint32_t>(vertexBaseWide);
        const std::array<Draw2DVertex, 4U> vertices{
            Draw2DVertex{left, top, packetColor}, Draw2DVertex{right, top, packetColor},
            Draw2DVertex{right, bottom, packetColor}, Draw2DVertex{left, bottom, packetColor}};
        const std::array<Draw2DIndex, 6U> indices{vertexBase, vertexBase + 1U, vertexBase + 2U,
                                                  vertexBase, vertexBase + 2U, vertexBase + 3U};

        if (packetBuilder != nullptr)
        {
            if (core::VoidResult appended = packetBuilder->AppendVertices(vertices);
                !appended.HasValue())
            {
                return MakeCompilerFailureValue<PassSummary>(
                    UiErrorCode::CompilationFailure,
                    "UI paint compiler could not append Draw2D vertices: " +
                        std::string{appended.GetError().GetMessage()});
            }
            if (core::VoidResult appended = packetBuilder->AppendIndices(indices);
                !appended.HasValue())
            {
                return MakeCompilerFailureValue<PassSummary>(
                    UiErrorCode::CompilationFailure,
                    "UI paint compiler could not append Draw2D indices: " +
                        std::string{appended.GetError().GetMessage()});
            }
        }

        const Draw2DScissor scissor = MakeScissor(effectiveClip, extent);
        const std::uint32_t firstIndex = static_cast<std::uint32_t>(firstIndexWide);
        const bool canMerge =
            pendingDraw.has_value() && IsSameScissor(pendingDraw->scissor, scissor) &&
            static_cast<std::uint64_t>(pendingDraw->firstIndex) + pendingDraw->indexCount ==
                firstIndexWide &&
            pendingDraw->indexCount <= std::numeric_limits<std::uint32_t>::max() - 6U;

        if (canMerge)
        {
            pendingDraw->indexCount += 6U;
            ++summary.mergedDrawRecordCount;
        }
        else
        {
            if (core::VoidResult flushed = flushPending(); !flushed.HasValue())
            {
                return core::Result<PassSummary>::FromError(std::move(flushed).GetError());
            }
            pendingDraw = Draw2DDrawRecord{firstIndex, 6U, 0, scissor};
            ++summary.drawRecordCount;
        }

        if (!summary.bounds.hasValue)
        {
            summary.bounds = CompiledPixelBounds{left, top, right, bottom, true};
        }
        else
        {
            summary.bounds.left = std::min(summary.bounds.left, left);
            summary.bounds.top = std::min(summary.bounds.top, top);
            summary.bounds.right = std::max(summary.bounds.right, right);
            summary.bounds.bottom = std::max(summary.bounds.bottom, bottom);
        }
        ++summary.visibleRectangleCount;
        summary.plannedOutput = InspectPaintCompileCounts(
            summary.visibleRectangleCount, summary.drawRecordCount, uiLimits, packetLimits);
        if (!summary.plannedOutput.IsValid())
        {
            return MakeCompilerFailureValue<PassSummary>(
                UiErrorCode::LimitExceeded,
                "UI compiled Draw2D output exceeds a checked hard limit.");
        }
    }

    if (core::VoidResult flushed = flushPending(); !flushed.HasValue())
    {
        return core::Result<PassSummary>::FromError(std::move(flushed).GetError());
    }
    if (clipStack.size() != 1U)
    {
        return MakeCompilerFailureValue<PassSummary>(
            UiErrorCode::UnbalancedPaintState,
            "UI paint compiler ended with an unbalanced clip stack.");
    }
    return summary;
}

[[nodiscard]] UiHardLimitKind MapPacketIssueToUiLimit(
    render::draw2d::Draw2DPacketValidationIssue issue) noexcept
{
    using Issue = render::draw2d::Draw2DPacketValidationIssue;
    switch (issue)
    {
    case Issue::VertexByteOverflow:
    case Issue::VertexCountUnrepresentable:
    case Issue::VertexCountLimit:
        return UiHardLimitKind::CompiledVertexCount;
    case Issue::IndexByteOverflow:
    case Issue::IndexCountUnrepresentable:
    case Issue::IndexCountLimit:
        return UiHardLimitKind::CompiledIndexCount;
    case Issue::DrawRecordByteOverflow:
    case Issue::DrawRecordCountLimit:
        return UiHardLimitKind::DrawRecordCount;
    case Issue::UploadByteOverflow:
    case Issue::UploadByteLimit:
        return UiHardLimitKind::UploadBytes;
    default:
        return UiHardLimitKind::DrawPacketBytes;
    }
}

void CopyPassSummary(PaintCompilerInspection& inspection, const PassSummary& summary) noexcept
{
    inspection.lastCommandIndex = summary.lastCommandIndex;
    inspection.processedCommandCount = summary.processedCommandCount;
    inspection.maximumClipDepth = summary.maximumClipDepth;
    inspection.visibleRectangleCount = summary.visibleRectangleCount;
    inspection.zeroAreaElisionCount = summary.zeroAreaElisionCount;
    inspection.transparentElisionCount = summary.transparentElisionCount;
    inspection.clippedElisionCount = summary.clippedElisionCount;
    inspection.mergedDrawRecordCount = summary.mergedDrawRecordCount;
    inspection.plannedOutput = summary.plannedOutput;
    inspection.outputBounds = summary.bounds;
    if (!summary.plannedOutput.IsValid() && summary.plannedOutput.hasRejectedLimit)
    {
        inspection.hasRejectedLimit = true;
        inspection.rejectedLimit = summary.plannedOutput.rejectedLimit;
    }
}
} // namespace
PaintCompileCountInspection InspectPaintCompileCounts(std::uint64_t visibleRectangleCount,
                                                      std::uint64_t drawRecordCount,
                                                      UiHardLimits uiLimits,
                                                      Draw2DPacketLimits packetLimits) noexcept
{
    PaintCompileCountInspection inspection;
    if (!IsValid(uiLimits) || !render::draw2d::IsValid(packetLimits))
    {
        inspection.issue = PaintCompileCountIssue::InvalidLimits;
        return inspection;
    }
    if (!CheckedMultiply(visibleRectangleCount, 4U, inspection.counts.vertexCount))
    {
        inspection.issue = PaintCompileCountIssue::VertexCountOverflow;
        inspection.hasRejectedLimit = true;
        inspection.rejectedLimit = UiLimitExceeded{
            UiHardLimitKind::CompiledVertexCount, std::numeric_limits<std::uint64_t>::max(),
            std::min(uiLimits.maxCompiledVertexCount, packetLimits.maxVertexCount)};
        return inspection;
    }
    if (!CheckedMultiply(visibleRectangleCount, 6U, inspection.counts.indexCount))
    {
        inspection.issue = PaintCompileCountIssue::IndexCountOverflow;
        inspection.hasRejectedLimit = true;
        inspection.rejectedLimit = UiLimitExceeded{
            UiHardLimitKind::CompiledIndexCount, std::numeric_limits<std::uint64_t>::max(),
            std::min(uiLimits.maxCompiledIndexCount, packetLimits.maxIndexCount)};
        return inspection;
    }
    inspection.counts.drawRecordCount = drawRecordCount;

    const Draw2DPacketLimits arithmeticLimits{
        std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max(),
        std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max(),
        std::numeric_limits<std::uint64_t>::max()};
    const render::draw2d::Draw2DPacketValidation arithmetic =
        render::draw2d::InspectDraw2DPacketCounts(inspection.counts, arithmeticLimits);
    inspection.packetStats = arithmetic.stats;
    if (!arithmetic.IsValid())
    {
        inspection.issue = PaintCompileCountIssue::LimitExceeded;
        inspection.hasRejectedLimit = true;
        const UiHardLimitKind kind = MapPacketIssueToUiLimit(arithmetic.issue);
        inspection.rejectedLimit =
            UiLimitExceeded{kind,
                            arithmetic.requested == 0U ? std::numeric_limits<std::uint64_t>::max()
                                                       : arithmetic.requested,
                            arithmetic.allowed};
        return inspection;
    }

    const auto reject = [&](UiHardLimitKind kind, std::uint64_t requested, std::uint64_t allowed)
    {
        inspection.issue = PaintCompileCountIssue::LimitExceeded;
        inspection.hasRejectedLimit = true;
        inspection.rejectedLimit = UiLimitExceeded{kind, requested, allowed};
    };

    const std::uint64_t vertexLimit =
        std::min(uiLimits.maxCompiledVertexCount, packetLimits.maxVertexCount);
    const std::uint64_t indexLimit =
        std::min(uiLimits.maxCompiledIndexCount, packetLimits.maxIndexCount);
    const std::uint64_t drawLimit =
        std::min(uiLimits.maxDrawRecordCount, packetLimits.maxDrawRecordCount);
    const std::uint64_t packetByteLimit =
        std::min(uiLimits.maxDrawPacketBytes, packetLimits.maxPacketBytes);
    const std::uint64_t uploadByteLimit =
        std::min(uiLimits.maxUploadBytes, packetLimits.maxUploadBytes);

    if (inspection.counts.vertexCount > vertexLimit)
    {
        reject(UiHardLimitKind::CompiledVertexCount, inspection.counts.vertexCount, vertexLimit);
    }
    else if (inspection.counts.indexCount > indexLimit)
    {
        reject(UiHardLimitKind::CompiledIndexCount, inspection.counts.indexCount, indexLimit);
    }
    else if (inspection.counts.drawRecordCount > drawLimit)
    {
        reject(UiHardLimitKind::DrawRecordCount, inspection.counts.drawRecordCount, drawLimit);
    }
    else if (inspection.packetStats.packetBytes > packetByteLimit)
    {
        reject(UiHardLimitKind::DrawPacketBytes, inspection.packetStats.packetBytes,
               packetByteLimit);
    }
    else if (inspection.packetStats.uploadBytes > uploadByteLimit)
    {
        reject(UiHardLimitKind::UploadBytes, inspection.packetStats.uploadBytes, uploadByteLimit);
    }
    return inspection;
}

PaintCompiler::PaintCompiler(UiHardLimits limits) noexcept
    : m_limits{limits}, m_packetBuilder{MakePacketLimits(limits)}
{
    m_inspection.clipStackCapacity = static_cast<std::uint64_t>(m_clipStack.capacity());
    m_inspection.packetBuilder = m_packetBuilder.GetSnapshot();
}

PaintCompiler::PaintCompiler(PaintCompiler&& other) noexcept
    : m_limits{other.m_limits}, m_packetBuilder{std::move(other.m_packetBuilder)},
      m_clipStack{std::move(other.m_clipStack)}, m_inspection{other.m_inspection}
{
    other.Reset();
}

PaintCompiler& PaintCompiler::operator=(PaintCompiler&& other) noexcept
{
    if (this != &other)
    {
        m_limits = other.m_limits;
        m_packetBuilder = std::move(other.m_packetBuilder);
        m_clipStack = std::move(other.m_clipStack);
        m_inspection = other.m_inspection;
        other.Reset();
    }
    return *this;
}

core::Result<render::draw2d::Draw2DPacket> PaintCompiler::Compile(const SealedPaintList& paintList,
                                                                  const UiTargetMetrics& metrics)
{
    if (!IsReady())
    {
        m_inspection = PaintCompilerInspection{};
        m_inspection.status = PaintCompileStatus::Failed;
        m_inspection.stage = PaintCompileStage::StateValidation;
        m_inspection.clipStackCapacity = static_cast<std::uint64_t>(m_clipStack.capacity());
        m_inspection.packetBuilder = m_packetBuilder.GetSnapshot();
        return MakeCompilerFailureValue<render::draw2d::Draw2DPacket>(
            UiErrorCode::InvalidPaintState,
            "UI paint compiler must be reset before another compilation.");
    }

    m_inspection = PaintCompilerInspection{};
    const auto fail =
        [&](PaintCompileStage stage, UiErrorCode code, std::string message,
            PaintCommandIndex rejectedCommand,
            std::source_location location =
                std::source_location::current()) -> core::Result<render::draw2d::Draw2DPacket>
    {
        m_packetBuilder.Reset();
        m_clipStack.clear();
        m_inspection.status = PaintCompileStatus::Failed;
        m_inspection.stage = stage;
        m_inspection.rejectedCommandIndex = rejectedCommand;
        m_inspection.clipStackCapacity = static_cast<std::uint64_t>(m_clipStack.capacity());
        m_inspection.packetBuilder = m_packetBuilder.GetSnapshot();
        return MakeCompilerFailureValue<render::draw2d::Draw2DPacket>(code, std::move(message),
                                                                      location);
    };

    if (!IsValid(m_limits))
    {
        return fail(PaintCompileStage::PaintValidation, UiErrorCode::InvalidPaintValue,
                    "UI paint compiler limits must all be non-zero.", kNoRejectedPaintCommand);
    }

    const auto checkInputLimit = [&](UiHardLimitKind kind, std::uint64_t requested) noexcept
    {
        const std::uint64_t allowed = GetLimitValue(m_limits, kind);
        if (requested <= allowed)
        {
            return true;
        }
        m_inspection.hasRejectedLimit = true;
        m_inspection.rejectedLimit = UiLimitExceeded{kind, requested, allowed};
        return false;
    };
    const std::uint64_t commandCount = static_cast<std::uint64_t>(paintList.GetCommands().size());
    if (!checkInputLimit(UiHardLimitKind::PaintCommandCount, commandCount))
    {
        return fail(PaintCompileStage::PaintValidation, UiErrorCode::LimitExceeded,
                    "UI paint list exceeds the compiler command-count hard limit.",
                    kNoRejectedPaintCommand);
    }

    const PaintListStats paintStats = paintList.GetStats();
    if (!checkInputLimit(UiHardLimitKind::PaintCommandPayloadBytes, paintStats.payloadBytes) ||
        !checkInputLimit(UiHardLimitKind::ClipDepth, paintStats.maxClipDepthObserved))
    {
        return fail(PaintCompileStage::PaintValidation, UiErrorCode::LimitExceeded,
                    "UI paint list exceeds a compiler input hard limit.", kNoRejectedPaintCommand);
    }

    PaintCommandIndex rejectedCommand{kNoRejectedPaintCommand};
    const core::VoidResult paintValidation =
        ValidatePaintListStructure(paintList, paintStats, rejectedCommand);
    if (!paintValidation.HasValue())
    {
        return fail(PaintCompileStage::PaintValidation,
                    static_cast<UiErrorCode>(paintValidation.GetError().GetCode().GetValue()),
                    std::string{paintValidation.GetError().GetMessage()}, rejectedCommand,
                    paintValidation.GetError().GetLocation());
    }

    if (!IsValid(metrics) || !IsDrawable(metrics))
    {
        return fail(PaintCompileStage::MetricsValidation, UiErrorCode::InvalidMetrics,
                    "UI paint compilation requires valid drawable target metrics.",
                    kNoRejectedPaintCommand);
    }
    const LogicalSize logicalSize = metrics.GetLogicalSize();
    const FramebufferPixelSize framebufferSize = metrics.GetFramebufferPixelSize();
    if (logicalSize.width <= 0.0F || logicalSize.height <= 0.0F || framebufferSize.width == 0U ||
        framebufferSize.height == 0U)
    {
        return fail(PaintCompileStage::MetricsValidation, UiErrorCode::InvalidMetrics,
                    "UI paint compilation cannot divide by a zero target extent.",
                    kNoRejectedPaintCommand);
    }

    const double scaleX =
        static_cast<double>(framebufferSize.width) / static_cast<double>(logicalSize.width);
    const double scaleY =
        static_cast<double>(framebufferSize.height) / static_cast<double>(logicalSize.height);
    if (!std::isfinite(scaleX) || !std::isfinite(scaleY) || scaleX <= 0.0 || scaleY <= 0.0)
    {
        return fail(PaintCompileStage::MetricsValidation, UiErrorCode::InvalidMetrics,
                    "UI logical-to-framebuffer scales must be finite and positive.",
                    kNoRejectedPaintCommand);
    }

    std::uint64_t scratchBytes{};
    if (!CheckedMultiply(paintStats.maxClipDepthObserved, sizeof(FractionalPixelRect),
                         scratchBytes) ||
        scratchBytes > m_limits.maxCompilerScratchBytes)
    {
        m_inspection.hasRejectedLimit = true;
        m_inspection.rejectedLimit = UiLimitExceeded{
            UiHardLimitKind::CompilerScratchBytes,
            scratchBytes == 0U ? std::numeric_limits<std::uint64_t>::max() : scratchBytes,
            m_limits.maxCompilerScratchBytes};
        return fail(PaintCompileStage::ScratchPreflight, UiErrorCode::LimitExceeded,
                    "UI paint compiler clip-stack scratch exceeds its hard limit.",
                    kNoRejectedPaintCommand);
    }
    m_inspection.scratchBytes = scratchBytes;

    try
    {
        m_clipStack.reserve(static_cast<std::size_t>(paintStats.maxClipDepthObserved));
    }
    catch (const std::bad_alloc&)
    {
        return fail(PaintCompileStage::ScratchPreflight, UiErrorCode::CompilationFailure,
                    "UI paint compiler could not reserve clip-stack scratch.",
                    kNoRejectedPaintCommand);
    }
    catch (const std::length_error&)
    {
        return fail(PaintCompileStage::ScratchPreflight, UiErrorCode::CompilationFailure,
                    "UI paint compiler clip-stack scratch exceeds the host maximum.",
                    kNoRejectedPaintCommand);
    }
    m_inspection.clipStackCapacity = static_cast<std::uint64_t>(m_clipStack.capacity());

    const Draw2DPixelExtent packetExtent{framebufferSize.width, framebufferSize.height};
    PassSummary preflightProgress;
    const core::Result<PassSummary> preflight =
        RunCompilationPass(paintList, scaleX, scaleY, packetExtent, m_clipStack, m_limits,
                           m_packetBuilder.GetLimits(), nullptr, preflightProgress);
    if (!preflight.HasValue())
    {
        CopyPassSummary(m_inspection, preflightProgress);
        return fail(PaintCompileStage::GeometryPreflight,
                    static_cast<UiErrorCode>(preflight.GetError().GetCode().GetValue()),
                    std::string{preflight.GetError().GetMessage()},
                    preflightProgress.lastCommandIndex, preflight.GetError().GetLocation());
    }
    CopyPassSummary(m_inspection, *preflight);

    if (core::VoidResult extentResult = m_packetBuilder.SetPixelExtent(packetExtent);
        !extentResult.HasValue())
    {
        return fail(PaintCompileStage::PacketBuild, UiErrorCode::CompilationFailure,
                    std::string{extentResult.GetError().GetMessage()}, kNoRejectedPaintCommand,
                    extentResult.GetError().GetLocation());
    }
    if (core::VoidResult reserveResult = m_packetBuilder.Reserve(m_inspection.plannedOutput.counts);
        !reserveResult.HasValue())
    {
        return fail(PaintCompileStage::PacketBuild, UiErrorCode::CompilationFailure,
                    std::string{reserveResult.GetError().GetMessage()}, kNoRejectedPaintCommand,
                    reserveResult.GetError().GetLocation());
    }

    PassSummary emittedProgress;
    const core::Result<PassSummary> emitted =
        RunCompilationPass(paintList, scaleX, scaleY, packetExtent, m_clipStack, m_limits,
                           m_packetBuilder.GetLimits(), &m_packetBuilder, emittedProgress);
    if (!emitted.HasValue())
    {
        CopyPassSummary(m_inspection, emittedProgress);
        return fail(PaintCompileStage::PacketBuild,
                    static_cast<UiErrorCode>(emitted.GetError().GetCode().GetValue()),
                    std::string{emitted.GetError().GetMessage()}, emittedProgress.lastCommandIndex,
                    emitted.GetError().GetLocation());
    }
    if (*emitted != *preflight)
    {
        return fail(PaintCompileStage::PacketBuild, UiErrorCode::CompilationFailure,
                    "UI paint compiler emission diverged from deterministic preflight.",
                    emitted->lastCommandIndex);
    }

    core::Result<render::draw2d::Draw2DPacket> packet = m_packetBuilder.Seal();
    if (!packet.HasValue())
    {
        return fail(PaintCompileStage::PacketSeal, UiErrorCode::CompilationFailure,
                    std::string{packet.GetError().GetMessage()}, emitted->lastCommandIndex,
                    packet.GetError().GetLocation());
    }

    CopyPassSummary(m_inspection, *emitted);
    m_inspection.status = PaintCompileStatus::Succeeded;
    m_inspection.stage = PaintCompileStage::None;
    m_inspection.rejectedCommandIndex = kNoRejectedPaintCommand;
    m_inspection.clipStackCapacity = static_cast<std::uint64_t>(m_clipStack.capacity());
    m_inspection.packetBuilder = m_packetBuilder.GetSnapshot();
    return core::Result<render::draw2d::Draw2DPacket>::FromValue(std::move(packet).GetValue());
}

void PaintCompiler::Reset() noexcept
{
    m_packetBuilder.Reset();
    m_clipStack.clear();
    m_inspection = PaintCompilerInspection{};
    m_inspection.clipStackCapacity = static_cast<std::uint64_t>(m_clipStack.capacity());
    m_inspection.packetBuilder = m_packetBuilder.GetSnapshot();
}

void PaintCompiler::Reset(render::draw2d::Draw2DPacket&& reusablePacket) noexcept
{
    m_packetBuilder.Reset(std::move(reusablePacket));
    m_clipStack.clear();
    m_inspection = PaintCompilerInspection{};
    m_inspection.clipStackCapacity = static_cast<std::uint64_t>(m_clipStack.capacity());
    m_inspection.packetBuilder = m_packetBuilder.GetSnapshot();
}

PaintCompilerInspection PaintCompiler::GetInspection() const noexcept
{
    return m_inspection;
}

UiHardLimits PaintCompiler::GetLimits() const noexcept
{
    return m_limits;
}

bool PaintCompiler::IsReady() const noexcept
{
    return m_inspection.status == PaintCompileStatus::Ready && m_packetBuilder.IsOpen() &&
           m_packetBuilder.IsEmpty();
}
} // namespace pond::ui::paint
