#include <ponder/math/Angle.hpp>

namespace
{
static_assert(pond::math::Radians{}.GetValue() == 0.0F);
static_assert(pond::math::Degrees{}.GetValue() == 0.0F);
static_assert(pond::math::Radians{1.0F} == pond::math::Radians{1.0F});
static_assert(pond::math::Degrees{1.0F} == pond::math::Degrees{1.0F});
} // namespace
