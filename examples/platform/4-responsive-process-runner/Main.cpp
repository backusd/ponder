#include <ponder/core/Exception.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/Process.hpp>
#include <ponder/platform/Runtime.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace
{
struct ExitStatus final
{
    const ponder::platform::ProcessExitStatus& value;
};
} // namespace

namespace std
{
template <>
struct formatter<ExitStatus> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ExitStatus& status, FormatContext& context) const
    {
        struct Visitor final
        {
            [[nodiscard]] std::string operator()(ponder::platform::ProcessNormalExit exit) const
            {
                return "normal exit code " + std::to_string(exit.exitCode);
            }

            [[nodiscard]] std::string operator()(ponder::platform::ProcessSignalTermination exit) const
            {
                return "signal termination " + std::to_string(exit.signal);
            }

            [[nodiscard]] std::string operator()(ponder::platform::ProcessUnknownTermination) const
            {
                return "unknown termination";
            }
        };

        return formatter<string>::format(std::visit(Visitor{}, status.value), context);
    }
};
} // namespace std

namespace
{
namespace core = ponder::core;
namespace io = pond::io;
namespace platform = ponder::platform;

using namespace std::chrono_literals;

enum class FlowKind : std::uint8_t
{
    Finite,
    Termination,
    Abandonment
};

enum class WorkerCommand : std::uint8_t
{
    None,
    GracefulTerminate,
    ForceTerminate
};

struct Options final
{
    std::optional<core::Duration> autoCloseAfter;
    std::uint32_t childSleepMilliseconds{250};
    int childExitCode{23};
    bool childMode{};
    bool headlessParent{};
    bool injectWorkerException{};
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
    std::vector<WorkerMessage> messages;
    std::atomic<WorkerCommand> command{WorkerCommand::None};
    std::atomic_bool finished{};
    std::exception_ptr failure;
};

struct WorkerController final
{
    std::optional<std::jthread> thread;
    std::shared_ptr<WorkerSharedState> shared;
    FlowKind flow{FlowKind::Finite};
    std::string label;

    ~WorkerController() noexcept
    {
        if (!thread || !shared || flow != FlowKind::Termination)
        {
            return;
        }

        WorkerCommand expected = WorkerCommand::None;
        static_cast<void>(
            shared->command.compare_exchange_strong(expected, WorkerCommand::ForceTerminate, std::memory_order_release, std::memory_order_relaxed));
        shared->command.notify_all();
    }
};

struct WindowSlot final
{
    platform::Window window;
};

struct AppState final
{
    platform::Runtime& runtime;
    const Options& options;
    const std::filesystem::path& selfExecutable;
    std::vector<WindowSlot>& windows;
    WorkerController worker;
    core::Timestamp startTimestamp;
    core::Timestamp lastTimestamp;
    std::uint64_t eventCount{};
    bool shutdownRequested{};
    std::string lastAction{"ready"};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0}, std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::print("Usage: {} [options]\n\n"
               "Options:\n"
               "  --auto-close-ms <milliseconds>  Exit the windowed parent after a run.\n"
               "  --headless-parent                Launch/wait for a child without runtime.\n"
               "  --inject-worker-exception        Verify worker exception marshalling.\n"
               "  --child                          Internal deterministic child mode.\n"
               "  --sleep-ms <milliseconds>        Child sleep duration.\n"
               "  --exit-code <0-255>              Child normal exit code.\n"
               "  --help                           Print this help text.\n\n"
               "Controls:\n"
               "  F1            Print this help text.\n"
               "  N             Start a bounded normal-exit child on a worker.\n"
               "  T             Start a long-running termination flow.\n"
               "  G / F         Request graceful-preferred or forced termination.\n"
               "  A             Start an abandonment flow.\n"
               "  Q / Escape    Request shutdown.\n",
               executableName);
}

