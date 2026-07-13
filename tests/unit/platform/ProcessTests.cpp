#include <ponder/platform/Process.hpp>

#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{
static_assert(!std::is_copy_constructible_v<pond::platform::Process>);
static_assert(!std::is_copy_assignable_v<pond::platform::Process>);
static_assert(std::is_nothrow_move_constructible_v<pond::platform::Process>);
static_assert(std::is_nothrow_move_assignable_v<pond::platform::Process>);
static_assert(std::is_nothrow_destructible_v<pond::platform::Process>);
static_assert(std::variant_size_v<pond::platform::ProcessExitStatus> == 3U);
static_assert(
    std::is_same_v<decltype(pond::platform::ProcessNormalExit{}.exitCode), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<pond::platform::Process&>().Wait()),
                             pond::core::Result<pond::platform::ProcessExitStatus>>);
static_assert(std::is_same_v<decltype(std::declval<pond::platform::Process&>().Terminate(
                                 pond::platform::ProcessTerminationMode::Force)),
                             pond::core::VoidResult>);

TEST(ProcessTypesTests, OwnsDescriptorArguments)
{
    const pond::platform::ProcessDesc desc{
        .executable = std::filesystem::path{"C:/Program Files/ponder/helper.exe"},
        .arguments = {"alpha beta", std::string{"angstrom-\xC3\x85"}}};

    EXPECT_EQ(desc.executable, std::filesystem::path{"C:/Program Files/ponder/helper.exe"});
    ASSERT_EQ(desc.arguments.size(), 2U);
    EXPECT_EQ(desc.arguments[0], "alpha beta");
    EXPECT_EQ(desc.arguments[1], std::string{"angstrom-\xC3\x85"});
}

TEST(ProcessTypesTests, RepresentsDistinctExitOutcomes)
{
    const pond::platform::ProcessExitStatus normal{
        pond::platform::ProcessNormalExit{.exitCode = 7U}};
    ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(normal));
    EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(normal).exitCode, 7U);

    const pond::platform::ProcessExitStatus highBitNormal{
        pond::platform::ProcessNormalExit{.exitCode = 0x80000000U}};
    ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(highBitNormal));
    EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(highBitNormal).exitCode, 0x80000000U);

    const pond::platform::ProcessExitStatus signaled{
        pond::platform::ProcessSignalTermination{.signal = 15}};
    ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessSignalTermination>(signaled));
    EXPECT_EQ(std::get<pond::platform::ProcessSignalTermination>(signaled).signal, 15);

    const pond::platform::ProcessExitStatus unknown{pond::platform::ProcessUnknownTermination{}};
    EXPECT_TRUE(std::holds_alternative<pond::platform::ProcessUnknownTermination>(unknown));
}
} // namespace
