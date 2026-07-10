#pragma once

#include <ponder/math/Vector3.hpp>

#include <cstddef>
#include <functional>

namespace pond::math
{
struct Matrix3x3 final
{
    float row0Column0{0.0F};
    float row1Column0{0.0F};
    float row2Column0{0.0F};
    float row0Column1{0.0F};
    float row1Column1{0.0F};
    float row2Column1{0.0F};
    float row0Column2{0.0F};
    float row1Column2{0.0F};
    float row2Column2{0.0F};

    constexpr Matrix3x3() noexcept = default;

    explicit constexpr Matrix3x3(float row0Column0Value, float row0Column1Value,
                                 float row0Column2Value, float row1Column0Value,
                                 float row1Column1Value, float row1Column2Value,
                                 float row2Column0Value, float row2Column1Value,
                                 float row2Column2Value) noexcept
        : row0Column0(row0Column0Value), row1Column0(row1Column0Value),
          row2Column0(row2Column0Value), row0Column1(row0Column1Value),
          row1Column1(row1Column1Value), row2Column1(row2Column1Value),
          row0Column2(row0Column2Value), row1Column2(row1Column2Value),
          row2Column2(row2Column2Value)
    {
    }

    explicit constexpr Matrix3x3(Vector3 column0, Vector3 column1, Vector3 column2) noexcept
        : row0Column0(column0.x), row1Column0(column0.y), row2Column0(column0.z),
          row0Column1(column1.x), row1Column1(column1.y), row2Column1(column1.z),
          row0Column2(column2.x), row1Column2(column2.y), row2Column2(column2.z)
    {
    }

    [[nodiscard]] static constexpr Matrix3x3 Zero() noexcept
    {
        return Matrix3x3{};
    }

    [[nodiscard]] static constexpr Matrix3x3 Identity() noexcept
    {
        return Matrix3x3{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    }

    [[nodiscard]] core::Result<Vector3> Row(std::size_t row) const
    {
        switch (row)
        {
        case 0:
            return Vector3{row0Column0, row0Column1, row0Column2};
        case 1:
            return Vector3{row1Column0, row1Column1, row1Column2};
        case 2:
            return Vector3{row2Column0, row2Column1, row2Column2};
        default:
            [[unlikely]] return core::Result<Vector3>::FromError(MakeIndexError());
        }
    }

    [[nodiscard]] core::Result<Vector3> Column(std::size_t column) const
    {
        switch (column)
        {
        case 0:
            return Vector3{row0Column0, row1Column0, row2Column0};
        case 1:
            return Vector3{row0Column1, row1Column1, row2Column1};
        case 2:
            return Vector3{row0Column2, row1Column2, row2Column2};
        default:
            [[unlikely]] return core::Result<Vector3>::FromError(MakeIndexError());
        }
    }

    [[nodiscard]] core::Result<std::reference_wrapper<float>> At(std::size_t row,
                                                                 std::size_t column)
    {
        if (row >= kDimension || column >= kDimension) [[unlikely]]
        {
            return core::Result<std::reference_wrapper<float>>::FromError(MakeIndexError());
        }

        return std::ref(AtUnchecked(row, column));
    }

    [[nodiscard]] core::Result<std::reference_wrapper<const float>> At(std::size_t row,
                                                                       std::size_t column) const
    {
        if (row >= kDimension || column >= kDimension) [[unlikely]]
        {
            return core::Result<std::reference_wrapper<const float>>::FromError(MakeIndexError());
        }

        return std::cref(AtUnchecked(row, column));
    }

    [[nodiscard]] friend constexpr bool operator==(const Matrix3x3& lhs,
                                                   const Matrix3x3& rhs) noexcept = default;

private:
    inline static constexpr std::size_t kDimension{3};

    [[nodiscard]] static core::Error MakeIndexError()
    {
        return core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                           "Matrix3x3 index is out of range."};
    }

    [[nodiscard]] float& AtUnchecked(std::size_t row, std::size_t column) noexcept
    {
        switch (column)
        {
        case 0:
            switch (row)
            {
            case 0:
                return row0Column0;
            case 1:
                return row1Column0;
            default:
                return row2Column0;
            }
        case 1:
            switch (row)
            {
            case 0:
                return row0Column1;
            case 1:
                return row1Column1;
            default:
                return row2Column1;
            }
        default:
            switch (row)
            {
            case 0:
                return row0Column2;
            case 1:
                return row1Column2;
            default:
                return row2Column2;
            }
        }
    }

