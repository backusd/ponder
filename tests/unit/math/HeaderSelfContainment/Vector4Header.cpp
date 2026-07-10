#include <ponder/math/Vector4.hpp>

namespace
{
static_assert(pond::math::Vector4{}.x == 0.0F);
static_assert(pond::math::Vector4{}.y == 0.0F);
static_assert(pond::math::Vector4{}.z == 0.0F);
static_assert(pond::math::Vector4{}.w == 0.0F);
static_assert(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F} ==
              pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F});
static_assert(pond::math::Vector4::Zero() == pond::math::Vector4{});
static_assert(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F} +
                  pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F} ==
              pond::math::Vector4{6.0F, 8.0F, 10.0F, 12.0F});
static_assert(pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F} -
                  pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F} ==
              pond::math::Vector4{4.0F, 4.0F, 4.0F, 4.0F});
static_assert(-pond::math::Vector4{1.0F, -2.0F, 3.0F, -4.0F} ==
              pond::math::Vector4{-1.0F, 2.0F, -3.0F, 4.0F});
static_assert(pond::math::Vector4{2.0F, 4.0F, 6.0F, 8.0F} / 2.0F ==
              pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F});
static_assert(pond::math::ComponentMultiply(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                                            pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F}) ==
              pond::math::Vector4{5.0F, 12.0F, 21.0F, 32.0F});
static_assert(pond::math::Dot(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                              pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F}) == 70.0F);
static_assert(pond::math::SquaredDistance(pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                                          pond::math::Vector4{2.0F, 4.0F, 6.0F, 8.0F}) == 30.0F);
} // namespace
