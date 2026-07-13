#include <ponder/core/Result.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Process.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace
{
namespace core = pond::core;
namespace io = pond::io;
namespace platform = pond::platform;

using namespace std::chrono_literals;

enum class FlowKind : std::uint8_t
{
    Finite,
    Termination,
    Abandonment
};

enum class WorkerCommand : std::uint8_t
{
    GracefulTerminate,
    ForceTerminate
};

struct Options final
{
    std::optional<platform::PlatformTimestamp::Duration> autoCloseAfter;
    std::uint32_t childSleepMilliseconds{250};
    int childExitCode{23};
    bool childMode{};
    bool headlessParent{};
    bool showHelp{};
    std::vector<std::string> childPayload;
};

struct WorkerMessage final
{
    std::string text;
};

struct WorkerSharedState final
{
    std::mutex mutex;
    std::condition_variable commandReady;
    std::vector<WorkerMessage> messages;
    std::optional<WorkerCommand> command;
    bool finished{};
};

struct WorkerController final
{
    std::optional<std::jthread> thread;
    std::shared_ptr<WorkerSharedState> shared;
    FlowKind flow{FlowKind::Finite};
    std::string label;
};

struct WindowSlot final
{
    platform::Window window;
    bool titleUpdateFailureReported{};
};

struct AppState final
{
    platform::PlatformRuntime& runtime;
    const Options& options;
    const std::filesystem::path& selfExecutable;
    std::vector<WindowSlot>& windows;
    WorkerController worker;
    platform::PlatformTimestamp startTimestamp;
    std::uint64_t eventCount{};
    bool shutdownRequested{};
    bool startupDemoRequested{true};
    std::string lastAction{"ready"};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0},
                       std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::cout
        << "Usage: " << executableName << " [options]\n\n"
        << "Options:\n"
        << "  --auto-close-ms <milliseconds>  Exit the windowed parent after a run.\n"
        << "  --headless-parent                Launch/wait for a child without runtime.\n"
        << "  --child                          Internal deterministic child mode.\n"
        << "  --sleep-ms <milliseconds>        Child sleep duration.\n"
        << "  --exit-code <0-255>              Child normal exit code.\n"
        << "  --help                           Print this help text.\n\n"
        << "Controls:\n"
        << "  F1            Print this help text.\n"
        << "  N             Start a bounded normal-exit child on a worker.\n"
        << "  T             Start a long-running termination flow.\n"
        << "  G / F         Request graceful-preferred or forced termination.\n"
        << "  A             Start an abandonment flow.\n"
        << "  Q / Escape    Request shutdown.\n";
}

[[nodiscard]] core::Result<std::uint32_t> ParseUnsigned(std::string_view text,
                                                        std::uint32_t maximum,
                                                        std::string_view label)
{
    std::uint32_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end || value > maximum)
    {
        return core::Result<std::uint32_t>::FromError(
            MakeOptionError(std::string{label} + " is out of range or not an integer."));
    }

    return value;
}

[[nodiscard]] core::Result<platform::PlatformTimestamp::Duration> ParseMilliseconds(
    std::string_view text)
{
    auto value = ParseUnsigned(text, 60'000U, "millisecond value");
    if (!value)
    {
        return core::Result<platform::PlatformTimestamp::Duration>::FromError(
            std::move(value).GetError());
    }

    return platform::PlatformTimestamp::Duration{std::chrono::milliseconds{*value}};
}

[[nodiscard]] core::Result<Options> ParseOptions(int argc, char** argv)
{
    Options options{};
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--")
        {
            while (++index < argc)
            {
                options.childPayload.emplace_back(argv[index]);
            }
            break;
        }
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
        }
        else if (argument == "--child")
        {
            options.childMode = true;
        }
        else if (argument == "--headless-parent")
        {
            options.headlessParent = true;
        }
        else if (argument == "--auto-close-ms")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--auto-close-ms requires a value."));
            }
            auto duration = ParseMilliseconds(argv[++index]);
            if (!duration)
            {
                return core::Result<Options>::FromError(std::move(duration).GetError());
            }
            options.autoCloseAfter = std::move(duration).GetValue();
        }
        else if (argument == "--sleep-ms")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--sleep-ms requires a value."));
            }
            auto milliseconds = ParseUnsigned(argv[++index], 60'000U, "--sleep-ms");
            if (!milliseconds)
            {
                return core::Result<Options>::FromError(std::move(milliseconds).GetError());
            }
            options.childSleepMilliseconds = *milliseconds;
        }
        else if (argument == "--exit-code")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--exit-code requires a value."));
            }
            auto exitCode = ParseUnsigned(argv[++index], 255U, "--exit-code");
            if (!exitCode)
            {
                return core::Result<Options>::FromError(std::move(exitCode).GetError());
            }
            options.childExitCode = static_cast<int>(*exitCode);
        }
        else
        {
            return core::Result<Options>::FromError(
                MakeOptionError("Unknown option: " + std::string{argument}));
        }
    }

    if (options.childMode && options.headlessParent)
    {
        return core::Result<Options>::FromError(
            MakeOptionError("--child and --headless-parent are mutually exclusive."));
    }

    return options;
}

