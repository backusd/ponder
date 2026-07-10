#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Quaternion.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <random>

namespace
{
using MatrixRows = std::array<std::array<double, 4>, 4>;
using MinorRows = std::array<std::array<double, 3>, 3>;

[[nodiscard]] pond::math::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::math::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

void ExpectVectorNear(pond::math::Vector4 actual, pond::math::Vector4 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
    EXPECT_NEAR(actual.w, expected.w, tolerance);
}

void ExpectVectorNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectMatrixNear(pond::math::Matrix4x4 actual, pond::math::Matrix4x4 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.row0Column0, expected.row0Column0, tolerance);
    EXPECT_NEAR(actual.row0Column1, expected.row0Column1, tolerance);
    EXPECT_NEAR(actual.row0Column2, expected.row0Column2, tolerance);
    EXPECT_NEAR(actual.row0Column3, expected.row0Column3, tolerance);
    EXPECT_NEAR(actual.row1Column0, expected.row1Column0, tolerance);
    EXPECT_NEAR(actual.row1Column1, expected.row1Column1, tolerance);
    EXPECT_NEAR(actual.row1Column2, expected.row1Column2, tolerance);
    EXPECT_NEAR(actual.row1Column3, expected.row1Column3, tolerance);
    EXPECT_NEAR(actual.row2Column0, expected.row2Column0, tolerance);
    EXPECT_NEAR(actual.row2Column1, expected.row2Column1, tolerance);
    EXPECT_NEAR(actual.row2Column2, expected.row2Column2, tolerance);
    EXPECT_NEAR(actual.row2Column3, expected.row2Column3, tolerance);
    EXPECT_NEAR(actual.row3Column0, expected.row3Column0, tolerance);
    EXPECT_NEAR(actual.row3Column1, expected.row3Column1, tolerance);
    EXPECT_NEAR(actual.row3Column2, expected.row3Column2, tolerance);
    EXPECT_NEAR(actual.row3Column3, expected.row3Column3, tolerance);
}

void ExpectInverseFailure(pond::core::Result<pond::math::Matrix4x4> result,
                          pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectMatrixFailure(pond::core::Result<pond::math::Matrix4x4> result,
                         pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

void ExpectVectorFailure(pond::core::Result<pond::math::Vector3> result,
                         pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

[[nodiscard]] MatrixRows ToRows(pond::math::Matrix4x4 matrix) noexcept
{
    return MatrixRows{
        {{matrix.row0Column0, matrix.row0Column1, matrix.row0Column2, matrix.row0Column3},
         {matrix.row1Column0, matrix.row1Column1, matrix.row1Column2, matrix.row1Column3},
         {matrix.row2Column0, matrix.row2Column1, matrix.row2Column2, matrix.row2Column3},
         {matrix.row3Column0, matrix.row3Column1, matrix.row3Column2, matrix.row3Column3}}};
}

[[nodiscard]] pond::math::Matrix4x4 ToMatrix(MatrixRows rows) noexcept
{
    return pond::math::Matrix4x4{static_cast<float>(rows[0][0]), static_cast<float>(rows[0][1]),
                                 static_cast<float>(rows[0][2]), static_cast<float>(rows[0][3]),
                                 static_cast<float>(rows[1][0]), static_cast<float>(rows[1][1]),
                                 static_cast<float>(rows[1][2]), static_cast<float>(rows[1][3]),
                                 static_cast<float>(rows[2][0]), static_cast<float>(rows[2][1]),
                                 static_cast<float>(rows[2][2]), static_cast<float>(rows[2][3]),
                                 static_cast<float>(rows[3][0]), static_cast<float>(rows[3][1]),
                                 static_cast<float>(rows[3][2]), static_cast<float>(rows[3][3])};
}

[[nodiscard]] MatrixRows MultiplyRows(MatrixRows lhs, MatrixRows rhs) noexcept
{
    MatrixRows result{};
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            for (std::size_t index = 0; index < 4; ++index)
            {
                result[row][column] += lhs[row][index] * rhs[index][column];
            }
        }
    }

    return result;
}

[[nodiscard]] MinorRows BuildMinor(MatrixRows rows, std::size_t excludedRow,
                                   std::size_t excludedColumn) noexcept
{
    MinorRows minor{};
    std::size_t minorRow = 0;
    for (std::size_t row = 0; row < 4; ++row)
    {
        if (row == excludedRow)
        {
            continue;
        }

        std::size_t minorColumn = 0;
        for (std::size_t column = 0; column < 4; ++column)
        {
            if (column == excludedColumn)
            {
                continue;
            }

            minor[minorRow][minorColumn] = rows[row][column];
            ++minorColumn;
        }
        ++minorRow;
    }

    return minor;
}

[[nodiscard]] double Determinant3x3(MinorRows rows) noexcept
{
    return rows[0][0] * (rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1]) -
           rows[0][1] * (rows[1][0] * rows[2][2] - rows[1][2] * rows[2][0]) +
           rows[0][2] * (rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]);
}

[[nodiscard]] double DeterminantRows(MatrixRows rows) noexcept
{
    double determinant = 0.0;
    for (std::size_t column = 0; column < 4; ++column)
    {
        const double sign = (column % 2 == 0) ? 1.0 : -1.0;
        determinant += sign * rows[0][column] * Determinant3x3(BuildMinor(rows, 0, column));
    }

    return determinant;
}

[[nodiscard]] MatrixRows InverseRows(MatrixRows rows) noexcept
{
    const double determinant = DeterminantRows(rows);
    MatrixRows inverse{};

    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            const double sign = ((row + column) % 2 == 0) ? 1.0 : -1.0;
            inverse[column][row] =
                sign * Determinant3x3(BuildMinor(rows, row, column)) / determinant;
        }
    }

    return inverse;
}

