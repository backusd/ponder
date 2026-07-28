#include <ponder/core/Exception.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Process.hpp>

#include <SDL3/SDL_error.h>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <list>
#include <new>
#include <optional>
#include <stdexcept>
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
using ponder::platform::detail::AbandonedProcessEntry;
using ponder::platform::detail::BackendProcessExitKind;
using ponder::platform::detail::BackendProcessExitStatus;
using ponder::platform::detail::BackendProcessKillResult;
using ponder::platform::detail::PlatformProcessBackend;
using ponder::platform::detail::PlatformProcessReaperBackend;

enum class FakeWaitException : std::uint8_t
{
    Project,
    Standard,
    Allocation,
    Unknown
};

struct FakeProcessHandle final
{
    BackendProcessExitStatus exitStatus{};
    std::atomic<bool> nonblockingWaitExits{true};
    std::atomic<int> nonblockingWaitFailuresRemaining{};
    std::atomic<int> nonblockingWaitExceptionsRemaining{};
    std::atomic<FakeWaitException> nonblockingWaitException{FakeWaitException::Standard};
    std::atomic<int> blockingWaitFailuresRemaining{};
    std::atomic<int> blockingWaitExceptionsRemaining{};
    std::atomic<FakeWaitException> blockingWaitException{FakeWaitException::Standard};
    std::atomic<int> killExceptionsRemaining{};
    bool writesWaitStatus{true};
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
    int createExceptionsRemaining{};
    BackendProcessExitStatus nextExitStatus{};
    bool nextNonblockingWaitExits{true};
    int nextNonblockingWaitFailures{};
    int nextNonblockingWaitExceptions{};
    FakeWaitException nextNonblockingWaitException{FakeWaitException::Standard};
    int nextBlockingWaitFailures{};
    int nextBlockingWaitExceptions{};
    FakeWaitException nextBlockingWaitException{FakeWaitException::Standard};
    bool nextWritesWaitStatus{true};
    BackendProcessKillResult nextGracefulKillResult{BackendProcessKillResult::Succeeded};
    BackendProcessKillResult nextForceKillResult{BackendProcessKillResult::Succeeded};
    int nextKillExceptions{};
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

[[nodiscard]] bool ConsumeFailure(std::atomic<int>& failuresRemaining) noexcept
{
    int failureCount = failuresRemaining.load(std::memory_order_relaxed);
    while (failureCount > 0 && !failuresRemaining.compare_exchange_weak(failureCount, failureCount - 1, std::memory_order_relaxed))
    {
    }
    return failureCount > 0;
}

[[nodiscard]] void* FakeCreateProcess(void* context, const char* const* arguments)
{
    FakeProcessBackend& fake = GetFake(context);
    if (fake.createExceptionsRemaining > 0)
    {
        --fake.createExceptionsRemaining;
        throw std::runtime_error{"synthetic create exception"};
    }
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
    handle.nonblockingWaitFailuresRemaining.store(fake.nextNonblockingWaitFailures, std::memory_order_relaxed);
    handle.nonblockingWaitExceptionsRemaining.store(fake.nextNonblockingWaitExceptions, std::memory_order_relaxed);
    handle.nonblockingWaitException.store(fake.nextNonblockingWaitException, std::memory_order_relaxed);
    handle.blockingWaitFailuresRemaining.store(fake.nextBlockingWaitFailures, std::memory_order_relaxed);
    handle.blockingWaitExceptionsRemaining.store(fake.nextBlockingWaitExceptions, std::memory_order_relaxed);
    handle.blockingWaitException.store(fake.nextBlockingWaitException, std::memory_order_relaxed);
    handle.writesWaitStatus = fake.nextWritesWaitStatus;
    handle.gracefulKillResult = fake.nextGracefulKillResult;
    handle.forceKillResult = fake.nextForceKillResult;
    handle.killExceptionsRemaining.store(fake.nextKillExceptions, std::memory_order_relaxed);
    return &handle;
}

[[nodiscard]] bool FakeWaitProcess(void*, void* process, bool block, BackendProcessExitStatus* status)
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