[[nodiscard]] core::Result<std::filesystem::path> GetSelfExecutablePath(int argc, char** argv)
{
    if (argc <= 0 || argv == nullptr || argv[0] == nullptr || std::string_view{argv[0]}.empty())
    {
        return core::Result<std::filesystem::path>::FromError(
            MakeOptionError("Cannot determine this executable path from argv[0]."));
    }

    std::filesystem::path path = io::PathFromUtf8(argv[0]);
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error)
    {
        return path;
    }

    return absolute;
}

template <typename Id>
[[nodiscard]] std::string FormatId(Id id)
{
    if (!id.IsValid())
    {
        return "invalid";
    }
    return std::to_string(id.GetValue());
}

[[nodiscard]] std::string FormatDuration(platform::PlatformTimestamp::Duration duration)
{
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return std::to_string(milliseconds.count()) + " ms";
}

[[nodiscard]] std::string FormatTimestamp(platform::PlatformTimestamp timestamp)
{
    return std::to_string(timestamp.GetTimeSinceEpoch().count()) + " ns";
}

[[nodiscard]] std::string QuoteText(std::string_view text)
{
    std::ostringstream stream;
    stream << '"';
    for (unsigned char character : text)
    {
        switch (character)
        {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            stream << static_cast<char>(character);
            break;
        }
    }
    stream << '"';
    return stream.str();
}

[[nodiscard]] std::string Shorten(std::string_view text, std::size_t maxLength)
{
    if (text.size() <= maxLength)
    {
        return std::string{text};
    }
    return std::string{text.substr(0, maxLength - 3U)} + "...";
}

[[nodiscard]] std::string_view ToString(FlowKind flow) noexcept
{
    switch (flow)
    {
    case FlowKind::Finite:
        return "finite";
    case FlowKind::Termination:
        return "termination";
    case FlowKind::Abandonment:
        return "abandonment";
    }

    return "unrecognized";
}

[[nodiscard]] std::string_view ToString(platform::PhysicalKey key) noexcept
{
    switch (key)
    {
    case platform::PhysicalKey::F1:
        return "F1";
    case platform::PhysicalKey::N:
        return "N";
    case platform::PhysicalKey::T:
        return "T";
    case platform::PhysicalKey::G:
        return "G";
    case platform::PhysicalKey::F:
        return "F";
    case platform::PhysicalKey::A:
        return "A";
    case platform::PhysicalKey::Q:
        return "Q";
    case platform::PhysicalKey::Escape:
        return "Escape";
    default:
        return "other";
    }
}

[[nodiscard]] std::string FormatExitStatus(const platform::ProcessExitStatus& status)
{
    struct Visitor final
    {
        [[nodiscard]] std::string operator()(platform::ProcessNormalExit exit) const
        {
            return "normal exit code " + std::to_string(exit.exitCode);
        }

        [[nodiscard]] std::string operator()(platform::ProcessSignalTermination exit) const
        {
            return "signal termination " + std::to_string(exit.signal);
        }

        [[nodiscard]] std::string operator()(platform::ProcessUnknownTermination) const
        {
            return "unknown termination";
        }
    };

    return std::visit(Visitor{}, status);
}

void PrintError(std::string_view operation, const core::Error& error)
{
    std::cout << operation << " failed: " << core::FormatError(error) << '\n';
}

[[nodiscard]] bool IsPlatformError(const core::Error& error, platform::PlatformErrorCode code)
{
    return error.GetCode() == platform::ToErrorCode(code);
}

void PushWorkerMessage(const std::shared_ptr<WorkerSharedState>& shared, std::string text)
{
    {
        std::lock_guard lock{shared->mutex};
        shared->messages.push_back(WorkerMessage{std::move(text)});
    }
    shared->commandReady.notify_all();
}

