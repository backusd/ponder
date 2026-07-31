# Application Library Boundary

`ponder_application` owns the high-level desktop application lifecycle above `ponder_platform`. It provides the reusable event loop, typed event
dispatch, window ownership, orderly dialog draining, managed background-process exit policy, and backend-independent wake surface used by a derived
application. The public namespace is `ponder::application`; public headers live under `include/ponder/application/`.

## Dependencies And Ownership

- `ponder_application` publicly depends on `ponder_core` and `ponder_platform` and has no direct SDL or operating-system dependency.
- One `Application` owns one `ponder::platform::Runtime` for the duration of `Run()`.
- `Application` stores its state directly. It has no PIMPL, backend interface, or application builder.
- A derived application is fully constructed before `Run()` invokes any virtual hook. No derived hook is called from the base constructor or destructor.
- An `Application` instance is non-copyable, non-movable, and single-use. `Run()` must execute on the process-entry thread required by platform.

## Lifecycle Contract

`Run()` is the non-virtual interface that owns policy. The overridable lifecycle hooks are:

1. `PrePlatformInitialization(Runtime&)`, after `Runtime::Create()` constructs
   and returns the owner of an uninitialized backend implementation and before
   the base calls `Runtime::Initialize()`;
2. `OnStart()`, after Runtime initialization;
3. event-specific hooks and requested `OnUpdate(Duration)` / `OnRender()` work;
4. `OnStop()`, after windows, dialogs, and managed processes have been resolved but before Runtime destruction.

The loop is event-driven. It performs one initial update and render. A platform event requests another update and render; derived code can explicitly
call `RequestUpdate()` or `RequestRender()` to schedule more work. Both methods wake the wait from any thread. Calling `Wake()` alone interrupts the
wait so owner-thread code can recheck shared application state without imposing a fixed frame schedule.

Application owns the exact startup sequence: `Runtime::Create()`, the
`PrePlatformInitialization(Runtime&)` hook, and then `Runtime::Initialize()`
with the `ApplicationDesc` metadata. `Create()` has already constructed the
backend object at this point; typed hints configure that object, and
`Initialize()` alone initializes it. Runtime no longer accepts a templated
pre-initialization callback. The `Runtime&` supplied to the hook is a synchronous
configuration borrow of the not-yet-initialized runtime and must not be retained.
Application deliberately exposes no general Runtime getter: protected
forwarding methods provide clipboard, dialog, timing, display, mouse, and URI
services without exposing event-loop control or an untracked window factory.

The six protected dialog forwarders for submission and pending-state observation are Result-bearing and `noexcept`. The three submission methods
mirror platform's exception-shielded Results. The three query methods add Application lifecycle and owner-thread validation around platform's direct
`noexcept` observers, so derived code can handle those validation failures locally. Completion polling and dialog shutdown remain private lifecycle
work owned by the base rather than additional derived-facing controls; the base consumes direct completion observations and checks the shutdown Result.

The first exception from a derived hook, or first unrecoverable Result error from base-owned dialog cleanup, becomes the primary failure. The base stops
invoking event and process callbacks, closes owned windows, drains dialogs, resolves managed processes, shuts down Runtime, and then reports that failure
after cleanup. No exception escapes a destructor.

## Event Dispatch

Every alternative of `ponder::platform::PlatformEvent` has one protected virtual hook. The base performs mandatory handling around selected events:

- `QuitRequestedEvent` begins shutdown and closes every logical window. A derived hook observes the request but cannot veto the exit policy.
- `WindowCloseRequestedEvent` marks the window closing, calls the derived hook while the borrowed window is still valid, then hides and destroys it when
  no native dialog lease remains.
- `DialogCompletedEvent` is delivered before a deferred parent window is destroyed.

There is deliberately no generic public `OnEvent` hook and no derived `ShouldExit` override.

## Window Ownership

