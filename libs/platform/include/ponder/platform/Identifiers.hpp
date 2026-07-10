#pragma once

#include <ponder/core/Hash.hpp>

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

class DialogRequestId final
{
public:
    using ValueType = std::uint64_t;

    constexpr DialogRequestId() noexcept = default;
    explicit constexpr DialogRequestId(ValueType value) noexcept : m_value(value) {}

    [[nodiscard]] static constexpr DialogRequestId Invalid() noexcept
    {
        return DialogRequestId{};
    }

    [[nodiscard]] constexpr ValueType GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return m_value != 0;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const DialogRequestId& lhs,
                                                    const DialogRequestId& rhs) noexcept = default;

private:
    ValueType m_value{};
};

} // namespace pond::platform

namespace std
{
template <>
struct hash<pond::platform::WindowId>
{
    [[nodiscard]] constexpr std::size_t operator()(pond::platform::WindowId id) const noexcept
    {
        return pond::core::HashIdentifierValue(id.GetValue());
    }
};

template <>
struct hash<pond::platform::DisplayId>
{
    [[nodiscard]] constexpr std::size_t operator()(pond::platform::DisplayId id) const noexcept
    {
        return pond::core::HashIdentifierValue(id.GetValue());
    }
};

template <>
struct hash<pond::platform::DialogRequestId>
{
    [[nodiscard]] constexpr std::size_t operator()(
        pond::platform::DialogRequestId id) const noexcept
    {
        return pond::core::HashIdentifierValue(id.GetValue());
    }
};
} // namespace std
