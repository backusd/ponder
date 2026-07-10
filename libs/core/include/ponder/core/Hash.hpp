#pragma once

#include <cstddef>
#include <cstdint>

namespace pond::core
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
} // namespace pond::core