#pragma once

#include <ponder/math/Angle.hpp>
#include <ponder/math/Matrix3x3.hpp>
#include <ponder/math/Vector3.hpp>
#include <ponder/math/Vector4.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <utility>

namespace pond::math
{
class Quaternion;

enum class ProjectionDepth
{
    ForwardZ,
    ReverseZ
};

struct Matrix4x4 final
{
    float row0Column0{0.0F};
    float row1Column0{0.0F};
    float row2Column0{0.0F};
    float row3Column0{0.0F};
    float row0Column1{0.0F};
    float row1Column1{0.0F};
    float row2Column1{0.0F};
    float row3Column1{0.0F};
    float row0Column2{0.0F};
    float row1Column2{0.0F};
    float row2Column2{0.0F};
    float row3Column2{0.0F};
    float row0Column3{0.0F};
    float row1Column3{0.0F};
    float row2Column3{0.0F};
    float row3Column3{0.0F};

    constexpr Matrix4x4() noexcept = default;

    explicit constexpr Matrix4x4(float row0Column0Value, float row0Column1Value,
                                 float row0Column2Value, float row0Column3Value,
                                 float row1Column0Value, float row1Column1Value,
                                 float row1Column2Value, float row1Column3Value,
                                 float row2Column0Value, float row2Column1Value,
                                 float row2Column2Value, float row2Column3Value,
                                 float row3Column0Value, float row3Column1Value,
                                 float row3Column2Value, float row3Column3Value) noexcept
        : row0Column0(row0Column0Value), row1Column0(row1Column0Value),
          row2Column0(row2Column0Value), row3Column0(row3Column0Value),
          row0Column1(row0Column1Value), row1Column1(row1Column1Value),
          row2Column1(row2Column1Value), row3Column1(row3Column1Value),
          row0Column2(row0Column2Value), row1Column2(row1Column2Value),
          row2Column2(row2Column2Value), row3Column2(row3Column2Value),
          row0Column3(row0Column3Value), row1Column3(row1Column3Value),
          row2Column3(row2Column3Value), row3Column3(row3Column3Value)
    {
    }

    explicit constexpr Matrix4x4(Vector4 column0, Vector4 column1, Vector4 column2,
                                 Vector4 column3) noexcept
        : row0Column0(column0.x), row1Column0(column0.y), row2Column0(column0.z),
          row3Column0(column0.w), row0Column1(column1.x), row1Column1(column1.y),
          row2Column1(column1.z), row3Column1(column1.w), row0Column2(column2.x),
          row1Column2(column2.y), row2Column2(column2.z), row3Column2(column2.w),
          row0Column3(column3.x), row1Column3(column3.y), row2Column3(column3.z),
          row3Column3(column3.w)
    {
    }

    [[nodiscard]] static constexpr Matrix4x4 Zero() noexcept
    {
        return Matrix4x4{};
    }

    [[nodiscard]] static constexpr Matrix4x4 Identity() noexcept
    {
        return Matrix4x4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                         0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    }

    [[nodiscard]] static constexpr Matrix4x4 Translation(Vector3 translation) noexcept
    {
        return Matrix4x4{1.0F, 0.0F, 0.0F, translation.x, 0.0F, 1.0F, 0.0F, translation.y,
                         0.0F, 0.0F, 1.0F, translation.z, 0.0F, 0.0F, 0.0F, 1.0F};
    }

    [[nodiscard]] static constexpr Matrix4x4 Scale(Vector3 scale) noexcept
    {
        return Matrix4x4{scale.x, 0.0F, 0.0F,    0.0F, 0.0F, scale.y, 0.0F, 0.0F,
                         0.0F,    0.0F, scale.z, 0.0F, 0.0F, 0.0F,    0.0F, 1.0F};
    }

    [[nodiscard]] static inline Matrix4x4 RotationX(Radians angle) noexcept
    {
        const float sine = std::sin(angle.GetValue());
        const float cosine = std::cos(angle.GetValue());
        return Matrix4x4{1.0F, 0.0F, 0.0F,   0.0F, 0.0F, cosine, -sine, 0.0F,
                         0.0F, sine, cosine, 0.0F, 0.0F, 0.0F,   0.0F,  1.0F};
    }

