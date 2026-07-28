#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Runtime.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace
{
static_assert(!std::same_as<ponder::platform::DialogRequestId, ponder::platform::WindowId>);
static_assert(!std::same_as<ponder::platform::DialogRequestId, ponder::platform::DisplayId>);
static_assert(!std::convertible_to<std::uint64_t, ponder::platform::DialogRequestId>);
static_assert(!std::convertible_to<ponder::platform::DialogRequestId, std::uint64_t>);
static_assert(std::is_trivially_copyable_v<ponder::platform::DialogRequestId>);
static_assert(std::variant_size_v<ponder::platform::DialogOutcome> == 3U);
static_assert(std::is_scoped_enum_v<ponder::platform::DialogKind>);

TEST(DialogRequestIdTests, DefaultsToInvalidZero)
{
    constexpr ponder::platform::DialogRequestId kInvalid;

    EXPECT_FALSE(kInvalid.IsValid());
    EXPECT_EQ(kInvalid, ponder::platform::DialogRequestId::Invalid());
    EXPECT_EQ(kInvalid.GetValue(), 0U);
}

TEST(DialogRequestIdTests, ComparesAndHashesByNumericValue)
{
    constexpr ponder::platform::DialogRequestId kFirst{1};
    constexpr ponder::platform::DialogRequestId kSameFirst{1};
    constexpr ponder::platform::DialogRequestId kSecond{2};

    EXPECT_EQ(kFirst, kSameFirst);
    EXPECT_LT(kFirst, kSecond);

    std::unordered_set<ponder::platform::DialogRequestId> ids;
    ids.insert(kFirst);
    EXPECT_TRUE(ids.contains(kSameFirst));
    EXPECT_FALSE(ids.contains(kSecond));
}

TEST(DialogTypesTests, OwnDescriptorsAndOutcomes)
{
    const ponder::platform::DialogRequestInfo request{.id = ponder::platform::DialogRequestId{5},
                                                      .kind = ponder::platform::DialogKind::OpenFile,
                                                      .requestedAt = ponder::core::Timestamp{std::chrono::nanoseconds{123}},
                                                      .parentWindowId = ponder::platform::WindowId{7},
                                                      .filterCount = 2,
                                                      .allowMultipleSelection = true};
    EXPECT_EQ(request.id, ponder::platform::DialogRequestId{5});
    EXPECT_EQ(request.kind, ponder::platform::DialogKind::OpenFile);
    EXPECT_EQ(request.parentWindowId, ponder::platform::WindowId{7});
    EXPECT_EQ(request.filterCount, 2U);
    EXPECT_TRUE(request.allowMultipleSelection);

    ponder::platform::OpenFileDialogDesc openDesc{.parentWindowId = ponder::platform::WindowId{7},
                                                  .defaultLocation = std::filesystem::path{"C:/tmp/molecule.sdf"},
                                                  .filters = {{.name = "Molecules", .pattern = "sdf;mol"}},
                                                  .allowMultipleSelection = true};
    EXPECT_EQ(openDesc.parentWindowId, ponder::platform::WindowId{7});
    EXPECT_EQ(openDesc.defaultLocation, std::filesystem::path{"C:/tmp/molecule.sdf"});
    ASSERT_EQ(openDesc.filters.size(), 1U);
    EXPECT_EQ(openDesc.filters.front().name, "Molecules");
    EXPECT_EQ(openDesc.filters.front().pattern, "sdf;mol");
    EXPECT_TRUE(openDesc.allowMultipleSelection);

    const ponder::platform::DialogSelection selection{.paths = {std::filesystem::path{"C:/tmp/water.sdf"}}, .selectedFilterIndex = 0U};
    const ponder::platform::DialogOutcome outcome{selection};
    ASSERT_TRUE(std::holds_alternative<ponder::platform::DialogSelection>(outcome));
    EXPECT_EQ(std::get<ponder::platform::DialogSelection>(outcome), selection);

    const ponder::platform::DialogOutcome cancellation{ponder::platform::DialogCancellation{}};
    EXPECT_TRUE(std::holds_alternative<ponder::platform::DialogCancellation>(cancellation));

    std::string failureMessage{"asynchronous callback failure"};
    ponder::platform::DialogOutcome failureOutcome{ponder::platform::DialogFailure{
        ponder::core::Error{ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::BackendFailure), failureMessage}}};
    failureMessage.assign("mutated source");
    const auto& failure = std::get<ponder::platform::DialogFailure>(failureOutcome);
    EXPECT_EQ(failure.error.GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::BackendFailure));
    EXPECT_EQ(failure.error.GetMessage(), "asynchronous callback failure");

    const ponder::platform::DialogOutcome copiedFailure = failureOutcome;
    EXPECT_EQ(copiedFailure, failureOutcome);
}
} // namespace
