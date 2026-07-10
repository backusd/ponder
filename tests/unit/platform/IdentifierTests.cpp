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
    constexpr pond::platform::WindowId kInvalidWindowId;
    constexpr pond::platform::WindowId kWindowId{7};
    constexpr pond::platform::WindowId kSameWindowId{7};
    constexpr pond::platform::WindowId kLaterWindowId{9};
    constexpr pond::platform::DisplayId kDisplayId{7};
    constexpr pond::platform::DialogRequestId kDialogRequestId{7};

    return !kInvalidWindowId.IsValid() && kInvalidWindowId == pond::platform::WindowId::Invalid() &&
           kInvalidWindowId.GetValue() == 0 && kWindowId.IsValid() && kWindowId.GetValue() == 7 &&
           kWindowId == kSameWindowId && kWindowId < kLaterWindowId && kDisplayId.IsValid() &&
           kDisplayId.GetValue() == 7 && kDialogRequestId.IsValid() &&
           kDialogRequestId.GetValue() == 7 &&
           pond::platform::DialogRequestId{} == pond::platform::DialogRequestId::Invalid();
}

constexpr bool IdentifierHashingIsConstexpr()
{
    constexpr pond::platform::WindowId kWindowId{42};
    constexpr pond::platform::DisplayId kDisplayId{42};
    constexpr pond::platform::DialogRequestId kDialogRequestId{42};

    return std::hash<pond::platform::WindowId>{}(kWindowId) ==
               std::hash<pond::platform::WindowId>{}(pond::platform::WindowId{42}) &&
           std::hash<pond::platform::DisplayId>{}(kDisplayId) ==
               std::hash<pond::platform::DisplayId>{}(pond::platform::DisplayId{42}) &&
           std::hash<pond::platform::DialogRequestId>{}(kDialogRequestId) ==
               std::hash<pond::platform::DialogRequestId>{}(pond::platform::DialogRequestId{42});
}

static_assert(IdentifierValueSemanticsAreConstexpr());
static_assert(IdentifierHashingIsConstexpr());
static_assert(!std::same_as<pond::platform::WindowId, pond::platform::DisplayId>);
static_assert(!std::same_as<pond::platform::WindowId, pond::platform::DialogRequestId>);
static_assert(!std::same_as<pond::platform::DisplayId, pond::platform::DialogRequestId>);
static_assert(!std::convertible_to<std::uint64_t, pond::platform::WindowId>);
static_assert(!std::convertible_to<pond::platform::WindowId, std::uint64_t>);
static_assert(!std::convertible_to<pond::platform::WindowId, pond::platform::DisplayId>);
static_assert(!std::convertible_to<std::uint64_t, pond::platform::DialogRequestId>);
static_assert(!std::convertible_to<pond::platform::DialogRequestId, std::uint64_t>);
static_assert(std::is_nothrow_constructible_v<pond::platform::WindowId, std::uint64_t>);
static_assert(std::is_nothrow_constructible_v<pond::platform::DisplayId, std::uint64_t>);
static_assert(std::is_nothrow_constructible_v<pond::platform::DialogRequestId, std::uint64_t>);
static_assert(std::is_trivially_copyable_v<pond::platform::WindowId>);
static_assert(std::is_trivially_copyable_v<pond::platform::DisplayId>);
static_assert(std::is_trivially_copyable_v<pond::platform::DialogRequestId>);

TEST(PlatformIdentifierTests, DefaultsToInvalidZero)
{
    const pond::platform::WindowId windowId;
    const pond::platform::DisplayId displayId;
    const pond::platform::DialogRequestId dialogRequestId;

    EXPECT_FALSE(windowId.IsValid());
    EXPECT_FALSE(displayId.IsValid());
    EXPECT_FALSE(dialogRequestId.IsValid());
    EXPECT_EQ(windowId.GetValue(), 0U);
    EXPECT_EQ(displayId.GetValue(), 0U);
    EXPECT_EQ(dialogRequestId.GetValue(), 0U);
}

TEST(PlatformIdentifierTests, ComparesByNumericValue)
{
    constexpr pond::platform::WindowId kFirst{1};
    constexpr pond::platform::WindowId kSecond{2};
    constexpr pond::platform::DialogRequestId kFirstDialogRequest{1};
    constexpr pond::platform::DialogRequestId kSecondDialogRequest{2};

    EXPECT_EQ(kFirst, pond::platform::WindowId{1});
    EXPECT_LT(kFirst, kSecond);
    EXPECT_EQ(kFirstDialogRequest, pond::platform::DialogRequestId{1});
    EXPECT_LT(kFirstDialogRequest, kSecondDialogRequest);
}

TEST(PlatformIdentifierTests, SupportsHashContainerLookup)
{
    std::unordered_set<pond::platform::WindowId> windowIds;
    std::unordered_set<pond::platform::DisplayId> displayIds;
    std::unordered_set<pond::platform::DialogRequestId> dialogRequestIds;

    windowIds.insert(pond::platform::WindowId{17});
    displayIds.insert(pond::platform::DisplayId{23});
    dialogRequestIds.insert(pond::platform::DialogRequestId{31});

    EXPECT_TRUE(windowIds.contains(pond::platform::WindowId{17}));
    EXPECT_TRUE(displayIds.contains(pond::platform::DisplayId{23}));
    EXPECT_TRUE(dialogRequestIds.contains(pond::platform::DialogRequestId{31}));
    EXPECT_FALSE(windowIds.contains(pond::platform::WindowId{18}));
    EXPECT_FALSE(displayIds.contains(pond::platform::DisplayId{24}));
    EXPECT_FALSE(dialogRequestIds.contains(pond::platform::DialogRequestId{32}));
}
} // namespace
