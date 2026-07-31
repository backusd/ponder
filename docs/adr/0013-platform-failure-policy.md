# ADR 0013: Platform Failure Policy

## Status

Accepted. The namespace-preservation clauses from the original error-handling
migration were superseded on 2026-07-23 by the platform-wide migration to
`ponder::platform`. The typed platform-error payload clauses were superseded
later that day by the single-`ponder::core::Exception` contract below; that
exception-type simplification left the exception-first versus retained-result
policy unchanged.
The 2026-07-24 flat `Runtime` consolidation renamed the runtime entry points,
removed manager facades, and changed clipboard text access to direct throwing
contracts. A later clipboard amendment restored locally actionable result
contracts, and the 2026-07-28 nonblocking process-wait amendment added
`Process::TryWait`; those amendments are reflected below.
The 2026-07-29 dialog-boundary amendment makes every public dialog operation
`noexcept`; the four fallible operations are Result-bearing so no ordinary
exception leaks from that API, while five observer operations return direct
values under documented preconditions. The
same amendment unifies platform timing on `ponder::core::Timestamp::Now()` and
removes all platform use of SDL's tick clock.
The 2026-07-30 lifecycle amendments split the direct runtime contract into
argument-free `Runtime::Create()`, explicit typed hint configuration, and
throwing `Runtime::Initialize(...)`; remove the callback-taking factory; and
make `Create()` eagerly construct the uninitialized implementation.

## Related Decisions

- [ADR 0007: SDL3 Platform Backend](0007-sdl3-platform-backend.md)
- [ADR 0012: Core Namespace And Failure Policy](0012-core-namespace-and-failure-policy.md)

## Context

ADR 0007 established a result-first platform boundary in which runtime and window
construction, ordinary SDL operations, and external-service failures all returned
`Result<T>`. That made every possible failure explicit, but it also required callers to
locally inspect and propagate failures for which they had no useful recovery.

ADR 0012 subsequently established the project-wide distinction between expected,
locally actionable failures and exceptional failures that should reach a higher-level
handler. It also moved core APIs from `pond::core` to `ponder::core` and renamed the
project exception root to `ponder::core::Exception`.

Platform needed to adopt that distinction without redesigning its unrelated public API.
When this failure migration was approved, its scope preserved the then-existing
`pond::platform` namespace, public include paths, ownership model, descriptors, event
types, and CMake targets. Only failure-related return types and `noexcept` declarations
changed in that migration. The later namespace migration is an independent amendment.

## Decision

### Namespace And Exception Type

Platform source and public headers now use `ponder::platform`. References to core types
use `ponder::core`; no compatibility namespace is introduced for either former name.

Exceptional platform failures throw `ponder::core::Exception`. Code-bearing
failures are constructed at the failure site with `PLATFORM_EXCEPTION`, which
embeds the formatted `ponder::platform::PlatformErrorCode` in the message.
Exceptional failures that do not need a platform code use `PONDER_EXCEPTION`.
Platform does not use `ExceptionWithData<PlatformErrorCode>` or define a public
exception alias or subclass. A dedicated exception type is deferred until a
real catch handler needs type-specific recovery.

`PlatformErrorCode` names and numeric values remain stable. `ToErrorCode`, the enum-name
formatter, and stream insertion retain their existing signatures and forged-value
fallback behavior. `DisplayOrientation::Unknown` remains valid unavailable data rather
than an exceptional forged value.

### Exceptional Failures

Platform throws for failures that should never or only rarely occur and for which the
current caller cannot complete a useful recovery. These include:

- runtime or window construction failure;
- ordinary SDL initialization, query, and mutation failure except for the explicitly
  retained result operations below;
- invalid lifecycle phase and wrong-thread use;
- invalid descriptors and forged closed-enum values;
- unsupported ordinary window, cursor, or hint operations; and
- malformed backend data that violates the platform abstraction.

Wrong-thread public calls normally throw an exception whose message includes
`PlatformErrorCode::WrongThread`. Invalid descriptors and forged enum inputs normally
include `PlatformErrorCode::InvalidArgument`. Public dialog entry points contain those
failures and return `BackendFailure`; the original formatted message and embedded
platform code remain diagnostic text and are not parsed for control flow. Moved-from use and impossible ownership or
lifecycle invariants outside that boundary remain release-active `PONDER_VERIFY`
failures and therefore throw the plain `ponder::core::Exception` base.

