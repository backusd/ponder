#include <ponder/math/MathError.hpp>

namespace
{
static_assert(pond::math::ToErrorCode(pond::math::MathErrorCode::InvalidArgument).GetCategory() ==
              pond::core::ErrorCategory::InvalidArgument);
static_assert(pond::math::ToErrorCode(pond::math::MathErrorCode::DegenerateInput).GetCategory() ==
              pond::core::ErrorCategory::General);
} // namespace