void PushWorkerError(const std::shared_ptr<WorkerSharedState>& shared,
                     std::string_view operation, const core::Error& error)
{
    PushWorkerMessage(shared, std::string{operation} + " failed: " + core::FormatError(error));
}

void FinishWorker(const std::shared_ptr<WorkerSharedState>& shared)
{
    {
        std::lock_guard lock{shared->mutex};
        shared->finished = true;
    }
    shared->commandReady.notify_all();
}

[[nodiscard]] WorkerCommand WaitForWorkerCommand(
    const std::shared_ptr<WorkerSharedState>& shared)
{
    std::unique_lock lock{shared->mutex};
    shared->commandReady.wait(lock, [&shared]() { return shared->command.has_value(); });
    return *shared->command;
}

[[nodiscard]] platform::ProcessDesc MakeChildProcessDesc(
    const std::filesystem::path& executable, std::uint32_t sleepMilliseconds, int exitCode,
    std::vector<std::string> payload)
{
    std::vector<std::string> arguments{
        "--child",
        "--sleep-ms",
        std::to_string(sleepMilliseconds),
        "--exit-code",
        std::to_string(exitCode),
        "--",
    };
    arguments.insert(arguments.end(), std::make_move_iterator(payload.begin()),
                     std::make_move_iterator(payload.end()));
    return platform::ProcessDesc{.executable = executable, .arguments = std::move(arguments)};
}

void RunFiniteWorker(std::shared_ptr<WorkerSharedState> shared, platform::ProcessDesc desc)
{
    PushWorkerMessage(shared, "Launching bounded child and entering blocking Wait().");
    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        PushWorkerError(shared, "LaunchProcess", processResult.GetError());
        FinishWorker(shared);
        return;
    }

    platform::Process process = std::move(processResult).GetValue();
    auto waitResult = process.Wait();
    if (!waitResult)
    {
        PushWorkerError(shared, "Process::Wait", waitResult.GetError());
        FinishWorker(shared);
        return;
    }

    PushWorkerMessage(shared, "Process::Wait returned " + FormatExitStatus(*waitResult) + ".");
    FinishWorker(shared);
}

void RunTerminationWorker(std::shared_ptr<WorkerSharedState> shared, platform::ProcessDesc desc)
{
    PushWorkerMessage(shared, "Launching long-running child for termination flow.");
    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        PushWorkerError(shared, "LaunchProcess", processResult.GetError());
        FinishWorker(shared);
        return;
    }

    platform::Process process = std::move(processResult).GetValue();
    PushWorkerMessage(shared, "Child is running; worker is waiting for G or F, not Wait().");

    const WorkerCommand command = WaitForWorkerCommand(shared);
    const platform::ProcessTerminationMode mode = command == WorkerCommand::ForceTerminate ?
        platform::ProcessTerminationMode::Force :
        platform::ProcessTerminationMode::GracefulPreferred;
    const std::string modeName = command == WorkerCommand::ForceTerminate ? "force" : "graceful";
    PushWorkerMessage(shared, "Calling Process::Terminate(" + modeName + ").");
    auto termination = process.Terminate(mode);
    if (!termination)
    {
        PushWorkerError(shared, "Process::Terminate", termination.GetError());
        PushWorkerMessage(shared, "Leaving process scope without Wait because termination failed.");
        FinishWorker(shared);
        return;
    }

    PushWorkerMessage(shared, "Terminate succeeded; now calling blocking Wait() on worker.");
    auto waitResult = process.Wait();
    if (!waitResult)
    {
        PushWorkerError(shared, "Process::Wait", waitResult.GetError());
        FinishWorker(shared);
        return;
    }

    PushWorkerMessage(shared, "Terminated child reported " + FormatExitStatus(*waitResult) + ".");
    FinishWorker(shared);
}

void RunAbandonmentWorker(std::shared_ptr<WorkerSharedState> shared, platform::ProcessDesc desc)
{
    PushWorkerMessage(shared, "Launching child that will be abandoned without Wait().");
    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        PushWorkerError(shared, "LaunchProcess", processResult.GetError());
        FinishWorker(shared);
        return;
    }

    {
        platform::Process process = std::move(processResult).GetValue();
        static_cast<void>(process);
        PushWorkerMessage(shared, "Destroying live Process now; destructor must not block.");
    }

    PushWorkerMessage(shared, "Process owner ended; private platform reaper tracks the child.");
    FinishWorker(shared);
}

