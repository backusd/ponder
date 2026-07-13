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
using pond::platform::detail::AbandonedProcessEntry;
using pond::platform::detail::BackendProcessExitKind;
using pond::platform::detail::BackendProcessExitStatus;
using pond::platform::detail::BackendProcessKillResult;
using pond::platform::detail::PlatformProcessBackend;
using pond::platform::detail::PlatformProcessReaperBackend;

struct FakeProcessHandle final
{
    BackendProcessExitStatus exitStatus{};
    std::atomic<bool> nonblockingWaitExits{true};
    std::atomic<int> nonblockingWaitFailuresRemaining{};
    std::atomic<int> blockingWaitFailuresRemaining{};
    BackendProcessKillResult gracefulKillResult{BackendProcessKillResult::Succeeded};
    BackendProcessKillResult forceKillResult{BackendProcessKillResult::Succeeded};
    std::atomic<int> waitCalls{};
    std::atomic<int> blockingWaitCalls{};
    std::atomic<int> nonblockingWaitCalls{};
    std::vector<bool> killForces;
    std::atomic<bool> destroyed{};
};

struct FakeProcessBackend final
{
    bool createFails{};
    BackendProcessExitStatus nextExitStatus{};
    bool nextNonblockingWaitExits{true};
    int nextNonblockingWaitFailures{};
    int nextBlockingWaitFailures{};
    BackendProcessKillResult nextGracefulKillResult{BackendProcessKillResult::Succeeded};
    BackendProcessKillResult nextForceKillResult{BackendProcessKillResult::Succeeded};
    std::atomic<int> destroyCalls{};
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
    FakeProcessHandle& handle = fake.handles.emplace_back();
    handle.exitStatus = fake.nextExitStatus;
    handle.nonblockingWaitExits.store(fake.nextNonblockingWaitExits, std::memory_order_relaxed);
    handle.nonblockingWaitFailuresRemaining.store(fake.nextNonblockingWaitFailures,
                                                  std::memory_order_relaxed);
    handle.blockingWaitFailuresRemaining.store(fake.nextBlockingWaitFailures,
                                               std::memory_order_relaxed);
    handle.gracefulKillResult = fake.nextGracefulKillResult;
    handle.forceKillResult = fake.nextForceKillResult;
    return &handle;
}

[[nodiscard]] bool FakeWaitProcess(void*, void* process, bool block,
                                   BackendProcessExitStatus* status)
{
    FakeProcessHandle& fakeProcess = GetFakeProcess(process);
    fakeProcess.waitCalls.fetch_add(1, std::memory_order_relaxed);
    if (block)
    {
        fakeProcess.blockingWaitCalls.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        fakeProcess.nonblockingWaitCalls.fetch_add(1, std::memory_order_relaxed);
    }

    std::atomic<int>& failuresRemaining = block ? fakeProcess.blockingWaitFailuresRemaining
                                                : fakeProcess.nonblockingWaitFailuresRemaining;
    int failureCount = failuresRemaining.load(std::memory_order_relaxed);
    while (failureCount > 0 && !failuresRemaining.compare_exchange_weak(
                                   failureCount, failureCount - 1, std::memory_order_relaxed))
    {
    }
    if (failureCount > 0)
    {
        static_cast<void>(SDL_SetError("synthetic wait failure"));
        return false;
    }
    if (!block && !fakeProcess.nonblockingWaitExits.load(std::memory_order_acquire))
    {
        SDL_ClearError();
        return false;
    }

    if (status != nullptr)
    {
        *status = fakeProcess.exitStatus;
    }
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
    fake.destroyCalls.fetch_add(1, std::memory_order_relaxed);
    GetFakeProcess(process).destroyed.store(true, std::memory_order_release);
}

