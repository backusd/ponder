#include <ponder/core/Numbers.hpp>
#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/Frustum.hpp>
#include <ponder/math/Matrix3x3.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Quaternion.hpp>
#include <ponder/math/RayBoxHit.hpp>
#include <ponder/math/RayTriangleHit.hpp>
#include <ponder/math/Sphere.hpp>
#include <ponder/math/Vector2.hpp>
#include <ponder/math/Vector3.hpp>
#include <ponder/math/Vector4.hpp>

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <gtest/gtest.h>

namespace
{
inline constexpr float kReferenceTolerance{1.0e-5F};

void ExpectVector2Near(pond::math::Vector2 actual, DirectX::XMVECTOR expected)
{
    EXPECT_NEAR(actual.x, DirectX::XMVectorGetX(expected), kReferenceTolerance);
    EXPECT_NEAR(actual.y, DirectX::XMVectorGetY(expected), kReferenceTolerance);
}

void ExpectVector3Near(pond::math::Vector3 actual, DirectX::XMVECTOR expected,
                       float tolerance = kReferenceTolerance)
{
    EXPECT_NEAR(actual.x, DirectX::XMVectorGetX(expected), tolerance);
    EXPECT_NEAR(actual.y, DirectX::XMVectorGetY(expected), tolerance);
    EXPECT_NEAR(actual.z, DirectX::XMVectorGetZ(expected), tolerance);
}

void ExpectVector4Near(pond::math::Vector4 actual, DirectX::XMVECTOR expected,
                       float tolerance = kReferenceTolerance)
{
    EXPECT_NEAR(actual.x, DirectX::XMVectorGetX(expected), tolerance);
    EXPECT_NEAR(actual.y, DirectX::XMVectorGetY(expected), tolerance);
    EXPECT_NEAR(actual.z, DirectX::XMVectorGetZ(expected), tolerance);
    EXPECT_NEAR(actual.w, DirectX::XMVectorGetW(expected), tolerance);
}

void ExpectQuaternionNear(pond::math::Quaternion actual, DirectX::XMVECTOR expected,
                          float tolerance = kReferenceTolerance)
{
    EXPECT_NEAR(actual.GetX(), DirectX::XMVectorGetX(expected), tolerance);
    EXPECT_NEAR(actual.GetY(), DirectX::XMVectorGetY(expected), tolerance);
    EXPECT_NEAR(actual.GetZ(), DirectX::XMVectorGetZ(expected), tolerance);
    EXPECT_NEAR(actual.GetW(), DirectX::XMVectorGetW(expected), tolerance);
}

void ExpectQuaternionSameRotation(pond::math::Quaternion actual, DirectX::XMVECTOR expected,
                                  float tolerance = kReferenceTolerance)
{
    const float dot = actual.GetX() * DirectX::XMVectorGetX(expected) +
                      actual.GetY() * DirectX::XMVectorGetY(expected) +
                      actual.GetZ() * DirectX::XMVectorGetZ(expected) +
                      actual.GetW() * DirectX::XMVectorGetW(expected);
    const DirectX::XMVECTOR adjusted = dot < 0.0F ? DirectX::XMVectorNegate(expected) : expected;
    ExpectQuaternionNear(actual, adjusted, tolerance);
}

[[nodiscard]] DirectX::XMVECTOR ToDirectXQuaternion(pond::math::Quaternion quaternion) noexcept
{
    return DirectX::XMVectorSet(quaternion.GetX(), quaternion.GetY(), quaternion.GetZ(),
                                quaternion.GetW());
}

[[nodiscard]] DirectX::XMMATRIX ToDirectXMatrix(pond::math::Matrix3x3 matrix) noexcept
{
    return DirectX::XMMatrixSet(matrix.row0Column0, matrix.row0Column1, matrix.row0Column2, 0.0F,
                                matrix.row1Column0, matrix.row1Column1, matrix.row1Column2, 0.0F,
                                matrix.row2Column0, matrix.row2Column1, matrix.row2Column2, 0.0F,
                                0.0F, 0.0F, 0.0F, 1.0F);
}

void ExpectMatrix3x3Near(pond::math::Matrix3x3 actual, const DirectX::XMMATRIX& expected,
                         float tolerance = kReferenceTolerance)
{
    DirectX::XMFLOAT4X4 stored{};
    DirectX::XMStoreFloat4x4(&stored, expected);

    EXPECT_NEAR(actual.row0Column0, stored._11, tolerance);
    EXPECT_NEAR(actual.row0Column1, stored._12, tolerance);
    EXPECT_NEAR(actual.row0Column2, stored._13, tolerance);
    EXPECT_NEAR(actual.row1Column0, stored._21, tolerance);
    EXPECT_NEAR(actual.row1Column1, stored._22, tolerance);
    EXPECT_NEAR(actual.row1Column2, stored._23, tolerance);
    EXPECT_NEAR(actual.row2Column0, stored._31, tolerance);
    EXPECT_NEAR(actual.row2Column1, stored._32, tolerance);
    EXPECT_NEAR(actual.row2Column2, stored._33, tolerance);
}

[[nodiscard]] DirectX::XMMATRIX ToDirectXMatrix(pond::math::Matrix4x4 matrix) noexcept
{
    return DirectX::XMMatrixSet(
        matrix.row0Column0, matrix.row0Column1, matrix.row0Column2, matrix.row0Column3,
        matrix.row1Column0, matrix.row1Column1, matrix.row1Column2, matrix.row1Column3,
        matrix.row2Column0, matrix.row2Column1, matrix.row2Column2, matrix.row2Column3,
        matrix.row3Column0, matrix.row3Column1, matrix.row3Column2, matrix.row3Column3);
}

void ExpectMatrix4x4Near(pond::math::Matrix4x4 actual, const DirectX::XMMATRIX& expected,
                         float tolerance = kReferenceTolerance)
{
    DirectX::XMFLOAT4X4 stored{};
    DirectX::XMStoreFloat4x4(&stored, expected);

    EXPECT_NEAR(actual.row0Column0, stored._11, tolerance);
    EXPECT_NEAR(actual.row0Column1, stored._12, tolerance);
    EXPECT_NEAR(actual.row0Column2, stored._13, tolerance);
    EXPECT_NEAR(actual.row0Column3, stored._14, tolerance);
    EXPECT_NEAR(actual.row1Column0, stored._21, tolerance);
    EXPECT_NEAR(actual.row1Column1, stored._22, tolerance);
    EXPECT_NEAR(actual.row1Column2, stored._23, tolerance);
    EXPECT_NEAR(actual.row1Column3, stored._24, tolerance);
    EXPECT_NEAR(actual.row2Column0, stored._31, tolerance);
    EXPECT_NEAR(actual.row2Column1, stored._32, tolerance);
    EXPECT_NEAR(actual.row2Column2, stored._33, tolerance);
    EXPECT_NEAR(actual.row2Column3, stored._34, tolerance);
    EXPECT_NEAR(actual.row3Column0, stored._41, tolerance);
    EXPECT_NEAR(actual.row3Column1, stored._42, tolerance);
    EXPECT_NEAR(actual.row3Column2, stored._43, tolerance);
    EXPECT_NEAR(actual.row3Column3, stored._44, tolerance);
}
[[nodiscard]] pond::math::CollisionClassification ToCollisionClassification(
    DirectX::ContainmentType containment) noexcept
{
    switch (containment)
    {
    case DirectX::DISJOINT:
        return pond::math::CollisionClassification::Disjoint;
    case DirectX::INTERSECTS:
        return pond::math::CollisionClassification::Intersects;
    case DirectX::CONTAINS:
        return pond::math::CollisionClassification::Contains;
    }

    return pond::math::CollisionClassification::Intersects;
}

[[nodiscard]] DirectX::BoundingBox ToDirectXCollisionBox(pond::math::AxisAlignedBox box) noexcept
{
    const pond::math::Vector3 minimum = box.GetMinimum();
    const pond::math::Vector3 maximum = box.GetMaximum();
    const float directXMinimumZ = -maximum.z;
    const float directXMaximumZ = -minimum.z;
    const DirectX::XMFLOAT3 center{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F,
                                   (directXMinimumZ + directXMaximumZ) * 0.5F};
    const DirectX::XMFLOAT3 extents{(maximum.x - minimum.x) * 0.5F, (maximum.y - minimum.y) * 0.5F,
                                    (directXMaximumZ - directXMinimumZ) * 0.5F};

    return DirectX::BoundingBox{center, extents};
}

[[nodiscard]] DirectX::BoundingSphere ToDirectXCollisionSphere(pond::math::Sphere sphere) noexcept
{
    const pond::math::Vector3 center = sphere.GetCenter();
    return DirectX::BoundingSphere{DirectX::XMFLOAT3{center.x, center.y, -center.z},
                                   sphere.GetRadius()};
}
[[nodiscard]] DirectX::XMVECTOR ToDirectXCollisionPoint(pond::math::Vector3 point) noexcept
{
    return DirectX::XMVectorSet(point.x, point.y, -point.z, 1.0F);
}

[[nodiscard]] DirectX::XMVECTOR ToDirectXCollisionDirection(pond::math::Vector3 direction) noexcept
{
    return DirectX::XMVectorSet(direction.x, direction.y, -direction.z, 0.0F);
}

TEST(MathDirectXReferenceTests, DirectXReferenceHeadersCompileInIsolatedTarget)
{
    const DirectX::XMVECTOR zero = DirectX::XMVectorZero();
    EXPECT_FLOAT_EQ(DirectX::XMVectorGetX(zero), 0.0F);

    DirectX::BoundingSphere sphere{};
    sphere.Center = DirectX::XMFLOAT3{0.0F, 0.0F, 0.0F};
    sphere.Radius = 1.0F;

    EXPECT_FLOAT_EQ(sphere.Radius, 1.0F);
}

TEST(MathDirectXReferenceTests, Vector2FiniteOperationsMatchDirectXMath)
{
    const pond::math::Vector2 a{1.25F, -2.5F};
    const pond::math::Vector2 b{-3.0F, 4.5F};
    const DirectX::XMVECTOR dxA = DirectX::XMVectorSet(a.x, a.y, 0.0F, 0.0F);
    const DirectX::XMVECTOR dxB = DirectX::XMVectorSet(b.x, b.y, 0.0F, 0.0F);

    ExpectVector2Near(a + b, DirectX::XMVectorAdd(dxA, dxB));
    ExpectVector2Near(a - b, DirectX::XMVectorSubtract(dxA, dxB));
    ExpectVector2Near(-a, DirectX::XMVectorNegate(dxA));
    ExpectVector2Near(a * 2.0F, DirectX::XMVectorScale(dxA, 2.0F));
    ExpectVector2Near(a / 2.0F, DirectX::XMVectorScale(dxA, 0.5F));
    ExpectVector2Near(pond::math::ComponentMultiply(a, b), DirectX::XMVectorMultiply(dxA, dxB));
    ExpectVector2Near(pond::math::ComponentDivide(a, b), DirectX::XMVectorDivide(dxA, dxB));
    ExpectVector2Near(pond::math::ComponentMin(a, b), DirectX::XMVectorMin(dxA, dxB));
    ExpectVector2Near(pond::math::ComponentMax(a, b), DirectX::XMVectorMax(dxA, dxB));
    ExpectVector2Near(pond::math::Lerp(a, b, 0.25F), DirectX::XMVectorLerp(dxA, dxB, 0.25F));
    EXPECT_NEAR(pond::math::Dot(a, b), DirectX::XMVectorGetX(DirectX::XMVector2Dot(dxA, dxB)),
                kReferenceTolerance);
    EXPECT_NEAR(pond::math::Length(a), DirectX::XMVectorGetX(DirectX::XMVector2Length(dxA)),
                kReferenceTolerance);

    auto normalized = pond::math::Normalize(a);
    ASSERT_TRUE(normalized.HasValue());
    ExpectVector2Near(normalized.GetValue(), DirectX::XMVector2Normalize(dxA));
}

TEST(MathDirectXReferenceTests, Vector3FiniteOperationsMatchDirectXMath)
{
    const pond::math::Vector3 a{1.25F, -2.5F, 3.75F};
    const pond::math::Vector3 b{-3.0F, 4.5F, -6.0F};
    const DirectX::XMVECTOR dxA = DirectX::XMVectorSet(a.x, a.y, a.z, 0.0F);
    const DirectX::XMVECTOR dxB = DirectX::XMVectorSet(b.x, b.y, b.z, 0.0F);

    ExpectVector3Near(a + b, DirectX::XMVectorAdd(dxA, dxB));
    ExpectVector3Near(a - b, DirectX::XMVectorSubtract(dxA, dxB));
    ExpectVector3Near(a * -2.0F, DirectX::XMVectorScale(dxA, -2.0F));
    ExpectVector3Near(pond::math::ComponentMultiply(a, b), DirectX::XMVectorMultiply(dxA, dxB));
    ExpectVector3Near(pond::math::ComponentDivide(a, b), DirectX::XMVectorDivide(dxA, dxB));
    ExpectVector3Near(pond::math::Cross(a, b), DirectX::XMVector3Cross(dxA, dxB));
    ExpectVector3Near(pond::math::Lerp(a, b, 0.75F), DirectX::XMVectorLerp(dxA, dxB, 0.75F));
    EXPECT_NEAR(pond::math::Dot(a, b), DirectX::XMVectorGetX(DirectX::XMVector3Dot(dxA, dxB)),
                kReferenceTolerance);
    EXPECT_NEAR(
        pond::math::Distance(a, b),
        DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(dxA, dxB))),
        kReferenceTolerance);

    auto normalized = pond::math::Normalize(a);
    ASSERT_TRUE(normalized.HasValue());
    ExpectVector3Near(normalized.GetValue(), DirectX::XMVector3Normalize(dxA));
}