[[nodiscard]] core::Result<std::uint32_t> ParseUnsigned(std::string_view text, std::uint32_t maximum, std::string_view label)
{
    std::uint32_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end || value > maximum)
    {
        return core::Result<std::uint32_t>::FromError(MakeOptionError(std::string{label} + " is out of range or not an integer."));
    }

    return value;
}

[[nodiscard]] core::Result<core::Duration> ParseMilliseconds(std::string_view text)
{
    auto value = ParseUnsigned(text, 60'000U, "millisecond value");
    if (!value)
    {
        return core::Result<core::Duration>::FromError(std::move(value).GetError());
    }

    return core::Duration{std::chrono::milliseconds{*value}};
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
        else if (argument == "--inject-worker-exception")
        {
            options.injectWorkerException = true;
        }
        else if (argument == "--auto-close-ms")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(MakeOptionError("--auto-close-ms requires a value."));
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
                return core::Result<Options>::FromError(MakeOptionError("--sleep-ms requires a value."));
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
                return core::Result<Options>::FromError(MakeOptionError("--exit-code requires a value."));
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
            return core::Result<Options>::FromError(MakeOptionError("Unknown option: " + std::string{argument}));
        }
    }

    if (options.childMode && options.headlessParent)
    {
        return core::Result<Options>::FromError(MakeOptionError("--child and --headless-parent are mutually exclusive."));
    }
    if (options.childMode && options.injectWorkerException)
    {
        return core::Result<Options>::FromError(MakeOptionError("--inject-worker-exception is a parent-mode option."));
    }
    if (options.headlessParent && options.injectWorkerException)
    {
        return core::Result<Options>::FromError(MakeOptionError("--inject-worker-exception requires the interactive parent."));
    }

    return options;
}

