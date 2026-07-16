#pragma once

#include <cstdint>

namespace pond::ui
{
enum class UiDrawOutcome : std::uint8_t
{
    Recorded,
    SkippedSuspended,
    SkippedEmpty
};

struct UiDrawCounters final
{
    std::uint64_t recorded{};
    std::uint64_t skippedSuspended{};
    std::uint64_t skippedEmpty{};
    std::uint64_t paintCommands{};
    std::uint64_t compiledVertices{};
    std::uint64_t compiledIndices{};
    std::uint64_t drawRecords{};
    std::uint64_t packetBytes{};
    std::uint64_t uploadBytes{};

    [[nodiscard]] friend constexpr bool operator==(const UiDrawCounters& lhs,
                                                   const UiDrawCounters& rhs) noexcept = default;
};

struct UiHighWaterMarks final
{
    std::uint64_t paintCommands{};
    std::uint64_t paintCommandPayloadBytes{};
    std::uint64_t clipDepth{};
    std::uint64_t compilerScratchBytes{};
    std::uint64_t compiledVertices{};
    std::uint64_t compiledIndices{};
    std::uint64_t drawRecords{};
    std::uint64_t packetBytes{};
    std::uint64_t uploadBytes{};

    [[nodiscard]] friend constexpr bool operator==(const UiHighWaterMarks& lhs,
                                                   const UiHighWaterMarks& rhs) noexcept = default;
};
} // namespace pond::ui