    [[nodiscard]] static inline Matrix4x4 RotationY(Radians angle) noexcept
    {
        const float sine = std::sin(angle.GetValue());
        const float cosine = std::cos(angle.GetValue());
        return Matrix4x4{cosine, 0.0F, sine,   0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                         -sine,  0.0F, cosine, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    }

    [[nodiscard]] static inline Matrix4x4 RotationZ(Radians angle) noexcept
    {
        const float sine = std::sin(angle.GetValue());
        const float cosine = std::cos(angle.GetValue());
        return Matrix4x4{cosine, -sine, 0.0F, 0.0F, sine, cosine, 0.0F, 0.0F,
                         0.0F,   0.0F,  1.0F, 0.0F, 0.0F, 0.0F,   0.0F, 1.0F};
    }

    [[nodiscard]] static constexpr Matrix4x4 Rotation(Quaternion rotation) noexcept;

    [[nodiscard]] static inline core::Result<Matrix4x4> LookAt(Vector3 eye, Vector3 target,
                                                               Vector3 up)
    {
        if (!IsFiniteVector3(eye) || !IsFiniteVector3(target) || !IsFiniteVector3(up)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewNonFiniteError());
        }

        return LookTo(eye, target - eye, up);
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> LookTo(Vector3 eye, Vector3 direction,
                                                               Vector3 up)
    {
        if (!IsFiniteVector3(eye) || !IsFiniteVector3(direction) || !IsFiniteVector3(up)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewNonFiniteError());
        }

        auto forward = Normalize(direction);
        if (!forward.HasValue()) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewDegenerateError());
        }