TEST(MathDirectXReferenceTests, Vector4FiniteOperationsMatchDirectXMath)
{
    const pond::math::Vector4 a{1.25F, -2.5F, 3.75F, -5.0F};
    const pond::math::Vector4 b{-3.0F, 4.5F, -6.0F, 7.5F};
    const DirectX::XMVECTOR dxA = DirectX::XMVectorSet(a.x, a.y, a.z, a.w);
    const DirectX::XMVECTOR dxB = DirectX::XMVectorSet(b.x, b.y, b.z, b.w);

    ExpectVector4Near(a + b, DirectX::XMVectorAdd(dxA, dxB));
    ExpectVector4Near(a - b, DirectX::XMVectorSubtract(dxA, dxB));
    ExpectVector4Near(-a, DirectX::XMVectorNegate(dxA));
    ExpectVector4Near(2.0F * a, DirectX::XMVectorScale(dxA, 2.0F));
    ExpectVector4Near(pond::math::ComponentMultiply(a, b), DirectX::XMVectorMultiply(dxA, dxB));
    ExpectVector4Near(pond::math::ComponentDivide(a, b), DirectX::XMVectorDivide(dxA, dxB));
    ExpectVector4Near(pond::math::ComponentMin(a, b), DirectX::XMVectorMin(dxA, dxB));
    ExpectVector4Near(pond::math::ComponentMax(a, b), DirectX::XMVectorMax(dxA, dxB));
    EXPECT_NEAR(pond::math::Dot(a, b), DirectX::XMVectorGetX(DirectX::XMVector4Dot(dxA, dxB)),
                kReferenceTolerance);
    EXPECT_NEAR(pond::math::Length(a), DirectX::XMVectorGetX(DirectX::XMVector4Length(dxA)),
                kReferenceTolerance);

    auto normalized = pond::math::Normalize(a);
    ASSERT_TRUE(normalized.HasValue());
    ExpectVector4Near(normalized.GetValue(), DirectX::XMVector4Normalize(dxA));
}

