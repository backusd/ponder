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

[[nodiscard]] constexpr bool CheckedMatrix4x4AccessWorks()
{
    pond::math::Matrix4x4 matrix{1.0F, 2.0F,  3.0F,  4.0F,  5.0F,  6.0F,  7.0F,  8.0F,
                                 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F};
    auto row = matrix.Row(2);
    auto column = matrix.Column(3);
    auto element = matrix.At(2, 3);
    if (!row.HasValue() || !column.HasValue() || !element.HasValue())
    {
        return false;
    }

    element->get() = 84.0F;
    const pond::math::Matrix4x4 constMatrix = matrix;
    auto constElement = constMatrix.At(2, 3);
    return row.GetValue() == pond::math::Vector4{9.0F, 10.0F, 11.0F, 12.0F} &&
           column.GetValue() == pond::math::Vector4{4.0F, 8.0F, 12.0F, 16.0F} &&
           constElement.HasValue() && constElement->get() == 84.0F;
}

[[nodiscard]] constexpr bool Matrix4x4CheckedArithmeticWorks()
{
    auto inverse =
        pond::math::Inverse(pond::math::Matrix4x4::Scale(pond::math::Vector3{2.0F, 4.0F, 8.0F}));
    auto orthographic = pond::math::Matrix4x4::Orthographic(-2.0F, 2.0F, -4.0F, 4.0F, 1.0F, 11.0F,
                                                            pond::math::ProjectionDepth::ForwardZ);
    auto ndc = pond::math::PerspectiveDivide(pond::math::Vector4{2.0F, 4.0F, 6.0F, 2.0F});
    auto transformedNormal = pond::math::TransformNormal(
        pond::math::Matrix4x4::Scale(pond::math::Vector3{2.0F, 4.0F, 8.0F}),
        pond::math::Vector3{2.0F, 4.0F, 8.0F});
    auto transformedPoint = pond::math::TransformPointToNdc(pond::math::Matrix4x4::Identity(),
                                                            pond::math::Vector3{1.0F, 2.0F, 3.0F});
    return inverse.HasValue() &&
           inverse.GetValue() == pond::math::Matrix4x4{0.5F, 0.0F, 0.0F, 0.0F, 0.0F,   0.25F,
                                                       0.0F, 0.0F, 0.0F, 0.0F, 0.125F, 0.0F,
                                                       0.0F, 0.0F, 0.0F, 1.0F} &&
           orthographic.HasValue() && orthographic->row0Column0 == 0.5F &&
           orthographic->row1Column1 == 0.25F && orthographic->row2Column2 == -0.1F &&
           ndc.HasValue() && ndc.GetValue() == pond::math::Vector3{1.0F, 2.0F, 3.0F} &&
           transformedNormal.HasValue() &&
           transformedNormal.GetValue() == pond::math::Vector3{1.0F, 1.0F, 1.0F} &&
           transformedPoint.HasValue() &&
           transformedPoint.GetValue() == pond::math::Vector3{1.0F, 2.0F, 3.0F};
}

[[nodiscard]] constexpr bool Matrix4x4NearComparisonWorks()
{
    auto tolerance = pond::core::Tolerance::Create(1.0e-5F, 1.0e-5F);
    return tolerance.HasValue() &&
           pond::math::IsNear(pond::math::Matrix4x4::Identity(), pond::math::Matrix4x4::Identity(),
                              tolerance.GetValue()) &&
           !pond::math::IsNear(pond::math::Matrix4x4::Identity(), pond::math::Matrix4x4::Zero(),
                               tolerance.GetValue());
}

static_assert(CheckedMatrix4x4AccessWorks());
static_assert(Matrix4x4CheckedArithmeticWorks());
static_assert(Matrix4x4NearComparisonWorks());

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
