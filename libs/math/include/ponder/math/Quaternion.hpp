#pragma once

#include <ponder/math/Angle.hpp>
#include <ponder/math/Matrix3x3.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Vector3.hpp>

#include <cmath>

namespace pond::math
{
class Quaternion final
{
public:
    constexpr Quaternion() noexcept = default;

    [[nodiscard]] static constexpr Quaternion Identity() noexcept
    {
        return Quaternion{};
    }

    [[nodiscard]] static inline core::Result<Quaternion> FromComponents(float x, float y, float z,
                                                                        float w)
    {
        return NormalizeFiniteComponents(x, y, z, w);
    }

    [[nodiscard]] static inline core::Result<Quaternion> FromAxisAngle(Vector3 axis, Radians angle)
    {
        if (!IsFinite(angle.GetValue())) [[unlikely]]
        {
            return core::Result<Quaternion>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "Quaternion axis-angle construction requires a finite angle."});
        }

        auto normalizedAxis = Normalize(axis);
        if (!normalizedAxis.HasValue()) [[unlikely]]
        {
            return core::Result<Quaternion>::FromError(normalizedAxis.GetError());
        }

        const float halfAngle = angle.GetValue() * 0.5F;
        const float sine = std::sin(halfAngle);
        const float cosine = std::cos(halfAngle);
        return FromComponents(normalizedAxis->x * sine, normalizedAxis->y * sine,
                              normalizedAxis->z * sine, cosine);
    }

    [[nodiscard]] constexpr float GetX() const noexcept
    {
        return m_x;
    }

    [[nodiscard]] constexpr float GetY() const noexcept
    {
        return m_y;
    }

    [[nodiscard]] constexpr float GetZ() const noexcept
    {
        return m_z;
    }

    [[nodiscard]] constexpr float GetW() const noexcept
    {
        return m_w;
    }

    [[nodiscard]] friend constexpr bool operator==(const Quaternion& lhs,
                                                   const Quaternion& rhs) noexcept = default;

private:
    friend constexpr Quaternion Conjugate(Quaternion quaternion) noexcept;
    friend constexpr Matrix3x3 ToMatrix3x3(Quaternion rotation) noexcept;
    friend inline Quaternion operator*(Quaternion lhs, Quaternion rhs);
    friend inline core::Result<Quaternion> Slerp(Quaternion start, Quaternion end, float amount);

    constexpr Quaternion(float x, float y, float z, float w) noexcept
        : m_x(x), m_y(y), m_z(z), m_w(w)
    {
    }

    [[nodiscard]] static inline core::Result<Quaternion> NormalizeFiniteComponents(float x, float y,
                                                                                   float z, float w)
    {
        if (!IsFinite(x) || !IsFinite(y) || !IsFinite(z) || !IsFinite(w)) [[unlikely]]
        {
            return core::Result<Quaternion>::FromError(
                core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                            "Quaternion construction requires finite components."});
        }

        const float maxMagnitude = Max(Max(Abs(x), Abs(y)), Max(Abs(z), Abs(w)));
        if (maxMagnitude == 0.0F) [[unlikely]]
        {
            return core::Result<Quaternion>::FromError(
                core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                            "Quaternion construction requires non-zero length."});
        }

        const Quaternion result = NormalizeFiniteComponentsUnchecked(x, y, z, w);
        if (!IsFinite(result.GetX()) || !IsFinite(result.GetY()) || !IsFinite(result.GetZ()) ||
            !IsFinite(result.GetW())) [[unlikely]]
        {
            return core::Result<Quaternion>::FromError(
                core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                            "Quaternion construction result is numerically unnormalizable."});
        }

        return result;
    }

    [[nodiscard]] static inline Quaternion NormalizeFiniteComponentsUnchecked(float x, float y,
                                                                              float z,
                                                                              float w) noexcept
    {
        const float maxMagnitude = Max(Max(Abs(x), Abs(y)), Max(Abs(z), Abs(w)));
        const double scaledX = static_cast<double>(x) / static_cast<double>(maxMagnitude);
        const double scaledY = static_cast<double>(y) / static_cast<double>(maxMagnitude);
        const double scaledZ = static_cast<double>(z) / static_cast<double>(maxMagnitude);
        const double scaledW = static_cast<double>(w) / static_cast<double>(maxMagnitude);
        const double length = std::sqrt(scaledX * scaledX + scaledY * scaledY + scaledZ * scaledZ +
                                        scaledW * scaledW);
        const double inverseLength = 1.0 / length;

        return Quaternion{static_cast<float>(scaledX * inverseLength),
                          static_cast<float>(scaledY * inverseLength),
                          static_cast<float>(scaledZ * inverseLength),
                          static_cast<float>(scaledW * inverseLength)};
    }

    [[nodiscard]] static constexpr float Abs(float value) noexcept
    {
        return value < 0.0F ? -value : value;
    }

    [[nodiscard]] static constexpr float Max(float lhs, float rhs) noexcept
    {
        return lhs < rhs ? rhs : lhs;
    }

    float m_x{0.0F};
    float m_y{0.0F};
    float m_z{0.0F};
    float m_w{1.0F};
};