        auto upDirection = Normalize(up);
        if (!upDirection.HasValue()) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewDegenerateError());
        }

        auto right = Normalize(Cross(forward.GetValue(), upDirection.GetValue()));
        if (!right.HasValue()) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewDegenerateError());
        }

        const Vector3 correctedUp = Cross(right.GetValue(), forward.GetValue());
        const Vector3 backward = -forward.GetValue();
        return Matrix4x4{right->x,      right->y,      right->z,      -Dot(right.GetValue(), eye),
                         correctedUp.x, correctedUp.y, correctedUp.z, -Dot(correctedUp, eye),
                         backward.x,    backward.y,    backward.z,    -Dot(backward, eye),
                         0.0F,          0.0F,          0.0F,          1.0F};
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> Perspective(Radians verticalFieldOfView,
                                                                    float aspectRatio,
                                                                    float nearDistance,
                                                                    float farDistance,
                                                                    ProjectionDepth depth)
    {
        if (!IsFinite(verticalFieldOfView.GetValue()) || !IsFinite(aspectRatio) ||
            !IsFinite(nearDistance) || !IsFinite(farDistance)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionNonFiniteError());
        }

        if (verticalFieldOfView.GetValue() <= 0.0F || verticalFieldOfView.GetValue() >= kPi ||
            aspectRatio <= 0.0F || nearDistance <= 0.0F || farDistance <= nearDistance) [[unlikely]]
        {
            [[unlikely]] return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        const float yScale = 1.0F / std::tan(verticalFieldOfView.GetValue() * 0.5F);
        const float xScale = yScale / aspectRatio;
        switch (depth)
        {
        case ProjectionDepth::ForwardZ:
            return MakeProjectionMatrix(
                Matrix4x4{xScale, 0.0F, 0.0F, 0.0F, 0.0F, yScale, 0.0F, 0.0F, 0.0F, 0.0F,
                          farDistance / (nearDistance - farDistance),
                          (nearDistance * farDistance) / (nearDistance - farDistance), 0.0F, 0.0F,
                          -1.0F, 0.0F});
        case ProjectionDepth::ReverseZ:
            return MakeProjectionMatrix(
                Matrix4x4{xScale, 0.0F, 0.0F, 0.0F, 0.0F, yScale, 0.0F, 0.0F, 0.0F, 0.0F,
                          nearDistance / (farDistance - nearDistance),
                          (nearDistance * farDistance) / (farDistance - nearDistance), 0.0F, 0.0F,
                          -1.0F, 0.0F});
        }

        [[unlikely]] return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> InfinitePerspective(
        Radians verticalFieldOfView, float aspectRatio, float nearDistance, ProjectionDepth depth)
    {
        if (!IsFinite(verticalFieldOfView.GetValue()) || !IsFinite(aspectRatio) ||
            !IsFinite(nearDistance)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionNonFiniteError());
        }

        if (verticalFieldOfView.GetValue() <= 0.0F || verticalFieldOfView.GetValue() >= kPi ||
            aspectRatio <= 0.0F || nearDistance <= 0.0F) [[unlikely]]
        {
            [[unlikely]] return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        const float yScale = 1.0F / std::tan(verticalFieldOfView.GetValue() * 0.5F);
        const float xScale = yScale / aspectRatio;
        switch (depth)
        {
        case ProjectionDepth::ForwardZ:
            return MakeProjectionMatrix(Matrix4x4{xScale, 0.0F, 0.0F, 0.0F, 0.0F, yScale, 0.0F,
                                                  0.0F, 0.0F, 0.0F, -1.0F, -nearDistance, 0.0F,
                                                  0.0F, -1.0F, 0.0F});
        case ProjectionDepth::ReverseZ:
            return MakeProjectionMatrix(Matrix4x4{xScale, 0.0F, 0.0F, 0.0F, 0.0F, yScale, 0.0F,
                                                  0.0F, 0.0F, 0.0F, 0.0F, nearDistance, 0.0F, 0.0F,
                                                  -1.0F, 0.0F});
        }

        [[unlikely]] return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> Orthographic(float left, float right,
                                                                     float bottom, float top,
                                                                     float nearDistance,
                                                                     float farDistance,
                                                                     ProjectionDepth depth)
    {
        if (!IsFinite(left) || !IsFinite(right) || !IsFinite(bottom) || !IsFinite(top) ||
            !IsFinite(nearDistance) || !IsFinite(farDistance)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionNonFiniteError());
        }

        if (right <= left || top <= bottom || nearDistance <= 0.0F || farDistance <= nearDistance) [[unlikely]]
        {
            [[unlikely]] return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        const float width = right - left;
        const float height = top - bottom;
        const float xScale = 2.0F / width;
        const float yScale = 2.0F / height;
        const float xOffset = -(right + left) / width;
        const float yOffset = -(top + bottom) / height;
        switch (depth)
        {
        case ProjectionDepth::ForwardZ:
            return MakeProjectionMatrix(
                Matrix4x4{xScale, 0.0F, 0.0F, xOffset, 0.0F, yScale, 0.0F, yOffset, 0.0F, 0.0F,
                          1.0F / (nearDistance - farDistance),
                          nearDistance / (nearDistance - farDistance), 0.0F, 0.0F, 0.0F, 1.0F});
        case ProjectionDepth::ReverseZ:
            return MakeProjectionMatrix(
                Matrix4x4{xScale, 0.0F, 0.0F, xOffset, 0.0F, yScale, 0.0F, yOffset, 0.0F, 0.0F,
                          1.0F / (farDistance - nearDistance),
                          farDistance / (farDistance - nearDistance), 0.0F, 0.0F, 0.0F, 1.0F});
        }

        [[unlikely]] return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
    }

    [[nodiscard]] core::Result<Vector4> Row(std::size_t row) const
    {
        switch (row)
        {
        case 0:
            return Vector4{row0Column0, row0Column1, row0Column2, row0Column3};
        case 1:
            return Vector4{row1Column0, row1Column1, row1Column2, row1Column3};
        case 2:
            return Vector4{row2Column0, row2Column1, row2Column2, row2Column3};
        case 3:
            return Vector4{row3Column0, row3Column1, row3Column2, row3Column3};
        default:
            [[unlikely]] return core::Result<Vector4>::FromError(MakeIndexError());
        }
    }

    [[nodiscard]] core::Result<Vector4> Column(std::size_t column) const
    {
        switch (column)
        {
        case 0:
            return Vector4{row0Column0, row1Column0, row2Column0, row3Column0};
        case 1:
            return Vector4{row0Column1, row1Column1, row2Column1, row3Column1};
        case 2:
            return Vector4{row0Column2, row1Column2, row2Column2, row3Column2};
        case 3:
            return Vector4{row0Column3, row1Column3, row2Column3, row3Column3};
        default:
            [[unlikely]] return core::Result<Vector4>::FromError(MakeIndexError());
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

    [[nodiscard]] friend constexpr bool operator==(const Matrix4x4& lhs,
                                                   const Matrix4x4& rhs) noexcept = default;

private:
    inline static constexpr std::size_t kDimension{4};

    [[nodiscard]] static core::Error MakeIndexError()
    {
        return core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                           "Matrix4x4 index is out of range."};
    }

    [[nodiscard]] static constexpr bool IsFiniteVector3(Vector3 vector) noexcept
    {
        return IsFinite(vector.x) && IsFinite(vector.y) && IsFinite(vector.z);
    }

    [[nodiscard]] static core::Error MakeViewNonFiniteError()
    {
        return core::Error{
            ToErrorCode(MathErrorCode::NonFiniteInput),
            "Matrix4x4 view construction requires finite eye, direction, target, and up."};
    }

    [[nodiscard]] static core::Error MakeViewDegenerateError()
    {
        return core::Error{
            ToErrorCode(MathErrorCode::DegenerateInput),
            "Matrix4x4 view construction requires non-zero, non-parallel direction and up."};
    }

    [[nodiscard]] static core::Error MakeProjectionNonFiniteError()
    {
        return core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                           "Matrix4x4 projection construction requires finite parameters."};
    }

    [[nodiscard]] static core::Error MakeProjectionInvalidArgumentError()
    {
        return core::Error{
            ToErrorCode(MathErrorCode::InvalidArgument),
            "Matrix4x4 projection construction parameters are outside the valid domain."};
    }

    [[nodiscard]] static constexpr bool IsFiniteMatrix(Matrix4x4 matrix) noexcept
    {
        return IsFinite(matrix.row0Column0) && IsFinite(matrix.row1Column0) &&
               IsFinite(matrix.row2Column0) && IsFinite(matrix.row3Column0) &&
               IsFinite(matrix.row0Column1) && IsFinite(matrix.row1Column1) &&
               IsFinite(matrix.row2Column1) && IsFinite(matrix.row3Column1) &&
               IsFinite(matrix.row0Column2) && IsFinite(matrix.row1Column2) &&
               IsFinite(matrix.row2Column2) && IsFinite(matrix.row3Column2) &&
               IsFinite(matrix.row0Column3) && IsFinite(matrix.row1Column3) &&
               IsFinite(matrix.row2Column3) && IsFinite(matrix.row3Column3);
    }

    [[nodiscard]] static core::Result<Matrix4x4> MakeProjectionMatrix(Matrix4x4 matrix)
    {
        if (!IsFiniteMatrix(matrix)) [[unlikely]]
        {
            [[unlikely]] return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        return matrix;
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
            case 2:
                return row2Column0;
            default:
                return row3Column0;
            }
        case 1:
            switch (row)
            {
            case 0:
                return row0Column1;
            case 1:
                return row1Column1;
            case 2:
                return row2Column1;
            default:
                return row3Column1;
            }
        case 2:
            switch (row)
            {
            case 0:
                return row0Column2;
            case 1:
                return row1Column2;
            case 2:
                return row2Column2;
            default:
                return row3Column2;
            }
        default:
            switch (row)
            {
            case 0:
                return row0Column3;
            case 1:
                return row1Column3;
            case 2:
                return row2Column3;
            default:
                return row3Column3;
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
            case 2:
                return row2Column0;
            default:
                return row3Column0;
            }
        case 1:
            switch (row)
            {
            case 0:
                return row0Column1;
            case 1:
                return row1Column1;
            case 2:
                return row2Column1;
            default:
                return row3Column1;
            }
        case 2:
            switch (row)
            {
            case 0:
                return row0Column2;
            case 1:
                return row1Column2;
            case 2:
                return row2Column2;
            default:
                return row3Column2;
            }
        default:
            switch (row)
            {
            case 0:
                return row0Column3;
            case 1:
                return row1Column3;
            case 2:
                return row2Column3;
            default:
                return row3Column3;
            }
        }
    }
};

