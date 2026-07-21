#pragma once

#include <ponder/core/Numbers.hpp>
#include <ponder/platform/Geometry.hpp>

#include <compare>
#include <cstdint>
#include <format>
#include <ostream>
#include <string>

namespace pond::platform
{
struct TextCompositionRange final
{
    // Composition offsets count Unicode characters, not UTF-8 bytes.
    std::uint32_t start{};
    std::uint32_t length{};

    [[nodiscard]] friend constexpr auto operator<=>(
        const TextCompositionRange& lhs, const TextCompositionRange& rhs) noexcept = default;
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
    return IsValid(area.rectangle) && core::IsFinite(area.cursorOffset);
}
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::TextCompositionRange> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::TextCompositionRange range, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("start={}, length={}", range.start, range.length), context);
    }
};

template <>
struct formatter<pond::platform::TextInputArea> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::TextInputArea& area, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("{}, cursorOffset={}", area.rectangle, area.cursorOffset), context);
    }
};
} // namespace std

namespace pond::platform
{
inline std::ostream& operator<<(std::ostream& output, TextCompositionRange range)
{
    return output << std::format("{}", range);
}

inline std::ostream& operator<<(std::ostream& output, const TextInputArea& area)
{
    return output << std::format("{}", area);
}
} // namespace pond::platform