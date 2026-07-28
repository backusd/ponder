#include <ponder/platform/Identifiers.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <type_traits>
#include <unordered_set>

namespace
{
constexpr bool IdentifierValueSemanticsAreConstexpr()
{
    constexpr ponder::platform::WindowId kInvalidWindowId;
    constexpr ponder::platform::WindowId kWindowId{7};
    constexpr ponder::platform::WindowId kSameWindowId{7};
    constexpr ponder::platform::WindowId kLaterWindowId{9};
    constexpr ponder::platform::DisplayId kDisplayId{7};
    constexpr ponder::platform::DialogRequestId kDialogRequestId{7};

    return !kInvalidWindowId.IsValid() && kInvalidWindowId == ponder::platform::WindowId::Invalid() && kInvalidWindowId.GetValue() == 0 &&
           kWindowId.IsValid() && kWindowId.GetValue() == 7 && kWindowId == kSameWindowId && kWindowId < kLaterWindowId && kDisplayId.IsValid() &&
           kDisplayId.GetValue() == 7 && kDialogRequestId.IsValid() && kDialogRequestId.GetValue() == 7 &&
           ponder::platform::DialogRequestId{} == ponder::platform::DialogRequestId::Invalid();
}

constexpr bool IdentifierHashingIsConstexpr()
{
    constexpr ponder::platform::WindowId kWindowId{42};
    constexpr ponder::platform::DisplayId kDisplayId{42};
    constexpr ponder::platform::DialogRequestId kDialogRequestId{42};

    return std::hash<ponder::platform::WindowId>{}(kWindowId) == std::hash<ponder::platform::WindowId>{}(ponder::platform::WindowId{42}) &&
           std::hash<ponder::platform::DisplayId>{}(kDisplayId) == std::hash<ponder::platform::DisplayId>{}(ponder::platform::DisplayId{42}) &&
           std::hash<ponder::platform::DialogRequestId>{}(kDialogRequestId) ==
               std::hash<ponder::platform::DialogRequestId>{}(ponder::platform::DialogRequestId{42});
}

static_assert(IdentifierValueSemanticsAreConstexpr());
static_assert(IdentifierHashingIsConstexpr());
static_assert(!std::same_as<ponder::platform::WindowId, ponder::platform::DisplayId>);
static_assert(!std::same_as<ponder::platform::WindowId, ponder::platform::DialogRequestId>);
static_assert(!std::same_as<ponder::platform::DisplayId, ponder::platform::DialogRequestId>);
static_assert(!std::convertible_to<std::uint64_t, ponder::platform::WindowId>);
static_assert(!std::convertible_to<ponder::platform::WindowId, std::uint64_t>);
static_assert(!std::convertible_to<ponder::platform::WindowId, ponder::platform::DisplayId>);
static_assert(!std::convertible_to<std::uint64_t, ponder::platform::DialogRequestId>);
static_assert(!std::convertible_to<ponder::platform::DialogRequestId, std::uint64_t>);
static_assert(std::is_nothrow_constructible_v<ponder::platform::WindowId, std::uint64_t>);
static_assert(std::is_nothrow_constructible_v<ponder::platform::DisplayId, std::uint64_t>);
static_assert(std::is_nothrow_constructible_v<ponder::platform::DialogRequestId, std::uint64_t>);
static_assert(std::is_trivially_copyable_v<ponder::platform::WindowId>);
static_assert(std::is_trivially_copyable_v<ponder::platform::DisplayId>);
static_assert(std::is_trivially_copyable_v<ponder::platform::DialogRequestId>);

TEST(PlatformIdentifierTests, DefaultsToInvalidZero)
{
    const ponder::platform::WindowId windowId;
    const ponder::platform::DisplayId displayId;
    const ponder::platform::DialogRequestId dialogRequestId;

    EXPECT_FALSE(windowId.IsValid());
    EXPECT_FALSE(displayId.IsValid());
    EXPECT_FALSE(dialogRequestId.IsValid());
    EXPECT_EQ(windowId.GetValue(), 0U);
    EXPECT_EQ(displayId.GetValue(), 0U);
    EXPECT_EQ(dialogRequestId.GetValue(), 0U);
}

TEST(PlatformIdentifierTests, ComparesByNumericValue)
{
    constexpr ponder::platform::WindowId kFirst{1};
    constexpr ponder::platform::WindowId kSecond{2};
    constexpr ponder::platform::DialogRequestId kFirstDialogRequest{1};
    constexpr ponder::platform::DialogRequestId kSecondDialogRequest{2};

    EXPECT_EQ(kFirst, ponder::platform::WindowId{1});
    EXPECT_LT(kFirst, kSecond);
    EXPECT_EQ(kFirstDialogRequest, ponder::platform::DialogRequestId{1});
    EXPECT_LT(kFirstDialogRequest, kSecondDialogRequest);
}

TEST(PlatformIdentifierTests, SupportsHashContainerLookup)
{
    std::unordered_set<ponder::platform::WindowId> windowIds;
    std::unordered_set<ponder::platform::DisplayId> displayIds;
    std::unordered_set<ponder::platform::DialogRequestId> dialogRequestIds;

    windowIds.insert(ponder::platform::WindowId{17});
    displayIds.insert(ponder::platform::DisplayId{23});
    dialogRequestIds.insert(ponder::platform::DialogRequestId{31});

    EXPECT_TRUE(windowIds.contains(ponder::platform::WindowId{17}));
    EXPECT_TRUE(displayIds.contains(ponder::platform::DisplayId{23}));
    EXPECT_TRUE(dialogRequestIds.contains(ponder::platform::DialogRequestId{31}));
    EXPECT_FALSE(windowIds.contains(ponder::platform::WindowId{18}));
    EXPECT_FALSE(displayIds.contains(ponder::platform::DisplayId{24}));
    EXPECT_FALSE(dialogRequestIds.contains(ponder::platform::DialogRequestId{32}));
}
} // namespace