[[nodiscard]] bool WorkerIsActive(const WorkerController& worker) noexcept
{
    return worker.thread.has_value();
}

void DrainWorkerMessages(AppState& state)
{
    if (!state.worker.shared)
    {
        return;
    }

    std::vector<WorkerMessage> messages;
    bool finished{};
    {
        std::lock_guard lock{state.worker.shared->mutex};
        messages.swap(state.worker.shared->messages);
        finished = state.worker.shared->finished;
    }

    for (const WorkerMessage& message : messages)
    {
        const platform::PlatformTimestamp now = state.runtime.Now();
        std::cout << "[worker " << state.worker.label << " at " << FormatTimestamp(now)
                  << " (+" << FormatDuration(now - state.startTimestamp) << ")] "
                  << message.text << '\n';
        state.lastAction = Shorten(message.text, 72);
    }

    if (finished)
    {
        if (state.worker.thread)
        {
            state.worker.thread.reset();
        }
        state.worker.shared.reset();
        state.worker.label.clear();
    }
}

void SendWorkerCommand(AppState& state, WorkerCommand command)
{
    if (!WorkerIsActive(state.worker) || !state.worker.shared)
    {
        std::cout << "No active worker is waiting for a process command.\n";
        return;
    }

    if (state.worker.flow != FlowKind::Termination)
    {
        std::cout << "The active " << ToString(state.worker.flow)
                  << " worker does not accept termination commands.\n";
        return;
    }

    {
        std::lock_guard lock{state.worker.shared->mutex};
        if (state.worker.shared->command)
        {
            std::cout << "A termination command has already been sent.\n";
            return;
        }
        state.worker.shared->command = command;
    }
    state.worker.shared->commandReady.notify_all();
}

void StartWorker(AppState& state, FlowKind flow)
{
    DrainWorkerMessages(state);
    if (WorkerIsActive(state.worker))
    {
        std::cout << "Worker " << state.worker.label << " is already active.\n";
        return;
    }

    std::uint32_t sleepMilliseconds = state.options.childSleepMilliseconds;
    int exitCode = state.options.childExitCode;
    std::vector<std::string> payload{"alpha beta", "angstrom-\xC3\x85", "no shell quoting"};
    std::string label{"finite"};

    switch (flow)
    {
    case FlowKind::Finite:
        break;
    case FlowKind::Termination:
        sleepMilliseconds = 30'000U;
        exitCode = 99;
        payload = {"termination flow", "worker owns this process"};
        label = "termination";
        break;
    case FlowKind::Abandonment:
        sleepMilliseconds = 3'000U;
        exitCode = 41;
        payload = {"abandonment flow", "asynchronous reaper"};
        label = "abandonment";
        break;
    }

    auto shared = std::make_shared<WorkerSharedState>();
    const platform::ProcessDesc desc = MakeChildProcessDesc(
        state.selfExecutable, sleepMilliseconds, exitCode, std::move(payload));

    state.worker.flow = flow;
    state.worker.label = label;
    state.worker.shared = shared;

    switch (flow)
    {
    case FlowKind::Finite:
        state.worker.thread.emplace(RunFiniteWorker, shared, desc);
        break;
    case FlowKind::Termination:
        state.worker.thread.emplace(RunTerminationWorker, shared, desc);
        break;
    case FlowKind::Abandonment:
        state.worker.thread.emplace(RunAbandonmentWorker, shared, desc);
        break;
    }
}

void ReleaseParentWindow(AppState& state)
{
    if (!state.windows.empty())
    {
        std::cout << "Releasing parent window.\n";
        state.windows.clear();
    }
}

void RequestShutdown(AppState& state, std::string_view reason)
{
    if (!state.shutdownRequested)
    {
        state.shutdownRequested = true;
        std::cout << "Shutdown requested by " << reason << ".\n";
    }

    if (!WorkerIsActive(state.worker))
    {
        ReleaseParentWindow(state);
        return;
    }

    if (state.worker.flow == FlowKind::Termination)
    {
        std::cout << "Shutdown is forcing the active termination flow to finish.\n";
        SendWorkerCommand(state, WorkerCommand::ForceTerminate);
    }
    else
    {
        std::cout << "Waiting for bounded " << ToString(state.worker.flow)
                  << " worker before releasing the parent.\n";
    }
}

