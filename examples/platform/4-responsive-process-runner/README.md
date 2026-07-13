# 4 Responsive Process Runner

This example demonstrates shell-free process launching without blocking the
platform event pump. It runs in three roles:

- normal interactive parent with a native window and responsive event loop;
- deterministic `--child` mode launched by the parent;
- `--headless-parent` mode, proving `LaunchProcess()` does not require a
  `PlatformRuntime`.

The default interactive run starts one bounded child and waits for it on a
worker thread. Termination and abandonment flows require explicit key commands.

## Features Exercised

- `ProcessDesc` construction from an executable path and owned UTF-8 arguments.
- Verbatim arguments containing spaces and non-ASCII text; no shell parsing or
  quoting is involved.
- Inherited environment and stdout/stderr, with null stdin from the platform
  process API.
- `Process` move-only ownership and launch-thread affinity: the worker that
  launches a child also terminates, waits, and destroys it.
- Blocking `Process::Wait()` kept off the platform/runtime event thread.
- `ProcessNormalExit`, `ProcessSignalTermination`, and
  `ProcessUnknownTermination` formatting through an exhaustive visitor.
- `GracefulPreferred` and `Force` termination commands where supported.
- Destruction/abandonment of a live process without blocking or terminating the
  child; private platform cleanup reaps it asynchronously.

## Controls

- `F1`: print controls.
- `N`: start another bounded normal-exit child if no worker is active.
- `T`: start a long-running child and wait for a termination command.
- `G`: request graceful-preferred termination for the active termination flow.
- `F`: request forced termination for the active termination flow.
- `A`: start an abandonment flow if no worker is active.
- `Q` or `Escape`: request shutdown. If a termination flow is active, shutdown
  sends a forced termination command so the event thread does not wait forever.

Useful command-line options:

```powershell
ponder-platform-4-responsive-process-runner --auto-close-ms 1500
ponder-platform-4-responsive-process-runner --headless-parent
```

## Process Contracts Demonstrated

`Process::Wait()` is blocking. This example calls it only from worker threads
or from explicit headless mode, never from the platform event pump. There is no
public nonblocking status query yet, so responsive applications should own the
process on a worker and communicate with the UI/event thread using their own
message channel.

The platform process API launches directly. It does not invoke a shell and does
not quote or reinterpret arguments. The child receives an executable `argv[0]`
followed by the exact UTF-8 argument strings supplied in `ProcessDesc`.

Termination results are host-dependent. A normal exit is required for the
default flow. Forced or graceful termination may report as a normal, signal, or
unknown status depending on the operating system and backend.

Destroying a live `Process` does not wait and does not terminate the child. The
platform library keeps private waitable state so the child can be reaped later.

## Build And Run

Configure and build a supported preset with examples enabled:

```powershell
cmake --preset windows-msvc-debug -DPONDER_BUILD_EXAMPLES=ON
cmake --build --preset windows-msvc-debug --target ponder_platform_4_responsive_process_runner
```

Run a short interactive smoke test:

```powershell
.\build\windows-msvc-debug\bin\ponder-platform-4-responsive-process-runner.exe --auto-close-ms 1500
```

Run the headless parent demonstration:

```powershell
.\build\windows-msvc-debug\bin\ponder-platform-4-responsive-process-runner.exe --headless-parent
```
