#include <ponder/core/PonderException.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Process.hpp>

#include <SDL3/SDL_error.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "ProcessBackend.hpp"

#ifndef PONDER_PLATFORM_PROCESS_HELPER_PATH
#error "PONDER_PLATFORM_PROCESS_HELPER_PATH must name the process test helper."
#endif

namespace
{
using pond::platform::detail::BackendProcessKillResult;
using pond::platform::detail::PlatformProcessBackend;

struct FakeProcessHandle final
{
    int exitCode{};
    bool waitSucceeds{true};
    BackendProcessKillResult gracefulKillResult{BackendProcessKillResult::Succeeded};
    BackendProcessKillResult forceKillResult{BackendProcessKillResult::Succeeded};
    int waitCalls{};
    std::vector<bool> killForces;
    bool destroyed{};
};

struct FakeProcessBackend final
{
    bool createFails{};
    int nextExitCode{};
    bool nextWaitSucceeds{true};
    BackendProcessKillResult nextGracefulKillResult{BackendProcessKillResult::Succeeded};
    BackendProcessKillResult nextForceKillResult{BackendProcessKillResult::Succeeded};
    int destroyCalls{};
    std::vector<std::vector<std::string>> launches;
    std::list<FakeProcessHandle> handles;
};

[[nodiscard]] FakeProcessBackend& GetFake(void* context)
{
    return *static_cast<FakeProcessBackend*>(context);
}

[[nodiscard]] FakeProcessHandle& GetFakeProcess(void* process)
{
    return *static_cast<FakeProcessHandle*>(process);
}

[[nodiscard]] void* FakeCreateProcess(void* context, const char* const* arguments)
{
    FakeProcessBackend& fake = GetFake(context);
    if (fake.createFails)
    {
        static_cast<void>(SDL_SetError("synthetic create failure"));
        return nullptr;
    }

    std::vector<std::string> copiedArguments;
    for (const char* const* current = arguments; *current != nullptr; ++current)
    {
        copiedArguments.emplace_back(*current);
    }
    fake.launches.push_back(std::move(copiedArguments));
    fake.handles.push_back(FakeProcessHandle{.exitCode = fake.nextExitCode,
                                             .waitSucceeds = fake.nextWaitSucceeds,
                                             .gracefulKillResult = fake.nextGracefulKillResult,
                                             .forceKillResult = fake.nextForceKillResult});
    return &fake.handles.back();
}

[[nodiscard]] bool FakeWaitProcess(void*, void* process, bool block, int* exitCode)
{
    EXPECT_TRUE(block);
    FakeProcessHandle& fakeProcess = GetFakeProcess(process);
    ++fakeProcess.waitCalls;
    if (!fakeProcess.waitSucceeds)
    {
        static_cast<void>(SDL_SetError("synthetic wait failure"));
        return false;
    }

    *exitCode = fakeProcess.exitCode;
    return true;
}

[[nodiscard]] BackendProcessKillResult FakeKillProcess(void*, void* process, bool force)
{
    FakeProcessHandle& fakeProcess = GetFakeProcess(process);
    fakeProcess.killForces.push_back(force);
    const BackendProcessKillResult result =
        force ? fakeProcess.forceKillResult : fakeProcess.gracefulKillResult;
    if (result == BackendProcessKillResult::Failed)
    {
        static_cast<void>(SDL_SetError("synthetic kill failure"));
    }
    return result;
}

void FakeDestroyProcess(void* context, void* process) noexcept
{
    FakeProcessBackend& fake = GetFake(context);
    ++fake.destroyCalls;
    GetFakeProcess(process).destroyed = true;
}

[[nodiscard]] PlatformProcessBackend MakeFakeBackend(FakeProcessBackend& fake)
{
    return PlatformProcessBackend{.context = &fake,
                                  .create = FakeCreateProcess,
                                  .wait = FakeWaitProcess,
                                  .kill = FakeKillProcess,
                                  .destroy = FakeDestroyProcess};
}

[[nodiscard]] pond::platform::ProcessDesc MakeFakeProcessDesc()
{
    return pond::platform::ProcessDesc{
        .executable = std::filesystem::path{"C:/tools/ponder-helper.exe"}, .arguments = {}};
}

[[nodiscard]] std::filesystem::path MakeTemporaryPath(std::string_view label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "ponder-platform-process-" + std::string{label} + "-" + std::to_string(ticks) + ".txt";
    return path;
}

class ScopedTemporaryFile final
{
public:
    explicit ScopedTemporaryFile(std::filesystem::path path) : m_path(std::move(path))
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }

