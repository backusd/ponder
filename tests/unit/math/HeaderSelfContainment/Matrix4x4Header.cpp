#include <ponder/math/Matrix4x4.hpp>

namespace
{
static_assert(pond::math::Matrix4x4{}.row0Column0 == 0.0F);
static_assert(pond::math::Matrix4x4{}.row3Column3 == 0.0F);
static_assert(pond::math::Matrix4x4::Zero() == pond::math::Matrix4x4{});
static_assert(pond::math::Matrix4x4::Identity() ==
              pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                    1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F});
static_assert(pond::math::Matrix4x4::Translation(pond::math::Vector3{1.0F, 2.0F, 3.0F}) ==
              pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 2.0F, 0.0F, 0.0F,
                                    1.0F, 3.0F, 0.0F, 0.0F, 0.0F, 1.0F});
static_assert(pond::math::ProjectionDepth::ForwardZ != pond::math::ProjectionDepth::ReverseZ);
static_assert(pond::math::Matrix4x4::Scale(pond::math::Vector3{2.0F, 3.0F, 4.0F}) ==
              pond::math::Matrix4x4{2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                    4.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F});
static_assert(pond::math::TransformPoint(
                  pond::math::Matrix4x4::Translation(pond::math::Vector3{1.0F, 2.0F, 3.0F}),
                  pond::math::Vector3{4.0F, 5.0F, 6.0F}) == pond::math::Vector3{5.0F, 7.0F, 9.0F});
static_assert(pond::math::TransformPointToClip(
                  pond::math::Matrix4x4::Translation(pond::math::Vector3{1.0F, 2.0F, 3.0F}),
                  pond::math::Vector3{4.0F, 5.0F, 6.0F}) ==
              pond::math::Vector4{5.0F, 7.0F, 9.0F, 1.0F});
static_assert(pond::math::TransformVector(
                  pond::math::Matrix4x4::Translation(pond::math::Vector3{1.0F, 2.0F, 3.0F}),
                  pond::math::Vector3{4.0F, 5.0F, 6.0F}) == pond::math::Vector3{4.0F, 5.0F, 6.0F});
static_assert(pond::math::Matrix4x4{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F,
                                    11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F}
                  .row2Column3 == 12.0F);
static_assert(pond::math::Matrix4x4{pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                                    pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F},
                                    pond::math::Vector4{9.0F, 10.0F, 11.0F, 12.0F},
                                    pond::math::Vector4{13.0F, 14.0F, 15.0F, 16.0F}}
                  .row3Column2 == 12.0F);
static_assert(pond::math::Matrix4x4::Identity() * pond::math::Vector4{1.0F, 2.0F, 3.0F, 1.0F} ==
              pond::math::Vector4{1.0F, 2.0F, 3.0F, 1.0F});
static_assert(pond::math::Matrix4x4::Identity() * pond::math::Matrix4x4::Identity() ==
              pond::math::Matrix4x4::Identity());
static_assert(pond::math::Matrix4x4::Identity() + pond::math::Matrix4x4::Identity() ==
              pond::math::Matrix4x4{2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                    2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F});
static_assert((pond::math::Matrix4x4::Identity() * 2.0F) / 2.0F ==
              pond::math::Matrix4x4::Identity());
static_assert(pond::math::Transpose(pond::math::Matrix4x4{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F,
                                                          8.0F, 9.0F, 10.0F, 11.0F, 12.0F, 13.0F,
                                                          14.0F, 15.0F, 16.0F})
                  .row1Column0 == 2.0F);
static_assert(pond::math::Determinant(pond::math::Matrix4x4{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F,
                                                            7.0F, 8.0F, 2.0F, 6.0F, 4.0F, 8.0F,
                                                            3.0F, 1.0F, 1.0F, 2.0F}) == 72.0F);
[[maybe_unused]] void UseCheckedProjectionAndNdcConstruction()
{
    auto projection = pond::math::Matrix4x4::Perspective(
        pond::math::Radians{1.0F}, 1.0F, 0.1F, 100.0F, pond::math::ProjectionDepth::ForwardZ);
    auto infiniteProjection = pond::math::Matrix4x4::InfinitePerspective(
        pond::math::Radians{1.0F}, 1.0F, 0.1F, pond::math::ProjectionDepth::ReverseZ);
    auto orthographic = pond::math::Matrix4x4::Orthographic(-1.0F, 1.0F, -1.0F, 1.0F, 0.1F, 100.0F,
                                                            pond::math::ProjectionDepth::ForwardZ);
    auto ndc = pond::math::PerspectiveDivide(pond::math::Vector4{2.0F, 4.0F, 6.0F, 2.0F});
    auto transformed = pond::math::TransformPointToNdc(pond::math::Matrix4x4::Identity(),
                                                       pond::math::Vector3{1.0F, 2.0F, 3.0F});
    (void)projection;
    (void)infiniteProjection;
    (void)orthographic;
    (void)ndc;
    (void)transformed;
}
} // namespace