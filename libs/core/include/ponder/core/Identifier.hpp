#pragma once

#include <ponder/core/Hash.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace pond::core
{
template <typename Tag>
class Identifier final
{
public:
    using ValueType = std::uint64_t;

    constexpr Identifier() noexcept = default;
    explicit constexpr Identifier(ValueType value) noexcept : m_value{value} {}

    [[nodiscard]] static constexpr Identifier Invalid() noexcept
    {
        return Identifier{};
    }

    [[nodiscard]] constexpr ValueType GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return m_value != 0;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const Identifier& lhs,
                                                    const Identifier& rhs) noexcept = default;

private:
    ValueType m_value{};
};
} // namespace pond::core

namespace std
{
template <typename Tag>
struct hash<pond::core::Identifier<Tag>>
{
    [[nodiscard]] constexpr std::size_t operator()(pond::core::Identifier<Tag> id) const noexcept
    {
        return pond::core::HashIdentifierValue(id.GetValue());
    }
};
} // namespace std
