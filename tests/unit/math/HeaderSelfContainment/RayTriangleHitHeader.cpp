#include <ponder/math/RayTriangleHit.hpp>

namespace
{
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
