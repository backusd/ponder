#pragma once

#include <ponder/ui/Error.hpp>

#include <cstdint>
#include <source_location>
#include <string>

namespace pond::ui
{
enum class UiHardLimitKind : std::uint8_t
{
    PaintCommandCount,
    PaintCommandPayloadBytes,
    ClipDepth,
    CompilerScratchBytes,
    CompiledVertexCount,
    CompiledIndexCount,
    DrawRecordCount,
    DrawPacketBytes,
    UploadBytes
};

struct UiHardLimits final
{
    std::uint64_t maxPaintCommandCount{1'048'576U};
    std::uint64_t maxPaintCommandPayloadBytes{64U * 1024U * 1024U};
    std::uint64_t maxClipDepth{1'024U};
    std::uint64_t maxCompilerScratchBytes{128U * 1024U * 1024U};
    std::uint64_t maxCompiledVertexCount{4'194'304U};
    std::uint64_t maxCompiledIndexCount{8'388'608U};
    std::uint64_t maxDrawRecordCount{1'048'576U};
    std::uint64_t maxDrawPacketBytes{256U * 1024U * 1024U};
    std::uint64_t maxUploadBytes{256U * 1024U * 1024U};

    [[nodiscard]] friend constexpr bool operator==(const UiHardLimits& lhs,
                                                   const UiHardLimits& rhs) noexcept = default;
};

struct UiLimitExceeded final
{
    UiHardLimitKind kind{};
    std::uint64_t requested{};
    std::uint64_t allowed{};

    [[nodiscard]] friend constexpr bool operator==(const UiLimitExceeded& lhs,
                                                   const UiLimitExceeded& rhs) noexcept = default;
};

inline constexpr UiHardLimits kDefaultUiHardLimits{};

[[nodiscard]] constexpr bool IsValid(UiHardLimits limits) noexcept
{
    return limits.maxPaintCommandCount > 0U && limits.maxPaintCommandPayloadBytes > 0U &&
           limits.maxClipDepth > 0U && limits.maxCompilerScratchBytes > 0U &&
           limits.maxCompiledVertexCount > 0U && limits.maxCompiledIndexCount > 0U &&
           limits.maxDrawRecordCount > 0U && limits.maxDrawPacketBytes > 0U &&
           limits.maxUploadBytes > 0U;
}

[[nodiscard]] constexpr std::uint64_t GetLimitValue(UiHardLimits limits,
                                                    UiHardLimitKind kind) noexcept
{
    switch (kind)
    {
    case UiHardLimitKind::PaintCommandCount:
        return limits.maxPaintCommandCount;
    case UiHardLimitKind::PaintCommandPayloadBytes:
        return limits.maxPaintCommandPayloadBytes;
    case UiHardLimitKind::ClipDepth:
        return limits.maxClipDepth;
    case UiHardLimitKind::CompilerScratchBytes:
        return limits.maxCompilerScratchBytes;
    case UiHardLimitKind::CompiledVertexCount:
        return limits.maxCompiledVertexCount;
    case UiHardLimitKind::CompiledIndexCount:
        return limits.maxCompiledIndexCount;
    case UiHardLimitKind::DrawRecordCount:
        return limits.maxDrawRecordCount;
    case UiHardLimitKind::DrawPacketBytes:
        return limits.maxDrawPacketBytes;
    case UiHardLimitKind::UploadBytes:
        return limits.maxUploadBytes;
    }

    return 0U;
}

[[nodiscard]] constexpr bool IsWithinLimit(std::uint64_t requested, std::uint64_t allowed) noexcept
{
    return requested <= allowed;
}

[[nodiscard]] constexpr UiLimitExceeded MakeUiLimitExceeded(UiHardLimitKind kind,
                                                            std::uint64_t requested,
                                                            std::uint64_t allowed) noexcept
{
    return UiLimitExceeded{kind, requested, allowed};
}

[[nodiscard]] inline core::VoidResult CheckUiHardLimit(
    UiHardLimitKind kind, std::uint64_t requested, UiHardLimits limits = kDefaultUiHardLimits,
    std::source_location location = std::source_location::current())
{
    const std::uint64_t allowed = GetLimitValue(limits, kind);
    if (IsWithinLimit(requested, allowed))
    {
        return core::VoidResult::Success();
    }

    return MakeUiFailure(UiErrorCode::LimitExceeded,
                         "UI hard limit exceeded: requested " + std::to_string(requested) +
                             ", allowed " + std::to_string(allowed) + ".",
                         location);
}
} // namespace pond::ui
