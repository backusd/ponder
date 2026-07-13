#pragma once

#include <ponder/math/Angle.hpp>
#include <ponder/math/MathError.hpp>
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

namespace detail
{
struct WideVector3 final
{
    double x{0.0};
    double y{0.0};
    double z{0.0};

    constexpr WideVector3() noexcept = default;

    constexpr WideVector3(double xValue, double yValue, double zValue) noexcept
        : x(xValue), y(yValue), z(zValue)
    {
    }
};

[[nodiscard]] constexpr double WideAbs(double value) noexcept
{
    return value < 0.0 ? -value : value;
}

[[nodiscard]] constexpr double WideMax(double lhs, double rhs) noexcept
{
    return lhs < rhs ? rhs : lhs;
}

[[nodiscard]] constexpr bool TryConvertFiniteFloat(double value, float& converted) noexcept
{
    constexpr double kLowestFloat = static_cast<double>(std::numeric_limits<float>::lowest());
    constexpr double kHighestFloat = static_cast<double>(std::numeric_limits<float>::max());
    if (!core::IsFinite(value) || value < kLowestFloat || value > kHighestFloat) [[unlikely]]
    {
        return false;
    }

    converted = static_cast<float>(value);
    return core::IsFinite(converted);
}

[[nodiscard]] constexpr bool TryConvertNonZeroFiniteFloat(double value, float& converted) noexcept
{
    return TryConvertFiniteFloat(value, converted) && converted != 0.0F;
}

[[nodiscard]] inline bool TryNormalize(WideVector3 value, WideVector3& normalized) noexcept
{
    const double maxMagnitude =
        WideMax(WideMax(WideAbs(value.x), WideAbs(value.y)), WideAbs(value.z));
    if (maxMagnitude == 0.0 || !core::IsFinite(maxMagnitude)) [[unlikely]]
    {
        return false;
    }

    const double scaledX = value.x / maxMagnitude;
    const double scaledY = value.y / maxMagnitude;
    const double scaledZ = value.z / maxMagnitude;
    const double length = std::sqrt(scaledX * scaledX + scaledY * scaledY + scaledZ * scaledZ);
    if (length == 0.0 || !core::IsFinite(length)) [[unlikely]]
    {
        return false;
    }

    normalized = WideVector3{scaledX / length, scaledY / length, scaledZ / length};
    return core::IsFinite(normalized.x) && core::IsFinite(normalized.y) &&
           core::IsFinite(normalized.z);
}

[[nodiscard]] constexpr WideVector3 Cross(WideVector3 lhs, WideVector3 rhs) noexcept
{
    return WideVector3{lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z,
                       lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] constexpr double Dot(WideVector3 lhs, Vector3 rhs) noexcept
{
    return lhs.x * static_cast<double>(rhs.x) + lhs.y * static_cast<double>(rhs.y) +
           lhs.z * static_cast<double>(rhs.z);
}
} // namespace detail

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

        return MakeView(eye,
                        detail::WideVector3{static_cast<double>(target.x) - eye.x,
                                            static_cast<double>(target.y) - eye.y,
                                            static_cast<double>(target.z) - eye.z},
                        up);
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> LookTo(Vector3 eye, Vector3 direction,
                                                               Vector3 up)
    {
        if (!IsFiniteVector3(eye) || !IsFiniteVector3(direction) || !IsFiniteVector3(up))
            [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewNonFiniteError());
        }

        return MakeView(eye, detail::WideVector3{direction.x, direction.y, direction.z}, up);
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> Perspective(Radians verticalFieldOfView,
                                                                    float aspectRatio,
                                                                    float nearDistance,
                                                                    float farDistance,
                                                                    ProjectionDepth depth)
    {
        if (!core::IsFinite(verticalFieldOfView.GetValue()) || !core::IsFinite(aspectRatio) ||
            !core::IsFinite(nearDistance) || !core::IsFinite(farDistance)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionNonFiniteError());
        }

