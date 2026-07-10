#include <ponder/core/Assert.hpp>
#include <ponder/core/PonderException.hpp>
#include <ponder/math/Vector2.hpp>
#include <ponder/math/Vector3.hpp>
#include <ponder/math/Vector4.hpp>

#include <concepts>
#include <cstddef>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
template <typename Vector>
concept HasVectorMultiply = requires(Vector lhs, Vector rhs) { lhs * rhs; };

template <typename Vector>
concept HasVectorDivide = requires(Vector lhs, Vector rhs) { lhs / rhs; };

static_assert(sizeof(pond::math::Vector2) == 8);
static_assert(sizeof(pond::math::Vector3) == 12);
static_assert(sizeof(pond::math::Vector4) == 16);
static_assert(alignof(pond::math::Vector2) == alignof(float));
static_assert(alignof(pond::math::Vector3) == alignof(float));
static_assert(alignof(pond::math::Vector4) == alignof(float));
static_assert(std::is_standard_layout_v<pond::math::Vector2>);
static_assert(std::is_standard_layout_v<pond::math::Vector3>);
static_assert(std::is_standard_layout_v<pond::math::Vector4>);
static_assert(std::is_trivially_copyable_v<pond::math::Vector2>);
static_assert(std::is_trivially_copyable_v<pond::math::Vector3>);
static_assert(std::is_trivially_copyable_v<pond::math::Vector4>);
static_assert(offsetof(pond::math::Vector2, x) == 0);
static_assert(offsetof(pond::math::Vector2, y) == sizeof(float));
static_assert(offsetof(pond::math::Vector3, x) == 0);
static_assert(offsetof(pond::math::Vector3, y) == sizeof(float));
static_assert(offsetof(pond::math::Vector3, z) == sizeof(float) * 2);
static_assert(offsetof(pond::math::Vector4, x) == 0);
static_assert(offsetof(pond::math::Vector4, y) == sizeof(float));
static_assert(offsetof(pond::math::Vector4, z) == sizeof(float) * 2);
static_assert(offsetof(pond::math::Vector4, w) == sizeof(float) * 3);
static_assert(!HasVectorMultiply<pond::math::Vector2>);
static_assert(!HasVectorMultiply<pond::math::Vector3>);
static_assert(!HasVectorMultiply<pond::math::Vector4>);
static_assert(!HasVectorDivide<pond::math::Vector2>);
static_assert(!HasVectorDivide<pond::math::Vector3>);
static_assert(!HasVectorDivide<pond::math::Vector4>);

void ExpectInvalidIndex(auto result)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::math::ToErrorCode(pond::math::MathErrorCode::InvalidArgument));
}

void IgnoreExpectedVerifyFailure(const pond::core::AssertionFailure&)
{
}

TEST(Vector2Tests, DefaultsToZeroAndConstructsFromComponents)
{
    constexpr pond::math::Vector2 defaultVector;
    constexpr pond::math::Vector2 componentVector{1.0F, 2.0F};

    EXPECT_EQ(defaultVector.x, 0.0F);
    EXPECT_EQ(defaultVector.y, 0.0F);
    EXPECT_EQ(componentVector.x, 1.0F);
    EXPECT_EQ(componentVector.y, 2.0F);
    EXPECT_EQ(pond::math::Vector2::Zero(), pond::math::Vector2{});
}

TEST(Vector2Tests, SupportsPublicMutationAndCheckedIndexing)
{
    pond::math::Vector2 vector{1.0F, 2.0F};
    vector.x = 3.0F;
    vector.y = 4.0F;

    auto x = vector.At(0);
    auto y = vector.At(1);
    ASSERT_TRUE(x.HasValue());
    ASSERT_TRUE(y.HasValue());

    x->get() = 5.0F;
    y->get() = 6.0F;

    EXPECT_EQ(vector[0], 5.0F);
    EXPECT_EQ(vector[1], 6.0F);

    vector[0] = 7.0F;
    vector[1] = 8.0F;

    EXPECT_EQ(vector, (pond::math::Vector2{7.0F, 8.0F}));
}


TEST(Vector2Tests, ProvidesConstIndexingAndRejectsInvalidIndices)
{
    const pond::math::Vector2 vector{1.0F, 2.0F};
    auto x = vector.At(0);
    auto y = vector.At(1);
    ASSERT_TRUE(x.HasValue());
    ASSERT_TRUE(y.HasValue());

    EXPECT_EQ(x->get(), 1.0F);
    EXPECT_EQ(y->get(), 2.0F);
    EXPECT_EQ(vector[0], 1.0F);
    EXPECT_EQ(vector[1], 2.0F);
    ExpectInvalidIndex(vector.At(2));
    const pond::core::ScopedVerifyFailureHandler verifyHandler{IgnoreExpectedVerifyFailure};
    EXPECT_THROW((void)vector[2], pond::core::PonderException);
}

TEST(Vector3Tests, DefaultsToZeroAndConstructsFromComponents)
{
    constexpr pond::math::Vector3 defaultVector;
    constexpr pond::math::Vector3 componentVector{1.0F, 2.0F, 3.0F};

    EXPECT_EQ(defaultVector.x, 0.0F);
    EXPECT_EQ(defaultVector.y, 0.0F);
    EXPECT_EQ(defaultVector.z, 0.0F);
    EXPECT_EQ(componentVector.x, 1.0F);
    EXPECT_EQ(componentVector.y, 2.0F);
    EXPECT_EQ(componentVector.z, 3.0F);
    EXPECT_EQ(pond::math::Vector3::Zero(), pond::math::Vector3{});
}