TEST(MathDirectXReferenceTests, Matrix3x3FiniteOperationsMatchDirectXMath)
{
    const pond::math::Matrix3x3 a{1.25F, -2.5F, 3.75F, 0.5F, 4.0F, -1.5F, -2.0F, 0.25F, 2.5F};
    const pond::math::Matrix3x3 b{-3.0F, 1.5F, 2.25F, 4.5F, -0.75F, 1.0F, 0.5F, 3.0F, -2.0F};
    const pond::math::Vector3 vector{2.0F, -1.5F, 0.25F};
    const DirectX::XMMATRIX dxA = ToDirectXMatrix(a);
    const DirectX::XMMATRIX dxB = ToDirectXMatrix(b);
    const DirectX::XMVECTOR dxVector = DirectX::XMVectorSet(vector.x, vector.y, vector.z, 0.0F);

    ExpectMatrix3x3Near(a + b, dxA + dxB);
    ExpectMatrix3x3Near(a - b, dxA - dxB);
    ExpectMatrix3x3Near(a * 2.0F, dxA * 2.0F);
    ExpectMatrix3x3Near(a / 2.0F, dxA * 0.5F);
    ExpectMatrix3x3Near(pond::math::Transpose(a), DirectX::XMMatrixTranspose(dxA));
    ExpectMatrix3x3Near(a * b, DirectX::XMMatrixMultiply(dxA, dxB), 1.0e-4F);

    ExpectVector3Near(a * vector,
                      DirectX::XMVector3TransformNormal(dxVector, DirectX::XMMatrixTranspose(dxA)),
                      1.0e-4F);

    EXPECT_NEAR(pond::math::Determinant(a),
                DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(dxA)), 1.0e-4F);

    DirectX::XMVECTOR dxDeterminant{};
    const DirectX::XMMATRIX dxInverse = DirectX::XMMatrixInverse(&dxDeterminant, dxA);
    auto inverse = pond::math::Inverse(a);
    ASSERT_TRUE(inverse.HasValue());
    ExpectMatrix3x3Near(inverse.GetValue(), dxInverse, 1.0e-4F);

    const pond::math::Matrix3x3 composed = b * a;
    const pond::math::Vector3 actual = composed * vector;
    const DirectX::XMMATRIX dxColumnVectorA = DirectX::XMMatrixTranspose(dxA);
    const DirectX::XMMATRIX dxColumnVectorB = DirectX::XMMatrixTranspose(dxB);
    const DirectX::XMVECTOR expected = DirectX::XMVector3TransformNormal(
        DirectX::XMVector3TransformNormal(dxVector, dxColumnVectorA), dxColumnVectorB);

    ExpectVector3Near(actual, expected, 1.0e-4F);
}