    std::atomic<int>& failuresRemaining = block ? fakeProcess.blockingWaitFailuresRemaining : fakeProcess.nonblockingWaitFailuresRemaining;
    if (ConsumeFailure(failuresRemaining))
    {
        static_cast<void>(SDL_SetError("synthetic wait failure"));
        return false;
    }

    std::atomic<int>& exceptionsRemaining = block ? fakeProcess.blockingWaitExceptionsRemaining : fakeProcess.nonblockingWaitExceptionsRemaining;
    if (ConsumeFailure(exceptionsRemaining))
    {
        const FakeWaitException exception = block ? fakeProcess.blockingWaitException.load(std::memory_order_relaxed)
                                                  : fakeProcess.nonblockingWaitException.load(std::memory_order_relaxed);
        switch (exception)
        {
        case FakeWaitException::Project:
            throw PLATFORM_EXCEPTION(ponder::platform::PlatformErrorCode::BackendFailure, "Synthetic project process wait failure.");
        case FakeWaitException::Standard:
            throw std::runtime_error{"synthetic wait exception"};
        case FakeWaitException::Allocation:
            throw std::bad_alloc{};
        case FakeWaitException::Unknown:
            throw 37;
        }
    }
    if (!block && !fakeProcess.nonblockingWaitExits.load(std::memory_order_acquire))
    {
        SDL_ClearError();
        return false;
    }

    if (status != nullptr && fakeProcess.writesWaitStatus)
    {
        *status = fakeProcess.exitStatus;
    }
    return true;
}

[[nodiscard]] BackendProcessKillResult FakeKillProcess(void*, void* process, bool force)
{
    FakeProcessHandle& fakeProcess = GetFakeProcess(process);
    fakeProcess.killForces.push_back(force);
    if (ConsumeFailure(fakeProcess.killExceptionsRemaining))
    {
        throw std::runtime_error{"synthetic kill exception"};
    }
    const BackendProcessKillResult result = force ? fakeProcess.forceKillResult : fakeProcess.gracefulKillResult;
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
    return PlatformProcessReaperBackend{.context = &fake, .ensureStarted = FakeEnsureProcessReaper, .enqueue = FakeEnqueueAbandonedProcess};
}

