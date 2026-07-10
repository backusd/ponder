#include <ponder/math/Vector2.hpp>

namespace
{
static_assert(pond::math::Vector2{}.x == 0.0F);
static_assert(pond::math::Vector2{}.y == 0.0F);
static_assert(pond::math::Vector2{1.0F, 2.0F} == pond::math::Vector2{1.0F, 2.0F});
static_assert(pond::math::Vector2::Zero() == pond::math::Vector2{});
static_assert(pond::math::Vector2{1.0F, 2.0F} + pond::math::Vector2{3.0F, 4.0F} ==
              pond::math::Vector2{4.0F, 6.0F});
static_assert(pond::math::Vector2{3.0F, 4.0F} - pond::math::Vector2{1.0F, 2.0F} ==
              pond::math::Vector2{2.0F, 2.0F});
static_assert(-pond::math::Vector2{1.0F, -2.0F} == pond::math::Vector2{-1.0F, 2.0F});
static_assert(pond::math::Vector2{1.0F, 2.0F} * 2.0F == pond::math::Vector2{2.0F, 4.0F});
static_assert(2.0F * pond::math::Vector2{1.0F, 2.0F} == pond::math::Vector2{2.0F, 4.0F});
static_assert(pond::math::Vector2{2.0F, 4.0F} / 2.0F == pond::math::Vector2{1.0F, 2.0F});
static_assert(pond::math::ComponentMultiply(pond::math::Vector2{2.0F, 3.0F},
                                            pond::math::Vector2{4.0F, 5.0F}) ==
              pond::math::Vector2{8.0F, 15.0F});
static_assert(pond::math::Dot(pond::math::Vector2{1.0F, 2.0F}, pond::math::Vector2{3.0F, 4.0F}) ==
              11.0F);
static_assert(pond::math::SquaredLength(pond::math::Vector2{3.0F, 4.0F}) == 25.0F);
} // namespace
