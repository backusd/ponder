#pragma once

#include <ponder/core/Numbers.hpp>
#include <ponder/ui/Error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pond::ui
{
struct SrgbStraightAlphaColor final
{
    float red{};
    float green{};
    float blue{};
    float alpha{};

    [[nodiscard]] friend constexpr bool operator==(
        const SrgbStraightAlphaColor& lhs, const SrgbStraightAlphaColor& rhs) noexcept = default;
};

struct LinearPremultipliedColor final
{
    float red{};
    float green{};
    float blue{};
    float alpha{};

    [[nodiscard]] friend constexpr bool operator==(const LinearPremultipliedColor& lhs,
                                                   const LinearPremultipliedColor& rhs) noexcept =
        default;
};

class PackedLinearPremultipliedRgba8 final
{
public:
    constexpr PackedLinearPremultipliedRgba8() noexcept = default;

    explicit constexpr PackedLinearPremultipliedRgba8(std::uint32_t value) noexcept : m_value{value}
    {
    }

    [[nodiscard]] static constexpr PackedLinearPremultipliedRgba8 FromChannels(
        std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) noexcept
    {
        return PackedLinearPremultipliedRgba8{
            static_cast<std::uint32_t>(red) | (static_cast<std::uint32_t>(green) << 8U) |
            (static_cast<std::uint32_t>(blue) << 16U) | (static_cast<std::uint32_t>(alpha) << 24U)};
    }

    [[nodiscard]] constexpr std::uint32_t GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] constexpr std::uint8_t GetRed() const noexcept
    {
        return static_cast<std::uint8_t>(m_value & 0xFFU);
    }

    [[nodiscard]] constexpr std::uint8_t GetGreen() const noexcept
    {
        return static_cast<std::uint8_t>((m_value >> 8U) & 0xFFU);
    }

    [[nodiscard]] constexpr std::uint8_t GetBlue() const noexcept
    {
        return static_cast<std::uint8_t>((m_value >> 16U) & 0xFFU);
    }

    [[nodiscard]] constexpr std::uint8_t GetAlpha() const noexcept
    {
        return static_cast<std::uint8_t>((m_value >> 24U) & 0xFFU);
    }

    [[nodiscard]] friend constexpr bool operator==(
        const PackedLinearPremultipliedRgba8& lhs,
        const PackedLinearPremultipliedRgba8& rhs) noexcept = default;

private:
    std::uint32_t m_value{};
};

[[nodiscard]] constexpr bool IsUnitInterval(float value) noexcept
{
    return core::IsFinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] constexpr bool IsValid(SrgbStraightAlphaColor color) noexcept
{
    return IsUnitInterval(color.red) && IsUnitInterval(color.green) && IsUnitInterval(color.blue) &&
           IsUnitInterval(color.alpha);
}

[[nodiscard]] constexpr bool IsValid(LinearPremultipliedColor color) noexcept
{
    return IsUnitInterval(color.alpha) && core::IsFinite(color.red) &&
           core::IsFinite(color.green) && core::IsFinite(color.blue) && color.red >= 0.0F &&
           color.green >= 0.0F && color.blue >= 0.0F && color.red <= color.alpha &&
           color.green <= color.alpha && color.blue <= color.alpha;
}

[[nodiscard]] constexpr bool IsTransparent(SrgbStraightAlphaColor color) noexcept
{
    return IsValid(color) && color.alpha == 0.0F;
}

[[nodiscard]] constexpr bool IsTransparent(LinearPremultipliedColor color) noexcept
{
    return IsValid(color) && color.alpha == 0.0F;
}

[[nodiscard]] inline core::Result<SrgbStraightAlphaColor> MakeSrgbStraightAlphaColor(float red,
                                                                                     float green,
                                                                                     float blue,
                                                                                     float alpha)
{
    const SrgbStraightAlphaColor color{red, green, blue, alpha};
    if (!IsValid(color))
    {
        return MakeUiFailure<SrgbStraightAlphaColor>(
            UiErrorCode::InvalidPaintValue,
            "UI sRGB straight-alpha color components must be finite values in [0, 1].");
    }

    return color;
}