`Runtime::Create()` constructs the heap-stable `RuntimeImpl` but does not call
its `Initialize()` method. Typed hints configure that existing implementation
before initialization. `Runtime::Initialize(...)` alone validates metadata and
calls `RuntimeImpl::Initialize()`. Calling an ordinary service before successful
initialization is a programmer error guarded by `PONDER_ASSERT`; Runtime
pass-throughs also assert that the implementation is non-null.

The following public APIs are direct or optional contracts:

- `Runtime::Create` returns `Runtime`;
- `Runtime::Initialize` returns `void`;
- `Runtime::WindowCreate` returns `Window`;
- `Runtime::DisplayEnumerate` returns `std::vector<DisplayInfo>`;
- `Runtime::MouseSetSystemCursor`, `MouseShowCursor`, and
  `MouseHideCursor` return `void`;
- runtime hint push/pop/clear returns `void`, while hint lookup returns
  `std::optional<T>`;
- every `Window` operation that previously returned `Result` or `VoidResult` becomes a
  direct value or `void`, except `GetNativeHandle` and `GetDisplayId`.

Removing `Result` does not imply `noexcept`. Any direct API that can allocate, verify a
contract, call a fallible backend operation, or construct an exception remains capable of
throwing.

### Retained Value-Based Failures

`Result<T>` remains when failure is a normal input, capability, or external-state
outcome and the caller or its immediate caller has a concrete recovery. The non-dialog
public platform result surface contains these twelve operations:

- `Runtime::ClipboardGetText` retains unavailable access, malformed external
  text, and host failures so callers can recover immediately.
- `Runtime::ClipboardSetText` retains invalid input, unavailable access, and
  host failures so callers can correct, retry, or report the operation.

- `Runtime::DisplayGetInfo` retains `NotFound` for a stale nonzero
  `DisplayId` so callers can re-enumerate displays.
- `Runtime::MouseSetCapture` retains unsupported or rejected host
  capability so callers can continue without global capture.
- `Runtime::MouseGetGlobalPosition` retains unsupported or unavailable
  global coordinates so callers can use window-relative input.
- `Runtime::UriOpenExternal` retains invalid user input and host
  launch/capability failure so callers can show or copy the URI.
- `Process::Wait` retains child and operating-system wait failures so callers
  can retry or report the process failure.
- `Process::TryWait` retains nonblocking child-status and operating-system wait
  failures so event-loop orchestration can retry without blocking.
- `Process::Terminate` retains graceful or forced termination failures so
  callers can retry or escalate termination.
- `LaunchProcess` retains invalid executable/input and operating-system launch
  failures so callers can correct, retry, or report.
- `Window::GetNativeHandle` retains incompatible graphics mode or an unsupported
  native driver so callers can select another rendering path.
- `Window::GetDisplayId` retains a transiently unresolved or disconnected
  display so callers can wait for topology or retry.

The dialog surface adds these four Result-bearing operations:

- `Runtime::DialogShowOpenFile`, `DialogShowSaveFile`, and `DialogShowOpenFolder`
  return `Result<dialogs::DialogRequestId>`; and
- `Runtime::DialogShutdown` returns `VoidResult`.

The five observer operations are direct `noexcept` contracts:
`DialogGetPendingCount` and `DialogGetOutstandingRequestCount` return
`std::size_t`, `DialogHasPending` returns `bool`, `DialogGetPending` returns its
request snapshot, and `DialogPollCompletion` returns an optional completion.
Owner-thread and initialized-runtime requirements are preconditions rather than
recoverable result errors.

A non-dialog function in the first table may still throw. Programming errors, wrong-thread use, broken
lifecycle contracts, and unexpected backend corruption are exceptional even when the
same operation returns `Result` for an expected outcome. Implementations do not catch an
exception merely to repackage it as `ponder::core::Error` outside the dialog boundary.

