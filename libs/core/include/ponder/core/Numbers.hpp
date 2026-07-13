#pragma once

#include <ponder/core/Result.hpp>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>
#include <optional>
#include <type_traits>

namespace pond::core
{
inline constexpr float kPi{0x1.921fb6p+1F};
inline constexpr float kTwoPi{0x1.921fb6p+2F};
inline constexpr float kHalfPi{0x1.921fb6p+0F};

namespace detail
{
[[nodiscard]] constexpr double AbsoluteValue(double value) noexcept
{
    return value < 0.0 ? -value : value;
}

[[nodiscard]] constexpr float NarrowToFloat(double value) noexcept
{
    constexpr double kLowest = static_cast<double>(std::numeric_limits<float>::lowest());
    constexpr double kHighest = static_cast<double>(std::numeric_limits<float>::max());

    if (value != value) [[unlikely]]
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    if (value < kLowest) [[unlikely]]
    {
        return -std::numeric_limits<float>::infinity();
    }

    if (value > kHighest) [[unlikely]]
    {
        return std::numeric_limits<float>::infinity();
    }

    return static_cast<float>(value);
}

template <typename T>
concept NonBoolIntegral =
    std::integral<std::remove_cvref_t<T>> && !std::same_as<std::remove_cvref_t<T>, bool>;

template <typename T>
concept RoundableIntegral =
    NonBoolIntegral<T> &&
    std::numeric_limits<std::remove_cvref_t<T>>::digits <= std::numeric_limits<long double>::digits;

template <typename T>
concept FiniteNumber = NonBoolIntegral<T> || std::floating_point<std::remove_cvref_t<T>>;

inline constexpr ErrorCode kNonFiniteNumberErrorCode{ErrorCategory::InvalidArgument, 0x0001'0001};
inline constexpr ErrorCode kInvalidNumberArgumentErrorCode{ErrorCategory::InvalidArgument,
                                                           0x0001'0002};
} // namespace detail

template <typename T>
    requires detail::FiniteNumber<T>
[[nodiscard]] constexpr bool IsFinite(T value) noexcept
{
    using Value = std::remove_cvref_t<T>;
    constexpr Value kLowest = std::numeric_limits<Value>::lowest();
    constexpr Value kHighest = std::numeric_limits<Value>::max();
    return value >= kLowest && value <= kHighest;
}

template <typename Target, typename Source>
    requires detail::RoundableIntegral<Target> && std::floating_point<std::remove_cvref_t<Source>>
[[nodiscard]] std::optional<std::remove_cvref_t<Target>> RoundToInteger(Source value) noexcept
{
    using TargetValue = std::remove_cvref_t<Target>;
    if (!IsFinite(value)) [[unlikely]]
    {
        return std::nullopt;
    }

    const long double rounded = std::round(static_cast<long double>(value));
    constexpr long double kLowest =
        static_cast<long double>(std::numeric_limits<TargetValue>::lowest());
    constexpr long double kHighest =
        static_cast<long double>(std::numeric_limits<TargetValue>::max());
    if (rounded < kLowest || rounded > kHighest) [[unlikely]]
    {
        return std::nullopt;
    }

    return static_cast<TargetValue>(rounded);
}

class Tolerance final
{
public:
    [[nodiscard]] static constexpr Result<Tolerance> Create(float absoluteTolerance,
                                                            float relativeTolerance)
    {
        if (!IsFinite(absoluteTolerance) || !IsFinite(relativeTolerance)) [[unlikely]]
        {
            return Result<Tolerance>::FromError(
                Error{detail::kNonFiniteNumberErrorCode, "Tolerance values must be finite."});
        }

        if (absoluteTolerance < 0.0F || relativeTolerance < 0.0F) [[unlikely]]
        {
            return Result<Tolerance>::FromError(Error{detail::kInvalidNumberArgumentErrorCode,
                                                      "Tolerance values must be non-negative."});
        }

        return Tolerance{absoluteTolerance, relativeTolerance};
    }

    [[nodiscard]] constexpr float GetAbsolute() const noexcept
    {
        return m_absolute;
    }

    [[nodiscard]] constexpr float GetRelative() const noexcept
    {
        return m_relative;
    }

    [[nodiscard]] friend constexpr bool operator==(const Tolerance& lhs,
                                                   const Tolerance& rhs) noexcept = default;

private:
    explicit constexpr Tolerance(float absoluteTolerance, float relativeTolerance) noexcept
        : m_absolute(absoluteTolerance), m_relative(relativeTolerance)
    {
    }

    float m_absolute{0.0F};
    float m_relative{0.0F};
};

[[nodiscard]] constexpr float Clamp(float value, float minimum, float maximum) noexcept
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

[[nodiscard]] constexpr float Lerp(float start, float end, float amount) noexcept
{
    if (amount == 0.0F)
    {
        return start;
    }

    if (amount == 1.0F)
    {
        return end;
    }

    const double startWide = static_cast<double>(start);
    const double endWide = static_cast<double>(end);
    const double amountWide = static_cast<double>(amount);
    if (!IsFinite(start) || !IsFinite(end)) [[unlikely]]
    {
        return detail::NarrowToFloat((1.0 - amountWide) * startWide + amountWide * endWide);
    }

    const double interpolated = startWide + (endWide - startWide) * amountWide;
    return detail::NarrowToFloat(interpolated);
}

[[nodiscard]] constexpr bool IsNear(float lhs, float rhs, Tolerance tolerance) noexcept
{
    if (!IsFinite(lhs) || !IsFinite(rhs)) [[unlikely]]
    {
        return false;
    }

    const double lhsWide = static_cast<double>(lhs);
    const double rhsWide = static_cast<double>(rhs);
    const double difference = detail::AbsoluteValue(lhsWide - rhsWide);
    if (difference <= static_cast<double>(tolerance.GetAbsolute()))
    {
        return true;
    }

    const double largestInput =
        std::max(detail::AbsoluteValue(lhsWide), detail::AbsoluteValue(rhsWide));
    return difference <= largestInput * static_cast<double>(tolerance.GetRelative());
}
} // namespace pond::core
