#pragma once

#include <concepts>
#include <limits>
#include <type_traits>

namespace pond::core
{
namespace detail
{
template <typename T>
concept FiniteNumber =
    (std::integral<std::remove_cvref_t<T>> && !std::same_as<std::remove_cvref_t<T>, bool>) ||
    std::floating_point<std::remove_cvref_t<T>>;
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
} // namespace pond::core