[[nodiscard]] constexpr Quaternion Conjugate(Quaternion quaternion) noexcept
{
    return Quaternion{-quaternion.m_x, -quaternion.m_y, -quaternion.m_z, quaternion.m_w};
}

[[nodiscard]] constexpr Quaternion Inverse(Quaternion quaternion) noexcept
{
    return Conjugate(quaternion);
}

[[nodiscard]] constexpr bool IsNear(Quaternion lhs, Quaternion rhs, Tolerance tolerance) noexcept
{
    return IsNear(lhs.GetX(), rhs.GetX(), tolerance) && IsNear(lhs.GetY(), rhs.GetY(), tolerance) &&
           IsNear(lhs.GetZ(), rhs.GetZ(), tolerance) && IsNear(lhs.GetW(), rhs.GetW(), tolerance);
}

[[nodiscard]] constexpr bool IsSameRotation(Quaternion lhs, Quaternion rhs,
                                            Tolerance tolerance) noexcept
{
    return IsNear(lhs, rhs, tolerance) || (IsNear(lhs.GetX(), -rhs.GetX(), tolerance) &&
                                           IsNear(lhs.GetY(), -rhs.GetY(), tolerance) &&
                                           IsNear(lhs.GetZ(), -rhs.GetZ(), tolerance) &&
                                           IsNear(lhs.GetW(), -rhs.GetW(), tolerance));
}

[[nodiscard]] inline Quaternion operator*(Quaternion lhs, Quaternion rhs)
{
    return Quaternion::NormalizeFiniteComponentsUnchecked(
        lhs.m_w * rhs.m_x + lhs.m_x * rhs.m_w + lhs.m_y * rhs.m_z - lhs.m_z * rhs.m_y,
        lhs.m_w * rhs.m_y - lhs.m_x * rhs.m_z + lhs.m_y * rhs.m_w + lhs.m_z * rhs.m_x,
        lhs.m_w * rhs.m_z + lhs.m_x * rhs.m_y - lhs.m_y * rhs.m_x + lhs.m_z * rhs.m_w,
        lhs.m_w * rhs.m_w - lhs.m_x * rhs.m_x - lhs.m_y * rhs.m_y - lhs.m_z * rhs.m_z);
}

[[nodiscard]] constexpr Vector3 Rotate(Quaternion rotation, Vector3 vector) noexcept
{
    const Vector3 vectorPart{rotation.GetX(), rotation.GetY(), rotation.GetZ()};
    const Vector3 doubledCross = 2.0F * Cross(vectorPart, vector);
    return vector + rotation.GetW() * doubledCross + Cross(vectorPart, doubledCross);
}

