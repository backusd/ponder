# 4 Responsive Process Runner

This example demonstrates shell-free process launching without blocking the
platform event pump. It runs in three roles:

- a normal interactive parent with a native window and responsive event loop;
- a deterministic `--child` mode launched by the parent; and
- a `--headless-parent` mode proving that `LaunchProcess()` does not require a
  `Runtime`.

The default interactive run starts one bounded child and waits for it on a
worker thread. Termination and abandonment flows require explicit key commands.

## Features Exercised

- Explicit `Runtime::Create()` construction of the uninitialized backend,
  typed hint configuration on that object, `Runtime::Initialize()` activation
  with application metadata, direct window construction,
  and direct window-title updates.
- `ProcessDesc` construction from an executable path and owned UTF-8 arguments.
- Verbatim arguments containing spaces and non-ASCII text; no shell parsing or
  quoting is involved.
- Inherited environment and stdout/stderr, with null stdin from the platform
  process API.
- Deliberate local handling of the retained `Result` contracts from
  `LaunchProcess()`, `Process::Wait()`, and `Process::Terminate()`.
- `Process` move-only ownership and launch-thread affinity: the worker that
  launches a child also terminates, waits for, and destroys it.
- Blocking `Process::Wait()` kept off the platform/runtime event thread.
- `ProcessNormalExit`, `ProcessSignalTermination`, and
  `ProcessUnknownTermination` formatting through an exhaustive visitor.
- `GracefulPreferred` termination, including the platform's fallback to force
  when graceful termination is unsupported, and explicit `Force` termination.
- Destruction or abandonment of a live process without blocking or terminating
  the child; private platform cleanup reaps it asynchronously.
- A `noexcept` worker-entry boundary that captures an exceptional worker
  failure for owner-thread rethrow without losing its dynamic exception type.

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
ponder-platform-4-responsive-process-runner --headless-parent --sleep-ms 25 --exit-code 7
ponder-platform-4-responsive-process-runner --inject-worker-exception
```

`--sleep-ms` and `--exit-code` configure the deterministic child. Invalid
interactive input remains an example-local `ponder::core::Result` because the
caller can print the error and correct the command line immediately.

## Failure Contract

Runtime creation, hint changes, explicit initialization, window creation, event
polling, and window title updates use direct platform contracts. Their rare failures propagate to
the high-level boundary in `main()`. That boundary catches one
`ponder::core::Exception` type and prints its message. Platform exceptions
embed their formatted platform error code inside that message rather than
carrying a typed payload. An unrelated standard exception receives its own
terminal diagnostic; it is not repackaged as an ordinary process result.

Process launch, wait, and termination deliberately retain `Result` because
missing executables, child-state changes, wait failures, and host rejection are
external outcomes the immediate orchestration layer can act on. A launch
failure is reported locally and leaves the worker idle so the user can retry
with `N`, `T`, or `A`. A wait failure is reported and retried once. A failed
`GracefulPreferred` termination is reported and escalated once to `Force`;
an explicit force request is not weakened. If the wait retry or force
termination fails, the worker reports that final outcome and abandons the
process safely. The no-throw `Process` destructor hands any still-live waitable
state to the private asynchronous reaper. Normal and nonzero child exits are
successful wait results and are printed as exit-status data rather than
treated as API failures.

Those retained-result functions may still throw for invalid lifecycle or
threading use, forged values, or unexpected internal failures. The `noexcept`
worker-entry wrapper catches every exception into `std::exception_ptr` and
marks the worker finished, so no exception can cross the thread entry point.
The owner thread drains queued output, joins the finished worker, and then
rethrows the original dynamic exception into its normal exception boundary. The
exception is neither flattened into a string nor mislabeled as a launch, wait,
or termination `Result`. `--inject-worker-exception` deterministically
exercises this marshalling path.

Headless parent mode has no event loop to protect, so blocking `Wait()` is
intentional there. It still handles launch and wait `Result` failures at that
same orchestration boundary, retries a wait failure once, and returns a nonzero
application status after reporting an unrecovered failure.

## Responsiveness, Cancellation, And Teardown

`Process::Wait()` is blocking. Interactive mode calls it only from a worker,
never from the platform event pump. While the worker waits, the owner thread
continues polling and printing platform events, updating the title, accepting
valid commands, and responding to close requests. There is no public
nonblocking process-status query yet, so responsive applications should use a
similar worker or a higher-level orchestration service.

The termination worker waits for a command before it calls `Wait()`. `G` asks
for `GracefulPreferred`; platform retries with `Force` when the backend reports
that graceful termination is unsupported, and the example escalates any
returned graceful failure to one explicit force attempt. `F` requests force
immediately. A quit, `Q`, `Escape`, or parent-window close escalates an active
termination flow to a force request. Bounded normal and abandonment workers are
allowed to finish instead of being killed.

Worker messages are transferred through a mutex-protected queue. On every
iteration the owner thread prints all transferred messages before retiring and
joining a worker that reported completion. Shutdown keeps the parent window
and runtime alive while any worker is active. If the event loop or worker
produces an exception while work remains active, interactive mode stores it,
requests shutdown, forces a termination flow if necessary, and rethrows only
after the worker has completed. The `WorkerController` destructor provides a
final no-throw force-command backstop before joining its `std::jthread`.

Messages produced by one worker retain their queue order. The child inherits
the console directly, however, so child stdout/stderr may interleave with
owner-thread event and worker-message output; the example does not claim a
global cross-process console order.

After terminal worker output has been consumed, the worker is joined, the
parent window is released, and the runtime is destroyed last. Only then does a
deferred exception reach `main()`. This preserves output ordering, process
thread affinity, and responsive close and unwinding paths without allowing an
exception to escape a thread entry point or destructor.

The platform process API launches directly. It does not invoke a shell and does
not quote or reinterpret arguments. The child receives an executable `argv[0]`
followed by the exact UTF-8 argument strings supplied in `ProcessDesc`.
Termination status is host-dependent: a terminated process can be reported as
a normal, signal, or unknown status depending on the operating system and
backend.

## Automated And Manual Checks

Safe automated runs are intentionally side-effect-light:

- `--headless-parent --sleep-ms 25 --exit-code 0` covers shell-free launch,
  argument transport, a normal exit, local wait handling, and clean teardown
  without creating a runtime.
- Repeating that command with `--exit-code 7` covers a nonzero exit as status
  data rather than a failed `Wait()`.
- A dummy-video-driver `--auto-close-ms` run covers direct runtime/window setup,
  continued event pumping while the startup worker waits, timer-driven
  shutdown, terminal-output draining, and teardown without native UI.
- A dummy-driver `--inject-worker-exception` run is expected to fail after the
  owner thread joins the worker and rethrows the captured
  `std::runtime_error` through the distinct standard-exception boundary.
- An intentionally invalid video driver is expected to fail direct runtime
  initialization through the `ponder::core::Exception` boundary with the
  platform error code embedded in its message.
- The focused platform process tests use the dedicated
  `ponder_platform_process_helper` and private backend seams for missing
  executable launch failure, injected wait failure, termination failure,
  graceful-to-force fallback, unexpected backend exceptions, and abandoned
  child cleanup. Prefer those deterministic seams over corrupting or deleting
  a local executable to manufacture failures.

Native window interaction and host termination semantics remain manual. On a
desktop host:

1. Start with `& $runner --sleep-ms 5000`. While the longer startup child waits,
   move or resize the window and press `F1`; confirm events and title updates
   remain responsive and the final worker message appears before the worker
   becomes idle.
2. Run with `--exit-code 7`, then press `N`; confirm the nonzero normal status is
   reported and a later run can be started.
3. Press `T`, then `G`; confirm the graceful-preferred request completes and an
   exhaustive exit-status alternative is printed. Repeat with `T`, then `F`.
4. Start a termination flow and close the window (or press `Q`/`Escape`). Confirm
   shutdown sends the force command, keeps the parent alive until the terminal
   worker output is consumed, and then releases the window cleanly.
5. Press `A`; confirm `Process` destruction returns immediately, the worker
   reports that private cleanup owns the child, and the UI remains responsive.

## Build And Run

Configure an isolated Debug tree with examples and tests enabled, and with
render plus UI/render integration disabled:

```powershell
$platformBuild = "build\windows-msvc-debug-plat-eh017"
cmake --fresh --preset windows-msvc-debug -B $platformBuild `
    -DPONDER_BUILD_TESTS=ON `
    -DPONDER_BUILD_EXAMPLES=ON `
    -DPONDER_BUILD_RENDER=OFF `
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build $platformBuild --config Debug --parallel 4 --target `
    ponder_platform_4_responsive_process_runner `
    ponder_platform_process_helper `
    ponder_platform_tests `
    ponder_platform_backend_tests
