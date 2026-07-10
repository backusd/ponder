#include <ponder/math/Ray.hpp>

namespace
{
[[maybe_unused]] void UseRayHeader()
{
    auto ray = pond::math::Ray::Create(pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                       pond::math::Vector3{0.0F, 0.0F, -1.0F});
    (void)ray;
}
} // namespace