#include <ponder/core/Numbers.hpp>
#include <ponder/math/Matrix3x3.hpp>

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <random>

namespace
{
using MatrixRows = std::array<std::array<double, 3>, 3>;

[[nodiscard]] pond::core::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::core::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

void ExpectMatrixNear(pond::math::Matrix3x3 actual, pond::math::Matrix3x3 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.row0Column0, expected.row0Column0, tolerance);
    EXPECT_NEAR(actual.row0Column1, expected.row0Column1, tolerance);
    EXPECT_NEAR(actual.row0Column2, expected.row0Column2, tolerance);
    EXPECT_NEAR(actual.row1Column0, expected.row1Column0, tolerance);
    EXPECT_NEAR(actual.row1Column1, expected.row1Column1, tolerance);
    EXPECT_NEAR(actual.row1Column2, expected.row1Column2, tolerance);
    EXPECT_NEAR(actual.row2Column0, expected.row2Column0, tolerance);
    EXPECT_NEAR(actual.row2Column1, expected.row2Column1, tolerance);
    EXPECT_NEAR(actual.row2Column2, expected.row2Column2, tolerance);
}

void ExpectInverseFailure(pond::core::Result<pond::math::Matrix3x3> result,
                          pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

[[nodiscard]] pond::math::Matrix3x3 ToMatrix(MatrixRows rows) noexcept
{
    return pond::math::Matrix3x3{static_cast<float>(rows[0][0]), static_cast<float>(rows[0][1]),
                                 static_cast<float>(rows[0][2]), static_cast<float>(rows[1][0]),
                                 static_cast<float>(rows[1][1]), static_cast<float>(rows[1][2]),
                                 static_cast<float>(rows[2][0]), static_cast<float>(rows[2][1]),
                                 static_cast<float>(rows[2][2])};
}

[[nodiscard]] MatrixRows MultiplyRows(MatrixRows lhs, MatrixRows rhs) noexcept
{
    MatrixRows result{};
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            for (std::size_t index = 0; index < 3; ++index)
            {
                result[row][column] += lhs[row][index] * rhs[index][column];
            }
        }
    }

    return result;
}

[[nodiscard]] double DeterminantRows(MatrixRows rows) noexcept
{
    return rows[0][0] * (rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1]) -
           rows[0][1] * (rows[1][0] * rows[2][2] - rows[1][2] * rows[2][0]) +
           rows[0][2] * (rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]);
}

[[nodiscard]] MatrixRows InverseRows(MatrixRows rows) noexcept
{
    const double determinant = DeterminantRows(rows);
    const double inverseDeterminant = 1.0 / determinant;

    return MatrixRows{{{(rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1]) * inverseDeterminant,
                        (rows[0][2] * rows[2][1] - rows[0][1] * rows[2][2]) * inverseDeterminant,
                        (rows[0][1] * rows[1][2] - rows[0][2] * rows[1][1]) * inverseDeterminant},
                       {(rows[1][2] * rows[2][0] - rows[1][0] * rows[2][2]) * inverseDeterminant,
                        (rows[0][0] * rows[2][2] - rows[0][2] * rows[2][0]) * inverseDeterminant,
                        (rows[0][2] * rows[1][0] - rows[0][0] * rows[1][2]) * inverseDeterminant},
                       {(rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]) * inverseDeterminant,
                        (rows[0][1] * rows[2][0] - rows[0][0] * rows[2][1]) * inverseDeterminant,
                        (rows[0][0] * rows[1][1] - rows[0][1] * rows[1][0]) * inverseDeterminant}}};
}