```

Run the focused deterministic process suites:

```powershell
$unitTests = Join-Path $platformBuild "bin\Debug\ponder_platform_tests.exe"
$backendTests = Join-Path $platformBuild "bin\Debug\ponder_platform_backend_tests.exe"
& $unitTests --gtest_filter=ProcessTypesTests.*
& $backendTests --gtest_filter=ProcessBackendTests.*
```

Run the headless normal and nonzero-exit checks:

```powershell
$runner = Join-Path $platformBuild "bin\Debug\ponder-platform-4-responsive-process-runner.exe"
& $runner --headless-parent --sleep-ms 25 --exit-code 0
& $runner --headless-parent --sleep-ms 25 --exit-code 7
```

Run the interactive example on a desktop host:

```powershell
& $runner
```

For a short dummy-driver smoke run that restores the caller's environment:

```powershell
$previousVideoDriver = $env:SDL_VIDEO_DRIVER
try {
    $env:SDL_VIDEO_DRIVER = "dummy"
    & $runner --auto-close-ms 500 --sleep-ms 25 --exit-code 0

    $workerOutput = & $runner --inject-worker-exception 2>&1 | Out-String
    $workerExitCode = $LASTEXITCODE
    $workerOutput
    if ($workerExitCode -ne 1 -or
        $workerOutput -notmatch "Injected worker exception for verification") {
        throw "Worker-exception marshalling check did not produce its expected failure."
    }

    $env:SDL_VIDEO_DRIVER = "ponder-invalid-video-driver"
    & $runner --auto-close-ms 500
    if ($LASTEXITCODE -ne 1) {
        throw "Invalid-driver platform exception check unexpectedly succeeded."
    }
} finally {
    if ($null -eq $previousVideoDriver) {
        Remove-Item Env:SDL_VIDEO_DRIVER -ErrorAction SilentlyContinue
    } else {
        $env:SDL_VIDEO_DRIVER = $previousVideoDriver
    }
}
```
