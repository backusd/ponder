#include <ponder/core/Identifier.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <type_traits>
#include <unordered_set>

namespace
{
struct MoleculeIdTag;
struct AtomIdTag;

using MoleculeId = pond::core::Identifier<MoleculeIdTag>;
using AtomId = pond::core::Identifier<AtomIdTag>;

constexpr bool IdentifierValueSemanticsAreConstexpr()
{
    constexpr MoleculeId kInvalid;
    constexpr MoleculeId kFirst{7};
    constexpr MoleculeId kSameFirst{7};
    constexpr MoleculeId kSecond{9};

    return !kInvalid.IsValid() && kInvalid == MoleculeId::Invalid() && kInvalid.GetValue() == 0 &&
           kFirst.IsValid() && kFirst.GetValue() == 7 && kFirst == kSameFirst && kFirst < kSecond;
}

constexpr bool IdentifierHashingIsConstexpr()
{
    constexpr MoleculeId kIdentifier{42};
    return std::hash<MoleculeId>{}(kIdentifier) == std::hash<MoleculeId>{}(MoleculeId{42}) &&
           std::hash<MoleculeId>{}(kIdentifier) != std::hash<MoleculeId>{}(MoleculeId{43});
}

static_assert(IdentifierValueSemanticsAreConstexpr());
static_assert(IdentifierHashingIsConstexpr());
static_assert(!std::same_as<MoleculeId, AtomId>);
static_assert(!std::convertible_to<std::uint64_t, MoleculeId>);
static_assert(!std::convertible_to<MoleculeId, std::uint64_t>);
static_assert(!std::convertible_to<MoleculeId, AtomId>);
static_assert(std::is_nothrow_constructible_v<MoleculeId, std::uint64_t>);
static_assert(std::is_trivially_copyable_v<MoleculeId>);

TEST(CoreIdentifierTests, DefaultsToInvalidZero)
{
    const MoleculeId id;

    EXPECT_FALSE(id.IsValid());
    EXPECT_EQ(id, MoleculeId::Invalid());
    EXPECT_EQ(id.GetValue(), 0U);
}

TEST(CoreIdentifierTests, ComparesByNumericValue)
{
    constexpr MoleculeId kFirst{1};
    constexpr MoleculeId kSecond{2};

    EXPECT_EQ(kFirst, MoleculeId{1});
    EXPECT_LT(kFirst, kSecond);
}

TEST(CoreIdentifierTests, SupportsHashContainerLookup)
{
    std::unordered_set<MoleculeId> ids;

    ids.insert(MoleculeId{17});

    EXPECT_TRUE(ids.contains(MoleculeId{17}));
    EXPECT_FALSE(ids.contains(MoleculeId{18}));
}
} // namespace