Every public dialog operation is `noexcept`. The four fallible operations are
explicit exception shields: they catch `ponder::core::Exception`,
`std::exception`, and unknown exceptions raised anywhere in their call stacks,
log through the core facilities with the `platform` category, perform required
rollback, and return `BackendFailure`. For `ponder::core::Exception`, the
returned diagnostic preserves the message, stack trace, and source location, so
any original platform code embedded in the message remains available for human
diagnosis. Implementations do not parse diagnostic text or reintroduce typed
platform exceptions. `DialogShutdown` may also return `InvalidArgument`
directly when outstanding requests prevent shutdown. The five direct observers
have no recoverable failure channel; misuse violates their asserted
preconditions.

For display lookup, `GetDisplayInfo` returns `NotFound` for a stale nonzero ID, throws
`InvalidArgument` for the invalid zero ID, and throws `BackendFailure` for malformed
backend data. `GetDisplayId` retains only transient topology outcomes.

Clipboard access is locally actionable after the later clipboard amendment.
Empty clipboard text is a successful value. Invalid UTF-8 or an embedded null
passed to clipboard set returns `InvalidArgument`; unavailable access,
malformed external clipboard text, and host failures return `Unsupported` or
`BackendFailure`. Wrong-thread and lifecycle misuse still throw. External-URI
validation remains locally actionable: empty, invalid UTF-8, or embedded-null
input returns `InvalidArgument`, while unavailable service and host launch
failure return `Unsupported` or `BackendFailure`.

Dialog completion occurs after the initiating stack has gone. Selection, cancellation,
and asynchronous failure therefore remain `DialogOutcome` data, and `DialogFailure`
continues to own a `ponder::core::Error`. This asynchronous outcome is distinct from a
synchronous Result error returned before a request is accepted.

### Platform Time Domain

Platform uses `ponder::core::Timestamp::Now()` for `Runtime::TimeNow()`, translated
event observation timestamps, dialog request/completion timestamps, and internal
event-wait elapsed/deadline measurements. It never calls `SDL_GetTicksNS()` and does not
expose SDL's native tick epoch. A translated event timestamp records when platform
observed and translated that event, not the timestamp carried in SDL's event storage.
Only differences between timestamps from the core steady-clock domain are meaningful.

### Backend Diagnostics And No-Throw Boundaries

SDL remains private. After a documented SDL failure, platform copies the SDL error text
immediately and before any other SDL call. The resulting core exception or retained
`ponder::core::Error` includes the operation and relevant runtime, window, or display
context. Diagnostic text is never parsed for control flow. Private helpers provide a
throwing path and, only for an approved retained-result contract, an error-value path.

No C++ exception may escape an SDL or operating-system callback, a thread entry point, or
a destructor. Destructors, move operations, and release paths remain `noexcept` and
contain operational cleanup failures with diagnostics. Impossible release-time
invariant violations retain their existing termination contract. Ordinary helpers lose
`noexcept` when they can now throw.

## Consequences

Most callers handle platform failures once at an application or worker boundary instead
of propagating `Result` through every intermediate layer. Expected capability,
external-state, and user-input failures remain explicit where immediate recovery is
realistic.

The original failure migration changed failure-related source signatures and exception
behavior without changing its contemporary platform namespace, include paths, CMake
targets, ownership, descriptors, event model, or unrelated symbols. The subsequent
namespace migration changes the current platform namespace to `ponder::platform`. The
subsequent exception simplification keeps one catchable core exception type and moves
the platform code into its diagnostic message without changing retained `Result` errors.
The flat-runtime amendment changes the runtime and subsystem method names and
removes manager-returning APIs. The later clipboard amendment restored its two
locally actionable results, and the nonblocking process-wait amendment adds
`Process::TryWait`. The dialog-boundary amendment adds four `noexcept` Result
operations whose broader containment contract deliberately differs from the
twelve retained non-dialog results, keeps five observers as direct `noexcept`
values, and standardizes platform timestamps on the core steady clock.
The explicit-lifecycle amendments separate hint configuration from backend
initialization while making implementation ownership immediate and consistent:
`Create()` constructs the uninitialized implementation, typed hints configure
it, and `Initialize()` alone initializes it.

This ADR amends ADR 0007's runtime-factory and Errors And Asynchronous Services failure
clauses. ADR 0007's other decisions remain in force. ADR 0012 remains authoritative for
the core namespace, exception hierarchy, construction macros, and assertion semantics.