[[nodiscard]] pond::math::Vector3 ProjectToNdc(pond::math::Matrix4x4 projection,
                                               pond::math::Vector3 point)
{
    auto ndc = pond::math::TransformPointToNdc(projection, point);
    EXPECT_TRUE(ndc.HasValue());
    if (!ndc.HasValue())
    {
        return pond::math::Vector3::Zero();
    }

    return ndc.GetValue();
}

TEST(Matrix4x4AlgebraTests, AppliesArithmeticAndScalarOperations)
{
    const pond::math::Matrix4x4 lhs{1.0F, 2.0F,  3.0F,  4.0F,  5.0F,  6.0F,  7.0F,  8.0F,
                                    9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F};
    const pond::math::Matrix4x4 rhs{16.0F, 15.0F, 14.0F, 13.0F, 12.0F, 11.0F, 10.0F, 9.0F,
                                    8.0F,  7.0F,  6.0F,  5.0F,  4.0F,  3.0F,  2.0F,  1.0F};

    EXPECT_EQ(lhs + rhs,
              (pond::math::Matrix4x4{17.0F, 17.0F, 17.0F, 17.0F, 17.0F, 17.0F, 17.0F, 17.0F, 17.0F,
                                     17.0F, 17.0F, 17.0F, 17.0F, 17.0F, 17.0F, 17.0F}));
    EXPECT_EQ(lhs - rhs,
              (pond::math::Matrix4x4{-15.0F, -13.0F, -11.0F, -9.0F, -7.0F, -5.0F, -3.0F, -1.0F,
                                     1.0F, 3.0F, 5.0F, 7.0F, 9.0F, 11.0F, 13.0F, 15.0F}));
    EXPECT_EQ(lhs * 2.0F,
              (pond::math::Matrix4x4{2.0F, 4.0F, 6.0F, 8.0F, 10.0F, 12.0F, 14.0F, 16.0F, 18.0F,
                                     20.0F, 22.0F, 24.0F, 26.0F, 28.0F, 30.0F, 32.0F}));
    EXPECT_EQ(2.0F * lhs, lhs * 2.0F);
    EXPECT_EQ((lhs * 2.0F) / 2.0F, lhs);
}

