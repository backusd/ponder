# Codex Guidance: Application

Read the root `AGENTS.md` before this file.

## Scope

Work here on the reusable high-level desktop lifecycle above `ponder_platform`: the non-overridable run loop, typed application callbacks, base-owned
windows, orderly dialog draining, managed-process shutdown, and backend-independent wait/wake behavior.

## Local Rules

- Keep public headers under `include/ponder/application/` and use `ponder::application`.
- Keep `ponder::core` and `ponder::platform` as public dependencies. Do not include SDL, OS, render, or UI types in this library.
- Keep `Application` state directly in the public class declaration. Do not add an Application PIMPL, builder, runtime-backend abstraction, generic event
  callback, overridable exit predicate, `IterationMode`, or `IterationSchedule`.
- Invoke virtual methods only from `Run()` after the derived object is fully constructed and before base destruction. Never invoke a derived hook from
  the `Application` constructor or destructor.
- Keep the base-owned startup order explicit: `Runtime::Create()`,
  `PrePlatformInitialization(Runtime&)`, then `Runtime::Initialize()` with the
  `ApplicationDesc` metadata. Treat the `Runtime&` passed to the hook as a
  hook-scoped borrow whose backend implementation is constructed but not yet
  initialized; derived code may configure typed hints on that implementation
  but must not retain the borrow. `Runtime::Initialize()` alone initializes the
  backend. Do not add a general Runtime getter;
  forward safe subsystem services and keep event polling, waiting, waking,
  shutdown, and raw window creation private to the base.
- Keep the six protected dialog forwarders (`DialogShowOpenFile`, `DialogShowSaveFile`, `DialogShowOpenFolder`, `DialogGetPendingCount`,
  `DialogHasPending`, and `DialogGetPending`) Result-bearing and `noexcept`. The three submission methods mirror platform's exception-shielded Results;
  the three query methods add Application lifecycle and owner-thread validation around platform's direct `noexcept` observers. Do not expose
  `DialogPollCompletion` or `DialogShutdown` to derived applications; they remain base-loop and cleanup responsibilities. Consume direct internal
  observer values normally, check the internal `DialogShutdown` Result, preserve the first failure, and continue mandatory cleanup.
- Preserve `Run()` as the non-virtual owner of lifecycle and shutdown policy. A quit request and last-window close must converge on the same cleanup
  path; derived code cannot veto or replace it.
- Keep one virtual hook per `PlatformEvent` alternative. Perform mandatory quit/window/dialog handling in the base and document whether a hook runs
  before or after that handling.
- Keep every `Window` owned in the base registry. Expose only IDs and borrowed references to derived code. Defer destruction of a dialog parent until its
  completion has been polled and the platform lease is released. Never move from a borrowed `Window&`, and never allow a new dialog to use a logically
  closing parent.
- Do not claim native SDL dialogs can be programmatically canceled. Hide closing parents and pump until every outstanding request completes.
- Track only processes launched through `Application::ProcessLaunch`. Poll them without blocking while windows or dialogs remain. At final teardown,
  force-and-wait only for opted-in processes and explicitly detach the others.
- Do not claim detachment notifies a child. A portable parent-exit notification requires a future cooperative IPC/liveness contract established at
  launch.
- Keep `Wake`, `RequestUpdate`, `RequestRender`, and `SetExitCode` safe to call from other threads. Synchronize Runtime wake access against teardown. All
  other protected APIs are Run-thread-only.
- Preserve event-driven update/render scheduling. Continuous work must request its next update or render explicitly; do not introduce an implicit busy
  loop or fixed global frame schedule.
- Contain callback failures, finish mandatory cleanup, and rethrow the first failure after Runtime teardown. No exception may escape a destructor.
- Add `std::formatter` specializations and stream insertion for every new public or private struct, class, or enum that is intended to be formatted.
- Use `Result` for locally recoverable dialog and process failures. Throw `ponder::core::Exception` with `APPLICATION_EXCEPTION` for lifecycle,
  ownership, programming, and wrong-thread failures outside the exception-shielded dialog forwarding surface.

## Verification

- Build `ponder_application`, `ponder_application_header_tests`, `ponder_application_tests`, and
  `ponder_application_1_basic_application` after relevant changes.
- Run `ponder_application_tests`; do not run graphical examples in automated verification.
- Compile behavioral tests with `PONDER_PLATFORM_USE_MOCK_RUNTIME` and the existing concrete mock-runtime source variant. Do not add production testing
  hooks, virtual runtime providers, or mutable backend-override globals.
- Cover explicit Create/hook/Initialize lifecycle order, descriptor forwarding, pre-initialization hints, every typed event hook, single and last-window closure, quit convergence,
  dialog-parent deferral, callback-failure cleanup, managed-process policies, cross-thread wake, single-use Run, formatting, and public-header
  self-containment.
- Use live SDL integration tests only for platform wait/wake behavior that cannot be established deterministically with the mock, and serialize them
  under the existing platform SDL resource lock.