    [[nodiscard]] const float& AtUnchecked(std::size_t row, std::size_t column) const noexcept
    {
        switch (column)
        {
        case 0:
            switch (row)
            {
            case 0:
                return row0Column0;
            case 1:
                return row1Column0;
            default:
                return row2Column0;
            }
        case 1:
            switch (row)
            {
            case 0:
                return row0Column1;
            case 1:
                return row1Column1;
            default:
                return row2Column1;
            }
        default:
            switch (row)
            {
            case 0:
                return row0Column2;
            case 1:
                return row1Column2;
            default:
                return row2Column2;
            }
        }
    }
};

[[nodiscard]] constexpr bool IsNear(Matrix3x3 lhs, Matrix3x3 rhs, Tolerance tolerance) noexcept
{
    return IsNear(lhs.row0Column0, rhs.row0Column0, tolerance) &&
           IsNear(lhs.row1Column0, rhs.row1Column0, tolerance) &&
           IsNear(lhs.row2Column0, rhs.row2Column0, tolerance) &&
           IsNear(lhs.row0Column1, rhs.row0Column1, tolerance) &&
           IsNear(lhs.row1Column1, rhs.row1Column1, tolerance) &&
           IsNear(lhs.row2Column1, rhs.row2Column1, tolerance) &&
           IsNear(lhs.row0Column2, rhs.row0Column2, tolerance) &&
           IsNear(lhs.row1Column2, rhs.row1Column2, tolerance) &&
           IsNear(lhs.row2Column2, rhs.row2Column2, tolerance);
}

[[nodiscard]] constexpr bool IsFinite(Matrix3x3 matrix) noexcept
{
    return IsFinite(matrix.row0Column0) && IsFinite(matrix.row1Column0) &&
           IsFinite(matrix.row2Column0) && IsFinite(matrix.row0Column1) &&
           IsFinite(matrix.row1Column1) && IsFinite(matrix.row2Column1) &&
           IsFinite(matrix.row0Column2) && IsFinite(matrix.row1Column2) &&
           IsFinite(matrix.row2Column2);
}

[[nodiscard]] constexpr Matrix3x3 operator+(Matrix3x3 lhs, Matrix3x3 rhs) noexcept
{
    return Matrix3x3{lhs.row0Column0 + rhs.row0Column0, lhs.row0Column1 + rhs.row0Column1,
                     lhs.row0Column2 + rhs.row0Column2, lhs.row1Column0 + rhs.row1Column0,
                     lhs.row1Column1 + rhs.row1Column1, lhs.row1Column2 + rhs.row1Column2,
                     lhs.row2Column0 + rhs.row2Column0, lhs.row2Column1 + rhs.row2Column1,
                     lhs.row2Column2 + rhs.row2Column2};
}

[[nodiscard]] constexpr Matrix3x3 operator-(Matrix3x3 lhs, Matrix3x3 rhs) noexcept
{
    return Matrix3x3{lhs.row0Column0 - rhs.row0Column0, lhs.row0Column1 - rhs.row0Column1,
                     lhs.row0Column2 - rhs.row0Column2, lhs.row1Column0 - rhs.row1Column0,
                     lhs.row1Column1 - rhs.row1Column1, lhs.row1Column2 - rhs.row1Column2,
                     lhs.row2Column0 - rhs.row2Column0, lhs.row2Column1 - rhs.row2Column1,
                     lhs.row2Column2 - rhs.row2Column2};
}

[[nodiscard]] constexpr Matrix3x3 operator*(Matrix3x3 matrix, float scalar) noexcept
{
    return Matrix3x3{
        matrix.row0Column0 * scalar, matrix.row0Column1 * scalar, matrix.row0Column2 * scalar,
        matrix.row1Column0 * scalar, matrix.row1Column1 * scalar, matrix.row1Column2 * scalar,
        matrix.row2Column0 * scalar, matrix.row2Column1 * scalar, matrix.row2Column2 * scalar};
}

[[nodiscard]] constexpr Matrix3x3 operator*(float scalar, Matrix3x3 matrix) noexcept
{
    return matrix * scalar;
}

[[nodiscard]] constexpr Matrix3x3 operator/(Matrix3x3 matrix, float scalar) noexcept
{
    return Matrix3x3{
        matrix.row0Column0 / scalar, matrix.row0Column1 / scalar, matrix.row0Column2 / scalar,
        matrix.row1Column0 / scalar, matrix.row1Column1 / scalar, matrix.row1Column2 / scalar,
        matrix.row2Column0 / scalar, matrix.row2Column1 / scalar, matrix.row2Column2 / scalar};
}

[[nodiscard]] constexpr Matrix3x3 Transpose(Matrix3x3 matrix) noexcept
{
    return Matrix3x3{matrix.row0Column0, matrix.row1Column0, matrix.row2Column0,
                     matrix.row0Column1, matrix.row1Column1, matrix.row2Column1,
                     matrix.row0Column2, matrix.row1Column2, matrix.row2Column2};
}

[[nodiscard]] constexpr float Determinant(Matrix3x3 matrix) noexcept
{
    return matrix.row0Column0 *
               (matrix.row1Column1 * matrix.row2Column2 - matrix.row1Column2 * matrix.row2Column1) -
           matrix.row0Column1 *
               (matrix.row1Column0 * matrix.row2Column2 - matrix.row1Column2 * matrix.row2Column0) +
           matrix.row0Column2 *
               (matrix.row1Column0 * matrix.row2Column1 - matrix.row1Column1 * matrix.row2Column0);
}

[[nodiscard]] constexpr Vector3 operator*(Matrix3x3 matrix, Vector3 vector) noexcept
{
    return Vector3{matrix.row0Column0 * vector.x + matrix.row0Column1 * vector.y +
                       matrix.row0Column2 * vector.z,
                   matrix.row1Column0 * vector.x + matrix.row1Column1 * vector.y +
                       matrix.row1Column2 * vector.z,
                   matrix.row2Column0 * vector.x + matrix.row2Column1 * vector.y +
                       matrix.row2Column2 * vector.z};
}

[[nodiscard]] constexpr Matrix3x3 operator*(Matrix3x3 lhs, Matrix3x3 rhs) noexcept
{
    return Matrix3x3{lhs * Vector3{rhs.row0Column0, rhs.row1Column0, rhs.row2Column0},
                     lhs * Vector3{rhs.row0Column1, rhs.row1Column1, rhs.row2Column1},
                     lhs * Vector3{rhs.row0Column2, rhs.row1Column2, rhs.row2Column2}};
}

[[nodiscard]] inline core::Result<Matrix3x3> Inverse(Matrix3x3 matrix)
{
    if (!IsFinite(matrix)) [[unlikely]]
    {
        return core::Result<Matrix3x3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Matrix3x3 inverse requires finite components."});
    }

    const double m00 = static_cast<double>(matrix.row0Column0);
    const double m01 = static_cast<double>(matrix.row0Column1);
    const double m02 = static_cast<double>(matrix.row0Column2);
    const double m10 = static_cast<double>(matrix.row1Column0);
    const double m11 = static_cast<double>(matrix.row1Column1);
    const double m12 = static_cast<double>(matrix.row1Column2);
    const double m20 = static_cast<double>(matrix.row2Column0);
    const double m21 = static_cast<double>(matrix.row2Column1);
    const double m22 = static_cast<double>(matrix.row2Column2);

    const double c00 = m11 * m22 - m12 * m21;
    const double c01 = m02 * m21 - m01 * m22;
    const double c02 = m01 * m12 - m02 * m11;
    const double c10 = m12 * m20 - m10 * m22;
    const double c11 = m00 * m22 - m02 * m20;
    const double c12 = m02 * m10 - m00 * m12;
    const double c20 = m10 * m21 - m11 * m20;
    const double c21 = m01 * m20 - m00 * m21;
    const double c22 = m00 * m11 - m01 * m10;
    const double determinant = m00 * c00 + m01 * c10 + m02 * c20;

    if (determinant == 0.0) [[unlikely]]
    {
        return core::Result<Matrix3x3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::SingularMatrix),
                        "Matrix3x3 inverse requires a nonsingular matrix."});
    }

    const double inverseDeterminant = 1.0 / determinant;
    const Matrix3x3 inverse{
        static_cast<float>(c00 * inverseDeterminant), static_cast<float>(c01 * inverseDeterminant),
        static_cast<float>(c02 * inverseDeterminant), static_cast<float>(c10 * inverseDeterminant),
        static_cast<float>(c11 * inverseDeterminant), static_cast<float>(c12 * inverseDeterminant),
        static_cast<float>(c20 * inverseDeterminant), static_cast<float>(c21 * inverseDeterminant),
        static_cast<float>(c22 * inverseDeterminant)};

    if (!IsFinite(inverse)) [[unlikely]]
    {
        return core::Result<Matrix3x3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::SingularMatrix),
                        "Matrix3x3 inverse is not representable as finite floats."});
    }

    return inverse;
}
} // namespace pond::math
