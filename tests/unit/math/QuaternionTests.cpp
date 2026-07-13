#include <ponder/core/Numbers.hpp>
#include <ponder/math/Quaternion.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>

namespace
{
static_assert(sizeof(pond::math::Quaternion) == sizeof(float) * 4);
static_assert(alignof(pond::math::Quaternion) == alignof(float));
static_assert(std::is_standard_layout_v<pond::math::Quaternion>);
static_assert(std::is_trivially_copyable_v<pond::math::Quaternion>);
static_assert(std::is_trivially_copy_constructible_v<pond::math::Quaternion>);
static_assert(std::is_trivially_move_constructible_v<pond::math::Quaternion>);
static_assert(std::is_trivially_copy_assignable_v<pond::math::Quaternion>);
static_assert(std::is_trivially_move_assignable_v<pond::math::Quaternion>);
static_assert(std::is_trivially_destructible_v<pond::math::Quaternion>);
static_assert(!std::is_aggregate_v<pond::math::Quaternion>);
static_assert(!std::is_constructible_v<pond::math::Quaternion, float, float, float, float>);

[[nodiscard]] pond::core::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::core::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

[[nodiscard]] float SquaredLength(pond::math::Quaternion quaternion) noexcept
{
    return quaternion.GetX() * quaternion.GetX() + quaternion.GetY() * quaternion.GetY() +
           quaternion.GetZ() * quaternion.GetZ() + quaternion.GetW() * quaternion.GetW();
}

void ExpectQuaternionNear(pond::math::Quaternion actual, float expectedX, float expectedY,
                          float expectedZ, float expectedW, float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.GetX(), expectedX, tolerance);
    EXPECT_NEAR(actual.GetY(), expectedY, tolerance);
    EXPECT_NEAR(actual.GetZ(), expectedZ, tolerance);
    EXPECT_NEAR(actual.GetW(), expectedW, tolerance);
}

void ExpectVectorNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                      float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void ExpectMatrix3x3Near(pond::math::Matrix3x3 actual, pond::math::Matrix3x3 expected,
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

void ExpectMatrix4x4Near(pond::math::Matrix4x4 actual, pond::math::Matrix4x4 expected,
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
void ExpectQuaternionFailure(pond::core::Result<pond::math::Quaternion> result,
                             pond::math::MathErrorCode expectedCode)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(), pond::math::ToErrorCode(expectedCode));
}

TEST(QuaternionTests, ProvidesIdentityAndReadOnlyComponentAccessors)
{
    constexpr pond::math::Quaternion defaultQuaternion;
    constexpr pond::math::Quaternion identity = pond::math::Quaternion::Identity();

    static_assert(defaultQuaternion == identity);
    static_assert(identity.GetX() == 0.0F);
    static_assert(identity.GetY() == 0.0F);
    static_assert(identity.GetZ() == 0.0F);
    static_assert(identity.GetW() == 1.0F);

    EXPECT_EQ(defaultQuaternion, identity);
    EXPECT_EQ(identity.GetX(), 0.0F);
    EXPECT_EQ(identity.GetY(), 0.0F);
    EXPECT_EQ(identity.GetZ(), 0.0F);
    EXPECT_EQ(identity.GetW(), 1.0F);

    const std::array<float, 4> storage = std::bit_cast<std::array<float, 4>>(identity);
    EXPECT_EQ(storage, (std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F}));
}