TEST(Matrix4x4AlgebraTests, TransformsVectorsWithIdentityDiagonalAffineAndProjectiveMatrices)
{
    const pond::math::Vector4 point{1.0F, 2.0F, 3.0F, 1.0F};
    const pond::math::Matrix4x4 scale{2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F,
                                      0.0F, 0.0F, 4.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const pond::math::Matrix4x4 translation{1.0F, 0.0F, 0.0F, 5.0F, 0.0F, 1.0F, 0.0F, -2.0F,
                                            0.0F, 0.0F, 1.0F, 1.5F, 0.0F, 0.0F, 0.0F, 1.0F};
    const pond::math::Matrix4x4 projective{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                           0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.5F, 1.0F};

    EXPECT_EQ(pond::math::Matrix4x4::Identity() * point, point);
    EXPECT_EQ(scale * point, (pond::math::Vector4{2.0F, 6.0F, 12.0F, 1.0F}));
    EXPECT_EQ(translation * point, (pond::math::Vector4{6.0F, 0.0F, 4.5F, 1.0F}));
    EXPECT_EQ((translation * pond::math::Vector4{1.0F, 2.0F, 3.0F, 0.0F}),
              (pond::math::Vector4{1.0F, 2.0F, 3.0F, 0.0F}));
    EXPECT_EQ((projective * pond::math::Vector4{2.0F, 4.0F, 6.0F, 1.0F}),
              (pond::math::Vector4{2.0F, 4.0F, 6.0F, 4.0F}));
}

TEST(Matrix4x4AlgebraTests, BuildsTranslationScaleAndAxisRotationTransforms)
{
    const pond::math::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);

    EXPECT_EQ(pond::math::Matrix4x4::Translation(pond::math::Vector3{3.0F, -2.0F, 1.5F}),
              (pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, 3.0F, 0.0F, 1.0F, 0.0F, -2.0F, 0.0F, 0.0F,
                                     1.0F, 1.5F, 0.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_EQ(pond::math::Matrix4x4::Scale(pond::math::Vector3{2.0F, -3.0F, 4.0F}),
              (pond::math::Matrix4x4{2.0F, 0.0F, 0.0F, 0.0F, 0.0F, -3.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                     4.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}));

    ExpectMatrixNear(pond::math::Matrix4x4::RotationX(pond::math::Radians{pond::math::kHalfPi}),
                     pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F,
                                           1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
                     2.0e-5F);
    ExpectMatrixNear(pond::math::Matrix4x4::RotationY(pond::math::Radians{pond::math::kHalfPi}),
                     pond::math::Matrix4x4{0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, -1.0F,
                                           0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
                     2.0e-5F);
    ExpectMatrixNear(pond::math::Matrix4x4::RotationZ(pond::math::Radians{pond::math::kHalfPi}),
                     pond::math::Matrix4x4{0.0F, -1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                           0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
                     2.0e-5F);

    EXPECT_TRUE(pond::math::IsNear(
        pond::math::Matrix4x4::RotationZ(pond::math::Radians{pond::math::kHalfPi}) *
            pond::math::Vector4{1.0F, 0.0F, 0.0F, 0.0F},
        pond::math::Vector4{0.0F, 1.0F, 0.0F, 0.0F}, tolerance));
}

TEST(Matrix4x4AlgebraTests, TransformsPointsVectorsAndNormalsWithDistinctSemantics)
{
    const pond::math::Matrix4x4 translation =
        pond::math::Matrix4x4::Translation(pond::math::Vector3{3.0F, -2.0F, 1.0F});
    const pond::math::Vector3 value{1.0F, 2.0F, 3.0F};

    ExpectVectorNear(pond::math::TransformPoint(translation, value),
                     pond::math::Vector3{4.0F, 0.0F, 4.0F});
    ExpectVectorNear(pond::math::TransformVector(translation, value), value);

    auto normal = pond::math::TransformNormal(translation, value);
    ASSERT_TRUE(normal.HasValue());
    ExpectVectorNear(normal.GetValue(), value);
}

TEST(Matrix4x4AlgebraTests, TransformsNormalsWithInverseTransposeForScaleAndShear)
{
    const pond::math::Matrix4x4 scale =
        pond::math::Matrix4x4::Scale(pond::math::Vector3{2.0F, -4.0F, 0.5F});
    auto scaledNormal = pond::math::TransformNormal(scale, pond::math::Vector3{4.0F, -8.0F, 1.0F});
    ASSERT_TRUE(scaledNormal.HasValue());
    ExpectVectorNear(scaledNormal.GetValue(), pond::math::Vector3{2.0F, 2.0F, 2.0F});

    const pond::math::Matrix4x4 shear{1.0F, 2.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                      0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    ExpectVectorNear(pond::math::TransformVector(shear, pond::math::Vector3{1.0F, 3.0F, 4.0F}),
                     pond::math::Vector3{7.0F, 3.0F, 4.0F});

    auto shearedNormal = pond::math::TransformNormal(shear, pond::math::Vector3{1.0F, 0.0F, 0.0F});
    ASSERT_TRUE(shearedNormal.HasValue());
    ExpectVectorNear(shearedNormal.GetValue(), pond::math::Vector3{1.0F, -2.0F, 0.0F});
}

TEST(Matrix4x4AlgebraTests, RejectsSingularNormalTransforms)
{
    ExpectVectorFailure(pond::math::TransformNormal(
                            pond::math::Matrix4x4::Scale(pond::math::Vector3{1.0F, 0.0F, 1.0F}),
                            pond::math::Vector3{0.0F, 1.0F, 0.0F}),
                        pond::math::MathErrorCode::SingularMatrix);
}

TEST(Matrix4x4AlgebraTests, ComposesSemanticTransformsInColumnVectorOrder)
{
    const pond::math::Matrix4x4 scale =
        pond::math::Matrix4x4::Scale(pond::math::Vector3{2.0F, 3.0F, -1.0F});
    const pond::math::Matrix4x4 rotation =
        pond::math::Matrix4x4::RotationZ(pond::math::Radians{pond::math::kHalfPi});
    const pond::math::Matrix4x4 translation =
        pond::math::Matrix4x4::Translation(pond::math::Vector3{5.0F, -2.0F, 1.0F});
    const pond::math::Matrix4x4 composed = translation * rotation * scale;
    const pond::math::Vector3 point{1.0F, 2.0F, 3.0F};

    ExpectVectorNear(pond::math::TransformPoint(composed, point),
                     pond::math::Vector3{-1.0F, 0.0F, -2.0F}, 2.0e-5F);
    ExpectVectorNear(pond::math::TransformVector(composed, point),
                     pond::math::Vector3{-6.0F, 2.0F, -3.0F}, 2.0e-5F);
    ExpectVectorNear(pond::math::TransformPoint(composed, point),
                     pond::math::TransformPoint(
                         translation, pond::math::TransformVector(
                                          rotation, pond::math::TransformVector(scale, point))),
                     2.0e-5F);
}

TEST(Matrix4x4AlgebraTests, QuaternionAndAxisDerivedMatrixRotationsAgree)
{
    const pond::math::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    auto quarterTurnX = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{1.0F, 0.0F, 0.0F}, pond::math::Radians{pond::math::kHalfPi});
    auto quarterTurnY = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 1.0F, 0.0F}, pond::math::Radians{pond::math::kHalfPi});
    auto quarterTurnZ = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, pond::math::Radians{pond::math::kHalfPi});
    ASSERT_TRUE(quarterTurnX.HasValue());
    ASSERT_TRUE(quarterTurnY.HasValue());
    ASSERT_TRUE(quarterTurnZ.HasValue());

    EXPECT_TRUE(pond::math::IsNear(
        pond::math::Matrix4x4::Rotation(quarterTurnX.GetValue()),
        pond::math::Matrix4x4::RotationX(pond::math::Radians{pond::math::kHalfPi}), tolerance));
    EXPECT_TRUE(pond::math::IsNear(
        pond::math::Matrix4x4::Rotation(quarterTurnY.GetValue()),
        pond::math::Matrix4x4::RotationY(pond::math::Radians{pond::math::kHalfPi}), tolerance));
    EXPECT_TRUE(pond::math::IsNear(
        pond::math::Matrix4x4::Rotation(quarterTurnZ.GetValue()),
        pond::math::Matrix4x4::RotationZ(pond::math::Radians{pond::math::kHalfPi}), tolerance));
}

TEST(Matrix4x4AlgebraTests, BuildsCanonicalOriginViewAsIdentity)
{
    auto lookAt = pond::math::Matrix4x4::LookAt(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{0.0F, 0.0F, -1.0F},
                                                pond::math::Vector3{0.0F, 1.0F, 0.0F});
    auto lookTo = pond::math::Matrix4x4::LookTo(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{0.0F, 0.0F, -1.0F},
                                                pond::math::Vector3{0.0F, 1.0F, 0.0F});
    ASSERT_TRUE(lookAt.HasValue());
    ASSERT_TRUE(lookTo.HasValue());

    EXPECT_EQ(lookAt.GetValue(), pond::math::Matrix4x4::Identity());
    EXPECT_EQ(lookTo.GetValue(), pond::math::Matrix4x4::Identity());
    ExpectVectorNear(
        pond::math::TransformPoint(lookAt.GetValue(), pond::math::Vector3{0.0F, 0.0F, -5.0F}),
        pond::math::Vector3{0.0F, 0.0F, -5.0F});
}

TEST(Matrix4x4AlgebraTests, BuildsTranslatedViewsAndRecoversWorldPoints)
{
    const pond::math::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    const pond::math::Vector3 eye{3.0F, -2.0F, 5.0F};
    auto view = pond::math::Matrix4x4::LookAt(eye, pond::math::Vector3{3.0F, -2.0F, 4.0F},
                                              pond::math::Vector3{0.0F, 1.0F, 0.0F});
    ASSERT_TRUE(view.HasValue());

    ExpectMatrixNear(view.GetValue(),
                     pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, -3.0F, 0.0F, 1.0F, 0.0F, 2.0F, 0.0F,
                                           0.0F, 1.0F, -5.0F, 0.0F, 0.0F, 0.0F, 1.0F});
    ExpectVectorNear(pond::math::TransformPoint(view.GetValue(), eye), pond::math::Vector3::Zero());
    ExpectVectorNear(
        pond::math::TransformPoint(view.GetValue(), pond::math::Vector3{3.0F, -2.0F, 4.0F}),
        pond::math::Vector3{0.0F, 0.0F, -1.0F});

    const pond::math::Vector3 worldPoint{-4.0F, 6.0F, 2.5F};
    auto inverse = pond::math::Inverse(view.GetValue());
    ASSERT_TRUE(inverse.HasValue());
    ExpectVectorNear(
        pond::math::TransformPoint(inverse.GetValue(),
                                   pond::math::TransformPoint(view.GetValue(), worldPoint)),
        worldPoint, 2.0e-5F);
    EXPECT_TRUE(pond::math::IsNear(view.GetValue() * inverse.GetValue(),
                                   pond::math::Matrix4x4::Identity(), tolerance));
}