[[nodiscard]] constexpr bool IsNear(Matrix4x4 lhs, Matrix4x4 rhs, Tolerance tolerance) noexcept
{
    return IsNear(lhs.row0Column0, rhs.row0Column0, tolerance) &&
           IsNear(lhs.row1Column0, rhs.row1Column0, tolerance) &&
           IsNear(lhs.row2Column0, rhs.row2Column0, tolerance) &&
           IsNear(lhs.row3Column0, rhs.row3Column0, tolerance) &&
           IsNear(lhs.row0Column1, rhs.row0Column1, tolerance) &&
           IsNear(lhs.row1Column1, rhs.row1Column1, tolerance) &&
           IsNear(lhs.row2Column1, rhs.row2Column1, tolerance) &&
           IsNear(lhs.row3Column1, rhs.row3Column1, tolerance) &&
           IsNear(lhs.row0Column2, rhs.row0Column2, tolerance) &&
           IsNear(lhs.row1Column2, rhs.row1Column2, tolerance) &&
           IsNear(lhs.row2Column2, rhs.row2Column2, tolerance) &&
           IsNear(lhs.row3Column2, rhs.row3Column2, tolerance) &&
           IsNear(lhs.row0Column3, rhs.row0Column3, tolerance) &&
           IsNear(lhs.row1Column3, rhs.row1Column3, tolerance) &&
           IsNear(lhs.row2Column3, rhs.row2Column3, tolerance) &&
           IsNear(lhs.row3Column3, rhs.row3Column3, tolerance);
}
[[nodiscard]] constexpr bool IsFinite(Matrix4x4 matrix) noexcept
{
    return IsFinite(matrix.row0Column0) && IsFinite(matrix.row1Column0) &&
           IsFinite(matrix.row2Column0) && IsFinite(matrix.row3Column0) &&
           IsFinite(matrix.row0Column1) && IsFinite(matrix.row1Column1) &&
           IsFinite(matrix.row2Column1) && IsFinite(matrix.row3Column1) &&
           IsFinite(matrix.row0Column2) && IsFinite(matrix.row1Column2) &&
           IsFinite(matrix.row2Column2) && IsFinite(matrix.row3Column2) &&
           IsFinite(matrix.row0Column3) && IsFinite(matrix.row1Column3) &&
           IsFinite(matrix.row2Column3) && IsFinite(matrix.row3Column3);
}