[[nodiscard]] ponder::platform::ProcessDesc MakeFakeProcessDesc()
{
    return ponder::platform::ProcessDesc{.executable = std::filesystem::path{"C:/tools/ponder-helper.exe"}, .arguments = {}};
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
    explicit ScopedTemporaryFile(std::filesystem::path path) :
        m_path(std::move(path))
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

void ExpectErrorCode(const ponder::core::Error& error, ponder::platform::PlatformErrorCode code)
{
    EXPECT_EQ(error.GetCode(), ponder::platform::ToErrorCode(code));
}

template <typename Action>
void ExpectPlatformException(Action&& action, ponder::platform::PlatformErrorCode expectedCode)
{
    try
    {
        action();
        FAIL() << "Expected ponder::core::Exception";
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(exception.GetMessage().starts_with(std::format("Platform error [{}]: ", expectedCode)));
    }
    catch (...)
    {
        FAIL() << "Expected ponder::core::Exception";
    }
}

[[nodiscard]] std::filesystem::path GetHelperPath()
{
    return std::filesystem::path{PONDER_PLATFORM_PROCESS_HELPER_PATH};
}

TEST(ProcessBackendTests, BuildsShellFreeArgumentVectorAndDestroysTrackingState)
{
    FakeProcessBackend fake;
    const ponder::platform::ProcessDesc desc{.executable = std::filesystem::path{"C:/Program Files/ponder helper.exe"},
                                             .arguments = {"alpha beta", std::string{"angstrom-\xC3\x85"}}};

    {
        auto result = ponder::platform::detail::LaunchProcess(desc, MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();

        ASSERT_EQ(fake.launches.size(), 1U);
        EXPECT_EQ(fake.launches.front(),
                  (std::vector<std::string>{"C:/Program Files/ponder helper.exe", "alpha beta", std::string{"angstrom-\xC3\x85"}}));
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
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        handle = &fake.handles.front();
        ponder::platform::Process process = std::move(result).GetValue();
    }

    ASSERT_NE(handle, nullptr);
    EXPECT_GE(handle->nonblockingWaitCalls.load(std::memory_order_relaxed), 1);
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
    auto firstResult = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(firstResult.HasValue()) << firstResult.GetError().GetMessage();
    FakeProcessHandle* const first = &fake.handles.back();

    fake.nextNonblockingWaitExits = true;
    fake.nextNonblockingWaitFailures = 1;
    auto secondResult = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(secondResult.HasValue()) << secondResult.GetError().GetMessage();
    FakeProcessHandle* const second = &fake.handles.back();

    {
        ponder::platform::Process firstProcess = std::move(firstResult).GetValue();
        ponder::platform::Process secondProcess = std::move(secondResult).GetValue();
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

TEST(ProcessBackendTests, ReaperContainsBackendWaitExceptionsAndRetriesTheSameHandle)
{
    struct WaitFailureScenario final
    {
        FakeWaitException exception;
        std::string_view name;
    };

    constexpr std::array scenarios{WaitFailureScenario{FakeWaitException::Project, "project exception"},
                                   WaitFailureScenario{FakeWaitException::Standard, "standard exception"},
                                   WaitFailureScenario{FakeWaitException::Allocation, "allocation failure"},
                                   WaitFailureScenario{FakeWaitException::Unknown, "unknown exception"}};

    for (const WaitFailureScenario& scenario : scenarios)
    {
        SCOPED_TRACE(std::string{scenario.name});
        FakeProcessBackend fake{.nextNonblockingWaitExceptions = 1, .nextNonblockingWaitException = scenario.exception};
        FakeProcessHandle* handle{};

        {
            auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
            ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
            handle = &fake.handles.front();
            ponder::platform::Process process = std::move(result).GetValue();
        }

        ASSERT_NE(handle, nullptr);
        ASSERT_TRUE(WaitUntil(
            [handle]()
            {
                return handle->destroyed.load(std::memory_order_acquire);
            },
            std::chrono::seconds{1}));
        EXPECT_EQ(handle->nonblockingWaitCalls.load(std::memory_order_relaxed), 2);
        EXPECT_EQ(fake.destroyCalls.load(std::memory_order_relaxed), 1);
    }
}

TEST(ProcessBackendTests, RejectsReaperStartupFailureBeforeCreatingChild)
{
    FakeProcessBackend fake;
    FakeProcessReaper reaper{.startSucceeds = false};

    ExpectPlatformException(
        [&fake, &reaper]()
        {
            static_cast<void>(ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake), MakeFakeReaperBackend(reaper)));
        },
        ponder::platform::PlatformErrorCode::BackendFailure);
    EXPECT_EQ(reaper.ensureCalls, 1);
    EXPECT_EQ(reaper.enqueued.load(std::memory_order_acquire), nullptr);
    EXPECT_TRUE(fake.launches.empty());
    EXPECT_TRUE(fake.handles.empty());
}

TEST(ProcessBackendTests, RejectsInvalidDescriptorsBeforeBackendLaunch)
{
    FakeProcessBackend fake;

    auto emptyExecutable = ponder::platform::detail::LaunchProcess(ponder::platform::ProcessDesc{}, MakeFakeBackend(fake));
    ASSERT_FALSE(emptyExecutable.HasValue());
    ExpectErrorCode(emptyExecutable.GetError(), ponder::platform::PlatformErrorCode::InvalidArgument);

    auto invalidArgument = ponder::platform::detail::LaunchProcess(
        ponder::platform::ProcessDesc{.executable = std::filesystem::path{"helper"}, .arguments = {std::string{"bad\0argument", 12}}},
        MakeFakeBackend(fake));
    ASSERT_FALSE(invalidArgument.HasValue());
    ExpectErrorCode(invalidArgument.GetError(), ponder::platform::PlatformErrorCode::InvalidArgument);

    auto invalidUtf8Argument = ponder::platform::detail::LaunchProcess(
        ponder::platform::ProcessDesc{.executable = std::filesystem::path{"helper"}, .arguments = {std::string{"\xC3\x28", 2}}},
        MakeFakeBackend(fake));
    ASSERT_FALSE(invalidUtf8Argument.HasValue());
    ExpectErrorCode(invalidUtf8Argument.GetError(), ponder::platform::PlatformErrorCode::InvalidArgument);

    auto invalidExecutable = ponder::platform::detail::LaunchProcess(
        ponder::platform::ProcessDesc{.executable = std::filesystem::path{std::string{"bad\0helper", 10}}}, MakeFakeBackend(fake));
    ASSERT_FALSE(invalidExecutable.HasValue());
    ExpectErrorCode(invalidExecutable.GetError(), ponder::platform::PlatformErrorCode::InvalidArgument);

    EXPECT_TRUE(fake.launches.empty());
}

TEST(ProcessBackendTests, MapsWaitExitStatusAlternatives)
{
    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 7U}};
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*wait));
        EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*wait).exitCode, 7U);
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 0x80000000U}};
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*wait));
        EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*wait).exitCode, 0x80000000U);
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Signal, .value = 15U}};
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessSignalTermination>(*wait));
        EXPECT_EQ(std::get<ponder::platform::ProcessSignalTermination>(*wait).signal, 15);
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Unknown}};
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        EXPECT_TRUE(std::holds_alternative<ponder::platform::ProcessUnknownTermination>(*wait));
    }
}

