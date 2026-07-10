#include <ponder/math/Matrix3x3.hpp>

namespace
{
static_assert(pond::math::Matrix3x3{}.row0Column0 == 0.0F);
static_assert(pond::math::Matrix3x3{}.row2Column2 == 0.0F);
static_assert(pond::math::Matrix3x3::Zero() == pond::math::Matrix3x3{});
static_assert(pond::math::Matrix3x3::Identity() ==
              pond::math::Matrix3x3{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F});
static_assert(pond::math::Matrix3x3{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F}
                  .row1Column2 == 6.0F);
static_assert(pond::math::Matrix3x3{pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                    pond::math::Vector3{4.0F, 5.0F, 6.0F},
                                    pond::math::Vector3{7.0F, 8.0F, 9.0F}}
                  .row2Column1 == 6.0F);
static_assert(pond::math::Matrix3x3::Identity() * pond::math::Vector3{1.0F, 2.0F, 3.0F} ==
              pond::math::Vector3{1.0F, 2.0F, 3.0F});
static_assert(pond::math::Matrix3x3::Identity() * pond::math::Matrix3x3::Identity() ==
              pond::math::Matrix3x3::Identity());
static_assert((pond::math::Matrix3x3::Identity() + pond::math::Matrix3x3::Identity()) ==
              (pond::math::Matrix3x3{2.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 0.0F, 2.0F}));
static_assert((pond::math::Matrix3x3::Identity() * 2.0F) / 2.0F ==
              pond::math::Matrix3x3::Identity());
static_assert(pond::math::Transpose(pond::math::Matrix3x3{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F,
                                                          8.0F, 9.0F}) ==
              pond::math::Matrix3x3{1.0F, 4.0F, 7.0F, 2.0F, 5.0F, 8.0F, 3.0F, 6.0F, 9.0F});
static_assert(pond::math::Determinant(pond::math::Matrix3x3{1.0F, 2.0F, 3.0F, 0.0F, 1.0F, 4.0F,
                                                            5.0F, 6.0F, 0.0F}) == 1.0F);
} // namespace
