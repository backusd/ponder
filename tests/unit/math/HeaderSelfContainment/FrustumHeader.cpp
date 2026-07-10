#include <ponder/math/Frustum.hpp>

namespace
{
[[maybe_unused]] void UseFrustumHeader()
{
    auto frustum = pond::math::Frustum::FromWorldToClip(pond::math::Matrix4x4::Identity());
    (void)frustum;
}
} // namespace