    ~ScopedTemporaryFile()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }

    ScopedTemporaryFile(const ScopedTemporaryFile&) = delete;
    ScopedTemporaryFile& operator=(const ScopedTemporaryFile&) = delete;
    ScopedTemporaryFile(ScopedTemporaryFile&&) = delete;
    ScopedTemporaryFile& operator=(ScopedTemporaryFile&&) = delete;

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

[[nodiscard]] bool WaitForFile(const std::filesystem::path& path, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (std::filesystem::exists(path))
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    return std::filesystem::exists(path);
}

[[nodiscard]] std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line))
    {
        lines.push_back(line);
    }
    return lines;
}

void ExpectErrorCode(const pond::core::Error& error, pond::platform::PlatformErrorCode code)
{
    EXPECT_EQ(error.GetCode(), pond::platform::ToErrorCode(code));
}

[[nodiscard]] std::filesystem::path GetHelperPath()
{
    return std::filesystem::path{PONDER_PLATFORM_PROCESS_HELPER_PATH};
}

TEST(ProcessBackendTests, BuildsShellFreeArgumentVectorAndDestroysTrackingState)
{
    FakeProcessBackend fake;
    const pond::platform::ProcessDesc desc{
        .executable = std::filesystem::path{"C:/Program Files/ponder helper.exe"},
        .arguments = {"alpha beta", std::string{"angstrom-\xC3\x85"}}};

    {
        auto result = pond::platform::detail::LaunchProcess(desc, MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        ASSERT_EQ(fake.launches.size(), 1U);
        EXPECT_EQ(fake.launches.front(),
                  (std::vector<std::string>{"C:/Program Files/ponder helper.exe", "alpha beta",
                                            std::string{"angstrom-\xC3\x85"}}));
        EXPECT_EQ(fake.destroyCalls, 0);
    }

    EXPECT_EQ(fake.destroyCalls, 1);
    ASSERT_FALSE(fake.handles.empty());
    EXPECT_TRUE(fake.handles.front().destroyed);
    EXPECT_TRUE(fake.handles.front().killForces.empty());
}

TEST(ProcessBackendTests, RejectsInvalidDescriptorsBeforeBackendLaunch)
{
    FakeProcessBackend fake;

    auto emptyExecutable =
        pond::platform::detail::LaunchProcess(pond::platform::ProcessDesc{}, MakeFakeBackend(fake));
    ASSERT_FALSE(emptyExecutable.HasValue());
    ExpectErrorCode(emptyExecutable.GetError(), pond::platform::PlatformErrorCode::InvalidArgument);

    auto invalidArgument = pond::platform::detail::LaunchProcess(
        pond::platform::ProcessDesc{.executable = std::filesystem::path{"helper"},
                                    .arguments = {std::string{"bad\0argument", 12}}},
        MakeFakeBackend(fake));
    ASSERT_FALSE(invalidArgument.HasValue());
    ExpectErrorCode(invalidArgument.GetError(), pond::platform::PlatformErrorCode::InvalidArgument);

    EXPECT_TRUE(fake.launches.empty());
}

TEST(ProcessBackendTests, MapsWaitExitStatusAlternatives)
{
    {
        FakeProcessBackend fake{.nextExitCode = 7};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(*wait));
        EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(*wait).exitCode, 7);
    }

    {
        FakeProcessBackend fake{.nextExitCode = -15};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessSignalTermination>(*wait));
        EXPECT_EQ(std::get<pond::platform::ProcessSignalTermination>(*wait).signal, 15);
    }

    {
        FakeProcessBackend fake{.nextExitCode = -255};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        EXPECT_TRUE(std::holds_alternative<pond::platform::ProcessUnknownTermination>(*wait));
    }
}