TEST(MathDirectXReferenceTests, Matrix4x4FiniteOperationsMatchDirectXMath)
{
    const pond::math::Matrix4x4 a{2.0F, 0.25F, 0.5F, 3.0F, 0.0F, 3.0F, -0.5F, -2.0F,
                                  0.0F, 0.0F,  4.0F, 1.0F, 0.0F, 0.0F, 0.25F, 1.0F};
    const pond::math::Matrix4x4 b{1.5F, -0.25F, 0.0F,  -1.0F, 0.5F, 2.0F, 0.25F, 2.0F,
                                  0.0F, 0.1F,   1.25F, 0.5F,  0.0F, 0.0F, 0.0F,  1.0F};
    const pond::math::Vector4 vector{2.0F, -1.5F, 0.25F, 1.0F};
    const DirectX::XMMATRIX dxA = ToDirectXMatrix(a);
    const DirectX::XMMATRIX dxB = ToDirectXMatrix(b);
    const DirectX::XMVECTOR dxVector = DirectX::XMVectorSet(vector.x, vector.y, vector.z, vector.w);

    ExpectMatrix4x4Near(a + b, dxA + dxB);
    ExpectMatrix4x4Near(a - b, dxA - dxB);
    ExpectMatrix4x4Near(a * 2.0F, dxA * 2.0F);
    ExpectMatrix4x4Near(a / 2.0F, dxA * 0.5F);
    ExpectMatrix4x4Near(pond::math::Transpose(a), DirectX::XMMatrixTranspose(dxA));
    ExpectMatrix4x4Near(a * b, DirectX::XMMatrixMultiply(dxA, dxB), 1.0e-4F);

    ExpectVector4Near(a * vector,
                      DirectX::XMVector4Transform(dxVector, DirectX::XMMatrixTranspose(dxA)),
                      1.0e-4F);

    EXPECT_NEAR(pond::math::Determinant(a),
                DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(dxA)), 1.0e-4F);

    DirectX::XMVECTOR dxDeterminant{};
    const DirectX::XMMATRIX dxInverse = DirectX::XMMatrixInverse(&dxDeterminant, dxA);
    auto inverse = pond::math::Inverse(a);
    ASSERT_TRUE(inverse.HasValue());
    ExpectMatrix4x4Near(inverse.GetValue(), dxInverse, 1.0e-4F);

    const pond::math::Matrix4x4 composed = b * a;
    const pond::math::Vector4 actual = composed * vector;
    const DirectX::XMMATRIX dxColumnVectorA = DirectX::XMMatrixTranspose(dxA);
    const DirectX::XMMATRIX dxColumnVectorB = DirectX::XMMatrixTranspose(dxB);
    const DirectX::XMVECTOR expected = DirectX::XMVector4Transform(
        DirectX::XMVector4Transform(dxVector, dxColumnVectorA), dxColumnVectorB);

    ExpectVector4Near(actual, expected, 1.0e-4F);
}

