#include <DirectXCollision.h>
#include <DirectXMath.h>

#include <gtest/gtest.h>

namespace
{
TEST(MathDirectXReferenceTests, DirectXReferenceHeadersCompileInIsolatedTarget)
{
    const DirectX::XMVECTOR zero = DirectX::XMVectorZero();
    EXPECT_FLOAT_EQ(DirectX::XMVectorGetX(zero), 0.0F);

    DirectX::BoundingSphere sphere{};
    sphere.Center = DirectX::XMFLOAT3{0.0F, 0.0F, 0.0F};
    sphere.Radius = 1.0F;

    EXPECT_FLOAT_EQ(sphere.Radius, 1.0F);
}
} // namespace
