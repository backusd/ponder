#pragma once

#include <ponder/core/Numbers.hpp>
#include <ponder/ui/Error.hpp>

#include <compare>
#include <cstdint>

namespace pond::ui
{
struct LogicalPoint final
{
    float x{};
    float y{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalPoint& lhs,
                                                   const LogicalPoint& rhs) noexcept = default;
};

struct LogicalSize final
{
    float width{};
    float height{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalSize& lhs,
                                                   const LogicalSize& rhs) noexcept = default;
};

struct LogicalRect final
{
    LogicalPoint origin{};
    LogicalSize size{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalRect& lhs,
                                                   const LogicalRect& rhs) noexcept = default;
};

struct FramebufferPixelSize final
{
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] friend constexpr auto operator<=>(
        const FramebufferPixelSize& lhs, const FramebufferPixelSize& rhs) noexcept = default;
};

[[nodiscard]] constexpr bool IsValid(LogicalPoint point) noexcept
{
    return core::IsFinite(point.x) && core::IsFinite(point.y);
}

[[nodiscard]] constexpr bool IsValid(LogicalSize size) noexcept
{
    return core::IsFinite(size.width) && core::IsFinite(size.height) && size.width >= 0.0F &&
           size.height >= 0.0F;
}

[[nodiscard]] constexpr float GetLeft(LogicalRect rectangle) noexcept
{
    return rectangle.origin.x;
}

[[nodiscard]] constexpr float GetTop(LogicalRect rectangle) noexcept
{
    return rectangle.origin.y;
}

[[nodiscard]] constexpr float GetRight(LogicalRect rectangle) noexcept
{
    return rectangle.origin.x + rectangle.size.width;
}

[[nodiscard]] constexpr float GetBottom(LogicalRect rectangle) noexcept
{
    return rectangle.origin.y + rectangle.size.height;
}

[[nodiscard]] constexpr bool IsValid(LogicalRect rectangle) noexcept
{
    if (!IsValid(rectangle.origin) || !IsValid(rectangle.size))
    {
        return false;
    }

    const float right = GetRight(rectangle);
    const float bottom = GetBottom(rectangle);
    return core::IsFinite(right) && core::IsFinite(bottom) &&
           (rectangle.size.width == 0.0F || right > rectangle.origin.x) &&
           (rectangle.size.height == 0.0F || bottom > rectangle.origin.y);
}

[[nodiscard]] constexpr bool IsEmpty(LogicalSize size) noexcept
{
    return IsValid(size) && (size.width == 0.0F || size.height == 0.0F);
}

[[nodiscard]] constexpr bool IsEmpty(LogicalRect rectangle) noexcept
{
    return IsValid(rectangle) && IsEmpty(rectangle.size);
}

[[nodiscard]] constexpr bool HasPositiveArea(LogicalSize size) noexcept
{
    return IsValid(size) && size.width > 0.0F && size.height > 0.0F;
}

[[nodiscard]] constexpr bool HasPositiveArea(LogicalRect rectangle) noexcept
{
    return IsValid(rectangle) && HasPositiveArea(rectangle.size);
}

[[nodiscard]] constexpr bool HasPositiveArea(FramebufferPixelSize size) noexcept
{
    return size.width > 0U && size.height > 0U;
}

[[nodiscard]] inline core::Result<LogicalRect> MakeLogicalRectFromEdges(float left, float top,
                                                                        float right, float bottom)
{
    if (!core::IsFinite(left) || !core::IsFinite(top) || !core::IsFinite(right) ||
        !core::IsFinite(bottom))
    {
        return MakeUiFailure<LogicalRect>(UiErrorCode::InvalidPaintValue,
                                          "UI logical rectangle edges must be finite.");
    }

    if (right < left || bottom < top)
    {
        return MakeUiFailure<LogicalRect>(
            UiErrorCode::InvalidPaintValue,
            "UI logical rectangle edges must describe non-reversed half-open bounds.");
    }

    const float width = right - left;
    const float height = bottom - top;
    if (!core::IsFinite(width) || !core::IsFinite(height))
    {
        return MakeUiFailure<LogicalRect>(
            UiErrorCode::InvalidPaintValue,
            "UI logical rectangle extents must remain finite after subtracting their edges.");
    }

    return LogicalRect{LogicalPoint{left, top}, LogicalSize{width, height}};
}
} // namespace pond::ui
