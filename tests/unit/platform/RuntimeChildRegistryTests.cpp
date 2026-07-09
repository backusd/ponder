#include "RuntimeChildRegistry.hpp"

#include <ponder/core/PonderException.hpp>

#include <gtest/gtest.h>

namespace
{
TEST(RuntimeChildRegistryTests, TracksChildrenAndRequestsIndependently)
{
    pond::platform::detail::RuntimeChildRegistry registry;
    const int child = 1;
    const int request = 2;

    EXPECT_TRUE(registry.IsEmpty());

    registry.RegisterChild(&child);
    registry.RegisterRequest(&request);

    EXPECT_FALSE(registry.IsEmpty());
    EXPECT_EQ(registry.GetChildCount(), 1U);
    EXPECT_EQ(registry.GetRequestCount(), 1U);

    registry.UnregisterRequest(&request);
    registry.UnregisterChild(&child);

    EXPECT_TRUE(registry.IsEmpty());
}

TEST(RuntimeChildRegistryTests, RejectsNullDuplicateAndUnknownEntries)
{
    pond::platform::detail::RuntimeChildRegistry registry;
    const int child = 1;
    const int request = 2;
    const int unknown = 3;

    EXPECT_THROW(registry.RegisterChild(nullptr), pond::core::PonderException);
    EXPECT_THROW(registry.RegisterRequest(nullptr), pond::core::PonderException);

    registry.RegisterChild(&child);
    registry.RegisterRequest(&request);

    EXPECT_THROW(registry.RegisterChild(&child), pond::core::PonderException);
    EXPECT_THROW(registry.RegisterRequest(&request), pond::core::PonderException);
    EXPECT_THROW(registry.UnregisterChild(&unknown), pond::core::PonderException);
    EXPECT_THROW(registry.UnregisterRequest(&unknown), pond::core::PonderException);

    registry.UnregisterChild(&child);
    registry.UnregisterRequest(&request);
}
} // namespace