[[nodiscard]] core::Result<std::filesystem::path> GetSelfExecutablePath(int argc, char** argv)
{
    if (argc <= 0 || argv == nullptr || argv[0] == nullptr || std::string_view{argv[0]}.empty())
    {
        return core::Result<std::filesystem::path>::FromError(MakeOptionError("Cannot determine this executable path from argv[0]."));
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

void PrintError(std::string_view operation, const core::Error& error)
{
    std::println("{} failed: {}", operation, error);
}

void PushWorkerMessage(const std::shared_ptr<WorkerSharedState>& shared, std::string text)
{
    {
        std::lock_guard lock{shared->mutex};
        shared->messages.push_back(WorkerMessage{std::move(text)});
    }
}

void PushWorkerError(const std::shared_ptr<WorkerSharedState>& shared, std::string_view operation, const core::Error& error)
{
    PushWorkerMessage(shared, std::string{operation} + " failed: " + std::format("{}", error));
}

void FinishWorker(const std::shared_ptr<WorkerSharedState>& shared, std::exception_ptr failure = {}) noexcept
{
    shared->failure = std::move(failure);
    shared->finished.store(true, std::memory_order_release);
}

[[nodiscard]] WorkerCommand WaitForWorkerCommand(const std::shared_ptr<WorkerSharedState>& shared)
{
    shared->command.wait(WorkerCommand::None, std::memory_order_acquire);
    return shared->command.load(std::memory_order_acquire);
}

[[nodiscard]] platform::ProcessDesc MakeChildProcessDesc(const std::filesystem::path& executable, std::uint32_t sleepMilliseconds, int exitCode,
                                                         std::vector<std::string> payload)
{
    std::vector<std::string> arguments{
        "--child", "--sleep-ms", std::to_string(sleepMilliseconds), "--exit-code", std::to_string(exitCode), "--",
    };
    arguments.insert(arguments.end(), std::make_move_iterator(payload.begin()), std::make_move_iterator(payload.end()));
    return platform::ProcessDesc{.executable = executable, .arguments = std::move(arguments)};
}

[[nodiscard]] core::Result<platform::ProcessExitStatus> WaitWithOneRetry(const std::shared_ptr<WorkerSharedState>& shared, platform::Process& process)
{
    auto waitResult = process.Wait();
    if (waitResult)
    {
        return waitResult;
    }

    PushWorkerError(shared, "Process::Wait", waitResult.GetError());
    PushWorkerMessage(shared, "Retrying Process::Wait once on the same process owner.");

    auto retryResult = process.Wait();
    if (!retryResult)
    {
        PushWorkerError(shared, "Process::Wait retry", retryResult.GetError());
    }
    return retryResult;
}

[[nodiscard]] core::VoidResult TerminateWithEscalation(const std::shared_ptr<WorkerSharedState>& shared, platform::Process& process,
                                                       platform::ProcessTerminationMode mode)
{
    auto termination = process.Terminate(mode);
    if (termination)
    {
        return termination;
    }

    PushWorkerError(shared, "Process::Terminate", termination.GetError());
    if (mode == platform::ProcessTerminationMode::Force)
    {
        return termination;
    }

    PushWorkerMessage(shared, "Graceful-preferred termination failed; escalating once to Process::Terminate(force).");
    auto forceTermination = process.Terminate(platform::ProcessTerminationMode::Force);
    if (!forceTermination)
    {
        PushWorkerError(shared, "Process::Terminate(force escalation)", forceTermination.GetError());
    }
    return forceTermination;
}

void RunFiniteWorker(std::shared_ptr<WorkerSharedState> shared, platform::ProcessDesc desc)
{
    PushWorkerMessage(shared, "Launching bounded child and entering blocking Wait().");
    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        PushWorkerError(shared, "LaunchProcess", processResult.GetError());
        PushWorkerMessage(shared, "Launch was not retried automatically; use N to retry after "
                                  "correcting the executable or host condition.");
        return;
    }

    platform::Process process = std::move(processResult).GetValue();
    auto waitResult = WaitWithOneRetry(shared, process);
    if (!waitResult)
    {
        PushWorkerMessage(shared, "Both waits failed; abandoning the live Process owner so the "
                                  "private reaper can finish cleanup.");
        return;
    }

    PushWorkerMessage(shared, "Process::Wait returned " + std::format("{}", ExitStatus{*waitResult}) + ".");
}

void RunTerminationWorker(std::shared_ptr<WorkerSharedState> shared, platform::ProcessDesc desc)
{
    PushWorkerMessage(shared, "Launching long-running child for termination flow.");
    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        PushWorkerError(shared, "LaunchProcess", processResult.GetError());
        PushWorkerMessage(shared, "Launch was not retried automatically; use T to retry after "
                                  "correcting the executable or host condition.");
        return;
    }

    platform::Process process = std::move(processResult).GetValue();
    PushWorkerMessage(shared, "Child is running; worker is waiting for G or F, not Wait().");

    const WorkerCommand command = WaitForWorkerCommand(shared);
    const platform::ProcessTerminationMode mode =
        command == WorkerCommand::ForceTerminate ? platform::ProcessTerminationMode::Force : platform::ProcessTerminationMode::GracefulPreferred;
    const std::string modeName = command == WorkerCommand::ForceTerminate ? "force" : "graceful";
    PushWorkerMessage(shared, "Calling Process::Terminate(" + modeName + ").");
    auto termination = TerminateWithEscalation(shared, process, mode);
    if (!termination)
    {
        PushWorkerMessage(shared, "Termination could not be confirmed; leaving Process scope "
                                  "without Wait so private cleanup can reap it.");
        return;
    }

    PushWorkerMessage(shared, "Terminate succeeded; now calling blocking Wait() on worker.");
    auto waitResult = WaitWithOneRetry(shared, process);
    if (!waitResult)
    {
        PushWorkerMessage(shared, "Both post-termination waits failed; private cleanup retains "
                                  "the process until exit can be confirmed.");
        return;
    }

    PushWorkerMessage(shared, "Terminated child reported " + std::format("{}", ExitStatus{*waitResult}) + ".");
}

