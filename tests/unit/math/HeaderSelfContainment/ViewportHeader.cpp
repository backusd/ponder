#include <ponder/math/Viewport.hpp>

namespace
{
[[maybe_unused]] void UseViewportHeader()
{
    auto viewport = pond::math::Viewport::Create(0.0F, 0.0F, 640.0F, 480.0F, 0.0F, 1.0F);
    auto ndc =
        pond::math::NdcToViewport(viewport.GetValue(), pond::math::Vector3{-1.0F, 1.0F, 0.0F});
    auto viewportPoint =
        pond::math::ViewportToNdc(viewport.GetValue(), pond::math::Vector3{0.0F, 0.0F, 0.0F});
    auto projected = pond::math::Project(pond::math::Matrix4x4::Identity(), viewport.GetValue(),
                                         pond::math::Vector3{1.0F, 2.0F, 3.0F});
    auto unprojected = pond::math::Unproject(pond::math::Matrix4x4::Identity(), viewport.GetValue(),
                                             pond::math::Vector3{320.0F, 240.0F, 0.5F});
    (void)ndc;
    (void)viewportPoint;
    (void)projected;
    (void)unprojected;
}
} // namespace