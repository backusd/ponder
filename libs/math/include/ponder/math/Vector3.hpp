#pragma once

#include <ponder/core/Assert.hpp>
#include <ponder/math/Scalar.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

namespace pond::math
{
struct Vector3 final
{
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    constexpr Vector3() noexcept = default;

    explicit constexpr Vector3(float xValue, float yValue, float zValue) noexcept
        : x(xValue), y(yValue), z(zValue)
    {
    }

    [[nodiscard]] static constexpr Vector3 Zero() noexcept
    {
        return Vector3{};
    }

    [[nodiscard]] float& operator[](std::size_t index)
    {
        PONDER_VERIFY(index < 3, "Vector3 index {} is out of range.", index);

        switch (index)
        {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    [[nodiscard]] const float& operator[](std::size_t index) const
    {
        PONDER_VERIFY(index < 3, "Vector3 index {} is out of range.", index);

        switch (index)
        {
        case 0:
            return x;
        case 1:
            return y;
        default:
            return z;
        }
    }

    [[nodiscard]] core::Result<std::reference_wrapper<float>> At(std::size_t index)
    {
        switch (index)
        {
        case 0:
            return std::ref(x);
        case 1:
            return std::ref(y);
        case 2:
            return std::ref(z);
        default: [[unlikely]]
            return core::Result<std::reference_wrapper<float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument), "Vector3 index is out of range."});
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
        case 2:
            return std::cref(z);
        default: [[unlikely]]
            return core::Result<std::reference_wrapper<const float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument), "Vector3 index is out of range."});
        }
    }

    [[nodiscard]] friend constexpr bool operator==(const Vector3& lhs,
                                                   const Vector3& rhs) noexcept = default;
};

[[nodiscard]] constexpr Vector3 operator+(Vector3 lhs, Vector3 rhs) noexcept
{
    return Vector3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] constexpr Vector3 operator-(Vector3 lhs, Vector3 rhs) noexcept
{
    return Vector3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] constexpr Vector3 operator-(Vector3 vector) noexcept
{
    return Vector3{-vector.x, -vector.y, -vector.z};
}

[[nodiscard]] constexpr Vector3 operator*(Vector3 vector, float scalar) noexcept
{
    return Vector3{vector.x * scalar, vector.y * scalar, vector.z * scalar};
}

[[nodiscard]] constexpr Vector3 operator*(float scalar, Vector3 vector) noexcept
{
    return vector * scalar;
}

[[nodiscard]] constexpr Vector3 operator/(Vector3 vector, float scalar) noexcept
{
    return Vector3{vector.x / scalar, vector.y / scalar, vector.z / scalar};
}

[[nodiscard]] constexpr Vector3 ComponentMultiply(Vector3 lhs, Vector3 rhs) noexcept
{
    return Vector3{lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
}

[[nodiscard]] constexpr Vector3 ComponentDivide(Vector3 lhs, Vector3 rhs) noexcept
{
    return Vector3{lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z};
}

[[nodiscard]] constexpr float Dot(Vector3 lhs, Vector3 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] constexpr Vector3 Cross(Vector3 lhs, Vector3 rhs) noexcept
{
    return Vector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                   lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] constexpr float SquaredLength(Vector3 vector) noexcept
{
    return Dot(vector, vector);
}

[[nodiscard]] inline float Length(Vector3 vector) noexcept
{
    return std::sqrt(SquaredLength(vector));
}

[[nodiscard]] constexpr float SquaredDistance(Vector3 lhs, Vector3 rhs) noexcept
{
    return SquaredLength(lhs - rhs);
}

[[nodiscard]] inline float Distance(Vector3 lhs, Vector3 rhs) noexcept
{
    return Length(lhs - rhs);
}

[[nodiscard]] constexpr Vector3 ComponentMin(Vector3 lhs, Vector3 rhs) noexcept
{
    return Vector3{lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y,
                   lhs.z < rhs.z ? lhs.z : rhs.z};
}

[[nodiscard]] constexpr Vector3 ComponentMax(Vector3 lhs, Vector3 rhs) noexcept
{
    return Vector3{lhs.x < rhs.x ? rhs.x : lhs.x, lhs.y < rhs.y ? rhs.y : lhs.y,
                   lhs.z < rhs.z ? rhs.z : lhs.z};
}

[[nodiscard]] constexpr Vector3 Lerp(Vector3 start, Vector3 end, float amount) noexcept
{
    return Vector3{Lerp(start.x, end.x, amount), Lerp(start.y, end.y, amount),
                   Lerp(start.z, end.z, amount)};
}

[[nodiscard]] constexpr bool IsNear(Vector3 lhs, Vector3 rhs, Tolerance tolerance) noexcept
{
    return IsNear(lhs.x, rhs.x, tolerance) && IsNear(lhs.y, rhs.y, tolerance) &&
           IsNear(lhs.z, rhs.z, tolerance);
}

[[nodiscard]] inline core::Result<Vector3> Normalize(Vector3 vector)
{
    if (!IsFinite(vector.x) || !IsFinite(vector.y) || !IsFinite(vector.z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Vector3 normalization requires finite components."});
    }

    const float maxMagnitude =
        std::max(std::max(std::abs(vector.x), std::abs(vector.y)), std::abs(vector.z));
    if (maxMagnitude == 0.0F) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector3 normalization requires non-zero length."});
    }

    const Vector3 scaled = vector / maxMagnitude;
    const float scaledLength = Length(scaled);
    if (!IsFinite(scaledLength) || scaledLength == 0.0F) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector3 normalization input is numerically unnormalizable."});
    }

    const Vector3 normalized = scaled / scaledLength;
    if (!IsFinite(normalized.x) || !IsFinite(normalized.y) || !IsFinite(normalized.z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector3 normalization result is numerically unnormalizable."});
    }

    return normalized;
}
} // namespace pond::math