TEST(QuaternionTests, NormalizesFiniteComponentConstructionAndPreservesOrder)
{
    const pond::core::Tolerance tolerance = RequireTolerance(1.0e-6F, 1.0e-6F);
    auto quaternion = pond::math::Quaternion::FromComponents(1.0F, 2.0F, 3.0F, 4.0F);
    ASSERT_TRUE(quaternion.HasValue());

    const float inverseLength = 1.0F / std::sqrt(30.0F);
    ExpectQuaternionNear(quaternion.GetValue(), 1.0F * inverseLength, 2.0F * inverseLength,
                         3.0F * inverseLength, 4.0F * inverseLength);
    EXPECT_TRUE(pond::core::IsNear(SquaredLength(quaternion.GetValue()), 1.0F, tolerance));

    const std::array<float, 4> storage = std::bit_cast<std::array<float, 4>>(quaternion.GetValue());
    EXPECT_FLOAT_EQ(storage[0], quaternion->GetX());
    EXPECT_FLOAT_EQ(storage[1], quaternion->GetY());
    EXPECT_FLOAT_EQ(storage[2], quaternion->GetZ());
    EXPECT_FLOAT_EQ(storage[3], quaternion->GetW());

    auto identity = pond::math::Quaternion::FromComponents(0.0F, 0.0F, 0.0F, 2.0F);
    ASSERT_TRUE(identity.HasValue());
    EXPECT_EQ(identity.GetValue(), pond::math::Quaternion::Identity());

    auto tiny = pond::math::Quaternion::FromComponents(std::numeric_limits<float>::denorm_min(),
                                                       0.0F, 0.0F, 0.0F);
    ASSERT_TRUE(tiny.HasValue());
    ExpectQuaternionNear(tiny.GetValue(), 1.0F, 0.0F, 0.0F, 0.0F, 0.0F);
}

TEST(QuaternionTests, ConstructsPrincipalAxisRotationsFromAxisAngle)
{
    const float halfSqrt = std::sqrt(0.5F);

    auto quarterTurnX = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{2.0F, 0.0F, 0.0F}, pond::math::Radians{pond::core::kHalfPi});
    ASSERT_TRUE(quarterTurnX.HasValue());
    ExpectQuaternionNear(quarterTurnX.GetValue(), halfSqrt, 0.0F, 0.0F, halfSqrt);

    auto halfTurnY = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 3.0F, 0.0F},
                                                           pond::math::Radians{pond::core::kPi});
    ASSERT_TRUE(halfTurnY.HasValue());
    ExpectQuaternionNear(halfTurnY.GetValue(), 0.0F, 1.0F, 0.0F, 0.0F);

    auto negativeQuarterTurnZ = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 0.0F, -4.0F}, pond::math::Radians{pond::core::kHalfPi});
    ASSERT_TRUE(negativeQuarterTurnZ.HasValue());
    ExpectQuaternionNear(negativeQuarterTurnZ.GetValue(), 0.0F, 0.0F, -halfSqrt, halfSqrt);

    auto zeroAngle = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 1.0F, 0.0F},
                                                           pond::math::Radians{0.0F});
    ASSERT_TRUE(zeroAngle.HasValue());
    ExpectQuaternionNear(zeroAngle.GetValue(), 0.0F, 0.0F, 0.0F, 1.0F);
}