[[nodiscard]] constexpr Matrix4x4 operator+(Matrix4x4 lhs, Matrix4x4 rhs) noexcept
{
    return Matrix4x4{lhs.row0Column0 + rhs.row0Column0, lhs.row0Column1 + rhs.row0Column1,
                     lhs.row0Column2 + rhs.row0Column2, lhs.row0Column3 + rhs.row0Column3,
                     lhs.row1Column0 + rhs.row1Column0, lhs.row1Column1 + rhs.row1Column1,
                     lhs.row1Column2 + rhs.row1Column2, lhs.row1Column3 + rhs.row1Column3,
                     lhs.row2Column0 + rhs.row2Column0, lhs.row2Column1 + rhs.row2Column1,
                     lhs.row2Column2 + rhs.row2Column2, lhs.row2Column3 + rhs.row2Column3,
                     lhs.row3Column0 + rhs.row3Column0, lhs.row3Column1 + rhs.row3Column1,
                     lhs.row3Column2 + rhs.row3Column2, lhs.row3Column3 + rhs.row3Column3};
}

[[nodiscard]] constexpr Matrix4x4 operator-(Matrix4x4 lhs, Matrix4x4 rhs) noexcept
{
    return Matrix4x4{lhs.row0Column0 - rhs.row0Column0, lhs.row0Column1 - rhs.row0Column1,
                     lhs.row0Column2 - rhs.row0Column2, lhs.row0Column3 - rhs.row0Column3,
                     lhs.row1Column0 - rhs.row1Column0, lhs.row1Column1 - rhs.row1Column1,
                     lhs.row1Column2 - rhs.row1Column2, lhs.row1Column3 - rhs.row1Column3,
                     lhs.row2Column0 - rhs.row2Column0, lhs.row2Column1 - rhs.row2Column1,
                     lhs.row2Column2 - rhs.row2Column2, lhs.row2Column3 - rhs.row2Column3,
                     lhs.row3Column0 - rhs.row3Column0, lhs.row3Column1 - rhs.row3Column1,
                     lhs.row3Column2 - rhs.row3Column2, lhs.row3Column3 - rhs.row3Column3};
}