All windows in an Application run are created through `Application::WindowCreate` and remain owned by the base-class registry. Derived applications
keep `WindowId` values and may borrow `Window&` through `WindowCreate`, `WindowFind`, or `WindowGet`; they never own a `Window`. Moving from a borrowed
`Window&` is forbidden because doing so would invalidate the base-owned registry entry. A borrow remains valid
until the corresponding close operation or close-request callback completes. There is no post-initialization Runtime accessor through which derived
code can call `Runtime::WindowCreate` and bypass this registry.

Closing a window is non-vetoable. The native window is hidden immediately when possible. If it is the parent of a pending native dialog, the base keeps
the hidden `Window` object alive until platform reports the dialog completion and releases its lease.

## Dialog Exit Policy

After the last logical window closes, Application accepts no new work through its managed creation APIs and continues pumping platform events until all
dialog completions have been consumed. It then destroys deferred parent windows, calls `Runtime::DialogShutdown()`, and explicitly handles its
`VoidResult`. Failures from internal pending queries, completion handling, or shutdown are retained while mandatory cleanup continues.

SDL3 exposes no portable API for canceling an already-open native file or folder dialog. Therefore Application cannot truthfully force-close such a
dialog. It hides closing parents and waits for the user/backend to complete or cancel each request. Applications that require immediate cancellability
must use a future in-application dialog implementation rather than a native SDL dialog.

## Managed Process Exit Policy

`ProcessLaunch(BackgroundProcessDesc)` is the only process launch path tracked by Application. It returns a strong, monotonic `BackgroundProcessId` and
retains the move-only platform `Process`. Direct `ponder::platform::LaunchProcess` calls are outside Application's registry and exit policy.

Application observes managed children with nonblocking `Process::TryWait()` while the desktop is live. Once all windows and dialogs are gone:

- If `forceProcessTerminationOnApplicationExit` is true, Application requests `Force` termination and performs the required blocking `Wait()` to
  confirm exit. If termination fails it still waits for natural exit; if the backend cannot confirm exit, it logs the failure and releases tracking.
- If the flag is false, Application calls `OnProcessDetached`, releases public ownership, and exits without terminating or waiting for that child. The
  platform's abandoned-process reaper retains only the native tracking handle until the child eventually exits.

There is no portable way to notify an arbitrary child that its parent application exited. Windows and POSIX normally allow a detached child to
continue, and the current process API has no cooperative IPC channel. A real notification contract requires a future launch-time parent-liveness pipe
or another explicit protocol understood by the child; `GracefulPreferred` termination is not a substitute because it may escalate to force.

## Threading And Wait/Wake

Runtime, window, dialog, and managed-process operations are bound to the `Run()` thread. `Wake`, `RequestUpdate`, `RequestRender`, and `SetExitCode` are
the explicit cross-thread exceptions. Application synchronizes wake calls against Runtime teardown; platform implements the backend-specific event-queue wake and
wait primitives without exposing SDL types.

Application timing uses `ponder::core::Timestamp` and `ponder::core::Duration` in the core steady-clock domain. It neither observes nor depends on SDL's
native tick epoch.

Managed processes are polled at a bounded cadence because they do not generate platform events. All other idle waits are event driven. Dialog backend
completion also wakes the platform event wait.

## Errors And Non-Responsibilities

Expected dialog, process-launch, and process-control failures remain `ponder::core::Result` values. Protected dialog calls are `noexcept` and preserve
the detailed platform error returned after platform catches and logs any underlying exception. Programming, lifecycle, ownership, and wrong-thread
misuse outside that dialog boundary throw `ponder::core::Exception` created with `APPLICATION_EXCEPTION`, which embeds `ApplicationErrorCode` in the
message without a typed payload.

This library does not own rendering, retained UI, fixed-timestep simulation, frame limiting, chemistry, project/workflow policy, plugins, or process IPC.
Render and UI may become dependencies only after their exact lifecycle boundary is designed.
