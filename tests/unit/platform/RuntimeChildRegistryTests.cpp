#include <ponder/core/Exception.hpp>

#include <gtest/gtest.h>

#include "RuntimeChildRegistry.hpp"

namespace
{
TEST(RuntimeChildRegistryTests, TracksChildren)
{
    ponder::platform::detail::RuntimeChildRegistry registry;
    const int child = 1;

    EXPECT_TRUE(registry.IsEmpty());

    registry.RegisterChild(&child);

    EXPECT_FALSE(registry.IsEmpty());
    EXPECT_EQ(registry.GetChildCount(), 1U);

    registry.UnregisterChild(&child);

    EXPECT_TRUE(registry.IsEmpty());
}

TEST(RuntimeChildRegistryTests, RejectsNullDuplicateAndUnknownEntries)
{
    ponder::platform::detail::RuntimeChildRegistry registry;
    const int child = 1;
    const int unknown = 2;

    EXPECT_THROW(registry.RegisterChild(nullptr), ponder::core::Exception);

    registry.RegisterChild(&child);

    EXPECT_THROW(registry.RegisterChild(&child), ponder::core::Exception);
    EXPECT_THROW(registry.UnregisterChild(&unknown), ponder::core::Exception);

    registry.UnregisterChild(&child);
}
} // namespace