TEST(MathDirectXReferenceTests, QuaternionAxisAngleFactoriesMatchDirectXMath)
{
    const pond::math::Vector3 xAxis{2.0F, 0.0F, 0.0F};
    const pond::math::Vector3 yAxis{0.0F, 3.0F, 0.0F};
    const pond::math::Vector3 arbitraryAxis{1.0F, -2.0F, 3.0F};

    auto quarterTurnX =
        pond::math::Quaternion::FromAxisAngle(xAxis, pond::math::Radians{pond::core::kHalfPi});
    ASSERT_TRUE(quarterTurnX.HasValue());
    ExpectQuaternionNear(
        quarterTurnX.GetValue(),
        DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(xAxis.x, xAxis.y, xAxis.z, 0.0F),
                                          pond::core::kHalfPi));

    auto halfTurnY =
        pond::math::Quaternion::FromAxisAngle(yAxis, pond::math::Radians{pond::core::kPi});
    ASSERT_TRUE(halfTurnY.HasValue());
    ExpectQuaternionNear(
        halfTurnY.GetValue(),
        DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(yAxis.x, yAxis.y, yAxis.z, 0.0F),
                                          pond::core::kPi));

    auto arbitrary =
        pond::math::Quaternion::FromAxisAngle(arbitraryAxis, pond::math::Radians{1.25F});
    ASSERT_TRUE(arbitrary.HasValue());
    ExpectQuaternionNear(
        arbitrary.GetValue(),
        DirectX::XMQuaternionRotationAxis(
            DirectX::XMVectorSet(arbitraryAxis.x, arbitraryAxis.y, arbitraryAxis.z, 0.0F), 1.25F));
}

TEST(MathDirectXReferenceTests, QuaternionAlgebraAndInterpolationMatchDirectXMath)
{
    auto a = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                                   pond::math::Radians{0.5F});
    auto b = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{0.0F, 1.0F, 0.0F},
                                                   pond::math::Radians{0.75F});
    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());

    const pond::math::Vector3 vector{0.25F, -1.5F, 2.0F};
    const DirectX::XMVECTOR dxA = ToDirectXQuaternion(a.GetValue());
    const DirectX::XMVECTOR dxB = ToDirectXQuaternion(b.GetValue());
    const DirectX::XMVECTOR dxVector = DirectX::XMVectorSet(vector.x, vector.y, vector.z, 0.0F);

    ExpectQuaternionNear(pond::math::Conjugate(a.GetValue()), DirectX::XMQuaternionConjugate(dxA));
    ExpectQuaternionNear(pond::math::Inverse(a.GetValue()), DirectX::XMQuaternionInverse(dxA));
    ExpectVector3Near(pond::math::Rotate(a.GetValue(), vector),
                      DirectX::XMVector3Rotate(dxVector, dxA), 1.0e-4F);

    const pond::math::Quaternion composed = b.GetValue() * a.GetValue();
    const DirectX::XMVECTOR expectedComposedVector =
        DirectX::XMVector3Rotate(DirectX::XMVector3Rotate(dxVector, dxA), dxB);
    ExpectVector3Near(pond::math::Rotate(composed, vector), expectedComposedVector, 1.0e-4F);

    auto interpolated = pond::math::Slerp(a.GetValue(), b.GetValue(), 0.35F);
    ASSERT_TRUE(interpolated.HasValue());
    ExpectQuaternionNear(interpolated.GetValue(), DirectX::XMQuaternionSlerp(dxA, dxB, 0.35F),
                         1.0e-4F);
}

TEST(MathDirectXReferenceTests, QuaternionMatrixConversionsMatchDirectXMath)
{
    auto tolerance = pond::core::Tolerance::Create(1.0e-4F, 1.0e-4F);
    ASSERT_TRUE(tolerance.HasValue());

    auto rotation = pond::math::Quaternion::FromAxisAngle(pond::math::Vector3{1.0F, -2.0F, 3.0F},
                                                          pond::math::Radians{1.25F});
    ASSERT_TRUE(rotation.HasValue());

    const DirectX::XMVECTOR dxQuaternion = ToDirectXQuaternion(rotation.GetValue());
    const DirectX::XMMATRIX dxRotation = DirectX::XMMatrixRotationQuaternion(dxQuaternion);
    const DirectX::XMMATRIX dxColumnVectorRotation = DirectX::XMMatrixTranspose(dxRotation);

    const pond::math::Matrix3x3 matrix3 = pond::math::ToMatrix3x3(rotation.GetValue());
    const pond::math::Matrix4x4 matrix4 = pond::math::ToMatrix4x4(rotation.GetValue());
    ExpectMatrix3x3Near(matrix3, dxColumnVectorRotation, 1.0e-4F);
    ExpectMatrix4x4Near(matrix4, dxColumnVectorRotation, 1.0e-4F);

    auto recovered3 = pond::math::ToQuaternion(matrix3, tolerance.GetValue());
    auto recovered4 = pond::math::ToQuaternion(matrix4, tolerance.GetValue());
    ASSERT_TRUE(recovered3.HasValue());
    ASSERT_TRUE(recovered4.HasValue());
    ExpectQuaternionSameRotation(
        recovered3.GetValue(),
        DirectX::XMQuaternionRotationMatrix(DirectX::XMMatrixTranspose(ToDirectXMatrix(matrix3))),
        1.0e-4F);
    ExpectQuaternionSameRotation(
        recovered4.GetValue(),
        DirectX::XMQuaternionRotationMatrix(DirectX::XMMatrixTranspose(ToDirectXMatrix(matrix4))),
        1.0e-4F);

    const pond::math::Vector3 vector{0.25F, -1.5F, 2.0F};
    const DirectX::XMVECTOR dxVector = DirectX::XMVectorSet(vector.x, vector.y, vector.z, 0.0F);
    ExpectVector3Near(matrix3 * vector, DirectX::XMVector3Rotate(dxVector, dxQuaternion), 1.0e-4F);
}

