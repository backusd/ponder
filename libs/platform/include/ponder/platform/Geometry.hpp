#pragma once

#include <ponder/core/Numbers.hpp>

#include <compare>
#include <cstdint>
#include <format>
#include <ostream>
#include <string>

namespace ponder::platform
{
struct ScreenPosition final
{
    std::int32_t x{};
    std::int32_t y{};

    [[nodiscard]] friend constexpr auto operator<=>(const ScreenPosition& lhs, const ScreenPosition& rhs) noexcept = default;
};

struct ScreenExtent final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr auto operator<=>(const ScreenExtent& lhs, const ScreenExtent& rhs) noexcept = default;
};

struct ScreenRectangle final
{
    ScreenPosition position{};
    ScreenExtent extent{};

    [[nodiscard]] friend constexpr auto operator<=>(const ScreenRectangle& lhs, const ScreenRectangle& rhs) noexcept = default;
};

struct LogicalPoint final
{
    float x{};
    float y{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalPoint& lhs, const LogicalPoint& rhs) noexcept = default;
};

struct LogicalExtent final
{
    float width{};
    float height{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalExtent& lhs, const LogicalExtent& rhs) noexcept = default;
};

struct LogicalRectangle final
{
    LogicalPoint origin{};
    LogicalExtent extent{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalRectangle& lhs, const LogicalRectangle& rhs) noexcept = default;
};

struct LogicalSize final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr auto operator<=>(const LogicalSize& lhs, const LogicalSize& rhs) noexcept = default;
};

struct PixelSize final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr auto operator<=>(const PixelSize& lhs, const PixelSize& rhs) noexcept = default;
};

[[nodiscard]] constexpr bool IsValid(LogicalPoint point) noexcept
{
    return ponder::core::IsFinite(point.x) && ponder::core::IsFinite(point.y);
}

[[nodiscard]] constexpr bool IsValid(LogicalExtent extent) noexcept
{
    return ponder::core::IsFinite(extent.width) && ponder::core::IsFinite(extent.height) && extent.width >= 0.0F && extent.height >= 0.0F;
}

[[nodiscard]] constexpr bool IsValid(LogicalRectangle rectangle) noexcept
{
    return IsValid(rectangle.origin) && IsValid(rectangle.extent);
}

inline std::ostream& operator<<(std::ostream& output, ScreenPosition position)
{
    return output << '(' << position.x << ", " << position.y << ')';
}

inline std::ostream& operator<<(std::ostream& output, ScreenExtent extent)
{
    return output << extent.width << 'x' << extent.height;
}

inline std::ostream& operator<<(std::ostream& output, ScreenRectangle rectangle)
{
    return output << rectangle.position << " / " << rectangle.extent;
}

inline std::ostream& operator<<(std::ostream& output, LogicalPoint point)
{
    return output << '(' << point.x << ", " << point.y << ')';
}

inline std::ostream& operator<<(std::ostream& output, LogicalExtent extent)
{
    return output << extent.width << 'x' << extent.height;
}

inline std::ostream& operator<<(std::ostream& output, LogicalRectangle rectangle)
{
    return output << rectangle.origin << " / " << rectangle.extent;
}

inline std::ostream& operator<<(std::ostream& output, LogicalSize size)
{
    return output << size.width << 'x' << size.height;
}

inline std::ostream& operator<<(std::ostream& output, PixelSize size)
{
    return output << size.width << 'x' << size.height;
}
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::ScreenPosition> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::ScreenPosition position, FormatContext& context) const
    {
        return formatter<string>::format(std::format("({}, {})", position.x, position.y), context);
    }
};

template <>
struct formatter<ponder::platform::ScreenExtent> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::ScreenExtent extent, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", extent.width, extent.height), context);
    }
};

template <>
struct formatter<ponder::platform::ScreenRectangle> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::ScreenRectangle rectangle, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{} / {}", rectangle.position, rectangle.extent), context);
    }
};

template <>
struct formatter<ponder::platform::LogicalPoint> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::LogicalPoint point, FormatContext& context) const
    {
        return formatter<string>::format(std::format("({}, {})", point.x, point.y), context);
    }
};

template <>
struct formatter<ponder::platform::LogicalExtent> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::LogicalExtent extent, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", extent.width, extent.height), context);
    }
};

template <>
struct formatter<ponder::platform::LogicalRectangle> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::LogicalRectangle rectangle, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{} / {}", rectangle.origin, rectangle.extent), context);
    }
};

template <>
struct formatter<ponder::platform::LogicalSize> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::LogicalSize size, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", size.width, size.height), context);
    }
};

template <>
struct formatter<ponder::platform::PixelSize> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::PixelSize size, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", size.width, size.height), context);
    }
};
} // namespace std