TEST(ProcessBackendTests, RepeatedWaitReturnsTheExactExitStatus)
{
    FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 42U}};
    auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
    ponder::platform::Process process = std::move(launch).GetValue();

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*wait));
        EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*wait).exitCode, 42U);
    }

    ASSERT_FALSE(fake.handles.empty());
    EXPECT_EQ(fake.handles.front().blockingWaitCalls.load(std::memory_order_relaxed), 2);
}

TEST(ProcessBackendTests, ResultFailuresLeaveTheProcessReusable)
{
    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 9U},
                                .nextBlockingWaitFailures = 1};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        auto failedWait = process.Wait();
        ASSERT_FALSE(failedWait.HasValue());
        ExpectErrorCode(failedWait.GetError(), ponder::platform::PlatformErrorCode::BackendFailure);

        auto retry = process.Wait();
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*retry));
        EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*retry).exitCode, 9U);
    }

    {
        FakeProcessBackend fake{.nextForceKillResult = BackendProcessKillResult::Failed};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();
        ASSERT_FALSE(fake.handles.empty());

        auto failedTermination = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
        ASSERT_FALSE(failedTermination.HasValue());
        ExpectErrorCode(failedTermination.GetError(), ponder::platform::PlatformErrorCode::BackendFailure);

        fake.handles.front().forceKillResult = BackendProcessKillResult::Succeeded;
        auto retry = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
        EXPECT_EQ(fake.handles.front().killForces, (std::vector<bool>{true, true}));
    }
}

TEST(ProcessBackendTests, UnexpectedBackendExceptionsPropagateAndLeaveStateReusable)
{
    {
        FakeProcessBackend fake{.createExceptionsRemaining = 1};
        EXPECT_THROW(static_cast<void>(ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake))), std::runtime_error);
        EXPECT_TRUE(fake.launches.empty());
        EXPECT_TRUE(fake.handles.empty());
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 11U},
                                .nextBlockingWaitExceptions = 1};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        EXPECT_THROW(static_cast<void>(process.Wait()), std::runtime_error);
        auto retry = process.Wait();
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*retry));
        EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*retry).exitCode, 11U);
    }

    {
        FakeProcessBackend fake{.nextKillExceptions = 1};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        EXPECT_THROW(static_cast<void>(process.Terminate(ponder::platform::ProcessTerminationMode::Force)), std::runtime_error);
        auto retry = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
        ASSERT_FALSE(fake.handles.empty());
        EXPECT_EQ(fake.handles.front().killForces, (std::vector<bool>{true, true}));
    }
}