TEST(MathDirectXReferenceTests, MatrixTransformFactoriesAndSemanticTransformsMatchDirectXMath)
{
    const pond::math::Vector3 translationVector{3.0F, -2.0F, 1.5F};
    const pond::math::Vector3 scaleVector{2.0F, -3.0F, 0.5F};
    const pond::math::Vector3 point{1.0F, 2.0F, -4.0F};
    const pond::math::Vector3 vector{-0.25F, 1.5F, 2.0F};
    const pond::math::Vector3 normal{4.0F, -6.0F, 1.0F};
    constexpr float kAngle{0.75F};

    const pond::math::Matrix4x4 translation = pond::math::Matrix4x4::Translation(translationVector);
    const pond::math::Matrix4x4 scale = pond::math::Matrix4x4::Scale(scaleVector);
    const pond::math::Matrix4x4 rotation =
        pond::math::Matrix4x4::RotationZ(pond::math::Radians{kAngle});
    const pond::math::Matrix4x4 affine = translation * rotation * scale;

    const DirectX::XMMATRIX dxTranslation =
        DirectX::XMMatrixTranslation(translationVector.x, translationVector.y, translationVector.z);
    const DirectX::XMMATRIX dxScale =
        DirectX::XMMatrixScaling(scaleVector.x, scaleVector.y, scaleVector.z);
    const DirectX::XMMATRIX dxRotation = DirectX::XMMatrixRotationZ(kAngle);
    const DirectX::XMMATRIX dxAffine = dxScale * dxRotation * dxTranslation;

    ExpectMatrix4x4Near(translation, DirectX::XMMatrixTranspose(dxTranslation));
    ExpectMatrix4x4Near(scale, DirectX::XMMatrixTranspose(dxScale));
    ExpectMatrix4x4Near(rotation, DirectX::XMMatrixTranspose(dxRotation), 1.0e-4F);
    ExpectMatrix4x4Near(affine, DirectX::XMMatrixTranspose(dxAffine), 1.0e-4F);

    const DirectX::XMVECTOR dxPoint = DirectX::XMVectorSet(point.x, point.y, point.z, 1.0F);
    const DirectX::XMVECTOR dxVector = DirectX::XMVectorSet(vector.x, vector.y, vector.z, 0.0F);
    const DirectX::XMVECTOR dxNormal = DirectX::XMVectorSet(normal.x, normal.y, normal.z, 0.0F);
    ExpectVector3Near(pond::math::TransformPoint(affine, point),
                      DirectX::XMVector3TransformCoord(dxPoint, dxAffine), 1.0e-4F);
    ExpectVector3Near(pond::math::TransformVector(affine, vector),
                      DirectX::XMVector3TransformNormal(dxVector, dxAffine), 1.0e-4F);

    auto transformedNormal = pond::math::TransformNormal(affine, normal);
    ASSERT_TRUE(transformedNormal.HasValue());
    DirectX::XMVECTOR dxDeterminant{};
    const DirectX::XMMATRIX dxColumnInverse =
        DirectX::XMMatrixInverse(&dxDeterminant, ToDirectXMatrix(affine));
    ExpectVector3Near(transformedNormal.GetValue(),
                      DirectX::XMVector3TransformNormal(dxNormal, dxColumnInverse), 1.0e-4F);
}

