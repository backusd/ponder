#include <ponder/platform/Dialog.hpp>

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>

namespace
{
static_assert(!std::same_as<pond::platform::DialogRequestId, pond::platform::WindowId>);
static_assert(!std::same_as<pond::platform::DialogRequestId, pond::platform::DisplayId>);
static_assert(!std::convertible_to<std::uint64_t, pond::platform::DialogRequestId>);
static_assert(!std::convertible_to<pond::platform::DialogRequestId, std::uint64_t>);
static_assert(std::is_trivially_copyable_v<pond::platform::DialogRequestId>);
static_assert(std::variant_size_v<pond::platform::DialogOutcome> == 3U);

TEST(DialogRequestIdTests, DefaultsToInvalidZero)
{
    constexpr pond::platform::DialogRequestId kInvalid;

    EXPECT_FALSE(kInvalid.IsValid());
    EXPECT_EQ(kInvalid, pond::platform::DialogRequestId::Invalid());
    EXPECT_EQ(kInvalid.GetValue(), 0U);
}

TEST(DialogRequestIdTests, ComparesAndHashesByNumericValue)
{
    constexpr pond::platform::DialogRequestId kFirst{1};
    constexpr pond::platform::DialogRequestId kSameFirst{1};
    constexpr pond::platform::DialogRequestId kSecond{2};

    EXPECT_EQ(kFirst, kSameFirst);
    EXPECT_LT(kFirst, kSecond);

    std::unordered_set<pond::platform::DialogRequestId> ids;
    ids.insert(kFirst);
    EXPECT_TRUE(ids.contains(kSameFirst));
    EXPECT_FALSE(ids.contains(kSecond));
}

TEST(DialogTypesTests, OwnDescriptorsAndOutcomes)
{
    pond::platform::OpenFileDialogDesc openDesc{
        .parentWindowId = pond::platform::WindowId{7},
        .defaultLocation = std::filesystem::path{"C:/tmp/molecule.sdf"},
        .filters = {{.name = "Molecules", .pattern = "sdf;mol"}},
        .allowMultipleSelection = true};
    EXPECT_EQ(openDesc.parentWindowId, pond::platform::WindowId{7});
    EXPECT_EQ(openDesc.defaultLocation, std::filesystem::path{"C:/tmp/molecule.sdf"});
    ASSERT_EQ(openDesc.filters.size(), 1U);
    EXPECT_EQ(openDesc.filters.front().name, "Molecules");
    EXPECT_EQ(openDesc.filters.front().pattern, "sdf;mol");
    EXPECT_TRUE(openDesc.allowMultipleSelection);

    const pond::platform::DialogSelection selection{
        .paths = {std::filesystem::path{"C:/tmp/water.sdf"}}, .selectedFilterIndex = 0U};
    const pond::platform::DialogOutcome outcome{selection};
    ASSERT_TRUE(std::holds_alternative<pond::platform::DialogSelection>(outcome));
    EXPECT_EQ(std::get<pond::platform::DialogSelection>(outcome), selection);

    const pond::platform::DialogOutcome cancellation{pond::platform::DialogCancellation{}};
    EXPECT_TRUE(std::holds_alternative<pond::platform::DialogCancellation>(cancellation));
}
} // namespace