TEST(QuaternionTests, RejectsZeroAndNonFiniteConstructionInputs)
{
    ExpectQuaternionFailure(pond::math::Quaternion::FromComponents(0.0F, 0.0F, 0.0F, 0.0F),
                            pond::math::MathErrorCode::DegenerateInput);
    ExpectQuaternionFailure(pond::math::Quaternion::FromComponents(
                                std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F, 1.0F),
                            pond::math::MathErrorCode::NonFiniteInput);
    ExpectQuaternionFailure(pond::math::Quaternion::FromComponents(
                                0.0F, std::numeric_limits<float>::infinity(), 0.0F, 1.0F),
                            pond::math::MathErrorCode::NonFiniteInput);

    ExpectQuaternionFailure(
        pond::math::Quaternion::FromAxisAngle(pond::math::Vector3::Zero(),
                                              pond::math::Radians{pond::core::kHalfPi}),
        pond::math::MathErrorCode::DegenerateInput);
    ExpectQuaternionFailure(
        pond::math::Quaternion::FromAxisAngle(
            pond::math::Vector3{1.0F, std::numeric_limits<float>::infinity(), 0.0F},
            pond::math::Radians{pond::core::kHalfPi}),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectQuaternionFailure(pond::math::Quaternion::FromAxisAngle(
                                pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                pond::math::Radians{std::numeric_limits<float>::infinity()}),
                            pond::math::MathErrorCode::NonFiniteInput);
}

TEST(QuaternionTests, UsesExactRepresentationEqualityOnly)
{
    auto identity = pond::math::Quaternion::FromComponents(0.0F, 0.0F, 0.0F, 2.0F);
    auto negativeIdentity = pond::math::Quaternion::FromComponents(0.0F, 0.0F, 0.0F, -2.0F);
    auto positiveX = pond::math::Quaternion::FromComponents(1.0F, 0.0F, 0.0F, 0.0F);
    auto negativeX = pond::math::Quaternion::FromComponents(-1.0F, 0.0F, 0.0F, 0.0F);
    ASSERT_TRUE(identity.HasValue());
    ASSERT_TRUE(negativeIdentity.HasValue());
    ASSERT_TRUE(positiveX.HasValue());
    ASSERT_TRUE(negativeX.HasValue());

    EXPECT_EQ(identity.GetValue(), pond::math::Quaternion::Identity());
    EXPECT_NE(negativeIdentity.GetValue(), pond::math::Quaternion::Identity());
    EXPECT_NE(positiveX.GetValue(), negativeX.GetValue());
    EXPECT_EQ(positiveX.GetValue(), positiveX.GetValue());
}

TEST(QuaternionTests, ConjugatesInvertsAndRotatesVectors)
{
    const float halfSqrt = std::sqrt(0.5F);
    auto quarterTurnZ = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, pond::math::Radians{pond::core::kHalfPi});
    ASSERT_TRUE(quarterTurnZ.HasValue());

    EXPECT_EQ(pond::math::Conjugate(pond::math::Quaternion::Identity()),
              pond::math::Quaternion::Identity());
    EXPECT_EQ(pond::math::Inverse(quarterTurnZ.GetValue()),
              pond::math::Conjugate(quarterTurnZ.GetValue()));
    ExpectQuaternionNear(pond::math::Conjugate(quarterTurnZ.GetValue()), 0.0F, 0.0F, -halfSqrt,
                         halfSqrt);

    const pond::math::Vector3 xAxis{1.0F, 0.0F, 0.0F};
    const pond::math::Vector3 rotated = pond::math::Rotate(quarterTurnZ.GetValue(), xAxis);
    ExpectVectorNear(rotated, pond::math::Vector3{0.0F, 1.0F, 0.0F});
    ExpectVectorNear(pond::math::Rotate(pond::math::Inverse(quarterTurnZ.GetValue()), rotated),
                     xAxis);
}

TEST(QuaternionTests, ComposesInDocumentedApplicationOrder)
{
    auto quarterTurnZ = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, pond::math::Radians{pond::core::kHalfPi});
    auto quarterTurnX = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{1.0F, 0.0F, 0.0F}, pond::math::Radians{pond::core::kHalfPi});
    ASSERT_TRUE(quarterTurnZ.HasValue());
    ASSERT_TRUE(quarterTurnX.HasValue());

    const pond::math::Vector3 xAxis{1.0F, 0.0F, 0.0F};
    const pond::math::Quaternion xAfterZ = quarterTurnX.GetValue() * quarterTurnZ.GetValue();
    const pond::math::Quaternion zAfterX = quarterTurnZ.GetValue() * quarterTurnX.GetValue();

    ExpectVectorNear(pond::math::Rotate(xAfterZ, xAxis), pond::math::Vector3{0.0F, 0.0F, 1.0F});
    ExpectVectorNear(pond::math::Rotate(zAfterX, xAxis), pond::math::Vector3{0.0F, 1.0F, 0.0F});
    ExpectVectorNear(pond::math::Rotate(xAfterZ, xAxis),
                     pond::math::Rotate(quarterTurnX.GetValue(),
                                        pond::math::Rotate(quarterTurnZ.GetValue(), xAxis)));
}

TEST(QuaternionTests, RestoresUnitLengthAcrossRepeatedComposition)
{
    const pond::core::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    auto step = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 0.0F, 1.0F},
                                                      pond::math::Radians{0.01F});
    ASSERT_TRUE(step.HasValue());

    pond::math::Quaternion accumulated = pond::math::Quaternion::Identity();
    for (int index = 0; index < 4096; ++index)
    {
        accumulated = step.GetValue() * accumulated;
    }

    EXPECT_TRUE(pond::core::IsNear(SquaredLength(accumulated), 1.0F, tolerance));
    const pond::math::Vector3 vector{0.25F, -0.5F, 2.0F};
    const pond::math::Vector3 rotated = pond::math::Rotate(accumulated, vector);
    ExpectVectorNear(pond::math::Rotate(pond::math::Inverse(accumulated), rotated), vector,
                     1.0e-4F);
}

