#include <ponder/core/String.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace pond::core
{
namespace
{
static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4,
              "String conversion supports 16-bit or 32-bit wchar_t only.");

constexpr ErrorCode kInvalidUtf8{ErrorCategory::Parse, 3};
constexpr ErrorCode kInvalidWideString{ErrorCategory::Parse, 4};
constexpr std::uint32_t kMaxUnicodeCodePoint{0x10FFFFU};
constexpr std::uint32_t kHighSurrogateFirst{0xD800U};
constexpr std::uint32_t kHighSurrogateLast{0xDBFFU};
constexpr std::uint32_t kLowSurrogateFirst{0xDC00U};
constexpr std::uint32_t kLowSurrogateLast{0xDFFFU};
constexpr std::uint32_t kSurrogateBase{0x10000U};
constexpr std::uint32_t kSurrogatePayloadBits{10U};
constexpr std::uint32_t kSurrogatePayloadMask{0x3FFU};

[[nodiscard]] constexpr std::uint8_t ToByte(char value) noexcept
{
    return static_cast<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint32_t ToWideUnit(wchar_t value) noexcept
{
    using UnsignedWchar = std::make_unsigned_t<wchar_t>;
    return static_cast<std::uint32_t>(static_cast<UnsignedWchar>(value));
}

[[nodiscard]] constexpr bool IsContinuationByte(std::uint8_t byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

[[nodiscard]] constexpr bool IsHighSurrogate(std::uint32_t value) noexcept
{
    return value >= kHighSurrogateFirst && value <= kHighSurrogateLast;
}

[[nodiscard]] constexpr bool IsLowSurrogate(std::uint32_t value) noexcept
{
    return value >= kLowSurrogateFirst && value <= kLowSurrogateLast;
}

[[nodiscard]] constexpr bool IsSurrogate(std::uint32_t value) noexcept
{
    return value >= kHighSurrogateFirst && value <= kLowSurrogateLast;
}

[[nodiscard]] constexpr bool HasRemaining(std::string_view text, std::size_t index,
                                          std::size_t count) noexcept
{
    return index <= text.size() && text.size() - index >= count;
}

[[nodiscard]] auto MakeInvalidUtf8Error()
{
    return MakeUnexpected(kInvalidUtf8, "Invalid UTF-8 string.");
}

[[nodiscard]] auto MakeInvalidWideStringError()
{
    return MakeUnexpected(kInvalidWideString,
                          "Invalid wide string; expected valid UTF-16 or UTF-32 code units.");
}

[[nodiscard]] bool TryDecodeUtf8CodePoint(std::string_view text, std::size_t& index,
                                          std::uint32_t& codePoint) noexcept
{
    const std::uint8_t kByte0 = ToByte(text[index]);

    if (kByte0 <= 0x7FU)
    {
        codePoint = kByte0;
        ++index;
        return true;
    }

    if (kByte0 >= 0xC2U && kByte0 <= 0xDFU)
    {
        if (!HasRemaining(text, index, 2))
        {
            return false;
        }

        const std::uint8_t kByte1 = ToByte(text[index + 1]);
        if (!IsContinuationByte(kByte1))
        {
            return false;
        }

        codePoint = ((kByte0 & 0x1FU) << 6U) | (kByte1 & 0x3FU);
        index += 2;
        return true;
    }

    if (kByte0 >= 0xE0U && kByte0 <= 0xEFU)
    {
        if (!HasRemaining(text, index, 3))
        {
            return false;
        }

        const std::uint8_t kByte1 = ToByte(text[index + 1]);
        const std::uint8_t kByte2 = ToByte(text[index + 2]);
        if (!IsContinuationByte(kByte1) || !IsContinuationByte(kByte2))
        {
            return false;
        }
        if ((kByte0 == 0xE0U && kByte1 < 0xA0U) || (kByte0 == 0xEDU && kByte1 > 0x9FU))
        {
            return false;
        }

        codePoint = ((kByte0 & 0x0FU) << 12U) | ((kByte1 & 0x3FU) << 6U) | (kByte2 & 0x3FU);
        index += 3;
        return true;
    }

    if (kByte0 >= 0xF0U && kByte0 <= 0xF4U)
    {
        if (!HasRemaining(text, index, 4))
        {
            return false;
        }

        const std::uint8_t kByte1 = ToByte(text[index + 1]);
        const std::uint8_t kByte2 = ToByte(text[index + 2]);
        const std::uint8_t kByte3 = ToByte(text[index + 3]);
        if (!IsContinuationByte(kByte1) || !IsContinuationByte(kByte2) ||
            !IsContinuationByte(kByte3))
        {
            return false;
        }
        if ((kByte0 == 0xF0U && kByte1 < 0x90U) || (kByte0 == 0xF4U && kByte1 > 0x8FU))
        {
            return false;
        }

        codePoint = ((kByte0 & 0x07U) << 18U) | ((kByte1 & 0x3FU) << 12U) |
                    ((kByte2 & 0x3FU) << 6U) | (kByte3 & 0x3FU);
        index += 4;
        return true;
    }

    return false;
}

void AppendWideCodePoint(std::wstring& output, std::uint32_t codePoint)
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        if (codePoint <= 0xFFFFU)
        {
            output.push_back(static_cast<wchar_t>(codePoint));
            return;
        }

        const std::uint32_t kPayload = codePoint - kSurrogateBase;
        output.push_back(
            static_cast<wchar_t>(kHighSurrogateFirst + (kPayload >> kSurrogatePayloadBits)));
        output.push_back(
            static_cast<wchar_t>(kLowSurrogateFirst + (kPayload & kSurrogatePayloadMask)));
    }
    else
    {
        output.push_back(static_cast<wchar_t>(codePoint));
    }
}

[[nodiscard]] bool TryReadWideCodePoint(std::wstring_view text, std::size_t& index,
                                        std::uint32_t& codePoint) noexcept
{
    const std::uint32_t kUnit0 = ToWideUnit(text[index]);

    if constexpr (sizeof(wchar_t) == 2)
    {
        if (IsHighSurrogate(kUnit0))
        {
            if (index + 1 >= text.size())
            {
                return false;
            }

            const std::uint32_t kUnit1 = ToWideUnit(text[index + 1]);
            if (!IsLowSurrogate(kUnit1))
            {
                return false;
            }

            codePoint = kSurrogateBase + ((kUnit0 - kHighSurrogateFirst) << kSurrogatePayloadBits) +
                        (kUnit1 - kLowSurrogateFirst);
            index += 2;
            return true;
        }

        if (IsLowSurrogate(kUnit0))
        {
            return false;
        }

        codePoint = kUnit0;
        ++index;
        return true;
    }
    else
    {
        if (kUnit0 > kMaxUnicodeCodePoint || IsSurrogate(kUnit0))
        {
            return false;
        }

        codePoint = kUnit0;
        ++index;
        return true;
    }
}

void AppendUtf8CodePoint(std::string& output, std::uint32_t codePoint)
{
    if (codePoint <= 0x7FU)
    {
        output.push_back(static_cast<char>(codePoint));
        return;
    }

    if (codePoint <= 0x7FFU)
    {
        output.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        return;
    }

    if (codePoint <= 0xFFFFU)
    {
        output.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        return;
    }

    output.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
    output.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
}
} // namespace

Result<std::wstring> Utf8ToWideString(std::string_view text)
{
    std::wstring output;
    output.reserve(text.size());

    std::size_t index{0};
    while (index < text.size())
    {
        std::uint32_t codePoint{};
        if (!TryDecodeUtf8CodePoint(text, index, codePoint))
        {
            return MakeInvalidUtf8Error();
        }

        AppendWideCodePoint(output, codePoint);
    }

    return output;
}

Result<std::string> WideStringToUtf8(std::wstring_view text)
{
    std::string output;
    output.reserve(text.size());

    std::size_t index{0};
    while (index < text.size())
    {
        std::uint32_t codePoint{};
        if (!TryReadWideCodePoint(text, index, codePoint))
        {
            return MakeInvalidWideStringError();
        }

        AppendUtf8CodePoint(output, codePoint);
    }

    return output;
}
} // namespace pond::core