TEST(Matrix4x4AlgebraTests, BuildsRotatedViewsUnderActiveWorldToViewConvention)
{
    auto lookTo = pond::math::Matrix4x4::LookTo(pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                                pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{0.0F, 1.0F, 0.0F});
    auto lookAt = pond::math::Matrix4x4::LookAt(pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                                pond::math::Vector3{2.0F, 2.0F, 3.0F},
                                                pond::math::Vector3{0.0F, 1.0F, 0.0F});
    ASSERT_TRUE(lookTo.HasValue());
    ASSERT_TRUE(lookAt.HasValue());

    const pond::math::Matrix4x4 expected{0.0F,  0.0F, 1.0F, -3.0F, 0.0F, 1.0F, 0.0F, -2.0F,
                                         -1.0F, 0.0F, 0.0F, 1.0F,  0.0F, 0.0F, 0.0F, 1.0F};
    ExpectMatrixNear(lookTo.GetValue(), expected);
    ExpectMatrixNear(lookAt.GetValue(), expected);
    ExpectVectorNear(
        pond::math::TransformPoint(lookTo.GetValue(), pond::math::Vector3{1.0F, 2.0F, 3.0F}),
        pond::math::Vector3::Zero());
    ExpectVectorNear(
        pond::math::TransformPoint(lookTo.GetValue(), pond::math::Vector3{2.0F, 2.0F, 3.0F}),
        pond::math::Vector3{0.0F, 0.0F, -1.0F});
    ExpectVectorNear(
        pond::math::TransformVector(lookTo.GetValue(), pond::math::Vector3{0.0F, 0.0F, 1.0F}),
        pond::math::Vector3{1.0F, 0.0F, 0.0F});
}

TEST(Matrix4x4AlgebraTests, AcceptsArbitraryValidUpVectorsAndMatchesLookDirection)
{
    const pond::math::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    const pond::math::Vector3 eye{2.0F, 3.0F, -4.0F};
    const pond::math::Vector3 direction{1.0F, -2.0F, -3.0F};
    const pond::math::Vector3 target = eye + direction;
    const pond::math::Vector3 up{0.5F, 1.0F, 0.25F};
    auto lookAt = pond::math::Matrix4x4::LookAt(eye, target, up);
    auto lookTo = pond::math::Matrix4x4::LookTo(eye, direction, up);
    auto normalizedDirection = pond::math::Normalize(direction);
    ASSERT_TRUE(lookAt.HasValue());
    ASSERT_TRUE(lookTo.HasValue());
    ASSERT_TRUE(normalizedDirection.HasValue());

    EXPECT_TRUE(pond::math::IsNear(lookAt.GetValue(), lookTo.GetValue(), tolerance));
    ExpectVectorNear(pond::math::TransformPoint(lookAt.GetValue(), eye),
                     pond::math::Vector3::Zero(), 2.0e-5F);
    ExpectVectorNear(pond::math::TransformVector(lookAt.GetValue(), normalizedDirection.GetValue()),
                     pond::math::Vector3{0.0F, 0.0F, -1.0F}, 2.0e-5F);

    const pond::math::Vector3 worldPoint{-3.0F, 1.5F, 2.0F};
    auto inverse = pond::math::Inverse(lookAt.GetValue());
    ASSERT_TRUE(inverse.HasValue());
    ExpectVectorNear(
        pond::math::TransformPoint(inverse.GetValue(),
                                   pond::math::TransformPoint(lookAt.GetValue(), worldPoint)),
        worldPoint, 3.0e-5F);
}