TEST(QuaternionTests, ComparesComponentsAndSameRotationsSeparately)
{
    const pond::core::Tolerance tolerance = RequireTolerance(1.0e-5F, 1.0e-5F);
    auto rotation = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 1.0F, 0.0F},
                                                          pond::math::Radians{0.75F});
    ASSERT_TRUE(rotation.HasValue());
    auto negated = pond::math::Quaternion::FromComponents(-rotation->GetX(), -rotation->GetY(),
                                                          -rotation->GetZ(), -rotation->GetW());
    ASSERT_TRUE(negated.HasValue());

    EXPECT_FALSE(rotation.GetValue() == negated.GetValue());
    EXPECT_FALSE(pond::math::IsNear(rotation.GetValue(), negated.GetValue(), tolerance));
    EXPECT_TRUE(pond::math::IsSameRotation(rotation.GetValue(), negated.GetValue(), tolerance));

    auto close = pond::math::Quaternion::FromComponents(
        rotation->GetX() + 1.0e-6F, rotation->GetY(), rotation->GetZ(), rotation->GetW());
    ASSERT_TRUE(close.HasValue());
    EXPECT_TRUE(pond::math::IsNear(rotation.GetValue(), close.GetValue(), tolerance));
    EXPECT_TRUE(pond::math::IsSameRotation(rotation.GetValue(), close.GetValue(), tolerance));
}

TEST(QuaternionTests, SlerpsShortestPathAndPreservesUnitLength)
{
    const pond::core::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    auto halfTurnY = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 1.0F, 0.0F},
                                                           pond::math::Radians{pond::core::kPi});
    ASSERT_TRUE(halfTurnY.HasValue());

    auto start = pond::math::Slerp(pond::math::Quaternion::Identity(), halfTurnY.GetValue(), 0.0F);
    auto end = pond::math::Slerp(pond::math::Quaternion::Identity(), halfTurnY.GetValue(), 1.0F);
    auto halfway =
        pond::math::Slerp(pond::math::Quaternion::Identity(), halfTurnY.GetValue(), 0.5F);
    ASSERT_TRUE(start.HasValue());
    ASSERT_TRUE(end.HasValue());
    ASSERT_TRUE(halfway.HasValue());

    EXPECT_EQ(start.GetValue(), pond::math::Quaternion::Identity());
    EXPECT_EQ(end.GetValue(), halfTurnY.GetValue());
    EXPECT_TRUE(pond::math::IsSameRotation(end.GetValue(), halfTurnY.GetValue(), tolerance));
    EXPECT_TRUE(pond::core::IsNear(SquaredLength(halfway.GetValue()), 1.0F, tolerance));

    const pond::math::Vector3 halfwayRotated =
        pond::math::Rotate(halfway.GetValue(), pond::math::Vector3{0.0F, 0.0F, 1.0F});
    EXPECT_NEAR(std::abs(halfwayRotated.x), 1.0F, 1.0e-5F);
    EXPECT_NEAR(halfwayRotated.y, 0.0F, 1.0e-5F);
    EXPECT_NEAR(halfwayRotated.z, 0.0F, 1.0e-5F);
}

TEST(QuaternionTests, SlerpsAntipodalAndSmallAngleInputs)
{
    const pond::core::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    auto rotation = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 0.0F, 1.0F},
                                                          pond::math::Radians{0.75F});
    ASSERT_TRUE(rotation.HasValue());
    auto negated = pond::math::Quaternion::FromComponents(-rotation->GetX(), -rotation->GetY(),
                                                          -rotation->GetZ(), -rotation->GetW());
    ASSERT_TRUE(negated.HasValue());

    auto antipodal = pond::math::Slerp(rotation.GetValue(), negated.GetValue(), 0.5F);
    ASSERT_TRUE(antipodal.HasValue());
    EXPECT_TRUE(pond::math::IsSameRotation(antipodal.GetValue(), rotation.GetValue(), tolerance));
    EXPECT_TRUE(pond::core::IsNear(SquaredLength(antipodal.GetValue()), 1.0F, tolerance));

    auto smallAngle = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                                            pond::math::Radians{1.0e-4F});
    auto expectedHalf = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                                              pond::math::Radians{5.0e-5F});
    ASSERT_TRUE(smallAngle.HasValue());
    ASSERT_TRUE(expectedHalf.HasValue());

    auto smallHalf =
        pond::math::Slerp(pond::math::Quaternion::Identity(), smallAngle.GetValue(), 0.5F);
    ASSERT_TRUE(smallHalf.HasValue());
    EXPECT_TRUE(
        pond::math::IsSameRotation(smallHalf.GetValue(), expectedHalf.GetValue(), tolerance));
    EXPECT_TRUE(pond::core::IsNear(SquaredLength(smallHalf.GetValue()), 1.0F, tolerance));
}

