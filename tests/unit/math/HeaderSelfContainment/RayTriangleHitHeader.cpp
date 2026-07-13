#include <ponder/math/RayTriangleHit.hpp>

namespace
{
[[nodiscard]] constexpr bool RayTriangleArithmeticIsConstantEvaluable()
{
    const auto triangle = pond::math::Triangle::Create(pond::math::Vector3{-1.0F, 0.0F, -5.0F},
                                                       pond::math::Vector3{1.0F, 0.0F, -5.0F},
                                                       pond::math::Vector3{0.0F, 1.0F, -5.0F});
    if (!triangle.HasValue())
    {
        return false;
    }

    const auto hit = pond::math::detail::IntersectRayTriangleValues(
        pond::math::Vector3{0.0F, 0.25F, 0.0F}, pond::math::Vector3{0.0F, 0.0F, -1.0F},
        triangle->GetVertex0(), triangle->GetVertex1(), triangle->GetVertex2());
    return hit.has_value() && hit->distance == 5.0 && hit->barycentric0 == 0.375 &&
           hit->barycentric1 == 0.375 && hit->barycentric2 == 0.25;
}

static_assert(RayTriangleArithmeticIsConstantEvaluable());

[[maybe_unused]] void UseRayTriangleHitHeader()
{
    auto ray = pond::math::Ray::Create(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                       pond::math::Vector3{0.0F, 0.0F, -1.0F});
    auto triangle = pond::math::Triangle::Create(pond::math::Vector3{-1.0F, 0.0F, -1.0F},
                                                 pond::math::Vector3{1.0F, 0.0F, -1.0F},
                                                 pond::math::Vector3{0.0F, 1.0F, -1.0F});
    if (ray.HasValue() && triangle.HasValue())
    {
        auto hit = pond::math::Intersect(ray.GetValue(), triangle.GetValue());
        (void)hit;
    }
}
} // namespace
