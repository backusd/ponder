#include <ponder/math/Triangle.hpp>

namespace
{
[[maybe_unused]] void UseTriangleHeader()
{
    auto triangle = pond::math::Triangle::Create(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                 pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                                 pond::math::Vector3{0.0F, 1.0F, 0.0F});
    (void)triangle;
}
} // namespace