TEST(Vector3Tests, SupportsPublicMutationAndCheckedIndexing)
{
    pond::math::Vector3 vector{1.0F, 2.0F, 3.0F};
    vector.x = 4.0F;
    vector.y = 5.0F;
    vector.z = 6.0F;

    auto x = vector.At(0);
    auto y = vector.At(1);
    auto z = vector.At(2);
    ASSERT_TRUE(x.HasValue());
    ASSERT_TRUE(y.HasValue());
    ASSERT_TRUE(z.HasValue());

    x->get() = 7.0F;
    y->get() = 8.0F;
    z->get() = 9.0F;

    EXPECT_EQ(vector[0], 7.0F);
    EXPECT_EQ(vector[1], 8.0F);
    EXPECT_EQ(vector[2], 9.0F);

    vector[0] = 10.0F;
    vector[1] = 11.0F;
    vector[2] = 12.0F;

    EXPECT_EQ(vector, (pond::math::Vector3{10.0F, 11.0F, 12.0F}));
}

TEST(Vector3Tests, ProvidesConstIndexingAndRejectsInvalidIndices)
{
    const pond::math::Vector3 vector{1.0F, 2.0F, 3.0F};
    auto x = vector.At(0);
    auto y = vector.At(1);
    auto z = vector.At(2);
    ASSERT_TRUE(x.HasValue());
    ASSERT_TRUE(y.HasValue());
    ASSERT_TRUE(z.HasValue());

    EXPECT_EQ(x->get(), 1.0F);
    EXPECT_EQ(y->get(), 2.0F);
    EXPECT_EQ(z->get(), 3.0F);
    EXPECT_EQ(vector[0], 1.0F);
    EXPECT_EQ(vector[1], 2.0F);
    EXPECT_EQ(vector[2], 3.0F);
    ExpectInvalidIndex(vector.At(3));
    const pond::core::ScopedVerifyFailureHandler verifyHandler{IgnoreExpectedVerifyFailure};
    EXPECT_THROW((void)vector[3], pond::core::PonderException);
}

TEST(Vector4Tests, DefaultsToZeroAndConstructsFromComponents)
{
    constexpr pond::math::Vector4 defaultVector;
    constexpr pond::math::Vector4 componentVector{1.0F, 2.0F, 3.0F, 4.0F};

    EXPECT_EQ(defaultVector.x, 0.0F);
    EXPECT_EQ(defaultVector.y, 0.0F);
    EXPECT_EQ(defaultVector.z, 0.0F);
    EXPECT_EQ(defaultVector.w, 0.0F);
    EXPECT_EQ(componentVector.x, 1.0F);
    EXPECT_EQ(componentVector.y, 2.0F);
    EXPECT_EQ(componentVector.z, 3.0F);
    EXPECT_EQ(componentVector.w, 4.0F);
    EXPECT_EQ(pond::math::Vector4::Zero(), pond::math::Vector4{});
}

TEST(Vector4Tests, SupportsPublicMutationAndCheckedIndexing)
{
    pond::math::Vector4 vector{1.0F, 2.0F, 3.0F, 4.0F};
    vector.x = 5.0F;
    vector.y = 6.0F;
    vector.z = 7.0F;
    vector.w = 8.0F;

    auto x = vector.At(0);
    auto y = vector.At(1);
    auto z = vector.At(2);
    auto w = vector.At(3);
    ASSERT_TRUE(x.HasValue());
    ASSERT_TRUE(y.HasValue());
    ASSERT_TRUE(z.HasValue());
    ASSERT_TRUE(w.HasValue());

    x->get() = 9.0F;
    y->get() = 10.0F;
    z->get() = 11.0F;
    w->get() = 12.0F;

    EXPECT_EQ(vector[0], 9.0F);
    EXPECT_EQ(vector[1], 10.0F);
    EXPECT_EQ(vector[2], 11.0F);
    EXPECT_EQ(vector[3], 12.0F);

    vector[0] = 13.0F;
    vector[1] = 14.0F;
    vector[2] = 15.0F;
    vector[3] = 16.0F;

    EXPECT_EQ(vector, (pond::math::Vector4{13.0F, 14.0F, 15.0F, 16.0F}));
}

TEST(Vector4Tests, ProvidesConstIndexingAndRejectsInvalidIndices)
{
    const pond::math::Vector4 vector{1.0F, 2.0F, 3.0F, 4.0F};
    auto x = vector.At(0);
    auto y = vector.At(1);
    auto z = vector.At(2);
    auto w = vector.At(3);
    ASSERT_TRUE(x.HasValue());
    ASSERT_TRUE(y.HasValue());
    ASSERT_TRUE(z.HasValue());
    ASSERT_TRUE(w.HasValue());

    EXPECT_EQ(x->get(), 1.0F);
    EXPECT_EQ(y->get(), 2.0F);
    EXPECT_EQ(z->get(), 3.0F);
    EXPECT_EQ(w->get(), 4.0F);
    EXPECT_EQ(vector[0], 1.0F);
    EXPECT_EQ(vector[1], 2.0F);
    EXPECT_EQ(vector[2], 3.0F);
    EXPECT_EQ(vector[3], 4.0F);
    ExpectInvalidIndex(vector.At(4));
    const pond::core::ScopedVerifyFailureHandler verifyHandler{IgnoreExpectedVerifyFailure};
    EXPECT_THROW((void)vector[4], pond::core::PonderException);
}
} // namespace
