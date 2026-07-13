#include <ponder/core/Numbers.hpp>

#include <limits>

namespace
{
constexpr auto kTolerance = pond::core::Tolerance::Create(0.01F, 0.01F);
static_assert(kTolerance.HasValue());
static_assert(pond::core::IsNear(100.0F, 100.5F, kTolerance.GetValue()));
static_assert(!pond::core::IsNear(100.0F, 102.0F, kTolerance.GetValue()));
static_assert(pond::core::Lerp(-std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max(), 0.5F) == 0.0F);
} // namespace
