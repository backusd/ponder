#include <ponder/core/Uuid.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <string>
#include <string_view>

namespace pond::core
{
namespace
{
constexpr std::size_t kCanonicalUuidLength{36};
constexpr std::array<std::size_t, 16> kByteTextOffsets{0,  2,  4,  6,  9,  11, 14, 16,
                                                       19, 21, 24, 26, 28, 30, 32, 34};
constexpr ErrorCode kInvalidUuidFormat{ErrorCategory::Parse, 1};
constexpr ErrorCode kUuidEntropyFailure{ErrorCategory::Internal, 2};

[[nodiscard]] constexpr char ToLowerHexDigit(std::uint8_t value) noexcept
{
    constexpr std::string_view kDigits{"0123456789abcdef"};
    return kDigits[value & 0x0FU];
}

[[nodiscard]] constexpr int FromHexDigit(char character) noexcept
{
    if (character >= '0' && character <= '9')
    {
        return character - '0';
    }

    if (character >= 'a' && character <= 'f')
    {
        return character - 'a' + 10;
    }

    if (character >= 'A' && character <= 'F')
    {
        return character - 'A' + 10;
    }

    return -1;
}

[[nodiscard]] bool HasCanonicalHyphenPositions(std::string_view text) noexcept
{
    return text[8] == '-' && text[13] == '-' && text[18] == '-' && text[23] == '-';
}

[[nodiscard]] std::unexpected<Error> MakeInvalidUuidFormatError()
{
    return MakeUnexpected(kInvalidUuidFormat, "Invalid UUID string; expected canonical "
                                              "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx format.");
}

[[nodiscard]] bool FillRandomUuidBytes(std::span<Uuid::Byte, Uuid::kByteCount> bytes) noexcept
{
    try
    {
        std::random_device randomDevice;
        std::uniform_int_distribution<unsigned int> distribution{0U, 255U};

        for (Uuid::Byte& byte : bytes)
        {
            byte = static_cast<Uuid::Byte>(distribution(randomDevice));
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace

std::string Uuid::ToString() const
{
    std::string formatted(kCanonicalUuidLength, '\0');
    std::size_t textIndex{0};

    for (std::size_t byteIndex = 0; byteIndex < m_bytes.size(); ++byteIndex)
    {
        if (byteIndex == 4 || byteIndex == 6 || byteIndex == 8 || byteIndex == 10)
        {
            formatted[textIndex] = '-';
            ++textIndex;
        }

        formatted[textIndex] = ToLowerHexDigit(static_cast<std::uint8_t>(m_bytes[byteIndex] >> 4U));
        ++textIndex;
        formatted[textIndex] = ToLowerHexDigit(m_bytes[byteIndex]);
        ++textIndex;
    }

    return formatted;
}

Result<Uuid> ParseUuid(std::string_view text)
{
    if (text.size() != kCanonicalUuidLength || !HasCanonicalHyphenPositions(text))
    {
        return MakeInvalidUuidFormatError();
    }

    Uuid::Bytes bytes{};

    for (std::size_t byteIndex = 0; byteIndex < bytes.size(); ++byteIndex)
    {
        std::size_t textOffset = kByteTextOffsets[byteIndex];
        int highNibble = FromHexDigit(text[textOffset]);
        int lowNibble = FromHexDigit(text[textOffset + 1]);

        if (highNibble < 0 || lowNibble < 0)
        {
            return MakeInvalidUuidFormatError();
        }

        bytes[byteIndex] = static_cast<Uuid::Byte>((highNibble << 4) | lowNibble);
    }

    return Uuid{bytes};
}

Uuid MakeUuidV4(Uuid::Bytes randomBytes) noexcept
{
    randomBytes[6] = static_cast<Uuid::Byte>((randomBytes[6] & 0x0FU) | 0x40U);
    randomBytes[8] = static_cast<Uuid::Byte>((randomBytes[8] & 0x3FU) | 0x80U);

    return Uuid{randomBytes};
}

Result<Uuid> GenerateUuidV4(UuidEntropySource entropySource)
{
    if (entropySource == nullptr)
    {
        return MakeUnexpected(kUuidEntropyFailure, "UUID entropy source is null.");
    }

    Uuid::Bytes bytes{};
    if (!entropySource(std::span<Uuid::Byte, Uuid::kByteCount>{bytes}))
    {
        return MakeUnexpected(kUuidEntropyFailure, "UUID entropy source failed.");
    }

    return MakeUuidV4(bytes);
}

Result<Uuid> GenerateUuidV4()
{
    return GenerateUuidV4(FillRandomUuidBytes);
}
} // namespace pond::core