#include <ponder/core/Numbers.hpp>
#include <ponder/math/Matrix3x3.hpp>
#include <ponder/math/Matrix4x4.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>

namespace
{
static_assert(sizeof(pond::math::Matrix3x3) == sizeof(float) * 9);
static_assert(sizeof(pond::math::Matrix4x4) == sizeof(float) * 16);
static_assert(alignof(pond::math::Matrix3x3) == alignof(float));
static_assert(alignof(pond::math::Matrix4x4) == alignof(float));
static_assert(std::is_standard_layout_v<pond::math::Matrix3x3>);
static_assert(std::is_standard_layout_v<pond::math::Matrix4x4>);
static_assert(std::is_trivially_copyable_v<pond::math::Matrix3x3>);
static_assert(std::is_trivially_copyable_v<pond::math::Matrix4x4>);
static_assert(offsetof(pond::math::Matrix3x3, row0Column0) == sizeof(float) * 0);
static_assert(offsetof(pond::math::Matrix3x3, row1Column0) == sizeof(float) * 1);
static_assert(offsetof(pond::math::Matrix3x3, row2Column0) == sizeof(float) * 2);
static_assert(offsetof(pond::math::Matrix3x3, row0Column1) == sizeof(float) * 3);
static_assert(offsetof(pond::math::Matrix3x3, row1Column1) == sizeof(float) * 4);
static_assert(offsetof(pond::math::Matrix3x3, row2Column1) == sizeof(float) * 5);
static_assert(offsetof(pond::math::Matrix3x3, row0Column2) == sizeof(float) * 6);
static_assert(offsetof(pond::math::Matrix3x3, row1Column2) == sizeof(float) * 7);
static_assert(offsetof(pond::math::Matrix3x3, row2Column2) == sizeof(float) * 8);
static_assert(offsetof(pond::math::Matrix4x4, row0Column0) == sizeof(float) * 0);
static_assert(offsetof(pond::math::Matrix4x4, row1Column0) == sizeof(float) * 1);
static_assert(offsetof(pond::math::Matrix4x4, row2Column0) == sizeof(float) * 2);
static_assert(offsetof(pond::math::Matrix4x4, row3Column0) == sizeof(float) * 3);
static_assert(offsetof(pond::math::Matrix4x4, row0Column1) == sizeof(float) * 4);
static_assert(offsetof(pond::math::Matrix4x4, row1Column1) == sizeof(float) * 5);
static_assert(offsetof(pond::math::Matrix4x4, row2Column1) == sizeof(float) * 6);
static_assert(offsetof(pond::math::Matrix4x4, row3Column1) == sizeof(float) * 7);
static_assert(offsetof(pond::math::Matrix4x4, row0Column2) == sizeof(float) * 8);
static_assert(offsetof(pond::math::Matrix4x4, row1Column2) == sizeof(float) * 9);
static_assert(offsetof(pond::math::Matrix4x4, row2Column2) == sizeof(float) * 10);
static_assert(offsetof(pond::math::Matrix4x4, row3Column2) == sizeof(float) * 11);
static_assert(offsetof(pond::math::Matrix4x4, row0Column3) == sizeof(float) * 12);
static_assert(offsetof(pond::math::Matrix4x4, row1Column3) == sizeof(float) * 13);
static_assert(offsetof(pond::math::Matrix4x4, row2Column3) == sizeof(float) * 14);
static_assert(offsetof(pond::math::Matrix4x4, row3Column3) == sizeof(float) * 15);

template <std::size_t Count, typename Matrix>
[[nodiscard]] std::array<float, Count> CopyStorage(Matrix matrix) noexcept
{
    return std::bit_cast<std::array<float, Count>>(matrix);
}

[[nodiscard]] pond::core::Tolerance RequireTolerance(float absoluteTolerance,
                                                     float relativeTolerance)
{
    auto result = pond::core::Tolerance::Create(absoluteTolerance, relativeTolerance);
    EXPECT_TRUE(result.HasValue());
    return result.GetValue();
}

void ExpectInvalidIndex(auto result)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::math::ToErrorCode(pond::math::MathErrorCode::InvalidArgument));
}

TEST(Matrix3x3Tests, DefaultsToZeroAndCreatesIdentity)
{
    constexpr pond::math::Matrix3x3 defaultMatrix;

    EXPECT_EQ(defaultMatrix, pond::math::Matrix3x3{});
    EXPECT_EQ(pond::math::Matrix3x3::Zero(), pond::math::Matrix3x3{});
    EXPECT_EQ(pond::math::Matrix3x3::Identity(),
              (pond::math::Matrix3x3{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F}));
}

