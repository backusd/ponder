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
static_assert(!std::is_copy_constructible_v<ponder::platform::Process>);
static_assert(!std::is_copy_assignable_v<ponder::platform::Process>);
static_assert(std::is_nothrow_move_constructible_v<ponder::platform::Process>);
static_assert(std::is_nothrow_move_assignable_v<ponder::platform::Process>);
static_assert(std::is_nothrow_destructible_v<ponder::platform::Process>);
static_assert(std::variant_size_v<ponder::platform::ProcessExitStatus> == 3U);
static_assert(std::is_same_v<decltype(ponder::platform::ProcessNormalExit{}.exitCode), std::uint32_t>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Process&>().Wait()), ponder::core::Result<ponder::platform::ProcessExitStatus>>);
static_assert(std::is_same_v<decltype(std::declval<ponder::platform::Process&>().Terminate(ponder::platform::ProcessTerminationMode::Force)),
                             ponder::core::VoidResult>);
static_assert(std::is_same_v<decltype(ponder::platform::LaunchProcess(std::declval<const ponder::platform::ProcessDesc&>())),
                             ponder::core::Result<ponder::platform::Process>>);

TEST(ProcessTypesTests, OwnsDescriptorArguments)
{
    const ponder::platform::ProcessDesc desc{.executable = std::filesystem::path{"C:/Program Files/ponder/helper.exe"},
                                             .arguments = {"alpha beta", std::string{"angstrom-\xC3\x85"}}};

    EXPECT_EQ(desc.executable, std::filesystem::path{"C:/Program Files/ponder/helper.exe"});
    ASSERT_EQ(desc.arguments.size(), 2U);
    EXPECT_EQ(desc.arguments[0], "alpha beta");
    EXPECT_EQ(desc.arguments[1], std::string{"angstrom-\xC3\x85"});
}

TEST(ProcessTypesTests, RepresentsDistinctExitOutcomes)
{
    const ponder::platform::ProcessExitStatus normal{ponder::platform::ProcessNormalExit{.exitCode = 7U}};
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(normal));
    EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(normal).exitCode, 7U);

    const ponder::platform::ProcessExitStatus highBitNormal{ponder::platform::ProcessNormalExit{.exitCode = 0x80000000U}};
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(highBitNormal));
    EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(highBitNormal).exitCode, 0x80000000U);

    const ponder::platform::ProcessExitStatus signaled{ponder::platform::ProcessSignalTermination{.signal = 15}};
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessSignalTermination>(signaled));
    EXPECT_EQ(std::get<ponder::platform::ProcessSignalTermination>(signaled).signal, 15);

    const ponder::platform::ProcessExitStatus unknown{ponder::platform::ProcessUnknownTermination{}};
    EXPECT_TRUE(std::holds_alternative<ponder::platform::ProcessUnknownTermination>(unknown));
}
} // namespace