TEST(QuaternionTests, SlerpAcceptsFiniteExtrapolationAmounts)
{
    const pond::core::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    auto quarterTurn = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, pond::math::Radians{pond::core::kHalfPi});
    auto expectedHalfTurn = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, pond::math::Radians{pond::core::kPi});
    ASSERT_TRUE(quarterTurn.HasValue());
    ASSERT_TRUE(expectedHalfTurn.HasValue());

    auto extrapolated =
        pond::math::Slerp(pond::math::Quaternion::Identity(), quarterTurn.GetValue(), 2.0F);
    ASSERT_TRUE(extrapolated.HasValue());
    EXPECT_TRUE(pond::math::IsSameRotation(extrapolated.GetValue(), expectedHalfTurn.GetValue(),
                                           tolerance));
}

TEST(QuaternionTests, RejectsInvalidSlerpParameters)
{
    ExpectQuaternionFailure(pond::math::Slerp(pond::math::Quaternion::Identity(),
                                              pond::math::Quaternion::Identity(),
                                              std::numeric_limits<float>::infinity()),
                            pond::math::MathErrorCode::NonFiniteInput);
    ExpectQuaternionFailure(pond::math::Slerp(pond::math::Quaternion::Identity(),
                                              pond::math::Quaternion::Identity(),
                                              std::numeric_limits<float>::quiet_NaN()),
                            pond::math::MathErrorCode::NonFiniteInput);
}

TEST(QuaternionTests, ConvertsIdentityAndPrincipalAxisRotationsToMatrices)
{
    const pond::core::Tolerance tolerance = RequireTolerance(1.0e-5F, 1.0e-5F);

    static_assert(pond::math::ToMatrix3x3(pond::math::Quaternion::Identity()) ==
                  pond::math::Matrix3x3::Identity());
    static_assert(pond::math::ToMatrix4x4(pond::math::Quaternion::Identity()) ==
                  pond::math::Matrix4x4::Identity());
    EXPECT_EQ(pond::math::ToMatrix3x3(pond::math::Quaternion::Identity()),
              pond::math::Matrix3x3::Identity());
    EXPECT_EQ(pond::math::ToMatrix4x4(pond::math::Quaternion::Identity()),
              pond::math::Matrix4x4::Identity());

    auto quarterTurnX = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{1.0F, 0.0F, 0.0F}, pond::math::Radians{pond::core::kHalfPi});
    auto quarterTurnY = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 1.0F, 0.0F}, pond::math::Radians{pond::core::kHalfPi});
    auto quarterTurnZ = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.0F, 0.0F, 1.0F}, pond::math::Radians{pond::core::kHalfPi});
    ASSERT_TRUE(quarterTurnX.HasValue());
    ASSERT_TRUE(quarterTurnY.HasValue());
    ASSERT_TRUE(quarterTurnZ.HasValue());

    ExpectMatrix3x3Near(
        pond::math::ToMatrix3x3(quarterTurnX.GetValue()),
        pond::math::Matrix3x3{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, -1.0F, 0.0F, 1.0F, 0.0F}, 2.0e-5F);
    ExpectMatrix3x3Near(
        pond::math::ToMatrix3x3(quarterTurnY.GetValue()),
        pond::math::Matrix3x3{0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F}, 2.0e-5F);
    ExpectMatrix3x3Near(
        pond::math::ToMatrix3x3(quarterTurnZ.GetValue()),
        pond::math::Matrix3x3{0.0F, -1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}, 2.0e-5F);

    const pond::math::Matrix4x4 quarterTurnZ4 = pond::math::ToMatrix4x4(quarterTurnZ.GetValue());
    ExpectMatrix4x4Near(quarterTurnZ4,
                        pond::math::Matrix4x4{0.0F, -1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                              0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
                        2.0e-5F);

    EXPECT_TRUE(pond::math::IsSameRotation(
        pond::math::ToQuaternion(pond::math::ToMatrix3x3(quarterTurnX.GetValue()), tolerance)
            .GetValue(),
        quarterTurnX.GetValue(), tolerance));
}

