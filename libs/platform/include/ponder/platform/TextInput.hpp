#pragma once

#include <ponder/platform/Geometry.hpp>

#include <compare>
#include <cstdint>

namespace pond::platform
{
struct TextCompositionRange final
{
    // Composition offsets count Unicode characters, not UTF-8 bytes.
    std::uint32_t start{};
    std::uint32_t length{};

    [[nodiscard]] friend constexpr auto operator<=>(
        const TextCompositionRange& lhs,
        const TextCompositionRange& rhs) noexcept = default;
};

struct TextInputArea final
{
    // The cursor offset is horizontal and relative to rectangle.origin.x.
    LogicalRectangle rectangle{};
    float cursorOffset{};

    [[nodiscard]] friend constexpr bool operator==(const TextInputArea& lhs,
                                                   const TextInputArea& rhs) noexcept = default;
};

[[nodiscard]] constexpr bool IsValid(TextInputArea area) noexcept
{
    return IsValid(area.rectangle) && detail::IsFinite(area.cursorOffset);
}
} // namespace pond::platform
