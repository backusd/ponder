#pragma once

#include <ponder/core/Numbers.hpp>
#include <ponder/ui/Error.hpp>
#include <ponder/ui/Geometry.hpp>

#include <cstdint>

namespace pond::ui
{
class UiTargetId final
{
public:
    constexpr UiTargetId() noexcept = default;

    explicit constexpr UiTargetId(std::uint64_t value) noexcept : m_value{value} {}

    [[nodiscard]] constexpr std::uint64_t GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const UiTargetId& lhs,
                                                    const UiTargetId& rhs) noexcept = default;

private:
    std::uint64_t m_value{};
};

class UiTargetRevision final
{
public:
    constexpr UiTargetRevision() noexcept = default;

    explicit constexpr UiTargetRevision(std::uint64_t value) noexcept : m_value{value} {}

    [[nodiscard]] constexpr std::uint64_t GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const UiTargetRevision& lhs,
                                                    const UiTargetRevision& rhs) noexcept = default;

private:
    std::uint64_t m_value{};
};

class UiMetricsRevision final
{
public:
    constexpr UiMetricsRevision() noexcept = default;

    explicit constexpr UiMetricsRevision(std::uint64_t value) noexcept : m_value{value} {}

    [[nodiscard]] constexpr std::uint64_t GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr auto operator<=>(
        const UiMetricsRevision& lhs, const UiMetricsRevision& rhs) noexcept = default;

private:
    std::uint64_t m_value{};
};

struct LogicalToFramebufferScale final
{
    float x{};
    float y{};

    [[nodiscard]] friend constexpr bool operator==(const LogicalToFramebufferScale& lhs,
                                                   const LogicalToFramebufferScale& rhs) noexcept =
        default;
};

[[nodiscard]] constexpr float DeriveLogicalToFramebufferScale(
    float logicalExtent, std::uint32_t framebufferExtent) noexcept
{
    if (logicalExtent <= 0.0F || framebufferExtent == 0U)
    {
        return 0.0F;
    }

    return static_cast<float>(framebufferExtent) / logicalExtent;
}

class UiTargetMetrics final
{
public:
    constexpr UiTargetMetrics() noexcept = default;

    [[nodiscard]] constexpr UiTargetId GetTargetId() const noexcept
    {
        return m_targetId;
    }

    [[nodiscard]] constexpr UiTargetRevision GetTargetRevision() const noexcept
    {
        return m_targetRevision;
    }

    [[nodiscard]] constexpr UiMetricsRevision GetMetricsRevision() const noexcept
    {
        return m_metricsRevision;
    }

    [[nodiscard]] constexpr LogicalSize GetLogicalSize() const noexcept
    {
        return m_logicalSize;
    }

    [[nodiscard]] constexpr FramebufferPixelSize GetFramebufferPixelSize() const noexcept
    {
        return m_framebufferPixelSize;
    }

    [[nodiscard]] constexpr LogicalToFramebufferScale GetLogicalToFramebufferScale() const noexcept
    {
        return m_logicalToFramebufferScale;
    }

    [[nodiscard]] friend constexpr bool operator==(const UiTargetMetrics& lhs,
                                                   const UiTargetMetrics& rhs) noexcept = default;

private:
    friend core::Result<UiTargetMetrics> MakeUiTargetMetrics(
        UiTargetId targetId, UiTargetRevision targetRevision, UiMetricsRevision metricsRevision,
        LogicalSize logicalSize, FramebufferPixelSize framebufferPixelSize);

    constexpr UiTargetMetrics(UiTargetId targetId, UiTargetRevision targetRevision,
                              UiMetricsRevision metricsRevision, LogicalSize logicalSize,
                              FramebufferPixelSize framebufferPixelSize,
                              LogicalToFramebufferScale logicalToFramebufferScale) noexcept
        : m_targetId{targetId}, m_targetRevision{targetRevision},
          m_metricsRevision{metricsRevision}, m_logicalSize{logicalSize},
          m_framebufferPixelSize{framebufferPixelSize},
          m_logicalToFramebufferScale{logicalToFramebufferScale}
    {
    }

    UiTargetId m_targetId{};
    UiTargetRevision m_targetRevision{};
    UiMetricsRevision m_metricsRevision{};
    LogicalSize m_logicalSize{};
    FramebufferPixelSize m_framebufferPixelSize{};
    LogicalToFramebufferScale m_logicalToFramebufferScale{};
};

[[nodiscard]] constexpr bool IsValid(UiTargetId targetId) noexcept
{
    return targetId.GetValue() != 0U;
}

[[nodiscard]] constexpr bool IsValid(UiTargetRevision revision) noexcept
{
    return revision.GetValue() != 0U;
}

[[nodiscard]] constexpr bool IsValid(UiMetricsRevision revision) noexcept
{
    return revision.GetValue() != 0U;
}

[[nodiscard]] constexpr bool IsValid(LogicalToFramebufferScale scale) noexcept
{
    return core::IsFinite(scale.x) && core::IsFinite(scale.y) && scale.x >= 0.0F && scale.y >= 0.0F;
}

[[nodiscard]] constexpr bool IsDrawable(const UiTargetMetrics& metrics) noexcept
{
    return HasPositiveArea(metrics.GetLogicalSize()) &&
           HasPositiveArea(metrics.GetFramebufferPixelSize());
}

[[nodiscard]] constexpr bool IsValid(const UiTargetMetrics& metrics) noexcept
{
    const LogicalSize logicalSize = metrics.GetLogicalSize();
    const FramebufferPixelSize framebufferPixelSize = metrics.GetFramebufferPixelSize();
    const LogicalToFramebufferScale scale = metrics.GetLogicalToFramebufferScale();
    const LogicalToFramebufferScale derivedScale{
        DeriveLogicalToFramebufferScale(logicalSize.width, framebufferPixelSize.width),
        DeriveLogicalToFramebufferScale(logicalSize.height, framebufferPixelSize.height)};

    return IsValid(metrics.GetTargetId()) && IsValid(metrics.GetTargetRevision()) &&
           IsValid(metrics.GetMetricsRevision()) && IsValid(logicalSize) && IsValid(scale) &&
           scale == derivedScale;
}

[[nodiscard]] inline core::Result<UiTargetMetrics> MakeUiTargetMetrics(
    UiTargetId targetId, UiTargetRevision targetRevision, UiMetricsRevision metricsRevision,
    LogicalSize logicalSize, FramebufferPixelSize framebufferPixelSize)
{
    const LogicalToFramebufferScale scale{
        DeriveLogicalToFramebufferScale(logicalSize.width, framebufferPixelSize.width),
        DeriveLogicalToFramebufferScale(logicalSize.height, framebufferPixelSize.height)};
    const UiTargetMetrics metrics{targetId,    targetRevision,       metricsRevision,
                                  logicalSize, framebufferPixelSize, scale};

    if (!IsValid(metrics))
    {
        return MakeUiFailure<UiTargetMetrics>(
            UiErrorCode::InvalidMetrics,
            "UI target metrics require valid identity, revisions, finite logical size, and "
            "finite extent-derived scale.");
    }

    return metrics;
}
} // namespace pond::ui