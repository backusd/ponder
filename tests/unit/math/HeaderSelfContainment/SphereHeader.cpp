#include <ponder/math/Sphere.hpp>

namespace
{
[[maybe_unused]] void UseSphereHeader()
{
    auto sphere = pond::math::Sphere::Create(pond::math::Vector3{1.0F, 2.0F, 3.0F}, 4.0F);
    (void)sphere;
}
} // namespace