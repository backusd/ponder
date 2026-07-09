#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace pond::platform
{
struct ScreenPosition final
{
    std::int32_t x{};
    std::int32_t y{};

    [[nodiscard]] friend constexpr auto operator<=>(const ScreenPosition& lhs,
                                                    const ScreenPosition& rhs) noexcept = default;
};

struct ScreenExtent final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr auto operator<=>(const ScreenExtent& lhs,
                                                    const ScreenExtent& rhs) noexcept = default;
};

struct ScreenRectangle final
{
    ScreenPosition position{};
    ScreenExtent extent{};

    [[nodiscard]] friend constexpr auto operator<=>(const ScreenRectangle& lhs,
                                                    const ScreenRectangle& rhs) noexcept = default;
};

struct LogicalPoint final
{
    float x{};
    float y{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalPoint& lhs,
                                                   const LogicalPoint& rhs) noexcept = default;
};

struct LogicalExtent final
{
    float width{};
    float height{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalExtent& lhs,
                                                   const LogicalExtent& rhs) noexcept = default;
};

struct LogicalRectangle final
{
    LogicalPoint origin{};
    LogicalExtent extent{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalRectangle& lhs,
                                                   const LogicalRectangle& rhs) noexcept = default;
};

struct LogicalSize final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr auto operator<=>(const LogicalSize& lhs,
                                                    const LogicalSize& rhs) noexcept = default;
};

struct PixelSize final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr auto operator<=>(const PixelSize& lhs,
                                                    const PixelSize& rhs) noexcept = default;
};

namespace detail
{
[[nodiscard]] constexpr bool IsFinite(float value) noexcept
{
    constexpr float kLowest = std::numeric_limits<float>::lowest();
    constexpr float kHighest = std::numeric_limits<float>::max();
    return value >= kLowest && value <= kHighest;
}
} // namespace detail

[[nodiscard]] constexpr bool IsValid(LogicalPoint point) noexcept
{
    return detail::IsFinite(point.x) && detail::IsFinite(point.y);
}

[[nodiscard]] constexpr bool IsValid(LogicalExtent extent) noexcept
{
    return detail::IsFinite(extent.width) && detail::IsFinite(extent.height) &&
           extent.width >= 0.0F && extent.height >= 0.0F;
}

[[nodiscard]] constexpr bool IsValid(LogicalRectangle rectangle) noexcept
{
    return IsValid(rectangle.origin) && IsValid(rectangle.extent);
}
} // namespace pond::platform