TEST(Matrix4x4AlgebraTests, RejectsInvalidViewConstructionInputs)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();
    const pond::math::Vector3 eye{0.0F, 0.0F, 0.0F};
    const pond::math::Vector3 target{0.0F, 0.0F, -1.0F};
    const pond::math::Vector3 direction{0.0F, 0.0F, -1.0F};
    const pond::math::Vector3 up{0.0F, 1.0F, 0.0F};

    ExpectMatrixFailure(
        pond::math::Matrix4x4::LookAt(pond::math::Vector3{infinity, 0.0F, 0.0F}, target, up),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::LookAt(eye, pond::math::Vector3{0.0F, quietNaN, -1.0F}, up),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::LookTo(eye, pond::math::Vector3{0.0F, infinity, -1.0F}, up),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::LookTo(eye, direction, pond::math::Vector3{0.0F, quietNaN, 0.0F}),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectMatrixFailure(pond::math::Matrix4x4::LookAt(eye, eye, up),
                        pond::math::MathErrorCode::DegenerateInput);
    ExpectMatrixFailure(pond::math::Matrix4x4::LookTo(eye, pond::math::Vector3::Zero(), up),
                        pond::math::MathErrorCode::DegenerateInput);
    ExpectMatrixFailure(pond::math::Matrix4x4::LookTo(eye, direction, pond::math::Vector3::Zero()),
                        pond::math::MathErrorCode::DegenerateInput);
    ExpectMatrixFailure(pond::math::Matrix4x4::LookTo(eye, direction, direction),
                        pond::math::MathErrorCode::DegenerateInput);
    ExpectMatrixFailure(pond::math::Matrix4x4::LookTo(eye, direction, -direction),
                        pond::math::MathErrorCode::DegenerateInput);
}

TEST(Matrix4x4AlgebraTests, TransformsPointsToClipCoordinates)
{
    const pond::math::Matrix4x4 worldToClip{1.0F, 2.0F, 3.0F,  4.0F, -1.0F, 0.5F, 0.0F,  2.0F,
                                            0.0F, 0.0F, -2.0F, 1.0F, 0.25F, 0.0F, -1.0F, 2.0F};
    const pond::math::Vector3 point{2.0F, -3.0F, 4.0F};

    ExpectVectorNear(pond::math::TransformPointToClip(worldToClip, point),
                     pond::math::Vector4{12.0F, -1.5F, -7.0F, -1.5F});
    ExpectVectorNear(pond::math::TransformPointToClip(pond::math::Matrix4x4::Identity(), point),
                     pond::math::Vector4{2.0F, -3.0F, 4.0F, 1.0F});
}

TEST(Matrix4x4AlgebraTests, PerspectiveDivideAcceptsPositiveAndNegativeW)
{
    auto positive = pond::math::PerspectiveDivide(pond::math::Vector4{2.0F, 4.0F, 6.0F, 2.0F});
    auto negative = pond::math::PerspectiveDivide(pond::math::Vector4{2.0F, -4.0F, 6.0F, -2.0F});

    ASSERT_TRUE(positive.HasValue());
    ASSERT_TRUE(negative.HasValue());
    ExpectVectorNear(positive.GetValue(), pond::math::Vector3{1.0F, 2.0F, 3.0F});
    ExpectVectorNear(negative.GetValue(), pond::math::Vector3{-1.0F, 2.0F, -3.0F});
}

TEST(Matrix4x4AlgebraTests, PerspectiveDivideRejectsUndefinedAndNonFiniteCoordinates)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float denormal = std::numeric_limits<float>::denorm_min();
    constexpr float finiteMaximum = std::numeric_limits<float>::max();
    constexpr float minimumNormal = std::numeric_limits<float>::min();

    ExpectVectorFailure(pond::math::PerspectiveDivide(pond::math::Vector4{1.0F, 2.0F, 3.0F, 0.0F}),
                        pond::math::MathErrorCode::UndefinedHomogeneousCoordinate);
    ExpectVectorFailure(
        pond::math::PerspectiveDivide(pond::math::Vector4{1.0F, 2.0F, 3.0F, infinity}),
        pond::math::MathErrorCode::UndefinedHomogeneousCoordinate);
    ExpectVectorFailure(
        pond::math::PerspectiveDivide(pond::math::Vector4{1.0F, 2.0F, 3.0F, quietNaN}),
        pond::math::MathErrorCode::UndefinedHomogeneousCoordinate);
    ExpectVectorFailure(
        pond::math::PerspectiveDivide(pond::math::Vector4{1.0F, 2.0F, 3.0F, denormal}),
        pond::math::MathErrorCode::UndefinedHomogeneousCoordinate);
    ExpectVectorFailure(pond::math::PerspectiveDivide(
                            pond::math::Vector4{finiteMaximum, 0.0F, 0.0F, minimumNormal}),
                        pond::math::MathErrorCode::UndefinedHomogeneousCoordinate);
    ExpectVectorFailure(
        pond::math::PerspectiveDivide(pond::math::Vector4{infinity, 0.0F, 0.0F, 1.0F}),
        pond::math::MathErrorCode::NonFiniteInput);
}