void RunAbandonmentWorker(std::shared_ptr<WorkerSharedState> shared, platform::ProcessDesc desc)
{
    PushWorkerMessage(shared, "Launching child that will be abandoned without Wait().");
    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        PushWorkerError(shared, "LaunchProcess", processResult.GetError());
        PushWorkerMessage(shared, "Launch was not retried automatically; use A to retry after "
                                  "correcting the executable or host condition.");
        return;
    }

    {
        platform::Process process = std::move(processResult).GetValue();
        static_cast<void>(process);
        PushWorkerMessage(shared, "Destroying live Process now; destructor must not block.");
    }

    PushWorkerMessage(shared, "Process owner ended; private platform reaper tracks the child.");
}

void RunWorkerEntry(std::shared_ptr<WorkerSharedState> shared, FlowKind flow, platform::ProcessDesc desc, bool injectWorkerException) noexcept
{
    std::exception_ptr failure;
    try
    {
        if (injectWorkerException)
        {
            throw std::runtime_error{"Injected worker exception for verification."};
        }

        switch (flow)
        {
        case FlowKind::Finite:
            RunFiniteWorker(shared, std::move(desc));
            break;
        case FlowKind::Termination:
            RunTerminationWorker(shared, std::move(desc));
            break;
        case FlowKind::Abandonment:
            RunAbandonmentWorker(shared, std::move(desc));
            break;
        }
    }
    catch (...)
    {
        failure = std::current_exception();
    }

    FinishWorker(shared, std::move(failure));
}

[[nodiscard]] bool WorkerIsActive(const WorkerController& worker) noexcept
{
    return worker.thread.has_value();
}

void DrainWorkerMessages(AppState& state, core::Timestamp observedAt)
{
    if (!state.worker.shared)
    {
        return;
    }

    const std::shared_ptr<WorkerSharedState> shared = state.worker.shared;
    const bool finished = shared->finished.load(std::memory_order_acquire);
    std::vector<WorkerMessage> messages;
    {
        std::lock_guard lock{shared->mutex};
        messages.swap(shared->messages);
    }

    for (const WorkerMessage& message : messages)
    {
        std::println("[worker {} observed at {} (+{})] {}", state.worker.label, observedAt, observedAt - state.startTimestamp, message.text);
        state.lastAction = Shorten(message.text, 72);
    }

    if (finished)
    {
        state.worker.thread.reset();
        const std::exception_ptr failure = shared->failure;
        state.worker.shared.reset();
        state.worker.label.clear();
        if (failure != nullptr)
        {
            std::rethrow_exception(failure);
        }
    }
}

void SendWorkerCommand(AppState& state, WorkerCommand command)
{
    if (!WorkerIsActive(state.worker) || !state.worker.shared)
    {
        std::println("No active worker is waiting for a process command.");
        return;
    }

    if (state.worker.flow != FlowKind::Termination)
    {
        std::println("The active {} worker does not accept termination commands.", ToString(state.worker.flow));
        return;
    }

    WorkerCommand expected = WorkerCommand::None;
    if (!state.worker.shared->command.compare_exchange_strong(expected, command, std::memory_order_release, std::memory_order_relaxed))
    {
        std::println("A termination command has already been sent.");
        return;
    }
    state.worker.shared->command.notify_all();
}