void HandleCommand(AppState& state, platform::PhysicalKey key)
{
    switch (key)
    {
    case platform::PhysicalKey::F1:
        PrintUsage("ponder-platform-4-responsive-process-runner");
        return;
    case platform::PhysicalKey::N:
        StartWorker(state, FlowKind::Finite);
        return;
    case platform::PhysicalKey::T:
        StartWorker(state, FlowKind::Termination);
        return;
    case platform::PhysicalKey::G:
        SendWorkerCommand(state, WorkerCommand::GracefulTerminate);
        return;
    case platform::PhysicalKey::F:
        SendWorkerCommand(state, WorkerCommand::ForceTerminate);
        return;
    case platform::PhysicalKey::A:
        StartWorker(state, FlowKind::Abandonment);
        return;
    case platform::PhysicalKey::Q:
    case platform::PhysicalKey::Escape:
        RequestShutdown(state, "keyboard");
        return;
    default:
        return;
    }
}

void PrintEventHeader(std::string_view name, platform::PlatformTimestamp timestamp,
                      const AppState& state)
{
    std::cout << "[event " << state.eventCount << "] " << name << " at "
              << FormatTimestamp(timestamp) << " (+"
              << FormatDuration(timestamp - state.startTimestamp) << ")";
}

struct EventVisitor final
{
    AppState& state;

    void operator()(const platform::QuitRequestedEvent& event) const
    {
        PrintEventHeader("QuitRequested", event.timestamp, state);
        std::cout << '\n';
        RequestShutdown(state, "quit event");
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
        RequestShutdown(state, "window close request");
    }

    void operator()(const platform::KeyboardKeyEvent& event) const
    {
        PrintEventHeader("KeyboardKey", event.timestamp, state);
        std::cout << " key=" << ToString(event.physicalKey) << " pressed=" << event.pressed
                  << " repeat=" << event.repeat << '\n';
        if (event.pressed && !event.repeat)
        {
            HandleCommand(state, event.physicalKey);
        }
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Other platform event", event.timestamp, state);
        std::cout << " observed while the process worker may be blocked.\n";
    }
};

void DrainEvents(AppState& state)
{
    while (std::optional<platform::PlatformEvent> event = state.runtime.PollEvent())
    {
        ++state.eventCount;
        std::visit(EventVisitor{state}, *event);
    }
}

void UpdateWindowTitle(AppState& state)
{
    if (state.windows.empty())
    {
        return;
    }

    const std::string workerText = WorkerIsActive(state.worker) ? state.worker.label : "idle";
    const std::string title = "Process Runner | worker " + workerText + " | " +
                              state.lastAction;
    auto result = state.windows.front().window.SetTitle(title);
    if (!result && !state.windows.front().titleUpdateFailureReported)
    {
        PrintError("SetTitle during title update", result.GetError());
        state.windows.front().titleUpdateFailureReported = true;
    }
}

[[nodiscard]] core::Result<WindowSlot> CreateParentWindow(platform::PlatformRuntime& runtime)
{
    const platform::WindowDesc desc{
        .title = "Ponder Responsive Process Runner",
        .logicalSize = {860, 520},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .minimumLogicalSize = platform::LogicalSize{360, 240},
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };

    auto window = runtime.CreateWindow(desc);
    if (!window)
    {
        return core::Result<WindowSlot>::FromError(std::move(window).GetError());
    }

    return WindowSlot{.window = std::move(window).GetValue()};
}

[[nodiscard]] core::VoidResult RunInteractiveParent(const Options& options,
                                                    const std::filesystem::path& selfExecutable,
                                                    int argc, char** argv)
{
    const platform::PlatformRuntimeDesc runtimeDesc{
        .applicationName = "Ponder Platform Responsive Process Runner",
        .applicationVersion = std::string{"0.1.0"},
        .applicationIdentifier = std::string{
            "org.ponder.examples.platform.responsive-process-runner"},
    };

    auto runtimeResult = platform::PlatformRuntime::Create(runtimeDesc);
    if (!runtimeResult)
    {
        return core::VoidResult::FromError(std::move(runtimeResult).GetError());
    }

    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    const platform::PlatformTimestamp startTimestamp = runtime.Now();
    std::vector<WindowSlot> windows;
    windows.reserve(1);
    auto parent = CreateParentWindow(runtime);
    if (!parent)
    {
        return core::VoidResult::FromError(std::move(parent).GetError());
    }
    windows.push_back(std::move(parent).GetValue());

    AppState state{.runtime = runtime,
                   .options = options,
                   .selfExecutable = selfExecutable,
                   .windows = windows,
                   .startTimestamp = startTimestamp};

    PrintUsage(argc > 0 ? argv[0] : "ponder-platform-4-responsive-process-runner");
    std::cout << "Self executable: " << QuoteText(io::PathToUtf8(selfExecutable)) << '\n';
    StartWorker(state, FlowKind::Finite);

    auto nextTitleUpdate = state.startTimestamp;
    while (!state.windows.empty() || WorkerIsActive(state.worker))
    {
        DrainEvents(state);
        DrainWorkerMessages(state);

        if (state.shutdownRequested && !WorkerIsActive(state.worker))
        {
            ReleaseParentWindow(state);
        }

        const platform::PlatformTimestamp now = runtime.Now();
        if (now - nextTitleUpdate >= 250ms)
        {
            UpdateWindowTitle(state);
            nextTitleUpdate = now;
        }

        if (options.autoCloseAfter && !state.shutdownRequested &&
            now - state.startTimestamp >= *options.autoCloseAfter)
        {
            std::cout << "Auto-close duration reached after "
                      << FormatDuration(now - state.startTimestamp) << ".\n";
            RequestShutdown(state, "auto close");
        }

        std::this_thread::sleep_for(8ms);
    }

    return {};
}

