#include <ponder/core/Hash.hpp>

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
constexpr bool IdentifierHashingIsConstexpr()
{
    return pond::core::HashIdentifierValue(0) == pond::core::HashIdentifierValue(0) &&
           pond::core::HashIdentifierValue(42) == pond::core::HashIdentifierValue(42) &&
           pond::core::HashIdentifierValue(42) != pond::core::HashIdentifierValue(43);
}

static_assert(IdentifierHashingIsConstexpr());
static_assert(noexcept(pond::core::HashIdentifierValue(42)));
static_assert(std::is_same_v<decltype(pond::core::HashIdentifierValue(42)), std::size_t>);

TEST(HashTests, ProducesStableValuesForIdentifierInputs)
{
    EXPECT_EQ(pond::core::HashIdentifierValue(0), pond::core::HashIdentifierValue(0));
    EXPECT_EQ(pond::core::HashIdentifierValue(42), pond::core::HashIdentifierValue(42));
    EXPECT_NE(pond::core::HashIdentifierValue(42), pond::core::HashIdentifierValue(43));
    EXPECT_NE(pond::core::HashIdentifierValue(std::uint64_t{0}),
              pond::core::HashIdentifierValue(std::uint64_t{1} << 63U));
}
} // namespace