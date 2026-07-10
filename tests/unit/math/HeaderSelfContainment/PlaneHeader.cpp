#include <ponder/math/Plane.hpp>

namespace
{
[[maybe_unused]] void UsePlaneHeader()
{
    auto plane = pond::math::Plane::Create(pond::math::Vector3{0.0F, 1.0F, 0.0F}, -2.0F);
    (void)plane;
}
} // namespace