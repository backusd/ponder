#pragma once

#include <ponder/core/Hash.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace ponder::core
{
template <typename Tag>
class Identifier final
{
public:
    using ValueType = std::uint64_t;

    constexpr Identifier() noexcept = default;
    explicit constexpr Identifier(ValueType value) noexcept :
        m_value{value}
    {
    }

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

    [[nodiscard]] friend constexpr auto operator<=>(const Identifier& lhs, const Identifier& rhs) noexcept = default;

private:
    ValueType m_value{};
};
} // namespace ponder::core

namespace std
{
template <typename Tag>
struct hash<ponder::core::Identifier<Tag>>
{
    [[nodiscard]] constexpr std::size_t operator()(ponder::core::Identifier<Tag> id) const noexcept
    {
        return ponder::core::HashIdentifierValue(id.GetValue());
    }
};
} // namespace std