void StartWorker(AppState& state, FlowKind flow)
{
    DrainWorkerMessages(state, state.lastTimestamp);
    if (state.shutdownRequested)
    {
        std::println("Cannot start a process worker after shutdown has been requested.");
        return;
    }
    if (WorkerIsActive(state.worker))
    {
        std::println("Worker {} is already active.", state.worker.label);
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
    platform::ProcessDesc desc = MakeChildProcessDesc(state.selfExecutable, sleepMilliseconds, exitCode, std::move(payload));

    state.worker.flow = flow;
    state.worker.label = label;
    state.worker.shared = shared;

    state.worker.thread.emplace(RunWorkerEntry, shared, flow, std::move(desc), state.options.injectWorkerException);
}

void ReleaseParentWindow(AppState& state)
{
    if (!state.windows.empty())
    {
        std::println("Releasing parent window.");
        state.windows.clear();
    }
}

void RequestShutdown(AppState& state, std::string_view reason)
{
    if (!state.shutdownRequested)
    {
        state.shutdownRequested = true;
        std::println("Shutdown requested by {}.", reason);
    }

    if (!WorkerIsActive(state.worker))
    {
        ReleaseParentWindow(state);
        return;
    }

    if (state.worker.flow == FlowKind::Termination)
    {
        std::println("Shutdown is forcing the active termination flow to finish.");
        SendWorkerCommand(state, WorkerCommand::ForceTerminate);
    }
    else
    {
        std::println("Waiting for bounded {} worker before releasing the parent.", ToString(state.worker.flow));
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

void PrintEventHeader(std::string_view name, core::Timestamp timestamp, const AppState& state)
{
    std::print("[event {}] {} at {} (+{})", state.eventCount, name, timestamp, timestamp - state.startTimestamp);
}

struct EventVisitor final
{
    AppState& state;

    void operator()(const platform::QuitRequestedEvent& event) const
    {
        PrintEventHeader("QuitRequested", event.timestamp, state);
        std::println();
        RequestShutdown(state, "quit event");
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::println(" window={}", event.windowId);
        RequestShutdown(state, "window close request");
    }

    void operator()(const platform::KeyboardKeyEvent& event) const
    {
        PrintEventHeader("KeyboardKey", event.timestamp, state);
        std::println(" key={} pressed={} repeat={}", ToString(event.physicalKey), event.pressed, event.repeat);
        if (event.pressed && !event.repeat)
        {
            HandleCommand(state, event.physicalKey);
        }
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Other platform event", event.timestamp, state);
        std::println(" observed while the process worker may be blocked.");
    }
};

void DrainEvents(AppState& state)
{
    while (std::optional<platform::PlatformEvent> event = state.runtime.EventPoll())
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
    const std::string title = "Process Runner | worker " + workerText + " | " + state.lastAction;
    state.windows.front().window.SetTitle(title);
}

[[nodiscard]] WindowSlot CreateParentWindow(platform::Runtime& runtime)
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

    return WindowSlot{.window = runtime.WindowCreate(desc)};
}

[[nodiscard]] int RunInteractiveParent(const Options& options, const std::filesystem::path& selfExecutable, int argc, char** argv)
{
    platform::Runtime runtime = platform::Runtime::Create();
    runtime.HintPush(platform::hints::MouseFocusClickThrough{true});
    runtime.HintPush(platform::hints::MouseAutoCapture{false});
    runtime.Initialize("Ponder Platform Responsive Process Runner", "0.1.0", "org.ponder.examples.platform.responsive-process-runner");
    const core::Timestamp startTimestamp = runtime.TimeNow();
    std::vector<WindowSlot> windows;
    windows.reserve(1);
    windows.push_back(CreateParentWindow(runtime));

    AppState state{.runtime = runtime,
                   .options = options,
                   .selfExecutable = selfExecutable,
                   .windows = windows,
                   .startTimestamp = startTimestamp,
                   .lastTimestamp = startTimestamp};

    PrintUsage(argc > 0 ? argv[0] : "ponder-platform-4-responsive-process-runner");
    std::println("Self executable: {}", QuoteText(io::PathToUtf8(selfExecutable)));
    StartWorker(state, FlowKind::Finite);

    auto nextTitleUpdate = state.startTimestamp;
    std::exception_ptr deferredFailure;
    while (!state.windows.empty() || WorkerIsActive(state.worker))
    {
        try
        {
            core::Timestamp now = state.lastTimestamp;
            if (deferredFailure == nullptr)
            {
                DrainEvents(state);
                now = state.runtime.TimeNow();
                state.lastTimestamp = now;
            }
            DrainWorkerMessages(state, now);

            if (state.shutdownRequested && !WorkerIsActive(state.worker))
            {
                ReleaseParentWindow(state);
            }

            if (deferredFailure == nullptr && now - nextTitleUpdate >= 250ms)
            {
                UpdateWindowTitle(state);
                nextTitleUpdate = now;
            }

            if (options.autoCloseAfter && !state.shutdownRequested && now - state.startTimestamp >= *options.autoCloseAfter)
            {
                std::println("Auto-close duration reached after {}.", now - state.startTimestamp);
                RequestShutdown(state, "auto close");
            }
        }
        catch (...)
        {
            if (deferredFailure != nullptr)
            {
                throw;
            }

            deferredFailure = std::current_exception();
            std::println(stderr, "A synchronous failure occurred; completing the active process flow "
                                 "before rethrowing on the owner thread.");
            RequestShutdown(state, "failure cleanup");
        }

        std::this_thread::sleep_for(8ms);
    }

    if (deferredFailure != nullptr)
    {
        std::rethrow_exception(deferredFailure);
    }
    return 0;
}

[[nodiscard]] int RunHeadlessParent(const Options& options, const std::filesystem::path& selfExecutable)
{
    std::println("Running headless parent without Runtime.");
    platform::ProcessDesc desc = MakeChildProcessDesc(selfExecutable, options.childSleepMilliseconds, options.childExitCode,
                                                      {"headless parent", "alpha beta", "angstrom-\xC3\x85"});

    auto processResult = platform::LaunchProcess(desc);
    if (!processResult)
    {
        PrintError("LaunchProcess", processResult.GetError());
        std::println("Correct the executable path or host condition, then retry the command.");
        return 1;
    }

    platform::Process process = std::move(processResult).GetValue();
    std::println("Headless parent launched child; blocking Wait() is acceptable here.");
    auto waitResult = process.Wait();
    if (!waitResult)
    {
        PrintError("Process::Wait", waitResult.GetError());
        std::println("Retrying Process::Wait once on the same process owner.");
        waitResult = process.Wait();
        if (!waitResult)
        {
            PrintError("Process::Wait retry", waitResult.GetError());
            std::println("The Process owner will be abandoned for private asynchronous cleanup.");
            return 1;
        }
    }

    std::println("Headless child completed with {}.", ExitStatus{*waitResult});
    return 0;
}

int RunChildMode(const Options& options, int argc, char** argv)
{
    std::println("[child] started with argc={}", argc);
    for (int index = 0; index < argc; ++index)
    {
        std::println("[child] argv[{}]={}", index, QuoteText(argv[index]));
    }

    if (!options.childPayload.empty())
    {
        std::println("[child] payload arguments after -- delimiter:");
        for (std::size_t index = 0; index < options.childPayload.size(); ++index)
        {
            std::println("[child] payload[{}]={}", index, QuoteText(options.childPayload[index]));
        }
    }

    std::println("[child] sleeping for {} ms.", options.childSleepMilliseconds);
    std::this_thread::sleep_for(std::chrono::milliseconds{options.childSleepMilliseconds});
    std::println("[child] exiting with code {}.", options.childExitCode);
    return options.childExitCode;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        auto optionsResult = ParseOptions(argc, argv);
        if (!optionsResult)
        {
            std::println(stderr, "ponder-platform-4-responsive-process-runner failed: {}", optionsResult.GetError());
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
            std::println(stderr, "ponder-platform-4-responsive-process-runner failed: {}", selfExecutable.GetError());
            return 1;
        }

        return options.headlessParent ? RunHeadlessParent(options, *selfExecutable) : RunInteractiveParent(options, *selfExecutable, argc, argv);
    }
    catch (const core::Exception& exception)
    {
        std::println(stderr, "ponder-platform-4-responsive-process-runner terminated with a ponder exception: {}", exception.GetMessage());
        return 1;
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "ponder-platform-4-responsive-process-runner terminated with an exception: {}", exception.what());
        return 1;
    }
    catch (...)
    {
        std::println(stderr, "ponder-platform-4-responsive-process-runner terminated with an unknown "
                             "exception.");
        return 1;
    }
}
