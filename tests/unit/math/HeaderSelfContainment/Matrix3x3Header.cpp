#include <ponder/math/Matrix3x3.hpp>

namespace
{
static_assert(pond::math::Matrix3x3{}.row0Column0 == 0.0F);
static_assert(pond::math::Matrix3x3{}.row2Column2 == 0.0F);
static_assert(pond::math::Matrix3x3::Zero() == pond::math::Matrix3x3{});
static_assert(pond::math::Matrix3x3::Identity() ==
              pond::math::Matrix3x3{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F});
static_assert(pond::math::Matrix3x3{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F}
                  .row1Column2 == 6.0F);
static_assert(pond::math::Matrix3x3{pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                    pond::math::Vector3{4.0F, 5.0F, 6.0F},
                                    pond::math::Vector3{7.0F, 8.0F, 9.0F}}
                  .row2Column1 == 6.0F);
static_assert(pond::math::Matrix3x3::Identity() * pond::math::Vector3{1.0F, 2.0F, 3.0F} ==
              pond::math::Vector3{1.0F, 2.0F, 3.0F});
static_assert(pond::math::Matrix3x3::Identity() * pond::math::Matrix3x3::Identity() ==
              pond::math::Matrix3x3::Identity());
static_assert((pond::math::Matrix3x3::Identity() + pond::math::Matrix3x3::Identity()) ==
              (pond::math::Matrix3x3{2.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 0.0F, 2.0F}));
static_assert((pond::math::Matrix3x3::Identity() * 2.0F) / 2.0F ==
              pond::math::Matrix3x3::Identity());
static_assert(pond::math::Transpose(pond::math::Matrix3x3{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F,
                                                          8.0F, 9.0F}) ==
              pond::math::Matrix3x3{1.0F, 4.0F, 7.0F, 2.0F, 5.0F, 8.0F, 3.0F, 6.0F, 9.0F});
static_assert(pond::math::Determinant(pond::math::Matrix3x3{1.0F, 2.0F, 3.0F, 0.0F, 1.0F, 4.0F,
                                                            5.0F, 6.0F, 0.0F}) == 1.0F);

[[nodiscard]] constexpr bool CheckedMatrix3x3AccessWorks()
{
    pond::math::Matrix3x3 matrix{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
    auto row = matrix.Row(1);
    auto column = matrix.Column(2);
    auto element = matrix.At(1, 2);
    if (!row.HasValue() || !column.HasValue() || !element.HasValue())
    {
        return false;
    }

    element->get() = 42.0F;
    const pond::math::Matrix3x3 constMatrix = matrix;
    auto constElement = constMatrix.At(1, 2);
    return row.GetValue() == pond::math::Vector3{4.0F, 5.0F, 6.0F} &&
           column.GetValue() == pond::math::Vector3{3.0F, 6.0F, 9.0F} && constElement.HasValue() &&
           constElement->get() == 42.0F;
}

[[nodiscard]] constexpr bool Matrix3x3InverseWorks()
{
    auto inverse = pond::math::Inverse(
        pond::math::Matrix3x3{2.0F, 0.0F, 0.0F, 0.0F, 4.0F, 0.0F, 0.0F, 0.0F, 8.0F});
    return inverse.HasValue() &&
           inverse.GetValue() ==
               pond::math::Matrix3x3{0.5F, 0.0F, 0.0F, 0.0F, 0.25F, 0.0F, 0.0F, 0.0F, 0.125F};
}

[[nodiscard]] constexpr bool Matrix3x3NearComparisonWorks()
{
    auto tolerance = pond::core::Tolerance::Create(1.0e-5F, 1.0e-5F);
    return tolerance.HasValue() &&
           pond::math::IsNear(pond::math::Matrix3x3::Identity(), pond::math::Matrix3x3::Identity(),
                              tolerance.GetValue()) &&
           !pond::math::IsNear(pond::math::Matrix3x3::Identity(), pond::math::Matrix3x3::Zero(),
                               tolerance.GetValue());
}

static_assert(CheckedMatrix3x3AccessWorks());
static_assert(Matrix3x3InverseWorks());
static_assert(Matrix3x3NearComparisonWorks());
} // namespace
