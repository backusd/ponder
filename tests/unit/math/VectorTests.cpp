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

template <typename Vector>
concept HasVectorSubscript = requires(Vector vector, const Vector constVector, std::size_t index) {
    vector[index];
    constVector[index];
};

[[nodiscard]] constexpr bool SupportsCompileTimeCheckedVectorAccess()
{
    pond::math::Vector2 vector2{1.0F, 2.0F};
    pond::math::Vector3 vector3{3.0F, 4.0F, 5.0F};
    pond::math::Vector4 vector4{6.0F, 7.0F, 8.0F, 9.0F};

    auto vector2Element = vector2.At(1);
    auto vector3Element = vector3.At(2);
    auto vector4Element = vector4.At(3);
    if (!vector2Element.HasValue() || !vector3Element.HasValue() || !vector4Element.HasValue())
    {
        return false;
    }

    vector2Element->get() = 20.0F;
    vector3Element->get() = 50.0F;
    vector4Element->get() = 90.0F;

    const pond::math::Vector2 constVector2 = vector2;
    const pond::math::Vector3 constVector3 = vector3;
    const pond::math::Vector4 constVector4 = vector4;
    const auto constVector2Element = constVector2.At(1);
    const auto constVector3Element = constVector3.At(2);
    const auto constVector4Element = constVector4.At(3);
    return constVector2Element.HasValue() && constVector2Element->get() == 20.0F &&
           constVector3Element.HasValue() && constVector3Element->get() == 50.0F &&
           constVector4Element.HasValue() && constVector4Element->get() == 90.0F;
}

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
static_assert(!HasVectorSubscript<pond::math::Vector2>);
static_assert(!HasVectorSubscript<pond::math::Vector3>);
static_assert(!HasVectorSubscript<pond::math::Vector4>);
static_assert(SupportsCompileTimeCheckedVectorAccess());

void ExpectInvalidIndex(auto result)
{
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::math::ToErrorCode(pond::math::MathErrorCode::InvalidArgument));
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

    EXPECT_EQ(vector, (pond::math::Vector2{5.0F, 6.0F}));
}

TEST(Vector2Tests, ProvidesConstCheckedIndexingAndRejectsInvalidIndices)
{
    const pond::math::Vector2 vector{1.0F, 2.0F};
    auto x = vector.At(0);
    auto y = vector.At(1);
    ASSERT_TRUE(x.HasValue());
    ASSERT_TRUE(y.HasValue());

    EXPECT_EQ(x->get(), 1.0F);
    EXPECT_EQ(y->get(), 2.0F);
    ExpectInvalidIndex(vector.At(2));
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

    EXPECT_EQ(vector, (pond::math::Vector3{7.0F, 8.0F, 9.0F}));
}

TEST(Vector3Tests, ProvidesConstCheckedIndexingAndRejectsInvalidIndices)
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
    ExpectInvalidIndex(vector.At(3));
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

    EXPECT_EQ(vector, (pond::math::Vector4{9.0F, 10.0F, 11.0F, 12.0F}));
}

TEST(Vector4Tests, ProvidesConstCheckedIndexingAndRejectsInvalidIndices)
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
    ExpectInvalidIndex(vector.At(4));
}
} // namespace