[[nodiscard]] core::VoidResult RunHeadlessParent(const Options& options,
                                                 const std::filesystem::path& selfExecutable)
{
    std::cout << "Running headless parent without PlatformRuntime.\n";
    platform::ProcessDesc desc = MakeChildProcessDesc(
        selfExecutable, options.childSleepMilliseconds, options.childExitCode,
        {"headless parent", "alpha beta", "angstrom-\xC3\x85"});

    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        return core::VoidResult::FromError(std::move(processResult).GetError());
    }

    platform::Process process = std::move(processResult).GetValue();
    std::cout << "Headless parent launched child; blocking Wait() is acceptable here.\n";
    auto waitResult = process.Wait();
    if (!waitResult)
    {
        return core::VoidResult::FromError(std::move(waitResult).GetError());
    }

    std::cout << "Headless child completed with " << FormatExitStatus(*waitResult) << ".\n";
    return {};
}

int RunChildMode(const Options& options, int argc, char** argv)
{
    std::cout << "[child] started with argc=" << argc << '\n';
    for (int index = 0; index < argc; ++index)
    {
        std::cout << "[child] argv[" << index << "]=" << QuoteText(argv[index]) << '\n';
    }

    if (!options.childPayload.empty())
    {
        std::cout << "[child] payload arguments after -- delimiter:\n";
        for (std::size_t index = 0; index < options.childPayload.size(); ++index)
        {
            std::cout << "[child] payload[" << index << "]="
                      << QuoteText(options.childPayload[index]) << '\n';
        }
    }

    std::cout << "[child] sleeping for " << options.childSleepMilliseconds << " ms.\n";
    std::this_thread::sleep_for(std::chrono::milliseconds{options.childSleepMilliseconds});
    std::cout << "[child] exiting with code " << options.childExitCode << ".\n";
    return options.childExitCode;
}
} // namespace

int main(int argc, char** argv)
{
    std::cout << std::boolalpha << std::unitbuf;

    try
    {
        auto optionsResult = ParseOptions(argc, argv);
        if (!optionsResult)
        {
            std::cerr << "ponder-platform-4-responsive-process-runner failed: "
                      << core::FormatError(optionsResult.GetError()) << '\n';
            return 1;
        }

        const Options options = std::move(optionsResult).GetValue();
        if (options.showHelp)
        {
            PrintUsage(argc > 0 ? argv[0] : "ponder-platform-4-responsive-process-runner");
            return 0;
        }

        if (options.childMode)
        {
            return RunChildMode(options, argc, argv);
        }

        auto selfExecutable = GetSelfExecutablePath(argc, argv);
        if (!selfExecutable)
        {
            std::cerr << "ponder-platform-4-responsive-process-runner failed: "
                      << core::FormatError(selfExecutable.GetError()) << '\n';
            return 1;
        }

        const auto result = options.headlessParent ?
                                RunHeadlessParent(options, *selfExecutable) :
                                RunInteractiveParent(options, *selfExecutable, argc, argv);
        if (!result)
        {
            std::cerr << "ponder-platform-4-responsive-process-runner failed: "
                      << core::FormatError(result.GetError()) << '\n';
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ponder-platform-4-responsive-process-runner terminated with an exception: "
                  << exception.what() << '\n';
        return 1;
    }

    return 0;
}