TEST(ProcessBackendTests, ReportsWaitLaunchAndTerminationFailures)
{
    {
        FakeProcessBackend fake{.createFails = true};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_FALSE(result.HasValue());
        ExpectErrorCode(result.GetError(), pond::platform::PlatformErrorCode::BackendFailure);
    }

    {
        FakeProcessBackend fake{.nextWaitSucceeds = false};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_FALSE(wait.HasValue());
        ExpectErrorCode(wait.GetError(), pond::platform::PlatformErrorCode::BackendFailure);
    }

    {
        FakeProcessBackend fake{.nextForceKillResult = BackendProcessKillResult::Failed};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto termination = process.Terminate(pond::platform::ProcessTerminationMode::Force);
        ASSERT_FALSE(termination.HasValue());
        ExpectErrorCode(termination.GetError(), pond::platform::PlatformErrorCode::BackendFailure);
    }

    {
        FakeProcessBackend fake{.nextForceKillResult = BackendProcessKillResult::Unsupported};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto termination = process.Terminate(pond::platform::ProcessTerminationMode::Force);
        ASSERT_FALSE(termination.HasValue());
        ExpectErrorCode(termination.GetError(), pond::platform::PlatformErrorCode::Unsupported);
    }
}

TEST(ProcessBackendTests, GracefulPreferredFallsBackToForceWhenUnsupported)
{
    FakeProcessBackend fake{.nextGracefulKillResult = BackendProcessKillResult::Unsupported,
                            .nextForceKillResult = BackendProcessKillResult::Succeeded};
    auto result =
        pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    pond::platform::Process process = std::move(result).GetValue();

    auto termination = process.Terminate(pond::platform::ProcessTerminationMode::GracefulPreferred);
    ASSERT_TRUE(termination.HasValue()) << termination.GetError().GetMessage();
    ASSERT_FALSE(fake.handles.empty());
    EXPECT_EQ(fake.handles.front().killForces, (std::vector<bool>{false, true}));
}

TEST(ProcessBackendTests, EnforcesLaunchingThreadAffinityAfterMove)
{
    FakeProcessBackend fake;
    auto result =
        pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    std::optional<pond::platform::Process> process{std::move(result).GetValue()};

    std::atomic<bool> caughtVerifyFailure{};
    std::thread worker{[&process, &caughtVerifyFailure]()
                       {
                           try
                           {
                               static_cast<void>(process->Wait());
                           }
                           catch (const pond::core::PonderException&)
                           {
                               caughtVerifyFailure.store(true);
                           }
                       }};
    worker.join();

    EXPECT_TRUE(caughtVerifyFailure.load());
    process.reset();
    EXPECT_EQ(fake.destroyCalls, 1);
}

TEST(ProcessBackendTests, HelperPreservesArgumentsAndReportsExitCode)
{
    ScopedTemporaryFile argumentsFile{MakeTemporaryPath("args")};
    const std::string nonAsciiArgument{"angstrom-\xC3\x85"};
    auto result = pond::platform::LaunchProcess(pond::platform::ProcessDesc{
        .executable = GetHelperPath(),
        .arguments = {"--write-args", argumentsFile.GetPath().string(), "--exit-code", "23", "--",
                      "alpha beta", nonAsciiArgument}});
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    pond::platform::Process process = std::move(result).GetValue();

    auto wait = process.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(*wait));
    EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(*wait).exitCode, 23);
    EXPECT_EQ(ReadLines(argumentsFile.GetPath()),
              (std::vector<std::string>{"alpha beta", nonAsciiArgument}));
}

TEST(ProcessBackendTests, DestroyingProcessDoesNotTerminateHelper)
{
    ScopedTemporaryFile completionFile{MakeTemporaryPath("completion")};
    {
        auto result = pond::platform::LaunchProcess(
            pond::platform::ProcessDesc{.executable = GetHelperPath(),
                                        .arguments = {"--sleep-ms", "250", "--touch-after-sleep",
                                                      completionFile.GetPath().string()}});
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();
    }

    EXPECT_TRUE(WaitForFile(completionFile.GetPath(), std::chrono::seconds{5}));
}

TEST(ProcessBackendTests, ForceTerminationStopsRunningHelper)
{
    ScopedTemporaryFile readyFile{MakeTemporaryPath("ready")};
    auto result = pond::platform::LaunchProcess(
        pond::platform::ProcessDesc{.executable = GetHelperPath(),
                                    .arguments = {"--touch-ready", readyFile.GetPath().string(),
                                                  "--sleep-ms", "5000", "--exit-code", "99"}});
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    pond::platform::Process process = std::move(result).GetValue();
    ASSERT_TRUE(WaitForFile(readyFile.GetPath(), std::chrono::seconds{2}));

    auto termination = process.Terminate(pond::platform::ProcessTerminationMode::Force);
    ASSERT_TRUE(termination.HasValue()) << termination.GetError().GetMessage();

    auto wait = process.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
}
} // namespace