TEST(ProcessBackendTests, VerifiesForgedBackendStatusValuesAndAllowsRetry)
{
    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = static_cast<BackendProcessExitKind>(0xFF), .value = 1U}};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        EXPECT_THROW(static_cast<void>(process.Wait()), ponder::core::Exception);

        ASSERT_FALSE(fake.handles.empty());
        fake.handles.front().exitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 3U};
        auto retry = process.Wait();
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
    }

    {
        FakeProcessBackend fake{.nextForceKillResult = static_cast<BackendProcessKillResult>(0xFF)};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        EXPECT_THROW(static_cast<void>(process.Terminate(ponder::platform::ProcessTerminationMode::Force)), ponder::core::Exception);

        ASSERT_FALSE(fake.handles.empty());
        fake.handles.front().forceKillResult = BackendProcessKillResult::Succeeded;
        auto retry = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Signal, .value = 0U}};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        EXPECT_THROW(static_cast<void>(process.Wait()), ponder::core::Exception);
        ASSERT_FALSE(fake.handles.empty());
        fake.handles.front().exitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 4U};
        auto retry = process.Wait();
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
    }

    {
        FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 5U},
                                .nextWritesWaitStatus = false};
        auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        EXPECT_THROW(static_cast<void>(process.Wait()), ponder::core::Exception);
        ASSERT_FALSE(fake.handles.empty());
        fake.handles.front().writesWaitStatus = true;
        auto retry = process.Wait();
        ASSERT_TRUE(retry.HasValue()) << retry.GetError().GetMessage();
    }
}

TEST(ProcessBackendTests, ReportsWaitLaunchAndTerminationFailures)
{
    {
        FakeProcessBackend fake{.createFails = true};
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_FALSE(result.HasValue());
        ExpectErrorCode(result.GetError(), ponder::platform::PlatformErrorCode::BackendFailure);
    }

    {
        FakeProcessBackend fake{.nextBlockingWaitFailures = 1};
        {
            auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
            ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
            ponder::platform::Process process = std::move(result).GetValue();

            auto wait = process.Wait();
            ASSERT_FALSE(wait.HasValue());
            ExpectErrorCode(wait.GetError(), ponder::platform::PlatformErrorCode::BackendFailure);
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
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();

        auto termination = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
        ASSERT_FALSE(termination.HasValue());
        ExpectErrorCode(termination.GetError(), ponder::platform::PlatformErrorCode::BackendFailure);
    }

    {
        FakeProcessBackend fake{.nextForceKillResult = BackendProcessKillResult::Unsupported};
        auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();

        auto termination = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
        ASSERT_FALSE(termination.HasValue());
        ExpectErrorCode(termination.GetError(), ponder::platform::PlatformErrorCode::Unsupported);
    }
}

TEST(ProcessBackendTests, GracefulPreferredFallsBackToForceWhenUnsupported)
{
    FakeProcessBackend fake{.nextGracefulKillResult = BackendProcessKillResult::Unsupported,
                            .nextForceKillResult = BackendProcessKillResult::Succeeded};
    auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    ponder::platform::Process process = std::move(result).GetValue();

    auto termination = process.Terminate(ponder::platform::ProcessTerminationMode::GracefulPreferred);
    ASSERT_TRUE(termination.HasValue()) << termination.GetError().GetMessage();
    ASSERT_FALSE(fake.handles.empty());
    EXPECT_EQ(fake.handles.front().killForces, (std::vector<bool>{false, true}));
}

TEST(ProcessBackendTests, UsesGracefulTerminationDirectlyWhenSupported)
{
    FakeProcessBackend fake;
    auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
    ponder::platform::Process process = std::move(launch).GetValue();

    auto termination = process.Terminate(ponder::platform::ProcessTerminationMode::GracefulPreferred);
    ASSERT_TRUE(termination.HasValue()) << termination.GetError().GetMessage();
    ASSERT_FALSE(fake.handles.empty());
    EXPECT_EQ(fake.handles.front().killForces, (std::vector<bool>{false}));
}

TEST(ProcessBackendTests, ReturnsExitedChildTerminationFailureAsAResult)
{
    FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 0U},
                            .nextForceKillResult = BackendProcessKillResult::Failed};
    auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
    ponder::platform::Process process = std::move(launch).GetValue();

    auto wait = process.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
    auto termination = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
    ASSERT_FALSE(termination.HasValue());
    ExpectErrorCode(termination.GetError(), ponder::platform::PlatformErrorCode::BackendFailure);
}