TEST(Matrix3x3Tests, ConstructsFromScalarsAndStoresColumnMajor)
{
    const pond::math::Matrix3x3 matrix{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};

    EXPECT_EQ(matrix.row0Column0, 1.0F);
    EXPECT_EQ(matrix.row0Column1, 2.0F);
    EXPECT_EQ(matrix.row0Column2, 3.0F);
    EXPECT_EQ(matrix.row1Column0, 4.0F);
    EXPECT_EQ(matrix.row1Column1, 5.0F);
    EXPECT_EQ(matrix.row1Column2, 6.0F);
    EXPECT_EQ(matrix.row2Column0, 7.0F);
    EXPECT_EQ(matrix.row2Column1, 8.0F);
    EXPECT_EQ(matrix.row2Column2, 9.0F);
    EXPECT_EQ(CopyStorage<9>(matrix),
              (std::array<float, 9>{1.0F, 4.0F, 7.0F, 2.0F, 5.0F, 8.0F, 3.0F, 6.0F, 9.0F}));
}

TEST(Matrix3x3Tests, ConstructsFromColumnsAndRetrievesRowsAndColumns)
{
    const pond::math::Matrix3x3 matrix{pond::math::Vector3{1.0F, 2.0F, 3.0F},
                                       pond::math::Vector3{4.0F, 5.0F, 6.0F},
                                       pond::math::Vector3{7.0F, 8.0F, 9.0F}};

    auto row1 = matrix.Row(1);
    auto column2 = matrix.Column(2);
    ASSERT_TRUE(row1.HasValue());
    ASSERT_TRUE(column2.HasValue());

    EXPECT_EQ(row1.GetValue(), (pond::math::Vector3{2.0F, 5.0F, 8.0F}));
    EXPECT_EQ(column2.GetValue(), (pond::math::Vector3{7.0F, 8.0F, 9.0F}));
    ExpectInvalidIndex(matrix.Row(3));
    ExpectInvalidIndex(matrix.Column(3));
}

TEST(Matrix3x3Tests, ProvidesCheckedMutableAndConstElementAccess)
{
    pond::math::Matrix3x3 matrix{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};

    auto element = matrix.At(1, 2);
    ASSERT_TRUE(element.HasValue());
    element->get() = 42.0F;
    EXPECT_EQ(matrix.row1Column2, 42.0F);

    const pond::math::Matrix3x3 constMatrix = matrix;
    auto readElement = constMatrix.At(1, 2);
    ASSERT_TRUE(readElement.HasValue());
    EXPECT_EQ(readElement->get(), 42.0F);
    ExpectInvalidIndex(matrix.At(3, 0));
    ExpectInvalidIndex(matrix.At(0, 3));
    ExpectInvalidIndex(constMatrix.At(3, 3));
}