TEST(MathDirectXReferenceTests, ViewConstructionMatchesDirectXMathRightHandedHelpers)
{
    const pond::math::Vector3 eye{2.0F, 3.0F, -4.0F};
    const pond::math::Vector3 target{-1.0F, 4.0F, -8.0F};
    const pond::math::Vector3 direction = target - eye;
    const pond::math::Vector3 up{0.25F, 1.0F, 0.5F};
    auto lookAt = pond::math::Matrix4x4::LookAt(eye, target, up);
    auto lookTo = pond::math::Matrix4x4::LookTo(eye, direction, up);
    ASSERT_TRUE(lookAt.HasValue());
    ASSERT_TRUE(lookTo.HasValue());

    const DirectX::XMVECTOR dxEye = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0F);
    const DirectX::XMVECTOR dxTarget = DirectX::XMVectorSet(target.x, target.y, target.z, 1.0F);
    const DirectX::XMVECTOR dxDirection =
        DirectX::XMVectorSet(direction.x, direction.y, direction.z, 0.0F);
    const DirectX::XMVECTOR dxUp = DirectX::XMVectorSet(up.x, up.y, up.z, 0.0F);
    const DirectX::XMMATRIX dxLookAt = DirectX::XMMatrixLookAtRH(dxEye, dxTarget, dxUp);
    const DirectX::XMMATRIX dxLookTo = DirectX::XMMatrixLookToRH(dxEye, dxDirection, dxUp);

    ExpectMatrix4x4Near(lookAt.GetValue(), DirectX::XMMatrixTranspose(dxLookAt), 1.0e-4F);
    ExpectMatrix4x4Near(lookTo.GetValue(), DirectX::XMMatrixTranspose(dxLookTo), 1.0e-4F);

    const pond::math::Vector3 worldPoint{-3.0F, 1.5F, 2.0F};
    const DirectX::XMVECTOR dxWorldPoint =
        DirectX::XMVectorSet(worldPoint.x, worldPoint.y, worldPoint.z, 1.0F);
    ExpectVector3Near(pond::math::TransformPoint(lookAt.GetValue(), worldPoint),
                      DirectX::XMVector3TransformCoord(dxWorldPoint, dxLookAt), 1.0e-4F);
}