TEST(QuaternionTests, RoundTripsArbitraryAndNearHalfTurnMatrixRotations)
{
    const pond::core::Tolerance tolerance = RequireTolerance(5.0e-5F, 5.0e-5F);
    auto arbitrary = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, -2.0F, 3.0F},
                                                           pond::math::Radians{1.25F});
    auto nearHalfTurn = pond::math::Quaternion::FromAxisAngle(
        pond::math::Vector3{0.25F, 1.0F, -0.5F}, pond::math::Radians{pond::core::kPi - 1.0e-4F});
    ASSERT_TRUE(arbitrary.HasValue());
    ASSERT_TRUE(nearHalfTurn.HasValue());

    const pond::math::Matrix3x3 arbitraryMatrix = pond::math::ToMatrix3x3(arbitrary.GetValue());
    auto arbitraryRoundTrip = pond::math::ToQuaternion(arbitraryMatrix, tolerance);
    ASSERT_TRUE(arbitraryRoundTrip.HasValue());
    EXPECT_TRUE(
        pond::math::IsSameRotation(arbitraryRoundTrip.GetValue(), arbitrary.GetValue(), tolerance));

    const pond::math::Matrix4x4 arbitraryMatrix4{arbitraryMatrix.row0Column0,
                                                 arbitraryMatrix.row0Column1,
                                                 arbitraryMatrix.row0Column2,
                                                 3.0F,
                                                 arbitraryMatrix.row1Column0,
                                                 arbitraryMatrix.row1Column1,
                                                 arbitraryMatrix.row1Column2,
                                                 -4.0F,
                                                 arbitraryMatrix.row2Column0,
                                                 arbitraryMatrix.row2Column1,
                                                 arbitraryMatrix.row2Column2,
                                                 5.0F,
                                                 0.0F,
                                                 0.0F,
                                                 0.0F,
                                                 1.0F};
    auto arbitraryRoundTrip4 = pond::math::ToQuaternion(arbitraryMatrix4, tolerance);
    ASSERT_TRUE(arbitraryRoundTrip4.HasValue());
    EXPECT_TRUE(pond::math::IsSameRotation(arbitraryRoundTrip4.GetValue(), arbitrary.GetValue(),
                                           tolerance));

    auto nearHalfTurnRoundTrip =
        pond::math::ToQuaternion(pond::math::ToMatrix3x3(nearHalfTurn.GetValue()), tolerance);
    ASSERT_TRUE(nearHalfTurnRoundTrip.HasValue());
    EXPECT_TRUE(pond::math::IsSameRotation(nearHalfTurnRoundTrip.GetValue(),
                                           nearHalfTurn.GetValue(), tolerance));
}

TEST(QuaternionTests, Matrix4x4ConversionUsesOnlyUpperLeftRotationBlock)
{
    const pond::core::Tolerance tolerance = RequireTolerance(2.0e-5F, 2.0e-5F);
    auto rotation = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, -2.0F, 3.0F},
                                                          pond::math::Radians{0.75F});
    ASSERT_TRUE(rotation.HasValue());

    pond::math::Matrix4x4 matrix = pond::math::ToMatrix4x4(rotation.GetValue());
    matrix.row0Column3 = 17.0F;
    matrix.row1Column3 = -23.0F;
    matrix.row2Column3 = 31.0F;
    matrix.row3Column0 = 2.0F;
    matrix.row3Column1 = 3.0F;
    matrix.row3Column2 = 4.0F;
    matrix.row3Column3 = 0.0F;

    auto extracted = pond::math::ToQuaternion(matrix, tolerance);
    ASSERT_TRUE(extracted.HasValue());
    EXPECT_TRUE(pond::math::IsSameRotation(extracted.GetValue(), rotation.GetValue(), tolerance));
}