TEST(Matrix3x3Tests, ComparesExactlyAndWithCallerTolerance)
{
    const pond::math::Matrix3x3 base{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
    const pond::math::Matrix3x3 close{1.005F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
    const pond::math::Matrix3x3 far{1.02F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
    const pond::math::Matrix3x3 nonFinite{
        std::numeric_limits<float>::infinity(), 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
    const pond::core::Tolerance tolerance = RequireTolerance(0.01F, 0.0F);

    EXPECT_EQ(base, base);
    EXPECT_FALSE(base == close);
    EXPECT_TRUE(pond::math::IsNear(base, close, tolerance));
    EXPECT_FALSE(pond::math::IsNear(base, far, tolerance));
    EXPECT_FALSE(pond::math::IsNear(base, nonFinite, tolerance));
}

TEST(Matrix4x4Tests, DefaultsToZeroAndCreatesIdentity)
{
    constexpr pond::math::Matrix4x4 defaultMatrix;

    EXPECT_EQ(defaultMatrix, pond::math::Matrix4x4{});
    EXPECT_EQ(pond::math::Matrix4x4::Zero(), pond::math::Matrix4x4{});
    EXPECT_EQ(pond::math::Matrix4x4::Identity(),
              (pond::math::Matrix4x4{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                     1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}));
}

TEST(Matrix4x4Tests, ConstructsFromScalarsAndStoresColumnMajor)
{
    const pond::math::Matrix4x4 matrix{1.0F, 2.0F,  3.0F,  4.0F,  5.0F,  6.0F,  7.0F,  8.0F,
                                       9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F};

    EXPECT_EQ(matrix.row0Column0, 1.0F);
    EXPECT_EQ(matrix.row0Column3, 4.0F);
    EXPECT_EQ(matrix.row1Column0, 5.0F);
    EXPECT_EQ(matrix.row2Column2, 11.0F);
    EXPECT_EQ(matrix.row3Column3, 16.0F);
    EXPECT_EQ(CopyStorage<16>(matrix),
              (std::array<float, 16>{1.0F, 5.0F, 9.0F, 13.0F, 2.0F, 6.0F, 10.0F, 14.0F, 3.0F, 7.0F,
                                     11.0F, 15.0F, 4.0F, 8.0F, 12.0F, 16.0F}));
}

TEST(Matrix4x4Tests, ConstructsFromColumnsAndRetrievesRowsAndColumns)
{
    const pond::math::Matrix4x4 matrix{pond::math::Vector4{1.0F, 2.0F, 3.0F, 4.0F},
                                       pond::math::Vector4{5.0F, 6.0F, 7.0F, 8.0F},
                                       pond::math::Vector4{9.0F, 10.0F, 11.0F, 12.0F},
                                       pond::math::Vector4{13.0F, 14.0F, 15.0F, 16.0F}};

    auto row2 = matrix.Row(2);
    auto column3 = matrix.Column(3);
    ASSERT_TRUE(row2.HasValue());
    ASSERT_TRUE(column3.HasValue());

    EXPECT_EQ(row2.GetValue(), (pond::math::Vector4{3.0F, 7.0F, 11.0F, 15.0F}));
    EXPECT_EQ(column3.GetValue(), (pond::math::Vector4{13.0F, 14.0F, 15.0F, 16.0F}));
    ExpectInvalidIndex(matrix.Row(4));
    ExpectInvalidIndex(matrix.Column(4));
}

TEST(Matrix4x4Tests, ProvidesCheckedMutableAndConstElementAccess)
{
    pond::math::Matrix4x4 matrix{1.0F, 2.0F,  3.0F,  4.0F,  5.0F,  6.0F,  7.0F,  8.0F,
                                 9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F};

    auto element = matrix.At(2, 3);
    ASSERT_TRUE(element.HasValue());
    element->get() = 84.0F;
    EXPECT_EQ(matrix.row2Column3, 84.0F);

    const pond::math::Matrix4x4 constMatrix = matrix;
    auto readElement = constMatrix.At(2, 3);
    ASSERT_TRUE(readElement.HasValue());
    EXPECT_EQ(readElement->get(), 84.0F);
    ExpectInvalidIndex(matrix.At(4, 0));
    ExpectInvalidIndex(matrix.At(0, 4));
    ExpectInvalidIndex(constMatrix.At(4, 4));
}

TEST(Matrix4x4Tests, ComparesExactlyAndWithCallerTolerance)
{
    const pond::math::Matrix4x4 base{1.0F, 2.0F,  3.0F,  4.0F,  5.0F,  6.0F,  7.0F,  8.0F,
                                     9.0F, 10.0F, 11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F};
    const pond::math::Matrix4x4 close{1.0F, 2.0F,  3.0F,  4.0F,    5.0F,  6.0F,  7.0F,  8.0F,
                                      9.0F, 10.0F, 11.0F, 12.005F, 13.0F, 14.0F, 15.0F, 16.0F};
    const pond::math::Matrix4x4 far{1.0F, 2.0F,  3.0F,  4.0F,   5.0F,  6.0F,  7.0F,  8.0F,
                                    9.0F, 10.0F, 11.0F, 12.02F, 13.0F, 14.0F, 15.0F, 16.0F};
    const pond::math::Matrix4x4 nonFinite{
        1.0F,  2.0F,  3.0F,  4.0F,  5.0F,  std::numeric_limits<float>::quiet_NaN(),
        7.0F,  8.0F,  9.0F,  10.0F, 11.0F, 12.0F,
        13.0F, 14.0F, 15.0F, 16.0F};
    const pond::core::Tolerance tolerance = RequireTolerance(0.01F, 0.0F);

    EXPECT_EQ(base, base);
    EXPECT_FALSE(base == close);
    EXPECT_TRUE(pond::math::IsNear(base, close, tolerance));
    EXPECT_FALSE(pond::math::IsNear(base, far, tolerance));
    EXPECT_FALSE(pond::math::IsNear(base, nonFinite, tolerance));
}
} // namespace
