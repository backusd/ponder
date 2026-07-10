#include <ponder/math/RayBoxHit.hpp>

namespace
{
[[maybe_unused]] void UseRayBoxHitHeader()
{
    auto ray = pond::math::Ray::Create(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                       pond::math::Vector3{1.0F, 0.0F, 0.0F});
    auto box = pond::math::AxisAlignedBox::Create(pond::math::Vector3{1.0F, -1.0F, -1.0F},
                                                  pond::math::Vector3{2.0F, 1.0F, 1.0F});
    if (ray.HasValue() && box.HasValue())
    {
        auto hit = pond::math::Intersect(ray.GetValue(), box.GetValue());
        (void)hit;
    }
}
} // namespace