[[nodiscard]] inline core::Result<Quaternion> Slerp(Quaternion start, Quaternion end, float amount)
{
    if (!IsFinite(amount)) [[unlikely]]
    {
        return core::Result<Quaternion>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Quaternion interpolation requires a finite amount."});
    }

    float endX = end.m_x;
    float endY = end.m_y;
    float endZ = end.m_z;
    float endW = end.m_w;
    float cosine = start.m_x * endX + start.m_y * endY + start.m_z * endZ + start.m_w * endW;

    if (cosine < 0.0F)
    {
        endX = -endX;
        endY = -endY;
        endZ = -endZ;
        endW = -endW;
        cosine = -cosine;
    }

    if (amount == 0.0F)
    {
        return start;
    }

    if (amount == 1.0F)
    {
        return Quaternion{endX, endY, endZ, endW};
    }

    cosine = Clamp(cosine, -1.0F, 1.0F);
    constexpr float kLinearThreshold{0.9995F};
    if (cosine > kLinearThreshold)
    {
        return Quaternion::NormalizeFiniteComponents(
            start.m_x + amount * (endX - start.m_x), start.m_y + amount * (endY - start.m_y),
            start.m_z + amount * (endZ - start.m_z), start.m_w + amount * (endW - start.m_w));
    }

    const double angle = std::acos(static_cast<double>(cosine));
    const double inverseSine = 1.0 / std::sin(angle);
    const double startWeight = std::sin((1.0 - static_cast<double>(amount)) * angle) * inverseSine;
    const double endWeight = std::sin(static_cast<double>(amount) * angle) * inverseSine;

    return Quaternion::NormalizeFiniteComponents(
        static_cast<float>(startWeight * start.m_x + endWeight * endX),
        static_cast<float>(startWeight * start.m_y + endWeight * endY),
        static_cast<float>(startWeight * start.m_z + endWeight * endZ),
        static_cast<float>(startWeight * start.m_w + endWeight * endW));
}

[[nodiscard]] constexpr Matrix3x3 ToMatrix3x3(Quaternion rotation) noexcept
{
    const float xx = rotation.m_x * rotation.m_x;
    const float yy = rotation.m_y * rotation.m_y;
    const float zz = rotation.m_z * rotation.m_z;
    const float xy = rotation.m_x * rotation.m_y;
    const float xz = rotation.m_x * rotation.m_z;
    const float yz = rotation.m_y * rotation.m_z;
    const float wx = rotation.m_w * rotation.m_x;
    const float wy = rotation.m_w * rotation.m_y;
    const float wz = rotation.m_w * rotation.m_z;

    return Matrix3x3{1.0F - 2.0F * (yy + zz), 2.0F * (xy - wz),        2.0F * (xz + wy),
                     2.0F * (xy + wz),        1.0F - 2.0F * (xx + zz), 2.0F * (yz - wx),
                     2.0F * (xz - wy),        2.0F * (yz + wx),        1.0F - 2.0F * (xx + yy)};
}

[[nodiscard]] constexpr Matrix4x4 ToMatrix4x4(Quaternion rotation) noexcept
{
    const Matrix3x3 matrix = ToMatrix3x3(rotation);
    return Matrix4x4{matrix.row0Column0,
                     matrix.row0Column1,
                     matrix.row0Column2,
                     0.0F,
                     matrix.row1Column0,
                     matrix.row1Column1,
                     matrix.row1Column2,
                     0.0F,
                     matrix.row2Column0,
                     matrix.row2Column1,
                     matrix.row2Column2,
                     0.0F,
                     0.0F,
                     0.0F,
                     0.0F,
                     1.0F};
}

constexpr Matrix4x4 Matrix4x4::Rotation(Quaternion rotation) noexcept
{
    return ToMatrix4x4(rotation);
}

namespace detail
{
[[nodiscard]] inline core::Result<void> ValidateRotationMatrix(Matrix3x3 matrix,
                                                               Tolerance tolerance)
{
    if (!IsFinite(matrix)) [[unlikely]]
    {
        return core::Result<void>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Quaternion conversion requires finite matrix components."});
    }

    const float determinant = Determinant(matrix);
    if (IsNear(determinant, 0.0F, tolerance)) [[unlikely]]
    {
        return core::Result<void>::FromError(
            core::Error{ToErrorCode(MathErrorCode::SingularMatrix),
                        "Quaternion conversion requires a nonsingular rotation matrix."});
    }

    const Vector3 column0{matrix.row0Column0, matrix.row1Column0, matrix.row2Column0};
    const Vector3 column1{matrix.row0Column1, matrix.row1Column1, matrix.row2Column1};
    const Vector3 column2{matrix.row0Column2, matrix.row1Column2, matrix.row2Column2};

    if (!IsNear(Dot(column0, column0), 1.0F, tolerance) ||
        !IsNear(Dot(column1, column1), 1.0F, tolerance) ||
        !IsNear(Dot(column2, column2), 1.0F, tolerance) ||
        !IsNear(Dot(column0, column1), 0.0F, tolerance) ||
        !IsNear(Dot(column0, column2), 0.0F, tolerance) ||
        !IsNear(Dot(column1, column2), 0.0F, tolerance) || !IsNear(determinant, 1.0F, tolerance)) [[unlikely]]
    {
        return core::Result<void>::FromError(
            core::Error{ToErrorCode(MathErrorCode::DegenerateInput),
                        "Quaternion conversion requires an orthonormal proper rotation matrix."});
    }

    return core::Result<void>::Success();
}
} // namespace detail