[[nodiscard]] constexpr Matrix4x4 operator*(Matrix4x4 matrix, float scalar) noexcept
{
    return Matrix4x4{
        matrix.row0Column0 * scalar, matrix.row0Column1 * scalar, matrix.row0Column2 * scalar,
        matrix.row0Column3 * scalar, matrix.row1Column0 * scalar, matrix.row1Column1 * scalar,
        matrix.row1Column2 * scalar, matrix.row1Column3 * scalar, matrix.row2Column0 * scalar,
        matrix.row2Column1 * scalar, matrix.row2Column2 * scalar, matrix.row2Column3 * scalar,
        matrix.row3Column0 * scalar, matrix.row3Column1 * scalar, matrix.row3Column2 * scalar,
        matrix.row3Column3 * scalar};
}

[[nodiscard]] constexpr Matrix4x4 operator*(float scalar, Matrix4x4 matrix) noexcept
{
    return matrix * scalar;
}

[[nodiscard]] constexpr Matrix4x4 operator/(Matrix4x4 matrix, float scalar) noexcept
{
    return Matrix4x4{
        matrix.row0Column0 / scalar, matrix.row0Column1 / scalar, matrix.row0Column2 / scalar,
        matrix.row0Column3 / scalar, matrix.row1Column0 / scalar, matrix.row1Column1 / scalar,
        matrix.row1Column2 / scalar, matrix.row1Column3 / scalar, matrix.row2Column0 / scalar,
        matrix.row2Column1 / scalar, matrix.row2Column2 / scalar, matrix.row2Column3 / scalar,
        matrix.row3Column0 / scalar, matrix.row3Column1 / scalar, matrix.row3Column2 / scalar,
        matrix.row3Column3 / scalar};
}

[[nodiscard]] constexpr Matrix4x4 Transpose(Matrix4x4 matrix) noexcept
{
    return Matrix4x4{
        matrix.row0Column0, matrix.row1Column0, matrix.row2Column0, matrix.row3Column0,
        matrix.row0Column1, matrix.row1Column1, matrix.row2Column1, matrix.row3Column1,
        matrix.row0Column2, matrix.row1Column2, matrix.row2Column2, matrix.row3Column2,
        matrix.row0Column3, matrix.row1Column3, matrix.row2Column3, matrix.row3Column3};
}

namespace detail
{
[[nodiscard]] constexpr float Determinant3x3(float row0Column0, float row0Column1,
                                             float row0Column2, float row1Column0,
                                             float row1Column1, float row1Column2,
                                             float row2Column0, float row2Column1,
                                             float row2Column2) noexcept
{
    return row0Column0 * (row1Column1 * row2Column2 - row1Column2 * row2Column1) -
           row0Column1 * (row1Column0 * row2Column2 - row1Column2 * row2Column0) +
           row0Column2 * (row1Column0 * row2Column1 - row1Column1 * row2Column0);
}

using Matrix4x4AugmentedRows = std::array<std::array<double, 8>, 4>;

[[nodiscard]] constexpr double Abs(double value) noexcept
{
    return value < 0.0 ? -value : value;
}

[[nodiscard]] inline bool IsFinite(Matrix4x4AugmentedRows rows) noexcept
{
    for (const auto& row : rows)
    {
        for (double value : row)
        {
            if (!std::isfinite(value))
            {
                return false;
            }
        }
    }

    return true;
}
} // namespace detail

[[nodiscard]] constexpr float Determinant(Matrix4x4 matrix) noexcept
{
    return matrix.row0Column0 *
               detail::Determinant3x3(matrix.row1Column1, matrix.row1Column2, matrix.row1Column3,
                                      matrix.row2Column1, matrix.row2Column2, matrix.row2Column3,
                                      matrix.row3Column1, matrix.row3Column2, matrix.row3Column3) -
           matrix.row0Column1 *
               detail::Determinant3x3(matrix.row1Column0, matrix.row1Column2, matrix.row1Column3,
                                      matrix.row2Column0, matrix.row2Column2, matrix.row2Column3,
                                      matrix.row3Column0, matrix.row3Column2, matrix.row3Column3) +
           matrix.row0Column2 *
               detail::Determinant3x3(matrix.row1Column0, matrix.row1Column1, matrix.row1Column3,
                                      matrix.row2Column0, matrix.row2Column1, matrix.row2Column3,
                                      matrix.row3Column0, matrix.row3Column1, matrix.row3Column3) -
           matrix.row0Column3 *
               detail::Determinant3x3(matrix.row1Column0, matrix.row1Column1, matrix.row1Column2,
                                      matrix.row2Column0, matrix.row2Column1, matrix.row2Column2,
                                      matrix.row3Column0, matrix.row3Column1, matrix.row3Column2);
}

