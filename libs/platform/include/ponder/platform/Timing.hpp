#pragma once

#include <chrono>
#include <compare>
#include <format>
#include <ostream>
#include <string>

namespace pond::platform
{
class Duration final
{
public:
    constexpr Duration() noexcept = default;

    template <typename Rep, typename Period>
    constexpr Duration(std::chrono::duration<Rep, Period> duration) noexcept
        : m_nanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(duration))
    {
    }

    [[nodiscard]] constexpr std::chrono::nanoseconds GetNanoseconds() const noexcept
    {
        return m_nanoseconds;
    }

    [[nodiscard]] constexpr std::chrono::nanoseconds::rep count() const noexcept
    {
        return m_nanoseconds.count();
    }

    [[nodiscard]] friend constexpr auto operator<=>(const Duration& lhs,
                                                    const Duration& rhs) noexcept = default;

private:
    std::chrono::nanoseconds m_nanoseconds{};
};

class Timestamp final
{
public:
    using Duration = ::pond::platform::Duration;

    constexpr Timestamp() noexcept = default;
    explicit constexpr Timestamp(Duration timeSinceEpoch) noexcept
        : m_timeSinceEpoch(timeSinceEpoch)
    {
    }

    template <typename Rep, typename Period>
    explicit constexpr Timestamp(std::chrono::duration<Rep, Period> timeSinceEpoch) noexcept
        : Timestamp(Duration{timeSinceEpoch})
    {
    }

    [[nodiscard]] constexpr Duration GetTimeSinceEpoch() const noexcept
    {
        return m_timeSinceEpoch;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const Timestamp& lhs,
                                                    const Timestamp& rhs) noexcept = default;

    [[nodiscard]] friend constexpr Duration operator-(Timestamp lhs, Timestamp rhs) noexcept
    {
        return Duration{lhs.m_timeSinceEpoch.GetNanoseconds() -
                        rhs.m_timeSinceEpoch.GetNanoseconds()};
    }

private:
    Duration m_timeSinceEpoch{};
};

inline std::ostream& operator<<(std::ostream& output, Duration duration)
{
    return output << duration.GetNanoseconds().count() << " ns";
}

inline std::ostream& operator<<(std::ostream& output, Timestamp timestamp)
{
    return output << timestamp.GetTimeSinceEpoch();
}
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::Duration> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::Duration duration, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("{} ns", duration.GetNanoseconds().count()), context);
    }
};

template <>
struct formatter<pond::platform::Timestamp> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::Timestamp timestamp, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}", timestamp.GetTimeSinceEpoch()),
                                         context);
    }
};
} // namespace std