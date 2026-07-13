#pragma once

#include <ponder/core/Numbers.hpp>
#include <ponder/math/MathError.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

namespace pond::math
{
struct Vector4 final
{
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{0.0F};

    constexpr Vector4() noexcept = default;

    explicit constexpr Vector4(float xValue, float yValue, float zValue, float wValue) noexcept
        : x(xValue), y(yValue), z(zValue), w(wValue)
    {
    }

    [[nodiscard]] static constexpr Vector4 Zero() noexcept
    {
        return Vector4{};
    }

    [[nodiscard]] constexpr core::Result<std::reference_wrapper<float>> At(std::size_t index)
    {
        switch (index)
        {
        case 0:
            return std::ref(x);
        case 1:
            return std::ref(y);
        case 2:
            return std::ref(z);
        case 3:
            return std::ref(w);
        default:
            [[unlikely]] return core::Result<std::reference_wrapper<float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument), "Vector4 index is out of range."});
        }
    }

    [[nodiscard]] constexpr core::Result<std::reference_wrapper<const float>> At(
        std::size_t index) const
    {
        switch (index)
        {
        case 0:
            return std::cref(x);
        case 1:
            return std::cref(y);
        case 2:
            return std::cref(z);
        case 3:
            return std::cref(w);
        default:
            [[unlikely]] return core::Result<std::reference_wrapper<const float>>::FromError(
                core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                            "Vector4 index is out of range."});
        }
    }

    [[nodiscard]] friend constexpr bool operator==(const Vector4& lhs,
                                                   const Vector4& rhs) noexcept = default;
};

[[nodiscard]] constexpr Vector4 operator+(Vector4 lhs, Vector4 rhs) noexcept
{
    return Vector4{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

[[nodiscard]] constexpr Vector4 operator-(Vector4 lhs, Vector4 rhs) noexcept
{
    return Vector4{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
}

[[nodiscard]] constexpr Vector4 operator-(Vector4 vector) noexcept
{
    return Vector4{-vector.x, -vector.y, -vector.z, -vector.w};
}

[[nodiscard]] constexpr Vector4 operator*(Vector4 vector, float scalar) noexcept
{
    return Vector4{vector.x * scalar, vector.y * scalar, vector.z * scalar, vector.w * scalar};
}

[[nodiscard]] constexpr Vector4 operator*(float scalar, Vector4 vector) noexcept
{
    return vector * scalar;
}

[[nodiscard]] constexpr Vector4 operator/(Vector4 vector, float scalar) noexcept
{
    return Vector4{vector.x / scalar, vector.y / scalar, vector.z / scalar, vector.w / scalar};
}

[[nodiscard]] constexpr Vector4 ComponentMultiply(Vector4 lhs, Vector4 rhs) noexcept
{
    return Vector4{lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w};
}

[[nodiscard]] constexpr Vector4 ComponentDivide(Vector4 lhs, Vector4 rhs) noexcept
{
    return Vector4{lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w};
}

[[nodiscard]] constexpr float Dot(Vector4 lhs, Vector4 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

[[nodiscard]] constexpr float SquaredLength(Vector4 vector) noexcept
{
    return Dot(vector, vector);
}

[[nodiscard]] inline float Length(Vector4 vector) noexcept
{
    return std::hypot(std::hypot(vector.x, vector.y), std::hypot(vector.z, vector.w));
}

[[nodiscard]] constexpr float SquaredDistance(Vector4 lhs, Vector4 rhs) noexcept
{
    return SquaredLength(lhs - rhs);
}

[[nodiscard]] inline float Distance(Vector4 lhs, Vector4 rhs) noexcept
{
    return std::hypot(std::hypot(lhs.x - rhs.x, lhs.y - rhs.y),
                      std::hypot(lhs.z - rhs.z, lhs.w - rhs.w));
}

[[nodiscard]] constexpr Vector4 ComponentMin(Vector4 lhs, Vector4 rhs) noexcept
{
    return Vector4{lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y,
                   lhs.z < rhs.z ? lhs.z : rhs.z, lhs.w < rhs.w ? lhs.w : rhs.w};
}

[[nodiscard]] constexpr Vector4 ComponentMax(Vector4 lhs, Vector4 rhs) noexcept
{
    return Vector4{lhs.x < rhs.x ? rhs.x : lhs.x, lhs.y < rhs.y ? rhs.y : lhs.y,
                   lhs.z < rhs.z ? rhs.z : lhs.z, lhs.w < rhs.w ? rhs.w : lhs.w};
}

[[nodiscard]] constexpr Vector4 Lerp(Vector4 start, Vector4 end, float amount) noexcept
{
    return Vector4{core::Lerp(start.x, end.x, amount), core::Lerp(start.y, end.y, amount),
                   core::Lerp(start.z, end.z, amount), core::Lerp(start.w, end.w, amount)};
}

[[nodiscard]] constexpr bool IsNear(Vector4 lhs, Vector4 rhs, core::Tolerance tolerance) noexcept
{
    return core::IsNear(lhs.x, rhs.x, tolerance) && core::IsNear(lhs.y, rhs.y, tolerance) &&
           core::IsNear(lhs.z, rhs.z, tolerance) && core::IsNear(lhs.w, rhs.w, tolerance);
}

[[nodiscard]] inline core::Result<Vector4> Normalize(Vector4 vector)
{
    if (!core::IsFinite(vector.x) || !core::IsFinite(vector.y) || !core::IsFinite(vector.z) ||
        !core::IsFinite(vector.w)) [[unlikely]]
    {
        return core::Result<Vector4>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Vector4 normalization requires finite components."});
    }

    const float maxMagnitude = std::max(std::max(std::abs(vector.x), std::abs(vector.y)),
                                        std::max(std::abs(vector.z), std::abs(vector.w)));
    if (maxMagnitude == 0.0F) [[unlikely]]
    {
        return core::Result<Vector4>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector4 normalization requires non-zero length."});
    }

    const Vector4 scaled = vector / maxMagnitude;
    const float scaledLength = Length(scaled);
    if (!core::IsFinite(scaledLength) || scaledLength == 0.0F) [[unlikely]]
    {
        return core::Result<Vector4>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector4 normalization input is numerically unnormalizable."});
    }

    const Vector4 normalized = scaled / scaledLength;
    if (!core::IsFinite(normalized.x) || !core::IsFinite(normalized.y) ||
        !core::IsFinite(normalized.z) || !core::IsFinite(normalized.w)) [[unlikely]]
    {
        return core::Result<Vector4>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Vector4 normalization result is numerically unnormalizable."});
    }

    return normalized;
}
} // namespace pond::math
