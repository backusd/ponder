#include <ponder/math/CollisionClassification.hpp>

namespace
{
[[maybe_unused]] void UseCollisionClassificationHeader()
{
    constexpr auto classification = pond::math::CollisionClassification::Intersects;
    (void)classification;
}
} // namespace