TEST(Matrix4x4AlgebraTests, TransformPointToNdcCoversFrustumInteriorExteriorAndComposition)
{
    auto forward =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kHalfPi}, 1.0F, 1.0F,
                                           11.0F, pond::math::ProjectionDepth::ForwardZ);
    auto reverse =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kHalfPi}, 1.0F, 1.0F,
                                           11.0F, pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(forward.HasValue());
    ASSERT_TRUE(reverse.HasValue());

    ExpectVectorNear(ProjectToNdc(forward.GetValue(), pond::math::Vector3{-1.0F, 1.0F, -1.0F}),
                     pond::math::Vector3{-1.0F, 1.0F, 0.0F});
    ExpectVectorNear(ProjectToNdc(forward.GetValue(), pond::math::Vector3{1.0F, -1.0F, -1.0F}),
                     pond::math::Vector3{1.0F, -1.0F, 0.0F});
    ExpectVectorNear(ProjectToNdc(forward.GetValue(), pond::math::Vector3{11.0F, 11.0F, -11.0F}),
                     pond::math::Vector3{1.0F, 1.0F, 1.0F}, 2.0e-5F);
    ExpectVectorNear(ProjectToNdc(reverse.GetValue(), pond::math::Vector3{-1.0F, 1.0F, -1.0F}),
                     pond::math::Vector3{-1.0F, 1.0F, 1.0F});
    ExpectVectorNear(ProjectToNdc(reverse.GetValue(), pond::math::Vector3{11.0F, -11.0F, -11.0F}),
                     pond::math::Vector3{1.0F, -1.0F, 0.0F}, 2.0e-5F);

    const pond::math::Vector3 interior{0.5F, -1.0F, -2.0F};
    ExpectVectorNear(ProjectToNdc(forward.GetValue(), interior),
                     pond::math::Vector3{0.25F, -0.5F, 0.55F}, 2.0e-5F);
    ExpectVectorNear(ProjectToNdc(forward.GetValue(), pond::math::Vector3{2.0F, -3.0F, -1.0F}),
                     pond::math::Vector3{2.0F, -3.0F, 0.0F}, 2.0e-5F);

    const pond::math::Vector4 clip = pond::math::TransformPointToClip(forward.GetValue(), interior);
    auto separate = pond::math::PerspectiveDivide(clip);
    auto composed = pond::math::TransformPointToNdc(forward.GetValue(), interior);
    ASSERT_TRUE(separate.HasValue());
    ASSERT_TRUE(composed.HasValue());
    ExpectVectorNear(composed.GetValue(), separate.GetValue());
}
TEST(Matrix4x4AlgebraTests, PerspectiveMapsFiniteForwardAndReverseDepth)
{
    constexpr float nearDistance{0.5F};
    constexpr float farDistance{10.0F};
    auto forward = pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kHalfPi},
                                                      2.0F, nearDistance, farDistance,
                                                      pond::math::ProjectionDepth::ForwardZ);
    auto reverse = pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kHalfPi},
                                                      2.0F, nearDistance, farDistance,
                                                      pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(forward.HasValue());
    ASSERT_TRUE(reverse.HasValue());

    ExpectVectorNear(
        ProjectToNdc(forward.GetValue(), pond::math::Vector3{0.0F, 0.0F, -nearDistance}),
        pond::math::Vector3{0.0F, 0.0F, 0.0F}, 1.0e-5F);
    ExpectVectorNear(
        ProjectToNdc(forward.GetValue(), pond::math::Vector3{0.0F, 0.0F, -farDistance}),
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, 1.0e-5F);
    ExpectVectorNear(
        ProjectToNdc(reverse.GetValue(), pond::math::Vector3{0.0F, 0.0F, -nearDistance}),
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, 1.0e-5F);
    ExpectVectorNear(
        ProjectToNdc(reverse.GetValue(), pond::math::Vector3{0.0F, 0.0F, -farDistance}),
        pond::math::Vector3{0.0F, 0.0F, 0.0F}, 1.0e-5F);
}

TEST(Matrix4x4AlgebraTests, PerspectiveMapsFrustumEdgesWithNdcYUp)
{
    constexpr float nearDistance{2.0F};
    constexpr float aspectRatio{2.0F};
    auto projection = pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kHalfPi},
                                                         aspectRatio, nearDistance, 32.0F,
                                                         pond::math::ProjectionDepth::ForwardZ);
    ASSERT_TRUE(projection.HasValue());

    constexpr float top{nearDistance};
    constexpr float right{nearDistance * aspectRatio};
    ExpectVectorNear(
        ProjectToNdc(projection.GetValue(), pond::math::Vector3{right, top, -nearDistance}),
        pond::math::Vector3{1.0F, 1.0F, 0.0F}, 2.0e-5F);
    ExpectVectorNear(
        ProjectToNdc(projection.GetValue(), pond::math::Vector3{-right, -top, -nearDistance}),
        pond::math::Vector3{-1.0F, -1.0F, 0.0F}, 2.0e-5F);
}

