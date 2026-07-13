#include <ponder/math/Sphere.hpp>

namespace
{
[[nodiscard]] constexpr bool SphereFactoryIsConstantEvaluable()
{
    const auto sphere = pond::math::Sphere::Create(pond::math::Vector3{1.0F, 2.0F, 3.0F}, 4.0F);
    return sphere.HasValue() &&
           sphere.GetValue().GetCenter() == pond::math::Vector3{1.0F, 2.0F, 3.0F} &&
           sphere.GetValue().GetRadius() == 4.0F;
}

static_assert(SphereFactoryIsConstantEvaluable());

[[maybe_unused]] void UseSphereHeader()
{
    auto sphere = pond::math::Sphere::Create(pond::math::Vector3{1.0F, 2.0F, 3.0F}, 4.0F);
    (void)sphere;
}
} // namespace
