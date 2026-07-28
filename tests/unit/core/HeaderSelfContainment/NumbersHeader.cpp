#include <ponder/core/Numbers.hpp>

#include <limits>

namespace
{
constexpr auto kTolerance = ponder::core::Tolerance::Create(0.01F, 0.01F);
static_assert(ponder::core::IsNear(100.0F, 100.5F, kTolerance));
static_assert(!ponder::core::IsNear(100.0F, 102.0F, kTolerance));
static_assert(ponder::core::Lerp(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.5F) == 0.0F);
} // namespace