[[nodiscard]] PlatformProcessBackend MakeFakeBackend(FakeProcessBackend& fake)
{
    return PlatformProcessBackend{.context = &fake,
                                  .create = FakeCreateProcess,
                                  .wait = FakeWaitProcess,
                                  .kill = FakeKillProcess,
                                  .destroy = FakeDestroyProcess};
}

struct FakeProcessReaper final
{
    bool startSucceeds{true};
    int ensureCalls{};
    std::atomic<AbandonedProcessEntry*> enqueued{};
};

[[nodiscard]] bool FakeEnsureProcessReaper(void* context) noexcept
{
    auto& fake = *static_cast<FakeProcessReaper*>(context);
    ++fake.ensureCalls;
    return fake.startSucceeds;
}

void FakeEnqueueAbandonedProcess(void* context, AbandonedProcessEntry* entry) noexcept
{
    auto& fake = *static_cast<FakeProcessReaper*>(context);
    fake.enqueued.store(entry, std::memory_order_release);
}

[[nodiscard]] PlatformProcessReaperBackend MakeFakeReaperBackend(FakeProcessReaper& fake)
{
    return PlatformProcessReaperBackend{.context = &fake,
                                        .ensureStarted = FakeEnsureProcessReaper,
                                        .enqueue = FakeEnqueueAbandonedProcess};
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

template <typename Predicate>
[[nodiscard]] bool WaitUntil(Predicate predicate, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    return predicate();
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
        EXPECT_EQ(fake.destroyCalls.load(std::memory_order_relaxed), 0);
    }

    EXPECT_EQ(fake.destroyCalls.load(std::memory_order_relaxed), 1);
    ASSERT_FALSE(fake.handles.empty());
    EXPECT_TRUE(fake.handles.front().destroyed.load(std::memory_order_acquire));
    EXPECT_TRUE(fake.handles.front().killForces.empty());
}

TEST(ProcessBackendTests, AbandonedRunningProcessIsReapedAsynchronously)
{
    FakeProcessBackend fake;
    fake.nextNonblockingWaitExits = false;
    FakeProcessHandle* handle{};

    {
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        handle = &fake.handles.front();
        pond::platform::Process process = std::move(result).GetValue();
    }

    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->nonblockingWaitCalls.load(std::memory_order_relaxed), 1);
    EXPECT_FALSE(handle->destroyed.load(std::memory_order_acquire));
    EXPECT_TRUE(WaitUntil(
        [handle]()
        {
            return handle->nonblockingWaitCalls.load(std::memory_order_acquire) > 1;
        },
        std::chrono::seconds{1}));
    EXPECT_FALSE(handle->destroyed.load(std::memory_order_acquire));

    handle->nonblockingWaitExits.store(true, std::memory_order_release);
    EXPECT_TRUE(WaitUntil(
        [handle]()
        {
            return handle->destroyed.load(std::memory_order_acquire);
        },
        std::chrono::seconds{1}));
    EXPECT_EQ(handle->blockingWaitCalls.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(fake.destroyCalls.load(std::memory_order_relaxed), 1);
}

TEST(ProcessBackendTests, ReaperPollsLaterProcessesWithoutWaitingForEarlierOnes)
{
    FakeProcessBackend fake;
    fake.nextNonblockingWaitExits = false;
    auto firstResult =
        pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(firstResult.HasValue()) << firstResult.GetError().GetMessage();
    FakeProcessHandle* const first = &fake.handles.back();

    fake.nextNonblockingWaitExits = true;
    fake.nextNonblockingWaitFailures = 1;
    auto secondResult =
        pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(secondResult.HasValue()) << secondResult.GetError().GetMessage();
    FakeProcessHandle* const second = &fake.handles.back();

    {
        pond::platform::Process firstProcess = std::move(firstResult).GetValue();
        pond::platform::Process secondProcess = std::move(secondResult).GetValue();
    }

    EXPECT_TRUE(WaitUntil(
        [second]()
        {
            return second->destroyed.load(std::memory_order_acquire);
        },
        std::chrono::seconds{1}));
    EXPECT_FALSE(first->destroyed.load(std::memory_order_acquire));

    first->nonblockingWaitExits.store(true, std::memory_order_release);
    EXPECT_TRUE(WaitUntil(
        [first]()
        {
            return first->destroyed.load(std::memory_order_acquire);
        },
        std::chrono::seconds{1}));
    EXPECT_EQ(fake.destroyCalls.load(std::memory_order_relaxed), 2);
}