[[nodiscard]] inline core::Result<LinearPremultipliedColor> MakeLinearPremultipliedColor(
    float red, float green, float blue, float alpha)
{
    const LinearPremultipliedColor color{red, green, blue, alpha};
    if (!IsValid(color))
    {
        return MakeUiFailure<LinearPremultipliedColor>(
            UiErrorCode::InvalidPaintValue,
            "UI linear-premultiplied color components must be finite, alpha-bounded values.");
    }

    return color;
}

[[nodiscard]] inline float ConvertSrgbChannelToLinear(float value) noexcept
{
    if (value <= 0.04045F)
    {
        return value / 12.92F;
    }

    return std::pow((value + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] inline float ConvertLinearChannelToSrgb(float value) noexcept
{
    if (value <= 0.0031308F)
    {
        return value * 12.92F;
    }

    return (1.055F * std::pow(value, 1.0F / 2.4F)) - 0.055F;
}

[[nodiscard]] inline LinearPremultipliedColor ToLinearPremultiplied(
    SrgbStraightAlphaColor color) noexcept
{
    return LinearPremultipliedColor{ConvertSrgbChannelToLinear(color.red) * color.alpha,
                                    ConvertSrgbChannelToLinear(color.green) * color.alpha,
                                    ConvertSrgbChannelToLinear(color.blue) * color.alpha,
                                    color.alpha};
}

[[nodiscard]] inline LinearPremultipliedColor BlendSourceOver(
    LinearPremultipliedColor source, LinearPremultipliedColor destination) noexcept
{
    const float destinationWeight = 1.0F - source.alpha;
    return LinearPremultipliedColor{source.red + destination.red * destinationWeight,
                                    source.green + destination.green * destinationWeight,
                                    source.blue + destination.blue * destinationWeight,
                                    source.alpha + destination.alpha * destinationWeight};
}

[[nodiscard]] inline core::Result<std::uint8_t> QuantizeUnitFloatToUnorm8(float value)
{
    if (!IsUnitInterval(value))
    {
        return MakeUiFailure<std::uint8_t>(
            UiErrorCode::InvalidPaintValue,
            "UI UNORM8 quantization requires a finite value in [0, 1].");
    }

    const float scaled = std::clamp(value, 0.0F, 1.0F) * 255.0F;
    return static_cast<std::uint8_t>(std::floor(scaled + 0.5F));
}

[[nodiscard]] inline core::Result<PackedLinearPremultipliedRgba8> PackLinearPremultipliedRgba8(
    LinearPremultipliedColor color)
{
    if (!IsValid(color))
    {
        return MakeUiFailure<PackedLinearPremultipliedRgba8>(
            UiErrorCode::InvalidPaintValue,
            "UI linear-premultiplied color must be valid before RGBA8 packing.");
    }

    const core::Result<std::uint8_t> red = QuantizeUnitFloatToUnorm8(color.red);
    const core::Result<std::uint8_t> green = QuantizeUnitFloatToUnorm8(color.green);
    const core::Result<std::uint8_t> blue = QuantizeUnitFloatToUnorm8(color.blue);
    const core::Result<std::uint8_t> alpha = QuantizeUnitFloatToUnorm8(color.alpha);
    if (!red.HasValue() || !green.HasValue() || !blue.HasValue() || !alpha.HasValue())
    {
        return MakeUiFailure<PackedLinearPremultipliedRgba8>(
            UiErrorCode::InvalidPaintValue,
            "UI linear-premultiplied color could not be quantized to RGBA8.");
    }

    return PackedLinearPremultipliedRgba8::FromChannels(*red, *green, *blue, *alpha);
}
} // namespace pond::ui
