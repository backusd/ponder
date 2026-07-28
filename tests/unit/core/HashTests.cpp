#include <ponder/core/Hash.hpp>

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
constexpr bool IdentifierHashingIsConstexpr()
{
    return ponder::core::HashIdentifierValue(0) == ponder::core::HashIdentifierValue(0) &&
           ponder::core::HashIdentifierValue(42) == ponder::core::HashIdentifierValue(42) &&
           ponder::core::HashIdentifierValue(42) != ponder::core::HashIdentifierValue(43);
}

static_assert(IdentifierHashingIsConstexpr());
static_assert(noexcept(ponder::core::HashIdentifierValue(42)));
static_assert(std::is_same_v<decltype(ponder::core::HashIdentifierValue(42)), std::size_t>);

TEST(HashTests, ProducesStableValuesForIdentifierInputs)
{
    EXPECT_EQ(ponder::core::HashIdentifierValue(0), ponder::core::HashIdentifierValue(0));
    EXPECT_EQ(ponder::core::HashIdentifierValue(42), ponder::core::HashIdentifierValue(42));
    EXPECT_NE(ponder::core::HashIdentifierValue(42), ponder::core::HashIdentifierValue(43));
    EXPECT_NE(ponder::core::HashIdentifierValue(std::uint64_t{0}), ponder::core::HashIdentifierValue(std::uint64_t{1} << 63U));
}
} // namespace