TEST(ProcessBackendTests, ThrowsForInvalidTerminationModeBeforeCallingBackend)
{
    FakeProcessBackend fake;
    auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    ponder::platform::Process process = std::move(result).GetValue();

    const auto invalidMode = static_cast<ponder::platform::ProcessTerminationMode>(0xFF);
    ExpectPlatformException(
        [&process, invalidMode]()
        {
            static_cast<void>(process.Terminate(invalidMode));
        },
        ponder::platform::PlatformErrorCode::InvalidArgument);
    EXPECT_TRUE(fake.handles.front().killForces.empty());
}

TEST(ProcessBackendTests, EnforcesLaunchingThreadAffinityAfterMove)
{
    FakeProcessBackend fake;
    auto result = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    std::optional<ponder::platform::Process> process{std::move(result).GetValue()};

    std::atomic<bool> caughtVerifyFailure{};
    std::thread worker{[&process, &caughtVerifyFailure]()
                       {
                           try
                           {
                               static_cast<void>(process->Wait());
                           }
                           catch (const ponder::core::Exception&)
                           {
                               caughtVerifyFailure.store(true);
                           }
                       }};
    worker.join();

    EXPECT_TRUE(caughtVerifyFailure.load());
    process.reset();
    EXPECT_EQ(fake.destroyCalls.load(std::memory_order_relaxed), 1);
}

TEST(ProcessBackendTests, VerifiesMovedFromProcessUse)
{
    FakeProcessBackend fake{.nextExitStatus = BackendProcessExitStatus{.kind = BackendProcessExitKind::Normal, .value = 6U}};
    auto launch = ponder::platform::detail::LaunchProcess(MakeFakeProcessDesc(), MakeFakeBackend(fake));
    ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
    ponder::platform::Process source = std::move(launch).GetValue();
    ponder::platform::Process destination{std::move(source)};

    EXPECT_THROW(static_cast<void>(source.Wait()), ponder::core::Exception);
    EXPECT_THROW(static_cast<void>(source.Terminate(ponder::platform::ProcessTerminationMode::Force)), ponder::core::Exception);

    auto wait = destination.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*wait));
    EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*wait).exitCode, 6U);
}

TEST(ProcessBackendTests, HelperPreservesArgumentsAndReportsExitCode)
{
    ScopedTemporaryFile argumentsFile{MakeTemporaryPath("args")};
    const std::string nonAsciiArgument{"angstrom-\xC3\x85"};
    auto result = ponder::platform::LaunchProcess(ponder::platform::ProcessDesc{
        .executable = GetHelperPath(),
        .arguments = {"--write-args", argumentsFile.GetPath().string(), "--exit-code", "23", "--", "alpha beta", nonAsciiArgument}});
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    ponder::platform::Process process = std::move(result).GetValue();

    auto wait = process.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*wait));
    EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*wait).exitCode, 23U);
    EXPECT_EQ(ReadLines(argumentsFile.GetPath()), (std::vector<std::string>{"alpha beta", nonAsciiArgument}));
}

