#include <ponder/core/Result.hpp>
#include <ponder/math/MathError.hpp>

#include <gtest/gtest.h>

namespace
{
TEST(MathConsumerLinkTests, MathTargetSuppliesInheritedCoreDependency)
{
    const pond::core::ErrorCode code =
        pond::math::ToErrorCode(pond::math::MathErrorCode::InvalidArgument);
    pond::core::Result<pond::core::ErrorCode> result = code;

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result->GetCategory(), pond::core::ErrorCategory::InvalidArgument);
    EXPECT_EQ(result->GetValue(), 0x0002'0001);
}
} // namespace