[[nodiscard]] inline core::Result<Quaternion> ToQuaternion(Matrix3x3 matrix, Tolerance tolerance)
{
    auto validation = detail::ValidateRotationMatrix(matrix, tolerance);
    if (!validation.HasValue()) [[unlikely]]
    {
        return core::Result<Quaternion>::FromError(validation.GetError());
    }

    const float trace = matrix.row0Column0 + matrix.row1Column1 + matrix.row2Column2;
    if (trace > 0.0F)
    {
        const double scale = std::sqrt(static_cast<double>(trace) + 1.0) * 2.0;
        return Quaternion::FromComponents(
            static_cast<float>((matrix.row2Column1 - matrix.row1Column2) / scale),
            static_cast<float>((matrix.row0Column2 - matrix.row2Column0) / scale),
            static_cast<float>((matrix.row1Column0 - matrix.row0Column1) / scale),
            static_cast<float>(0.25 * scale));
    }

    if (matrix.row0Column0 >= matrix.row1Column1 && matrix.row0Column0 >= matrix.row2Column2)
    {
        const double scale = std::sqrt(1.0 + static_cast<double>(matrix.row0Column0) -
                                       static_cast<double>(matrix.row1Column1) -
                                       static_cast<double>(matrix.row2Column2)) *
                             2.0;
        return Quaternion::FromComponents(
            static_cast<float>(0.25 * scale),
            static_cast<float>((matrix.row0Column1 + matrix.row1Column0) / scale),
            static_cast<float>((matrix.row0Column2 + matrix.row2Column0) / scale),
            static_cast<float>((matrix.row2Column1 - matrix.row1Column2) / scale));
    }

    if (matrix.row1Column1 >= matrix.row2Column2)
    {
        const double scale = std::sqrt(1.0 + static_cast<double>(matrix.row1Column1) -
                                       static_cast<double>(matrix.row0Column0) -
                                       static_cast<double>(matrix.row2Column2)) *
                             2.0;
        return Quaternion::FromComponents(
            static_cast<float>((matrix.row0Column1 + matrix.row1Column0) / scale),
            static_cast<float>(0.25 * scale),
            static_cast<float>((matrix.row1Column2 + matrix.row2Column1) / scale),
            static_cast<float>((matrix.row0Column2 - matrix.row2Column0) / scale));
    }

    const double scale = std::sqrt(1.0 + static_cast<double>(matrix.row2Column2) -
                                   static_cast<double>(matrix.row0Column0) -
                                   static_cast<double>(matrix.row1Column1)) *
                         2.0;
    return Quaternion::FromComponents(
        static_cast<float>((matrix.row0Column2 + matrix.row2Column0) / scale),
        static_cast<float>((matrix.row1Column2 + matrix.row2Column1) / scale),
        static_cast<float>(0.25 * scale),
        static_cast<float>((matrix.row1Column0 - matrix.row0Column1) / scale));
}

[[nodiscard]] inline core::Result<Quaternion> ToQuaternion(Matrix4x4 matrix, Tolerance tolerance)
{
    return ToQuaternion(Matrix3x3{matrix.row0Column0, matrix.row0Column1, matrix.row0Column2,
                                  matrix.row1Column0, matrix.row1Column1, matrix.row1Column2,
                                  matrix.row2Column0, matrix.row2Column1, matrix.row2Column2},
                        tolerance);
}
} // namespace pond::math