TEST(ProcessBackendTests, MissingExecutableIsARecoverableLaunchFailure)
{
    const std::filesystem::path missingExecutable = MakeTemporaryPath("missing-executable");
    ASSERT_FALSE(std::filesystem::exists(missingExecutable));

    auto result = ponder::platform::LaunchProcess(ponder::platform::ProcessDesc{.executable = missingExecutable, .arguments = {}});

    ASSERT_FALSE(result.HasValue());
    ExpectErrorCode(result.GetError(), ponder::platform::PlatformErrorCode::BackendFailure);
    EXPECT_NE(result.GetError().GetMessage().find("SDL_CreateProcess"), std::string_view::npos);
}

TEST(ProcessBackendTests, HelperReportsZeroAndSignalExitStatus)
{
    {
        auto launch =
            ponder::platform::LaunchProcess(ponder::platform::ProcessDesc{.executable = GetHelperPath(), .arguments = {"--exit-code", "0"}});
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*wait));
        EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*wait).exitCode, 0U);
    }

#if !defined(_WIN32)
    {
        auto launch = ponder::platform::LaunchProcess(
            ponder::platform::ProcessDesc{.executable = GetHelperPath(), .arguments = {"--raise-signal", std::to_string(SIGTERM)}});
        ASSERT_TRUE(launch.HasValue()) << launch.GetError().GetMessage();
        ponder::platform::Process process = std::move(launch).GetValue();

        auto wait = process.Wait();
        ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
        ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessSignalTermination>(*wait));
        EXPECT_EQ(std::get<ponder::platform::ProcessSignalTermination>(*wait).signal, SIGTERM);
    }
#endif
}

#if defined(_WIN32)
TEST(ProcessBackendTests, PreservesHighBitWindowsExitStatusAsNormalExit)
{
    auto result =
        ponder::platform::LaunchProcess(ponder::platform::ProcessDesc{.executable = GetHelperPath(), .arguments = {"--exit-code", "-2147483648"}});
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    ponder::platform::Process process = std::move(result).GetValue();

    auto wait = process.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*wait));
    EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*wait).exitCode, 0x80000000U);
}
#endif

TEST(ProcessBackendTests, DestroyingProcessDoesNotTerminateHelper)
{
    ScopedTemporaryFile completionFile{MakeTemporaryPath("completion")};
    {
        auto result = ponder::platform::LaunchProcess(
            ponder::platform::ProcessDesc{.executable = GetHelperPath(),
                                          .arguments = {"--sleep-ms", "250", "--touch-after-sleep", completionFile.GetPath().string()}});
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        ponder::platform::Process process = std::move(result).GetValue();
    }

    EXPECT_TRUE(WaitForFile(completionFile.GetPath(), std::chrono::seconds{5}));
}

TEST(ProcessBackendTests, ForceTerminationStopsRunningHelper)
{
    ScopedTemporaryFile readyFile{MakeTemporaryPath("ready")};
    auto result = ponder::platform::LaunchProcess(
        ponder::platform::ProcessDesc{.executable = GetHelperPath(),
                                      .arguments = {"--touch-ready", readyFile.GetPath().string(), "--sleep-ms", "5000", "--exit-code", "99"}});
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    ponder::platform::Process process = std::move(result).GetValue();
    ASSERT_TRUE(WaitForFile(readyFile.GetPath(), std::chrono::seconds{2}));

    auto termination = process.Terminate(ponder::platform::ProcessTerminationMode::Force);
    ASSERT_TRUE(termination.HasValue()) << termination.GetError().GetMessage();

    auto wait = process.Wait();
    ASSERT_TRUE(wait.HasValue()) << wait.GetError().GetMessage();
#if !defined(_WIN32)
    EXPECT_TRUE(std::holds_alternative<ponder::platform::ProcessSignalTermination>(*wait));
#endif
}
} // namespace