TEST(Matrix3x3AlgebraTests, AppliesArithmeticAndScalarOperations)
{
    const pond::math::Matrix3x3 lhs{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
    const pond::math::Matrix3x3 rhs{9.0F, 8.0F, 7.0F, 6.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F};

    EXPECT_EQ(lhs + rhs, (pond::math::Matrix3x3{10.0F, 10.0F, 10.0F, 10.0F, 10.0F, 10.0F, 10.0F,
                                                10.0F, 10.0F}));
    EXPECT_EQ(lhs - rhs,
              (pond::math::Matrix3x3{-8.0F, -6.0F, -4.0F, -2.0F, 0.0F, 2.0F, 4.0F, 6.0F, 8.0F}));
    EXPECT_EQ(lhs * 2.0F,
              (pond::math::Matrix3x3{2.0F, 4.0F, 6.0F, 8.0F, 10.0F, 12.0F, 14.0F, 16.0F, 18.0F}));
    EXPECT_EQ(2.0F * lhs, lhs * 2.0F);
    EXPECT_EQ((lhs * 2.0F) / 2.0F, lhs);
}

TEST(Matrix3x3AlgebraTests, TransformsVectorsWithIdentityScaleShearAndRotation)
{
    const pond::math::Vector3 vector{1.0F, 2.0F, 3.0F};
    const pond::math::Matrix3x3 scale{2.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F, 0.0F, 4.0F};
    const pond::math::Matrix3x3 shear{1.0F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const pond::math::Matrix3x3 quarterTurnZ{0.0F, -1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

    EXPECT_EQ(pond::math::Matrix3x3::Identity() * vector, vector);
    EXPECT_EQ(scale * vector, (pond::math::Vector3{2.0F, 6.0F, 12.0F}));
    EXPECT_EQ(shear * vector, (pond::math::Vector3{2.0F, 2.0F, 3.0F}));
    EXPECT_EQ((quarterTurnZ * pond::math::Vector3{1.0F, 0.0F, 0.0F}),
              (pond::math::Vector3{0.0F, 1.0F, 0.0F}));
}

TEST(Matrix3x3AlgebraTests, TransposesAndComputesDeterminants)
{
    const pond::math::Matrix3x3 matrix{1.0F, 2.0F, 3.0F, 0.0F, 1.0F, 4.0F, 5.0F, 6.0F, 0.0F};

    EXPECT_EQ(pond::math::Transpose(matrix),
              (pond::math::Matrix3x3{1.0F, 0.0F, 5.0F, 2.0F, 1.0F, 6.0F, 3.0F, 4.0F, 0.0F}));
    EXPECT_EQ(pond::math::Determinant(pond::math::Matrix3x3::Identity()), 1.0F);
    EXPECT_EQ(pond::math::Determinant(matrix), 1.0F);
    EXPECT_EQ(pond::math::Determinant(
                  pond::math::Matrix3x3{2.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F, 0.0F, 4.0F}),
              24.0F);
}

TEST(Matrix3x3AlgebraTests, ComposesInColumnVectorApplicationOrder)
{
    const pond::math::Matrix3x3 scale{2.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F, 0.0F, 4.0F};
    const pond::math::Matrix3x3 shear{1.0F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const pond::math::Vector3 vector{1.0F, 1.0F, 1.0F};

    const pond::math::Matrix3x3 shearAfterScale = shear * scale;
    const pond::math::Matrix3x3 scaleAfterShear = scale * shear;

    EXPECT_EQ(shearAfterScale * vector, (pond::math::Vector3{3.5F, 3.0F, 4.0F}));
    EXPECT_EQ(scaleAfterShear * vector, (pond::math::Vector3{3.0F, 3.0F, 4.0F}));
    EXPECT_EQ(shearAfterScale * vector, shear * (scale * vector));
    EXPECT_EQ(scaleAfterShear * vector, scale * (shear * vector));
}

TEST(Matrix3x3AlgebraTests, InvertsRepresentativeMatrices)
{
    const pond::core::Tolerance tolerance = RequireTolerance(1.0e-5F, 1.0e-5F);
    const pond::math::Matrix3x3 matrix{1.0F, 2.0F, 3.0F, 0.0F, 1.0F, 4.0F, 5.0F, 6.0F, 0.0F};
    const pond::math::Matrix3x3 expectedInverse{-24.0F, 18.0F, 5.0F, 20.0F, -15.0F,
                                                -4.0F,  -5.0F, 4.0F, 1.0F};
    const pond::math::Matrix3x3 quarterTurnZ{0.0F, -1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

    auto inverse = pond::math::Inverse(matrix);
    ASSERT_TRUE(inverse.HasValue());
    EXPECT_EQ(inverse.GetValue(), expectedInverse);
    EXPECT_TRUE(pond::math::IsNear(matrix * inverse.GetValue(), pond::math::Matrix3x3::Identity(),
                                   tolerance));
    EXPECT_TRUE(pond::math::IsNear(inverse.GetValue() * matrix, pond::math::Matrix3x3::Identity(),
                                   tolerance));

    auto rotationInverse = pond::math::Inverse(quarterTurnZ);
    ASSERT_TRUE(rotationInverse.HasValue());
    EXPECT_EQ(rotationInverse.GetValue(), pond::math::Transpose(quarterTurnZ));

    auto scaleInverse = pond::math::Inverse(
        pond::math::Matrix3x3{2.0F, 0.0F, 0.0F, 0.0F, 4.0F, 0.0F, 0.0F, 0.0F, 8.0F});
    ASSERT_TRUE(scaleInverse.HasValue());
    EXPECT_EQ(scaleInverse.GetValue(),
              (pond::math::Matrix3x3{0.5F, 0.0F, 0.0F, 0.0F, 0.25F, 0.0F, 0.0F, 0.0F, 0.125F}));
}

TEST(Matrix3x3AlgebraTests, RejectsSingularNonFiniteAndUnrepresentableInverses)
{
    ExpectInverseFailure(pond::math::Inverse(pond::math::Matrix3x3{1.0F, 2.0F, 3.0F, 2.0F, 4.0F,
                                                                   6.0F, 7.0F, 8.0F, 9.0F}),
                         pond::math::MathErrorCode::SingularMatrix);

    ExpectInverseFailure(pond::math::Inverse(pond::math::Matrix3x3{
                             1.0F, 0.0F, 0.0F, 0.0F, std::numeric_limits<float>::infinity(), 0.0F,
                             0.0F, 0.0F, 1.0F}),
                         pond::math::MathErrorCode::NonFiniteInput);

    ExpectInverseFailure(
        pond::math::Inverse(pond::math::Matrix3x3{std::numeric_limits<float>::denorm_min(), 0.0F,
                                                  0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F}),
        pond::math::MathErrorCode::SingularMatrix);
}

TEST(Matrix3x3AlgebraTests, SeededWellConditionedMatricesMatchDoubleReference)
{
    std::mt19937 generator{0x4D335833U};
    std::uniform_real_distribution<float> distribution{-3.0F, 3.0F};
    constexpr double kMinimumDeterminantMagnitude{0.5};
    constexpr float kMatrixTolerance{2.0e-4F};

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

            rows[0][0] += 2.0;
            rows[1][1] -= 1.5;
            rows[2][2] += 1.0;
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

        const pond::math::Matrix3x3 matrix = ToMatrix(rows);
        const pond::math::Matrix3x3 other = ToMatrix(otherRows);
        const float determinantTolerance =
            static_cast<float>(std::abs(determinant) * 1.0e-5 + 1.0e-4);

        EXPECT_NEAR(pond::math::Determinant(matrix), static_cast<float>(determinant),
                    determinantTolerance);
        ExpectMatrixNear(matrix * other, ToMatrix(MultiplyRows(rows, otherRows)), kMatrixTolerance);

        auto inverse = pond::math::Inverse(matrix);
        ASSERT_TRUE(inverse.HasValue());
        ExpectMatrixNear(inverse.GetValue(), ToMatrix(InverseRows(rows)), 1.0e-3F);
        ExpectMatrixNear(matrix * inverse.GetValue(), pond::math::Matrix3x3::Identity(), 1.0e-3F);
        ExpectMatrixNear(inverse.GetValue() * matrix, pond::math::Matrix3x3::Identity(), 1.0e-3F);
    }
}
} // namespace
