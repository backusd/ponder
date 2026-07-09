#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace pond::platform
{
class WindowId final
{
public:
    using ValueType = std::uint64_t;

    constexpr WindowId() noexcept = default;
    explicit constexpr WindowId(ValueType value) noexcept : m_value(value) {}

    [[nodiscard]] static constexpr WindowId Invalid() noexcept
    {
        return WindowId{};
    }

    [[nodiscard]] constexpr ValueType GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return m_value != 0;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const WindowId& lhs,
                                                    const WindowId& rhs) noexcept = default;

private:
    ValueType m_value{};
};

class DisplayId final
{
public:
    using ValueType = std::uint64_t;

    constexpr DisplayId() noexcept = default;
    explicit constexpr DisplayId(ValueType value) noexcept : m_value(value) {}

    [[nodiscard]] static constexpr DisplayId Invalid() noexcept
    {
        return DisplayId{};
    }

    [[nodiscard]] constexpr ValueType GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return m_value != 0;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const DisplayId& lhs,
                                                    const DisplayId& rhs) noexcept = default;

private:
    ValueType m_value{};
};

namespace detail
{
[[nodiscard]] constexpr std::size_t HashIdentifierValue(std::uint64_t value) noexcept
{
    std::size_t hashValue{};
    std::size_t prime{};

    if constexpr (sizeof(std::size_t) == 8)
    {
        hashValue = 1469598103934665603ULL;
        prime = 1099511628211ULL;
    }
    else
    {
        hashValue = 2166136261U;
        prime = 16777619U;
    }

    for (std::size_t byteIndex = 0; byteIndex < sizeof(value); ++byteIndex)
    {
        const auto shift = static_cast<unsigned int>(byteIndex * 8U);
        const auto byte = static_cast<std::uint8_t>(value >> shift);
        hashValue ^= static_cast<std::size_t>(byte);
        hashValue *= prime;
    }

    return hashValue;
}
} // namespace detail
} // namespace pond::platform

namespace std
{
template <>
struct hash<pond::platform::WindowId>
{
    [[nodiscard]] constexpr std::size_t operator()(
        pond::platform::WindowId id) const noexcept
    {
        return pond::platform::detail::HashIdentifierValue(id.GetValue());
    }
};

template <>
struct hash<pond::platform::DisplayId>
{
    [[nodiscard]] constexpr std::size_t operator()(
        pond::platform::DisplayId id) const noexcept
    {
        return pond::platform::detail::HashIdentifierValue(id.GetValue());
    }
};
} // namespace std
