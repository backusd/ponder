#pragma once

#include <chrono>
#include <compare>

namespace pond::platform
{
class PlatformTimestamp final
{
public:
    using Duration = std::chrono::nanoseconds;

    constexpr PlatformTimestamp() noexcept = default;
    explicit constexpr PlatformTimestamp(Duration timeSinceEpoch) noexcept
        : m_timeSinceEpoch(timeSinceEpoch)
    {
    }

    [[nodiscard]] constexpr Duration GetTimeSinceEpoch() const noexcept
    {
        return m_timeSinceEpoch;
    }

    [[nodiscard]] friend constexpr auto operator<=>(
        const PlatformTimestamp& lhs, const PlatformTimestamp& rhs) noexcept = default;

    [[nodiscard]] friend constexpr Duration operator-(PlatformTimestamp lhs,
                                                      PlatformTimestamp rhs) noexcept
    {
        return lhs.m_timeSinceEpoch - rhs.m_timeSinceEpoch;
    }

private:
    Duration m_timeSinceEpoch{};
};
} // namespace pond::platform