[[nodiscard]] constexpr Vector4 operator*(Matrix4x4 matrix, Vector4 vector) noexcept
{
    return Vector4{matrix.row0Column0 * vector.x + matrix.row0Column1 * vector.y +
                       matrix.row0Column2 * vector.z + matrix.row0Column3 * vector.w,
                   matrix.row1Column0 * vector.x + matrix.row1Column1 * vector.y +
                       matrix.row1Column2 * vector.z + matrix.row1Column3 * vector.w,
                   matrix.row2Column0 * vector.x + matrix.row2Column1 * vector.y +
                       matrix.row2Column2 * vector.z + matrix.row2Column3 * vector.w,
                   matrix.row3Column0 * vector.x + matrix.row3Column1 * vector.y +
                       matrix.row3Column2 * vector.z + matrix.row3Column3 * vector.w};
}

[[nodiscard]] constexpr Matrix4x4 operator*(Matrix4x4 lhs, Matrix4x4 rhs) noexcept
{
    return Matrix4x4{
        lhs * Vector4{rhs.row0Column0, rhs.row1Column0, rhs.row2Column0, rhs.row3Column0},
        lhs * Vector4{rhs.row0Column1, rhs.row1Column1, rhs.row2Column1, rhs.row3Column1},
        lhs * Vector4{rhs.row0Column2, rhs.row1Column2, rhs.row2Column2, rhs.row3Column2},
        lhs * Vector4{rhs.row0Column3, rhs.row1Column3, rhs.row2Column3, rhs.row3Column3}};
}

[[nodiscard]] constexpr Vector4 TransformPointToClip(Matrix4x4 worldToClip, Vector3 point) noexcept
{
    return worldToClip * Vector4{point.x, point.y, point.z, 1.0F};
}

[[nodiscard]] constexpr Vector3 TransformPoint(Matrix4x4 transform, Vector3 point) noexcept
{
    const Vector4 transformed = TransformPointToClip(transform, point);
    return Vector3{transformed.x, transformed.y, transformed.z};
}

[[nodiscard]] constexpr Vector3 TransformVector(Matrix4x4 transform, Vector3 vector) noexcept
{
    const Vector4 transformed = transform * Vector4{vector.x, vector.y, vector.z, 0.0F};
    return Vector3{transformed.x, transformed.y, transformed.z};
}