        if (verticalFieldOfView.GetValue() <= 0.0F || verticalFieldOfView.GetValue() >= core::kPi ||
            aspectRatio <= 0.0F || nearDistance <= 0.0F || farDistance <= nearDistance) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        const double yScale =
            1.0 / std::tan(static_cast<double>(verticalFieldOfView.GetValue()) * 0.5);
        const double xScale = yScale / static_cast<double>(aspectRatio);
        const double nearValue = static_cast<double>(nearDistance);
        const double farValue = static_cast<double>(farDistance);
        const double depthSpan = farValue - nearValue;

        float xScaleFloat{0.0F};
        float yScaleFloat{0.0F};
        if (!detail::TryConvertNonZeroFiniteFloat(xScale, xScaleFloat) ||
            !detail::TryConvertNonZeroFiniteFloat(yScale, yScaleFloat)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        double depthScale{0.0};
        double depthOffset{0.0};
        switch (depth)
        {
        case ProjectionDepth::ForwardZ:
            depthScale = -farValue / depthSpan;
            depthOffset = -(nearValue * farValue) / depthSpan;
            break;
        case ProjectionDepth::ReverseZ:
            depthScale = nearValue / depthSpan;
            depthOffset = (nearValue * farValue) / depthSpan;
            break;
        default:
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        float depthScaleFloat{0.0F};
        float depthOffsetFloat{0.0F};
        if (!detail::TryConvertNonZeroFiniteFloat(depthScale, depthScaleFloat) ||
            !detail::TryConvertNonZeroFiniteFloat(depthOffset, depthOffsetFloat)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        return Matrix4x4{xScaleFloat,
                         0.0F,
                         0.0F,
                         0.0F,
                         0.0F,
                         yScaleFloat,
                         0.0F,
                         0.0F,
                         0.0F,
                         0.0F,
                         depthScaleFloat,
                         depthOffsetFloat,
                         0.0F,
                         0.0F,
                         -1.0F,
                         0.0F};
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> InfinitePerspective(
        Radians verticalFieldOfView, float aspectRatio, float nearDistance, ProjectionDepth depth)
    {
        if (!core::IsFinite(verticalFieldOfView.GetValue()) || !core::IsFinite(aspectRatio) ||
            !core::IsFinite(nearDistance)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionNonFiniteError());
        }

        if (verticalFieldOfView.GetValue() <= 0.0F || verticalFieldOfView.GetValue() >= core::kPi ||
            aspectRatio <= 0.0F || nearDistance <= 0.0F) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        const double yScale =
            1.0 / std::tan(static_cast<double>(verticalFieldOfView.GetValue()) * 0.5);
        const double xScale = yScale / static_cast<double>(aspectRatio);
        float xScaleFloat{0.0F};
        float yScaleFloat{0.0F};
        if (!detail::TryConvertNonZeroFiniteFloat(xScale, xScaleFloat) ||
            !detail::TryConvertNonZeroFiniteFloat(yScale, yScaleFloat)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        switch (depth)
        {
        case ProjectionDepth::ForwardZ:
            return Matrix4x4{xScaleFloat, 0.0F, 0.0F,  0.0F, 0.0F,  yScaleFloat,
                             0.0F,        0.0F, 0.0F,  0.0F, -1.0F, -nearDistance,
                             0.0F,        0.0F, -1.0F, 0.0F};
        case ProjectionDepth::ReverseZ:
            return Matrix4x4{xScaleFloat, 0.0F, 0.0F, 0.0F,         0.0F, yScaleFloat, 0.0F,  0.0F,
                             0.0F,        0.0F, 0.0F, nearDistance, 0.0F, 0.0F,        -1.0F, 0.0F};
        default:
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }
    }

    [[nodiscard]] static constexpr core::Result<Matrix4x4> Orthographic(float left, float right,
                                                                        float bottom, float top,
                                                                        float nearDistance,
                                                                        float farDistance,
                                                                        ProjectionDepth depth)
    {
        if (!core::IsFinite(left) || !core::IsFinite(right) || !core::IsFinite(bottom) ||
            !core::IsFinite(top) || !core::IsFinite(nearDistance) || !core::IsFinite(farDistance))
            [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionNonFiniteError());
        }

        if (right <= left || top <= bottom || nearDistance <= 0.0F || farDistance <= nearDistance)
            [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        const double leftValue = static_cast<double>(left);
        const double rightValue = static_cast<double>(right);
        const double bottomValue = static_cast<double>(bottom);
        const double topValue = static_cast<double>(top);
        const double nearValue = static_cast<double>(nearDistance);
        const double farValue = static_cast<double>(farDistance);
        const double width = rightValue - leftValue;
        const double height = topValue - bottomValue;
        const double depthSpan = farValue - nearValue;

        float xScale{0.0F};
        float yScale{0.0F};
        float xOffset{0.0F};
        float yOffset{0.0F};
        if (!detail::TryConvertNonZeroFiniteFloat(2.0 / width, xScale) ||
            !detail::TryConvertNonZeroFiniteFloat(2.0 / height, yScale) ||
            !detail::TryConvertFiniteFloat(-(rightValue + leftValue) / width, xOffset) ||
            !detail::TryConvertFiniteFloat(-(topValue + bottomValue) / height, yOffset))
            [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        double depthScale{0.0};
        double depthOffset{0.0};
        switch (depth)
        {
        case ProjectionDepth::ForwardZ:
            depthScale = -1.0 / depthSpan;
            depthOffset = -nearValue / depthSpan;
            break;
        case ProjectionDepth::ReverseZ:
            depthScale = 1.0 / depthSpan;
            depthOffset = farValue / depthSpan;
            break;
        default:
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        float depthScaleFloat{0.0F};
        float depthOffsetFloat{0.0F};
        if (!detail::TryConvertNonZeroFiniteFloat(depthScale, depthScaleFloat) ||
            !detail::TryConvertNonZeroFiniteFloat(depthOffset, depthOffsetFloat)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeProjectionInvalidArgumentError());
        }

        return Matrix4x4{xScale,
                         0.0F,
                         0.0F,
                         xOffset,
                         0.0F,
                         yScale,
                         0.0F,
                         yOffset,
                         0.0F,
                         0.0F,
                         depthScaleFloat,
                         depthOffsetFloat,
                         0.0F,
                         0.0F,
                         0.0F,
                         1.0F};
    }

    [[nodiscard]] constexpr core::Result<Vector4> Row(std::size_t row) const
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

    [[nodiscard]] constexpr core::Result<Vector4> Column(std::size_t column) const
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

    [[nodiscard]] constexpr core::Result<std::reference_wrapper<float>> At(std::size_t row,
                                                                           std::size_t column)
    {
        if (row >= kDimension || column >= kDimension) [[unlikely]]
        {
            return core::Result<std::reference_wrapper<float>>::FromError(MakeIndexError());
        }

        return std::ref(AtUnchecked(row, column));
    }

    [[nodiscard]] constexpr core::Result<std::reference_wrapper<const float>> At(
        std::size_t row, std::size_t column) const
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
        return core::IsFinite(vector.x) && core::IsFinite(vector.y) && core::IsFinite(vector.z);
    }

    [[nodiscard]] static inline core::Result<Matrix4x4> MakeView(Vector3 eye,
                                                                 detail::WideVector3 direction,
                                                                 Vector3 up)
    {
        detail::WideVector3 forward{};
        detail::WideVector3 upDirection{};
        if (!detail::TryNormalize(direction, forward) ||
            !detail::TryNormalize(detail::WideVector3{static_cast<double>(up.x),
                                                      static_cast<double>(up.y),
                                                      static_cast<double>(up.z)},
                                  upDirection)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewDegenerateError());
        }

        detail::WideVector3 right{};
        if (!detail::TryNormalize(detail::Cross(forward, upDirection), right)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewDegenerateError());
        }

        const detail::WideVector3 correctedUp = detail::Cross(right, forward);
        const detail::WideVector3 backward{-forward.x, -forward.y, -forward.z};
        const double rightOffset = -detail::Dot(right, eye);
        const double upOffset = -detail::Dot(correctedUp, eye);
        const double backwardOffset = -detail::Dot(backward, eye);

        float rightX{0.0F};
        float rightY{0.0F};
        float rightZ{0.0F};
        float correctedUpX{0.0F};
        float correctedUpY{0.0F};
        float correctedUpZ{0.0F};
        float backwardX{0.0F};
        float backwardY{0.0F};
        float backwardZ{0.0F};
        float rightOffsetFloat{0.0F};
        float upOffsetFloat{0.0F};
        float backwardOffsetFloat{0.0F};
        if (!detail::TryConvertFiniteFloat(right.x, rightX) ||
            !detail::TryConvertFiniteFloat(right.y, rightY) ||
            !detail::TryConvertFiniteFloat(right.z, rightZ) ||
            !detail::TryConvertFiniteFloat(correctedUp.x, correctedUpX) ||
            !detail::TryConvertFiniteFloat(correctedUp.y, correctedUpY) ||
            !detail::TryConvertFiniteFloat(correctedUp.z, correctedUpZ) ||
            !detail::TryConvertFiniteFloat(backward.x, backwardX) ||
            !detail::TryConvertFiniteFloat(backward.y, backwardY) ||
            !detail::TryConvertFiniteFloat(backward.z, backwardZ) ||
            !detail::TryConvertFiniteFloat(rightOffset, rightOffsetFloat) ||
            !detail::TryConvertFiniteFloat(upOffset, upOffsetFloat) ||
            !detail::TryConvertFiniteFloat(backwardOffset, backwardOffsetFloat)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewUnrepresentableError());
        }

        const Matrix4x4 view{rightX,       rightY,       rightZ,       rightOffsetFloat,
                             correctedUpX, correctedUpY, correctedUpZ, upOffsetFloat,
                             backwardX,    backwardY,    backwardZ,    backwardOffsetFloat,
                             0.0F,         0.0F,         0.0F,         1.0F};
        if (!IsFiniteMatrix(view)) [[unlikely]]
        {
            return core::Result<Matrix4x4>::FromError(MakeViewUnrepresentableError());
        }

        return view;
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

    [[nodiscard]] static core::Error MakeViewUnrepresentableError()
    {
        return core::Error{
            ToErrorCode(MathErrorCode::InvalidArgument),
            "Matrix4x4 view construction result is not representable as finite floats."};
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
        return core::IsFinite(matrix.row0Column0) && core::IsFinite(matrix.row1Column0) &&
               core::IsFinite(matrix.row2Column0) && core::IsFinite(matrix.row3Column0) &&
               core::IsFinite(matrix.row0Column1) && core::IsFinite(matrix.row1Column1) &&
               core::IsFinite(matrix.row2Column1) && core::IsFinite(matrix.row3Column1) &&
               core::IsFinite(matrix.row0Column2) && core::IsFinite(matrix.row1Column2) &&
               core::IsFinite(matrix.row2Column2) && core::IsFinite(matrix.row3Column2) &&
               core::IsFinite(matrix.row0Column3) && core::IsFinite(matrix.row1Column3) &&
               core::IsFinite(matrix.row2Column3) && core::IsFinite(matrix.row3Column3);
    }

    [[nodiscard]] constexpr float& AtUnchecked(std::size_t row, std::size_t column) noexcept
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

    [[nodiscard]] constexpr const float& AtUnchecked(std::size_t row,
                                                     std::size_t column) const noexcept
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

[[nodiscard]] constexpr bool IsNear(Matrix4x4 lhs, Matrix4x4 rhs,
                                    core::Tolerance tolerance) noexcept
{
    return core::IsNear(lhs.row0Column0, rhs.row0Column0, tolerance) &&
           core::IsNear(lhs.row1Column0, rhs.row1Column0, tolerance) &&
           core::IsNear(lhs.row2Column0, rhs.row2Column0, tolerance) &&
           core::IsNear(lhs.row3Column0, rhs.row3Column0, tolerance) &&
           core::IsNear(lhs.row0Column1, rhs.row0Column1, tolerance) &&
           core::IsNear(lhs.row1Column1, rhs.row1Column1, tolerance) &&
           core::IsNear(lhs.row2Column1, rhs.row2Column1, tolerance) &&
           core::IsNear(lhs.row3Column1, rhs.row3Column1, tolerance) &&
           core::IsNear(lhs.row0Column2, rhs.row0Column2, tolerance) &&
           core::IsNear(lhs.row1Column2, rhs.row1Column2, tolerance) &&
           core::IsNear(lhs.row2Column2, rhs.row2Column2, tolerance) &&
           core::IsNear(lhs.row3Column2, rhs.row3Column2, tolerance) &&
           core::IsNear(lhs.row0Column3, rhs.row0Column3, tolerance) &&
           core::IsNear(lhs.row1Column3, rhs.row1Column3, tolerance) &&
           core::IsNear(lhs.row2Column3, rhs.row2Column3, tolerance) &&
           core::IsNear(lhs.row3Column3, rhs.row3Column3, tolerance);
}
[[nodiscard]] constexpr bool IsFinite(Matrix4x4 matrix) noexcept
{
    return core::IsFinite(matrix.row0Column0) && core::IsFinite(matrix.row1Column0) &&
           core::IsFinite(matrix.row2Column0) && core::IsFinite(matrix.row3Column0) &&
           core::IsFinite(matrix.row0Column1) && core::IsFinite(matrix.row1Column1) &&
           core::IsFinite(matrix.row2Column1) && core::IsFinite(matrix.row3Column1) &&
           core::IsFinite(matrix.row0Column2) && core::IsFinite(matrix.row1Column2) &&
           core::IsFinite(matrix.row2Column2) && core::IsFinite(matrix.row3Column2) &&
           core::IsFinite(matrix.row0Column3) && core::IsFinite(matrix.row1Column3) &&
           core::IsFinite(matrix.row2Column3) && core::IsFinite(matrix.row3Column3);
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

[[nodiscard]] constexpr bool IsFinite(const Matrix4x4AugmentedRows& rows) noexcept
{
    for (const auto& row : rows)
    {
        for (double value : row)
        {
            if (!core::IsFinite(value))
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

namespace detail
{
[[nodiscard]] constexpr core::Result<Vector3> PerspectiveDivideWide(double clipX, double clipY,
                                                                    double clipZ, double clipW)
{
    if (!core::IsFinite(clipW) || clipW == 0.0) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(core::Error{
            ToErrorCode(MathErrorCode::UndefinedHomogeneousCoordinate),
            "Perspective division requires a finite, non-zero homogeneous w coordinate."});
    }

    if (!core::IsFinite(clipX) || !core::IsFinite(clipY) || !core::IsFinite(clipZ)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Perspective division requires finite clip coordinates."});
    }

    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    if (!TryConvertFiniteFloat(clipX / clipW, x) || !TryConvertFiniteFloat(clipY / clipW, y) ||
        !TryConvertFiniteFloat(clipZ / clipW, z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(core::Error{
            ToErrorCode(MathErrorCode::UndefinedHomogeneousCoordinate),
            "Perspective division result is not representable as finite Cartesian coordinates."});
    }

    return Vector3{x, y, z};
}
} // namespace detail

[[nodiscard]] constexpr core::Result<Vector3> PerspectiveDivide(Vector4 clip)
{
    return detail::PerspectiveDivideWide(static_cast<double>(clip.x), static_cast<double>(clip.y),
                                         static_cast<double>(clip.z), static_cast<double>(clip.w));
}

[[nodiscard]] constexpr core::Result<Vector3> TransformPointToNdc(Matrix4x4 worldToClip,
                                                                  Vector3 point)
{
    if (!IsFinite(worldToClip) || !core::IsFinite(point.x) || !core::IsFinite(point.y) ||
        !core::IsFinite(point.z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Checked homogeneous transformation requires finite matrix and point "
                        "components."});
    }

    const double x = static_cast<double>(worldToClip.row0Column0) * point.x +
                     static_cast<double>(worldToClip.row0Column1) * point.y +
                     static_cast<double>(worldToClip.row0Column2) * point.z +
                     static_cast<double>(worldToClip.row0Column3);
    const double y = static_cast<double>(worldToClip.row1Column0) * point.x +
                     static_cast<double>(worldToClip.row1Column1) * point.y +
                     static_cast<double>(worldToClip.row1Column2) * point.z +
                     static_cast<double>(worldToClip.row1Column3);
    const double z = static_cast<double>(worldToClip.row2Column0) * point.x +
                     static_cast<double>(worldToClip.row2Column1) * point.y +
                     static_cast<double>(worldToClip.row2Column2) * point.z +
                     static_cast<double>(worldToClip.row2Column3);
    const double w = static_cast<double>(worldToClip.row3Column0) * point.x +
                     static_cast<double>(worldToClip.row3Column1) * point.y +
                     static_cast<double>(worldToClip.row3Column2) * point.z +
                     static_cast<double>(worldToClip.row3Column3);
    return detail::PerspectiveDivideWide(x, y, z, w);
}

[[nodiscard]] constexpr core::Result<Vector3> TransformNormal(Matrix4x4 transform, Vector3 normal)
{
    if (!IsFinite(transform) || !core::IsFinite(normal.x) || !core::IsFinite(normal.y) ||
        !core::IsFinite(normal.z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::NonFiniteInput),
                        "Normal transformation requires finite matrix and vector components."});
    }

    const double m00 = static_cast<double>(transform.row0Column0);
    const double m01 = static_cast<double>(transform.row0Column1);
    const double m02 = static_cast<double>(transform.row0Column2);
    const double m10 = static_cast<double>(transform.row1Column0);
    const double m11 = static_cast<double>(transform.row1Column1);
    const double m12 = static_cast<double>(transform.row1Column2);
    const double m20 = static_cast<double>(transform.row2Column0);
    const double m21 = static_cast<double>(transform.row2Column1);
    const double m22 = static_cast<double>(transform.row2Column2);

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
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::SingularMatrix),
                        "Normal transformation requires a nonsingular linear transform."});
    }

    const double normalX = static_cast<double>(normal.x);
    const double normalY = static_cast<double>(normal.y);
    const double normalZ = static_cast<double>(normal.z);
    const double transformedX = (c00 * normalX + c10 * normalY + c20 * normalZ) / determinant;
    const double transformedY = (c01 * normalX + c11 * normalY + c21 * normalZ) / determinant;
    const double transformedZ = (c02 * normalX + c12 * normalY + c22 * normalZ) / determinant;

    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    if (!detail::TryConvertFiniteFloat(transformedX, x) ||
        !detail::TryConvertFiniteFloat(transformedY, y) ||
        !detail::TryConvertFiniteFloat(transformedZ, z)) [[unlikely]]
    {
        return core::Result<Vector3>::FromError(
            core::Error{ToErrorCode(MathErrorCode::InvalidArgument),
                        "Normal transformation result is not representable as finite floats."});
    }

    return Vector3{x, y, z};
}

[[nodiscard]] constexpr core::Result<Matrix4x4> Inverse(Matrix4x4 matrix)
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
