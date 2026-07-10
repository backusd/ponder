#include <ponder/math/Vector3.hpp>

namespace
{
static_assert(pond::math::Vector3{}.x == 0.0F);
static_assert(pond::math::Vector3{}.y == 0.0F);
static_assert(pond::math::Vector3{}.z == 0.0F);
static_assert(pond::math::Vector3{1.0F, 2.0F, 3.0F} == pond::math::Vector3{1.0F, 2.0F, 3.0F});
static_assert(pond::math::Vector3::Zero() == pond::math::Vector3{});
static_assert(pond::math::Vector3{1.0F, 2.0F, 3.0F} + pond::math::Vector3{4.0F, 5.0F, 6.0F} ==
              pond::math::Vector3{5.0F, 7.0F, 9.0F});
static_assert(pond::math::Vector3{4.0F, 5.0F, 6.0F} - pond::math::Vector3{1.0F, 2.0F, 3.0F} ==
              pond::math::Vector3{3.0F, 3.0F, 3.0F});
static_assert(-pond::math::Vector3{1.0F, -2.0F, 3.0F} == pond::math::Vector3{-1.0F, 2.0F, -3.0F});
static_assert(pond::math::Vector3{1.0F, 2.0F, 3.0F} * 2.0F ==
              pond::math::Vector3{2.0F, 4.0F, 6.0F});
static_assert(pond::math::ComponentDivide(pond::math::Vector3{8.0F, 9.0F, 10.0F},
                                          pond::math::Vector3{2.0F, 3.0F, 5.0F}) ==
              pond::math::Vector3{4.0F, 3.0F, 2.0F});
static_assert(pond::math::Dot(pond::math::Vector3{1.0F, 2.0F, 3.0F},
                              pond::math::Vector3{4.0F, 5.0F, 6.0F}) == 32.0F);
static_assert(pond::math::Cross(pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                pond::math::Vector3{0.0F, 1.0F, 0.0F}) ==
              pond::math::Vector3{0.0F, 0.0F, 1.0F});
} // namespace
