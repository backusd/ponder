#include <ponder/math/Scalar.hpp>

namespace
{
static_assert(pond::math::kPi == 0x1.921fb6p+1F);
static_assert(pond::math::IsFinite(0.0F));
static_assert(pond::math::Clamp(2.0F, 0.0F, 1.0F) == 1.0F);
static_assert(pond::math::Lerp(2.0F, 6.0F, 0.25F) == 3.0F);
} // namespace
