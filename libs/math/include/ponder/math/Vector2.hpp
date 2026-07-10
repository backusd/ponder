#pragma once

#include <ponder/core/Assert.hpp>
#include <ponder/math/Scalar.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

namespace pond::math
{
struct Vector2 final
{
    float x{0.0F};
    float y{0.0F};

    constexpr Vector2() noexcept = default;

    explicit constexpr Vector2(float xValue, float yValue) noexcept : x(xValue), y(yValue) {}

    [[nodiscard]] static constexpr Vector2 Zero() noexcept
    {
        return Vector2{};
    }

    [[nodiscard]] float& operator[](std::size_t index)
    {
        PONDER_VERIFY(index < 2, "Vector2 index {} is out of range.", index);
        return index == 0 ? x : y;
    }

    [[nodiscard]] const float& operator[](std::size_t index) const
    {
        PONDER_VERIFY(index < 2, "Vector2 index {} is out of range.", index);
        return index == 0 ? x : y;
    }

    [[nodiscard]] core::Result<std::reference_wrapper<float>> At(std::size_t index)
    {
        switch (index)
        {
        case 0:
            return std::ref(x);
        case 1:
            return std::ref(y);
        default: [[unlikely]]
            return core::Result<std::reference_wrapper<float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument), "Vector2 index is out of range."});
        }
    }

    [[nodiscard]] core::Result<std::reference_wrapper<const float>> At(std::size_t index) const
    {
        switch (index)
        {
        case 0:
            return std::cref(x);
        case 1:
            return std::cref(y);
        default: [[unlikely]]
            return core::Result<std::reference_wrapper<const float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument), "Vector2 index is out of range."});
        }
    }

    [[nodiscard]] friend constexpr bool operator==(const Vector2& lhs,
                                                   const Vector2& rhs) noexcept = default;
};

[[nodiscard]] constexpr Vector2 operator+(Vector2 lhs, Vector2 rhs) noexcept
{
    return Vector2{lhs.x + rhs.x, lhs.y + rhs.y};
}

[[nodiscard]] constexpr Vector2 operator-(Vector2 lhs, Vector2 rhs) noexcept
{
    return Vector2{lhs.x - rhs.x, lhs.y - rhs.y};
}

[[nodiscard]] constexpr Vector2 operator-(Vector2 vector) noexcept
{
    return Vector2{-vector.x, -vector.y};
}

[[nodiscard]] constexpr Vector2 operator*(Vector2 vector, float scalar) noexcept
{
    return Vector2{vector.x * scalar, vector.y * scalar};
}

[[nodiscard]] constexpr Vector2 operator*(float scalar, Vector2 vector) noexcept
{
    return vector * scalar;
}

[[nodiscard]] constexpr Vector2 operator/(Vector2 vector, float scalar) noexcept
{
    return Vector2{vector.x / scalar, vector.y / scalar};
}

[[nodiscard]] constexpr Vector2 ComponentMultiply(Vector2 lhs, Vector2 rhs) noexcept
{
    return Vector2{lhs.x * rhs.x, lhs.y * rhs.y};
}

[[nodiscard]] constexpr Vector2 ComponentDivide(Vector2 lhs, Vector2 rhs) noexcept
{
    return Vector2{lhs.x / rhs.x, lhs.y / rhs.y};
}

[[nodiscard]] constexpr float Dot(Vector2 lhs, Vector2 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

[[nodiscard]] constexpr float SquaredLength(Vector2 vector) noexcept
{
    return Dot(vector, vector);
}

[[nodiscard]] inline float Length(Vector2 vector) noexcept
{
    return std::sqrt(SquaredLength(vector));
}

[[nodiscard]] constexpr float SquaredDistance(Vector2 lhs, Vector2 rhs) noexcept
{
    return SquaredLength(lhs - rhs);
}

[[nodiscard]] inline float Distance(Vector2 lhs, Vector2 rhs) noexcept
{
    return Length(lhs - rhs);
}

[[nodiscard]] constexpr Vector2 ComponentMin(Vector2 lhs, Vector2 rhs) noexcept
{
    return Vector2{lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y};
}

[[nodiscard]] constexpr Vector2 ComponentMax(Vector2 lhs, Vector2 rhs) noexcept
{
    return Vector2{lhs.x < rhs.x ? rhs.x : lhs.x, lhs.y < rhs.y ? rhs.y : lhs.y};
}

[[nodiscard]] constexpr Vector2 Lerp(Vector2 start, Vector2 end, float amount) noexcept
{
    return Vector2{Lerp(start.x, end.x, amount), Lerp(start.y, end.y, amount)};
}

[[nodiscard]] constexpr bool IsNear(Vector2 lhs, Vector2 rhs, Tolerance tolerance) noexcept
{
    return IsNear(lhs.x, rhs.x, tolerance) && IsNear(lhs.y, rhs.y, tolerance);
}

[[nodiscard]] inline core::Result<Vector2> Normalize(Vector2 vector)
{
    if (!IsFinite(vector.x) || !IsFinite(vector.y)) [[unlikely]]
    {
        return core::Result<Vector2>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Vector2 normalization requires finite components."});
    }

    const float maxMagnitude = std::max(std::abs(vector.x), std::abs(vector.y));
    if (maxMagnitude == 0.0F) [[unlikely]]
    {
        return core::Result<Vector2>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector2 normalization requires non-zero length."});
    }

    const Vector2 scaled = vector / maxMagnitude;
    const float scaledLength = Length(scaled);
    if (!IsFinite(scaledLength) || scaledLength == 0.0F) [[unlikely]]
    {
        return core::Result<Vector2>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector2 normalization input is numerically unnormalizable."});
    }

    const Vector2 normalized = scaled / scaledLength;
    if (!IsFinite(normalized.x) || !IsFinite(normalized.y)) [[unlikely]]
    {
        return core::Result<Vector2>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector2 normalization result is numerically unnormalizable."});
    }

    return normalized;
}
} // namespace pond::math