TEST(Matrix4x4AlgebraTests, InfinitePerspectiveMatchesDepthLimits)
{
    constexpr float nearDistance{0.25F};
    constexpr float sampleDistance{1000.0F};
    auto forward = pond::math::Matrix4x4::InfinitePerspective(
        pond::math::Radians{pond::math::kHalfPi}, 1.0F, nearDistance,
        pond::math::ProjectionDepth::ForwardZ);
    auto reverse = pond::math::Matrix4x4::InfinitePerspective(
        pond::math::Radians{pond::math::kHalfPi}, 1.0F, nearDistance,
        pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(forward.HasValue());
    ASSERT_TRUE(reverse.HasValue());

    ExpectVectorNear(
        ProjectToNdc(forward.GetValue(), pond::math::Vector3{0.0F, 0.0F, -nearDistance}),
        pond::math::Vector3{0.0F, 0.0F, 0.0F});
    ExpectVectorNear(
        ProjectToNdc(reverse.GetValue(), pond::math::Vector3{0.0F, 0.0F, -nearDistance}),
        pond::math::Vector3{0.0F, 0.0F, 1.0F});
    ExpectVectorNear(
        ProjectToNdc(forward.GetValue(), pond::math::Vector3{0.0F, 0.0F, -sampleDistance}),
        pond::math::Vector3{0.0F, 0.0F, 1.0F - nearDistance / sampleDistance}, 1.0e-5F);
    ExpectVectorNear(
        ProjectToNdc(reverse.GetValue(), pond::math::Vector3{0.0F, 0.0F, -sampleDistance}),
        pond::math::Vector3{0.0F, 0.0F, nearDistance / sampleDistance}, 1.0e-5F);
}

TEST(Matrix4x4AlgebraTests, OrthographicMapsExtentsAndFiniteDepth)
{
    constexpr float left{-2.0F};
    constexpr float right{6.0F};
    constexpr float bottom{-3.0F};
    constexpr float top{5.0F};
    constexpr float nearDistance{1.0F};
    constexpr float farDistance{11.0F};
    auto forward = pond::math::Matrix4x4::Orthographic(
        left, right, bottom, top, nearDistance, farDistance, pond::math::ProjectionDepth::ForwardZ);
    auto reverse = pond::math::Matrix4x4::Orthographic(
        left, right, bottom, top, nearDistance, farDistance, pond::math::ProjectionDepth::ReverseZ);
    ASSERT_TRUE(forward.HasValue());
    ASSERT_TRUE(reverse.HasValue());

    ExpectVectorNear(
        ProjectToNdc(forward.GetValue(), pond::math::Vector3{left, bottom, -nearDistance}),
        pond::math::Vector3{-1.0F, -1.0F, 0.0F});
    ExpectVectorNear(
        ProjectToNdc(forward.GetValue(), pond::math::Vector3{right, top, -farDistance}),
        pond::math::Vector3{1.0F, 1.0F, 1.0F});
    ExpectVectorNear(
        ProjectToNdc(reverse.GetValue(), pond::math::Vector3{left, bottom, -nearDistance}),
        pond::math::Vector3{-1.0F, -1.0F, 1.0F});
    ExpectVectorNear(
        ProjectToNdc(reverse.GetValue(), pond::math::Vector3{right, top, -farDistance}),
        pond::math::Vector3{1.0F, 1.0F, 0.0F});
    ExpectVectorNear(ProjectToNdc(forward.GetValue(), pond::math::Vector3{2.0F, 1.0F, -6.0F}),
                     pond::math::Vector3{0.0F, 0.0F, 0.5F});
}

TEST(Matrix4x4AlgebraTests, RejectsInvalidProjectionConstructionInputs)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr pond::math::ProjectionDepth forward = pond::math::ProjectionDepth::ForwardZ;

    ExpectMatrixFailure(pond::math::Matrix4x4::Perspective(pond::math::Radians{infinity}, 1.0F,
                                                           1.0F, 2.0F, forward),
                        pond::math::MathErrorCode::NonFiniteInput);
    ExpectMatrixFailure(pond::math::Matrix4x4::Perspective(pond::math::Radians{1.0F}, quietNaN,
                                                           1.0F, 2.0F, forward),
                        pond::math::MathErrorCode::NonFiniteInput);
    ExpectMatrixFailure(pond::math::Matrix4x4::Perspective(pond::math::Radians{1.0F}, 1.0F,
                                                           infinity, 2.0F, forward),
                        pond::math::MathErrorCode::NonFiniteInput);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Orthographic(quietNaN, 1.0F, -1.0F, 1.0F, 1.0F, 2.0F, forward),
        pond::math::MathErrorCode::NonFiniteInput);

    ExpectMatrixFailure(
        pond::math::Matrix4x4::Perspective(pond::math::Radians{0.0F}, 1.0F, 1.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::math::kPi},
                                                           1.0F, 1.0F, 2.0F, forward),
                        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Perspective(pond::math::Radians{1.0F}, 0.0F, 1.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Perspective(pond::math::Radians{1.0F}, 1.0F, 0.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Perspective(pond::math::Radians{1.0F}, 1.0F, 2.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::InfinitePerspective(pond::math::Radians{1.0F}, -1.0F, 1.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Orthographic(1.0F, 1.0F, -1.0F, 1.0F, 1.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Orthographic(-1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Orthographic(-1.0F, 1.0F, -1.0F, 1.0F, 0.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
    ExpectMatrixFailure(
        pond::math::Matrix4x4::Orthographic(-1.0F, 1.0F, -1.0F, 1.0F, 2.0F, 2.0F, forward),
        pond::math::MathErrorCode::InvalidArgument);
}
TEST(Matrix4x4AlgebraTests, TransposesAndComputesDeterminants)
{
    const pond::math::Matrix4x4 matrix{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F,
                                       2.0F, 6.0F, 4.0F, 8.0F, 3.0F, 1.0F, 1.0F, 2.0F};

    EXPECT_EQ(pond::math::Transpose(matrix),
              (pond::math::Matrix4x4{1.0F, 5.0F, 2.0F, 3.0F, 2.0F, 6.0F, 6.0F, 1.0F, 3.0F, 7.0F,
                                     4.0F, 1.0F, 4.0F, 8.0F, 8.0F, 2.0F}));
    EXPECT_EQ(pond::math::Determinant(pond::math::Matrix4x4::Identity()), 1.0F);
    EXPECT_EQ(pond::math::Determinant(matrix), 72.0F);
    EXPECT_EQ(pond::math::Determinant(pond::math::Matrix4x4{2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 3.0F,
                                                            0.0F, 0.0F, 0.0F, 0.0F, 4.0F, 0.0F,
                                                            0.0F, 0.0F, 0.0F, 5.0F}),
              120.0F);
}

TEST(Matrix4x4AlgebraTests, ComposesInColumnVectorOrderAndPreservesTranslation)
{
    const pond::math::Matrix4x4 translation{1.0F, 0.0F, 0.0F, 3.0F, 0.0F, 1.0F, 0.0F, -2.0F,
                                            0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const pond::math::Matrix4x4 scale{2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F,
                                      0.0F, 0.0F, 4.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const pond::math::Vector4 point{1.0F, 1.0F, 1.0F, 1.0F};

    const pond::math::Matrix4x4 scaleAfterTranslation = scale * translation;
    const pond::math::Matrix4x4 translationAfterScale = translation * scale;

    EXPECT_EQ(scaleAfterTranslation * point, scale * (translation * point));
    EXPECT_EQ(translationAfterScale * point, translation * (scale * point));
    EXPECT_EQ(scaleAfterTranslation.row0Column3, 6.0F);
    EXPECT_EQ(scaleAfterTranslation.row1Column3, -6.0F);
    EXPECT_EQ(scaleAfterTranslation.row2Column3, 4.0F);
    EXPECT_EQ(scaleAfterTranslation.row3Column3, 1.0F);
    EXPECT_EQ(translationAfterScale.row0Column3, 3.0F);
    EXPECT_EQ(translationAfterScale.row1Column3, -2.0F);
    EXPECT_EQ(translationAfterScale.row2Column3, 1.0F);
    EXPECT_EQ(translationAfterScale.row3Column3, 1.0F);
}

TEST(Matrix4x4AlgebraTests, InvertsRepresentativeMatrices)
{
    const pond::math::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    const pond::math::Matrix4x4 matrix{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F,
                                       2.0F, 6.0F, 4.0F, 8.0F, 3.0F, 1.0F, 1.0F, 2.0F};
    const pond::math::Matrix4x4 affine{2.0F, 0.0F, 0.0F, 5.0F, 0.0F, 4.0F, 0.0F, -3.0F,
                                       0.0F, 0.0F, 0.5F, 7.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const pond::math::Matrix4x4 projective{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,  0.0F,
                                           0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.25F, 1.0F};

    auto inverse = pond::math::Inverse(matrix);
    ASSERT_TRUE(inverse.HasValue());
    ExpectMatrixNear(inverse.GetValue(), ToMatrix(InverseRows(ToRows(matrix))), 1.0e-5F);
    EXPECT_TRUE(pond::math::IsNear(matrix * inverse.GetValue(), pond::math::Matrix4x4::Identity(),
                                   tolerance));
    EXPECT_TRUE(pond::math::IsNear(inverse.GetValue() * matrix, pond::math::Matrix4x4::Identity(),
                                   tolerance));

    auto affineInverse = pond::math::Inverse(affine);
    ASSERT_TRUE(affineInverse.HasValue());
    ExpectMatrixNear(affineInverse.GetValue(),
                     pond::math::Matrix4x4{0.5F, 0.0F, 0.0F, -2.5F, 0.0F, 0.25F, 0.0F, 0.75F, 0.0F,
                                           0.0F, 2.0F, -14.0F, 0.0F, 0.0F, 0.0F, 1.0F});
    EXPECT_EQ(affineInverse.GetValue().row0Column3, -2.5F);
    EXPECT_EQ(affineInverse.GetValue().row1Column3, 0.75F);
    EXPECT_EQ(affineInverse.GetValue().row2Column3, -14.0F);
    EXPECT_EQ(affineInverse.GetValue().row3Column3, 1.0F);

    auto projectiveInverse = pond::math::Inverse(projective);
    ASSERT_TRUE(projectiveInverse.HasValue());
    ExpectMatrixNear(projectiveInverse.GetValue(),
                     pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F,
                                           0.0F, 1.0F, 0.0F, 0.0F, 0.0F, -0.25F, 1.0F});
}

TEST(Matrix4x4AlgebraTests, RejectsSingularNonFiniteAndUnrepresentableInverses)
{
    ExpectInverseFailure(pond::math::Inverse(pond::math::Matrix4x4{
                             1.0F, 2.0F, 3.0F, 4.0F, 2.0F, 4.0F, 6.0F, 8.0F, 9.0F, 10.0F, 11.0F,
                             12.0F, 13.0F, 14.0F, 15.0F, 16.0F}),
                         pond::math::MathErrorCode::SingularMatrix);

    ExpectInverseFailure(pond::math::Inverse(pond::math::Matrix4x4{
                             1.0F, 0.0F, 0.0F, 0.0F, 0.0F, std::numeric_limits<float>::infinity(),
                             0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}),
                         pond::math::MathErrorCode::NonFiniteInput);

    ExpectInverseFailure(pond::math::Inverse(pond::math::Matrix4x4{
                             std::numeric_limits<float>::denorm_min(), 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                             0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}),
                         pond::math::MathErrorCode::SingularMatrix);
}

TEST(Matrix4x4AlgebraTests, SeededWellConditionedMatricesMatchDoubleReference)
{
    std::mt19937 generator{0x4D345834U};
    std::uniform_real_distribution<float> distribution{-3.0F, 3.0F};
    constexpr double kMinimumDeterminantMagnitude{2.0};
    constexpr float kMatrixTolerance{2.0e-3F};

    for (int iteration = 0; iteration < 32; ++iteration)
    {
        MatrixRows rows{};
        double determinant = 0.0;
        do
        {
            for (auto& row : rows)
            {
                for (double& element : row)
                {
                    element = static_cast<double>(distribution(generator));
                }
            }

            rows[0][0] += 4.0;
            rows[1][1] -= 3.0;
            rows[2][2] += 2.0;
            rows[3][3] += 1.5;
            determinant = DeterminantRows(rows);
        } while (std::abs(determinant) < kMinimumDeterminantMagnitude);

        MatrixRows otherRows{};
        for (auto& row : otherRows)
        {
            for (double& element : row)
            {
                element = static_cast<double>(distribution(generator));
            }
        }

        const pond::math::Matrix4x4 matrix = ToMatrix(rows);
        const pond::math::Matrix4x4 other = ToMatrix(otherRows);
        const float determinantTolerance =
            static_cast<float>(std::abs(determinant) * 1.0e-5 + 5.0e-3);

        EXPECT_NEAR(pond::math::Determinant(matrix), static_cast<float>(determinant),
                    determinantTolerance);
        ExpectMatrixNear(matrix * other, ToMatrix(MultiplyRows(rows, otherRows)), kMatrixTolerance);

        auto inverse = pond::math::Inverse(matrix);
        ASSERT_TRUE(inverse.HasValue());
        ExpectMatrixNear(inverse.GetValue(), ToMatrix(InverseRows(rows)), kMatrixTolerance);
        ExpectMatrixNear(matrix * inverse.GetValue(), pond::math::Matrix4x4::Identity(),
                         kMatrixTolerance);
        ExpectMatrixNear(inverse.GetValue() * matrix, pond::math::Matrix4x4::Identity(),
                         kMatrixTolerance);
    }
}
} // namespace