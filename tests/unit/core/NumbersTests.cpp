#include <ponder/core/Numbers.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <type_traits>

namespace
{
template <typename T>
concept HasIsFinite = requires(T value) { pond::core::IsFinite(value); };

static_assert(pond::core::IsFinite(0));
static_assert(pond::core::IsFinite(-42));
static_assert(pond::core::IsFinite(0.0F));
static_assert(pond::core::IsFinite(std::numeric_limits<float>::max()));
static_assert(!pond::core::IsFinite(std::numeric_limits<float>::infinity()));
static_assert(!pond::core::IsFinite(-std::numeric_limits<float>::infinity()));
static_assert(!pond::core::IsFinite(std::numeric_limits<float>::quiet_NaN()));
static_assert(HasIsFinite<int>);
static_assert(HasIsFinite<unsigned int>);
static_assert(HasIsFinite<float>);
static_assert(HasIsFinite<double>);
static_assert(!HasIsFinite<bool>);
static_assert(!HasIsFinite<void*>);
static_assert(std::is_same_v<decltype(pond::core::IsFinite(1.0F)), bool>);

TEST(NumberTests, TreatsIntegralValuesAsFinite)
{
    EXPECT_TRUE(pond::core::IsFinite(0));
    EXPECT_TRUE(pond::core::IsFinite(std::numeric_limits<int>::lowest()));
    EXPECT_TRUE(pond::core::IsFinite(std::numeric_limits<unsigned int>::max()));
}

TEST(NumberTests, RejectsFloatingPointInfinityAndNan)
{
    EXPECT_TRUE(pond::core::IsFinite(0.0F));
    EXPECT_TRUE(pond::core::IsFinite(-0.0));
    EXPECT_TRUE(pond::core::IsFinite(std::numeric_limits<long double>::max()));

    EXPECT_FALSE(pond::core::IsFinite(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(pond::core::IsFinite(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(pond::core::IsFinite(std::numeric_limits<long double>::quiet_NaN()));
}
} // namespace