TEST(ProcessBackendTests, RejectsReaperStartupFailureBeforeCreatingChild)
{
    FakeProcessBackend fake;
    FakeProcessReaper reaper{.startSucceeds = false};

    auto result = pond::platform::detail::LaunchProcess(
        MakeFakeProcessDesc(), MakeFakeBackend(fake), MakeFakeReaperBackend(reaper));

    ASSERT_FALSE(result.HasValue());
    ExpectErrorCode(result.GetError(), pond::platform::PlatformErrorCode::BackendFailure);
    EXPECT_EQ(reaper.ensureCalls, 1);
    EXPECT_EQ(reaper.enqueued.load(std::memory_order_acquire), nullptr);
    EXPECT_TRUE(fake.launches.empty());
    EXPECT_TRUE(fake.handles.empty());
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
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{
                                    .kind = BackendProcessExitKind::Normal, .value = 7U}};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(*wait));
        EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(*wait).exitCode, 7U);
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{
                                    .kind = BackendProcessExitKind::Normal, .value = 0x80000000U}};
        auto result =
            pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(*wait));
        EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(*wait).exitCode, 0x80000000U);
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{
                                    .kind = BackendProcessExitKind::Signal, .value = 15U}};
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
        FakeProcessBackend fake{
            .nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Unknown}};
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
        FakeProcessBackend fake{.nextBlockingWaitFailures = 1};
        {
            auto result =
                pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
            ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
            pond::platform::Process process = std::move(result).GetValue();

            auto wait = process.Wait();
            ASSERT_FALSE(wait.HasValue());
            ExpectErrorCode(wait.GetError(), pond::platform::PlatformErrorCode::BackendFailure);
        }
        EXPECT_TRUE(WaitUntil(
            [&fake]()
            {
                return fake.destroyCalls.load(std::memory_order_acquire) == 1;
            },
            std::chrono::seconds{1}));
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

TEST(ProcessBackendTests, RejectsInvalidTerminationModeBeforeCallingBackend)
{
    FakeProcessBackend fake;
    auto result =
        pond::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    pond::platform::Process process = std::move(result).GetValue();

    const auto invalidMode = static_cast<pond::platform::ProcessTerminationMode>(0xFF);
    auto termination = process.Terminate(invalidMode);

    ASSERT_FALSE(termination.HasValue());
    ExpectErrorCode(termination.GetError(), pond::platform::PlatformErrorCode::InvalidArgument);
    EXPECT_TRUE(fake.handles.front().killForces.empty());
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
    EXPECT_EQ(fake.destroyCalls.load(std::memory_order_relaxed), 1);
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
    EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(*wait).exitCode, 23U);
    EXPECT_EQ(ReadLines(argumentsFile.GetPath()),
              (std::vector<std::string>{"alpha beta", nonAsciiArgument}));
}

#if defined(_WIN32)
TEST(ProcessBackendTests, PreservesHighBitWindowsExitStatusAsNormalExit)
{
    auto result = pond::platform::LaunchProcess(pond::platform::ProcessDesc{
        .executable = GetHelperPath(), .arguments = {"--exit-code", "-2147483648"}});
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    pond::platform::Process process = std::move(result).GetValue();

    auto wait = process.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(*wait));
    EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(*wait).exitCode, 0x80000000U);
}
#endif

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
#if !defined(_WIN32)
    EXPECT_TRUE(std::holds_alternative<pond::platform::ProcessSignalTermination>(*wait));
#endif
}
} // namespace