TEST(MathDirectXReferenceTests, ProjectionFiniteForwardZFormsMatchDirectXMath)
{
    constexpr float kFovY{0.75F};
    constexpr float kAspectRatio{1.6F};
    constexpr float kNearDistance{0.25F};
    constexpr float kFarDistance{100.0F};
    auto perspective =
        pond::math::Matrix4x4::Perspective(pond::math::Radians{kFovY}, kAspectRatio, kNearDistance,
                                           kFarDistance, pond::math::ProjectionDepth::ForwardZ);
    ASSERT_TRUE(perspective.HasValue());
    ExpectMatrix4x4Near(perspective.GetValue(),
                        DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovRH(
                            kFovY, kAspectRatio, kNearDistance, kFarDistance)),
                        1.0e-4F);

    constexpr float kLeft{-3.0F};
    constexpr float kRight{7.0F};
    constexpr float kBottom{-2.0F};
    constexpr float kTop{4.0F};
    auto orthographic =
        pond::math::Matrix4x4::Orthographic(kLeft, kRight, kBottom, kTop, kNearDistance,
                                            kFarDistance, pond::math::ProjectionDepth::ForwardZ);
    ASSERT_TRUE(orthographic.HasValue());
    ExpectMatrix4x4Near(orthographic.GetValue(),
                        DirectX::XMMatrixTranspose(DirectX::XMMatrixOrthographicOffCenterRH(
                            kLeft, kRight, kBottom, kTop, kNearDistance, kFarDistance)),
                        1.0e-5F);
}
TEST(MathDirectXReferenceTests, FrustumClassificationMatchesDirectXCollisionAwayFromBoundaries)
{
    constexpr float kNearDistance{1.0F};
    constexpr float kFarDistance{10.0F};
    auto projection = pond::math::Matrix4x4::Perspective(pond::math::Radians{pond::core::kHalfPi},
                                                         1.0F, kNearDistance, kFarDistance,
                                                         pond::math::ProjectionDepth::ForwardZ);
    ASSERT_TRUE(projection.HasValue());

    auto frustum = pond::math::Frustum::FromWorldToClip(projection.GetValue());
    ASSERT_TRUE(frustum.HasValue());

    DirectX::BoundingFrustum reference{};
    DirectX::BoundingFrustum::CreateFromMatrix(
        reference,
        DirectX::XMMatrixPerspectiveFovLH(pond::core::kHalfPi, 1.0F, kNearDistance, kFarDistance));

    const auto expectBoxMatches = [&](pond::math::Vector3 minimum, pond::math::Vector3 maximum)
    {
        auto box = pond::math::AxisAlignedBox::Create(minimum, maximum);
        ASSERT_TRUE(box.HasValue());
        EXPECT_EQ(frustum->Classify(box.GetValue()), ToCollisionClassification(reference.Contains(
                                                         ToDirectXCollisionBox(box.GetValue()))));
    };

    const auto expectSphereMatches = [&](pond::math::Vector3 center, float radius)
    {
        auto sphere = pond::math::Sphere::Create(center, radius);
        ASSERT_TRUE(sphere.HasValue());
        EXPECT_EQ(frustum->Classify(sphere.GetValue()),
                  ToCollisionClassification(
                      reference.Contains(ToDirectXCollisionSphere(sphere.GetValue()))));
    };

    expectBoxMatches(pond::math::Vector3{-0.5F, -0.5F, -3.0F},
                     pond::math::Vector3{0.5F, 0.5F, -2.0F});
    expectBoxMatches(pond::math::Vector3{-3.0F, -0.5F, -2.0F},
                     pond::math::Vector3{-0.5F, 0.5F, -1.5F});
    expectBoxMatches(pond::math::Vector3{-4.0F, -0.5F, -2.0F},
                     pond::math::Vector3{-3.0F, 0.5F, -1.5F});

    expectSphereMatches(pond::math::Vector3{0.0F, 0.0F, -3.0F}, 0.5F);
    expectSphereMatches(pond::math::Vector3{-1.0F, 0.0F, -2.0F}, 0.5F);
    expectSphereMatches(pond::math::Vector3{0.0F, 0.0F, -0.25F}, 0.1F);
}
TEST(MathDirectXReferenceTests, RayBoxIntersectionEntryDistancesMatchDirectXCollision)
{
    const auto expectRayBoxMatches = [](pond::math::Vector3 origin, pond::math::Vector3 direction,
                                        pond::math::Vector3 minimum, pond::math::Vector3 maximum)
    {
        auto ray = pond::math::Ray::Create(origin, direction);
        auto box = pond::math::AxisAlignedBox::Create(minimum, maximum);
        ASSERT_TRUE(ray.HasValue());
        ASSERT_TRUE(box.HasValue());

        const auto hit = pond::math::Intersect(ray.GetValue(), box.GetValue());
        const DirectX::BoundingBox referenceBox = ToDirectXCollisionBox(box.GetValue());
        float referenceDistance = -1.0F;
        const bool referenceHit = referenceBox.Intersects(
            ToDirectXCollisionPoint(ray->GetOrigin()),
            ToDirectXCollisionDirection(ray->GetDirection()), referenceDistance);

        EXPECT_EQ(hit.has_value(), referenceHit);
        if (hit.has_value() && referenceHit)
        {
            EXPECT_NEAR(hit->GetEntryDistance(), referenceDistance, 1.0e-5F);
        }
    };

    expectRayBoxMatches(
        pond::math::Vector3{-3.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F},
        pond::math::Vector3{-1.0F, -1.0F, -1.0F}, pond::math::Vector3{1.0F, 1.0F, 1.0F});

    expectRayBoxMatches(
        pond::math::Vector3{-1.0F, 1.0F, 0.5F}, pond::math::Vector3{1.0F, -1.0F, 0.0F},
        pond::math::Vector3{0.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 1.0F, 1.0F});
    expectRayBoxMatches(
        pond::math::Vector3{3.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F},
        pond::math::Vector3{-1.0F, -1.0F, -1.0F}, pond::math::Vector3{1.0F, 1.0F, 1.0F});
}
TEST(MathDirectXReferenceTests, RayTriangleIntersectionDistancesMatchDirectXCollision)
{
    const auto expectRayTriangleMatches =
        [](pond::math::Vector3 origin, pond::math::Vector3 direction, pond::math::Vector3 vertex0,
           pond::math::Vector3 vertex1, pond::math::Vector3 vertex2)
    {
        auto ray = pond::math::Ray::Create(origin, direction);
        auto triangle = pond::math::Triangle::Create(vertex0, vertex1, vertex2);
        ASSERT_TRUE(ray.HasValue());
        ASSERT_TRUE(triangle.HasValue());

        const auto hit = pond::math::Intersect(ray.GetValue(), triangle.GetValue());
        float referenceDistance = -1.0F;
        const bool referenceHit = DirectX::TriangleTests::Intersects(
            ToDirectXCollisionPoint(ray->GetOrigin()),
            ToDirectXCollisionDirection(ray->GetDirection()),
            ToDirectXCollisionPoint(triangle->GetVertex0()),
            ToDirectXCollisionPoint(triangle->GetVertex1()),
            ToDirectXCollisionPoint(triangle->GetVertex2()), referenceDistance);

        EXPECT_EQ(hit.has_value(), referenceHit);
        if (hit.has_value() && referenceHit)
        {
            EXPECT_NEAR(hit->GetDistance(), referenceDistance, 1.0e-5F);
        }
    };

    expectRayTriangleMatches(
        pond::math::Vector3{0.0F, 0.25F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F},
        pond::math::Vector3{-1.0F, 0.0F, -5.0F}, pond::math::Vector3{1.0F, 0.0F, -5.0F},
        pond::math::Vector3{0.0F, 1.0F, -5.0F});
    expectRayTriangleMatches(
        pond::math::Vector3{0.0F, 0.25F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F},
        pond::math::Vector3{-1.0F, 0.0F, -5.0F}, pond::math::Vector3{0.0F, 1.0F, -5.0F},
        pond::math::Vector3{1.0F, 0.0F, -5.0F});
    expectRayTriangleMatches(
        pond::math::Vector3{0.0F, 0.0F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F},
        pond::math::Vector3{-1.0F, 0.0F, -5.0F}, pond::math::Vector3{1.0F, 0.0F, -5.0F},
        pond::math::Vector3{0.0F, 1.0F, -5.0F});
    expectRayTriangleMatches(
        pond::math::Vector3{2.0F, 2.0F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F},
        pond::math::Vector3{-1.0F, 0.0F, -5.0F}, pond::math::Vector3{1.0F, 0.0F, -5.0F},
        pond::math::Vector3{0.0F, 1.0F, -5.0F});
    expectRayTriangleMatches(
        pond::math::Vector3{0.0F, 0.25F, -6.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F},
        pond::math::Vector3{-1.0F, 0.0F, -5.0F}, pond::math::Vector3{1.0F, 0.0F, -5.0F},
        pond::math::Vector3{0.0F, 1.0F, -5.0F});
}
} // namespace
