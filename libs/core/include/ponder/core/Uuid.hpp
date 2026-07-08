#pragma once

#include <ponder/core/Result.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace pond::core
{
class Uuid final
{
public:
    static constexpr std::size_t kByteCount{16};

    using Byte = std::uint8_t;
    using Bytes = std::array<Byte, kByteCount>;

    constexpr Uuid() noexcept = default;
    explicit constexpr Uuid(Bytes bytes) noexcept : m_bytes(bytes) {}

    [[nodiscard]] static constexpr Uuid Nil() noexcept
    {
        return Uuid{};
    }

    [[nodiscard]] constexpr const Bytes& GetBytes() const noexcept
    {
        return m_bytes;
    }

    [[nodiscard]] constexpr bool IsNil() const noexcept
    {
        return m_bytes == Bytes{};
    }

    [[nodiscard]] constexpr std::uint8_t GetVersion() const noexcept
    {
        return static_cast<std::uint8_t>((m_bytes[6] >> 4U) & 0x0FU);
    }

    [[nodiscard]] constexpr bool HasRfc4122Variant() const noexcept
    {
        return (m_bytes[8] & 0xC0U) == 0x80U;
    }

    [[nodiscard]] constexpr std::string ToString() const
    {
        std::string formatted(kCanonicalStringLength, '\0');
        std::size_t textIndex{0};

        for (std::size_t byteIndex = 0; byteIndex < m_bytes.size(); ++byteIndex)
        {
            if (byteIndex == 4 || byteIndex == 6 || byteIndex == 8 || byteIndex == 10)
            {
                formatted[textIndex] = '-';
                ++textIndex;
            }

            formatted[textIndex] = ToLowerHexDigit(static_cast<Byte>(m_bytes[byteIndex] >> 4U));
            ++textIndex;
            formatted[textIndex] = ToLowerHexDigit(m_bytes[byteIndex]);
            ++textIndex;
        }

        return formatted;
    }

    [[nodiscard]] friend constexpr auto operator<=>(const Uuid& lhs,
                                                    const Uuid& rhs) noexcept = default;

private:
    static constexpr std::size_t kCanonicalStringLength{36};

    [[nodiscard]] static constexpr char ToLowerHexDigit(Byte value) noexcept
    {
        constexpr std::string_view kDigits{"0123456789abcdef"};
        return kDigits[value & 0x0FU];
    }

    Bytes m_bytes{};
};

using UuidEntropySource = bool (*)(std::span<Uuid::Byte, Uuid::kByteCount> bytes) noexcept;

[[nodiscard]] Result<Uuid> ParseUuid(std::string_view text);

[[nodiscard]] constexpr Uuid MakeUuidV4(Uuid::Bytes randomBytes) noexcept
{
    randomBytes[6] = static_cast<Uuid::Byte>((randomBytes[6] & 0x0FU) | 0x40U);
    randomBytes[8] = static_cast<Uuid::Byte>((randomBytes[8] & 0x3FU) | 0x80U);

    return Uuid{randomBytes};
}

[[nodiscard]] Result<Uuid> GenerateUuidV4(UuidEntropySource entropySource);
[[nodiscard]] Result<Uuid> GenerateUuidV4();
} // namespace pond::core

namespace std
{
template <>
struct hash<pond::core::Uuid>
{
    [[nodiscard]] constexpr std::size_t operator()(const pond::core::Uuid& uuid) const noexcept
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

        for (pond::core::Uuid::Byte byte : uuid.GetBytes())
        {
            hashValue ^= static_cast<std::size_t>(byte);
            hashValue *= prime;
        }

        return hashValue;
    }
};
} // namespace std