TEST(QuaternionTests, MatrixAndQuaternionApplicationRotateVectorsEquivalently)
{
    auto rotation = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, -2.0F, 3.0F},
                                                          pond::math::Radians{1.25F});
    ASSERT_TRUE(rotation.HasValue());

    const pond::math::Vector3 vector{0.5F, -1.25F, 2.0F};
    const pond::math::Vector3 expected = pond::math::Rotate(rotation.GetValue(), vector);
    ExpectVectorNear(pond::math::ToMatrix3x3(rotation.GetValue()) * vector, expected, 1.0e-5F);

    const pond::math::Vector4 homogeneousVector{vector.x, vector.y, vector.z, 0.0F};
    const pond::math::Vector4 matrix4Rotated =
        pond::math::ToMatrix4x4(rotation.GetValue()) * homogeneousVector;
    ExpectVectorNear(pond::math::Vector3{matrix4Rotated.x, matrix4Rotated.y, matrix4Rotated.z},
                     expected, 1.0e-5F);
    EXPECT_NEAR(matrix4Rotated.w, 0.0F, 1.0e-5F);
}

TEST(QuaternionTests, MatrixCompositionMatchesQuaternionComposition)
{
    auto a = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                                   pond::math::Radians{0.5F});
    auto b = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 1.0F, 0.0F},
                                                   pond::math::Radians{-0.75F});
    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());

    const pond::math::Quaternion quaternionComposed = b.GetValue() * a.GetValue();
    const pond::math::Matrix3x3 matrixComposed =
        pond::math::ToMatrix3x3(b.GetValue()) * pond::math::ToMatrix3x3(a.GetValue());
    ExpectMatrix3x3Near(pond::math::ToMatrix3x3(quaternionComposed), matrixComposed, 1.0e-5F);

    const pond::math::Vector3 vector{0.25F, 1.5F, -2.0F};
    ExpectVectorNear(matrixComposed * vector, pond::math::Rotate(quaternionComposed, vector),
                     1.0e-5F);
}

TEST(QuaternionTests, RejectsInvalidMatrixRotationInputs)
{
    const pond::core::Tolerance tolerance = RequireTolerance(1.0e-5F, 1.0e-5F);

    ExpectQuaternionFailure(
        pond::math::ToQuaternion(pond::math::Matrix3x3{std::numeric_limits<float>::infinity(), 0.0F,
                                                       0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
                                 tolerance),
        pond::math::MathErrorCode::NonFiniteInput);
    ExpectQuaternionFailure(
        pond::math::ToQuaternion(
            pond::math::Matrix3x3{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}, tolerance),
        pond::math::MathErrorCode::SingularMatrix);
    ExpectQuaternionFailure(
        pond::math::ToQuaternion(
            pond::math::Matrix3x3{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, -1.0F},
            tolerance),
        pond::math::MathErrorCode::DegenerateInput);
    ExpectQuaternionFailure(
        pond::math::ToQuaternion(
            pond::math::Matrix3x3{2.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F}, tolerance),
        pond::math::MathErrorCode::DegenerateInput);
    ExpectQuaternionFailure(
        pond::math::ToQuaternion(
            pond::math::Matrix3x3{1.0F, 0.25F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
            tolerance),
        pond::math::MathErrorCode::DegenerateInput);
    ExpectQuaternionFailure(
        pond::math::ToQuaternion(pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F,
                                                       0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F,
                                                       0.0F, 1.0F},
                                 tolerance),
        pond::math::MathErrorCode::DegenerateInput);
}

TEST(QuaternionTests, CallerToleranceControlsMatrixRotationValidation)
{
    const pond::core::Tolerance tightTolerance = RequireTolerance(1.0e-6F, 1.0e-6F);
    const pond::core::Tolerance looseTolerance = RequireTolerance(1.0e-3F, 1.0e-3F);
    const pond::math::Matrix3x3 slightlyScaledIdentity{1.0001F, 0.0F, 0.0F, 0.0F, 1.0F,
                                                       0.0F,    0.0F, 0.0F, 1.0F};

    ExpectQuaternionFailure(pond::math::ToQuaternion(slightlyScaledIdentity, tightTolerance),
                            pond::math::MathErrorCode::DegenerateInput);
    auto looseResult = pond::math::ToQuaternion(slightlyScaledIdentity, looseTolerance);
    ASSERT_TRUE(looseResult.HasValue());
    EXPECT_TRUE(pond::math::IsSameRotation(looseResult.GetValue(),
                                           pond::math::Quaternion::Identity(), looseTolerance));
}
} // namespace