[[nodiscard]] inline core::Result<Vector3> PerspectiveDivide(Vector4 clip)
{
    constexpr float kMinimumUsableHomogeneousW = std::numeric_limits<float>::min();
    const auto abs = [](float value) noexcept
    {
        return value < 0.0F ? -value : value;
    };

    if (!IsFinite(clip.w) || abs(clip.w) < kMinimumUsableHomogeneousW) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(core::Error{
            ToErrorCode(MathErrorCode::UndefinedHomogeneousCoordinate),
            "Perspective division requires a finite, usable homogeneous w coordinate."});
    }

    if (!IsFinite(clip.x) || !IsFinite(clip.y) || !IsFinite(clip.z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Perspective division requires finite clip coordinates."});
    }

    const Vector3 ndc{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
    if (!IsFinite(ndc.x) || !IsFinite(ndc.y) || !IsFinite(ndc.z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(core::Error{
            ToErrorCode(MathErrorCode::UndefinedHomogeneousCoordinate),
            "Perspective division result is not representable as finite NDC coordinates."});
    }

    return ndc;
}

[[nodiscard]] inline core::Result<Vector3> TransformPointToNdc(Matrix4x4 worldToClip, Vector3 point)
{
    return PerspectiveDivide(TransformPointToClip(worldToClip, point));
}

[[nodiscard]] inline core::Result<Vector3> TransformNormal(Matrix4x4 transform, Vector3 normal)
{
    const Matrix3x3 linearPart{transform.row0Column0, transform.row0Column1, transform.row0Column2,
                               transform.row1Column0, transform.row1Column1, transform.row1Column2,
                               transform.row2Column0, transform.row2Column1, transform.row2Column2};
    auto inverse = Inverse(linearPart);
    if (!inverse.HasValue()) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(inverse.GetError());
    }

    return Transpose(inverse.GetValue()) * normal;
}

[[nodiscard]] inline core::Result<Matrix4x4> Inverse(Matrix4x4 matrix)
{
    if (!IsFinite(matrix)) [[unlikely]]
    {
        return core::Result<Matrix4x4>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Matrix4x4 inverse requires finite components."});
    }

    detail::Matrix4x4AugmentedRows rows{{
        {{matrix.row0Column0, matrix.row0Column1, matrix.row0Column2, matrix.row0Column3, 1.0, 0.0,
          0.0, 0.0}},
        {{matrix.row1Column0, matrix.row1Column1, matrix.row1Column2, matrix.row1Column3, 0.0, 1.0,
          0.0, 0.0}},
        {{matrix.row2Column0, matrix.row2Column1, matrix.row2Column2, matrix.row2Column3, 0.0, 0.0,
          1.0, 0.0}},
        {{matrix.row3Column0, matrix.row3Column1, matrix.row3Column2, matrix.row3Column3, 0.0, 0.0,
          0.0, 1.0}},
    }};

    for (std::size_t pivotIndex = 0; pivotIndex < 4; ++pivotIndex)
    {
        std::size_t pivotRow = pivotIndex;
        double pivotMagnitude = detail::Abs(rows[pivotRow][pivotIndex]);
        for (std::size_t row = pivotIndex + 1; row < 4; ++row)
        {
            const double candidateMagnitude = detail::Abs(rows[row][pivotIndex]);
            if (pivotMagnitude < candidateMagnitude)
            {
                pivotMagnitude = candidateMagnitude;
                pivotRow = row;
            }
        }

        if (pivotMagnitude == 0.0) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(
                core::Error{ToErrorCode(MathErrorCode::SingularMatrix),
                            "Matrix4x4 inverse requires a nonsingular matrix."});
        }

        if (pivotRow != pivotIndex)
        {
            std::swap(rows[pivotIndex], rows[pivotRow]);
        }

        const double pivot = rows[pivotIndex][pivotIndex];
        for (double& value : rows[pivotIndex])
        {
            value /= pivot;
        }

        for (std::size_t row = 0; row < 4; ++row)
        {
            if (row == pivotIndex)
            {
                continue;
            }

            const double factor = rows[row][pivotIndex];
            if (factor == 0.0)
            {
                continue;
            }

            for (std::size_t column = 0; column < 8; ++column)
            {
                rows[row][column] -= factor * rows[pivotIndex][column];
            }
        }

        if (!detail::IsFinite(rows)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(
                core::Error{ToErrorCode(MathErrorCode::SingularMatrix),
                            "Matrix4x4 inverse is not representable as finite floats."});
        }
    }

    const Matrix4x4 inverse{static_cast<float>(rows[0][4]), static_cast<float>(rows[0][5]),
                            static_cast<float>(rows[0][6]), static_cast<float>(rows[0][7]),
                            static_cast<float>(rows[1][4]), static_cast<float>(rows[1][5]),
                            static_cast<float>(rows[1][6]), static_cast<float>(rows[1][7]),
                            static_cast<float>(rows[2][4]), static_cast<float>(rows[2][5]),
                            static_cast<float>(rows[2][6]), static_cast<float>(rows[2][7]),
                            static_cast<float>(rows[3][4]), static_cast<float>(rows[3][5]),
                            static_cast<float>(rows[3][6]), static_cast<float>(rows[3][7])};

    if (!IsFinite(inverse)) [[unlikely]]
    {
        return core::Result<Matrix4x4>::FromError(
            core::Error{ToErrorCode(MathErrorCode::SingularMatrix),
                        "Matrix4x4 inverse is not representable as finite floats."});
    }

    return inverse;
}
} // namespace pond::math
