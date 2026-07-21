# Codex Guidance: Platform

Read the root `AGENTS.md` before this file.

## Scope

Work here on reusable operating-system and desktop-platform integration backed
privately by SDL3.

## Local Rules

- Keep public headers under `include/ponder/platform/` and use the
  `pond::platform` namespace.
- Public headers may use standard-library and `ponder_core` types. Never expose
  SDL headers, SDL types, direct OS types, or OS headers.
- Declare `ponder::core` as a public target dependency and `ponder::io` plus
  `ponder::SDL3` as private target dependencies. Do not add another production
  dependency without a boundary decision.
- `ponder_platform` intentionally uses no precompiled header. Every header and
  source includes its direct dependencies. Do not enable a platform PCH unless
  platform-only measurements demonstrate a material benefit and this policy is
  deliberately revised.
- Keep SDL and OS-specific helpers under `src/`. Use PIMPL or another
  heap-stable private representation for native resource owners.
- Runtime state owns its `IPlatformRuntimeBackend`, `IPlatformWindowBackend`, and
  `IPlatformDisplayBackend` instances through `std::unique_ptr`s produced by private
  factories. A `WindowImpl` may borrow the runtime-owned window backend only while it is
  registered as a runtime child, and stores native identity as the strong,
  zero-invalid `BackendWindowHandle`.
- Keep `IPlatformWindowBackend` backend-agnostic and value-oriented: use typed
  `Result<T>` queries, `VoidResult` commands, `std::string_view` inputs, and
  owned string outputs instead of raw window pointers, C-style out parameters,
  or backend-specific status enums. Preserve plain `bool` only for infallible
  state queries where false is data. Convert between `BackendWindowHandle` and
  `SDL_Window*` only inside the SDL backend, and capture documented SDL failures
  there before returning an error.
- Keep `IPlatformDisplayBackend` backend-agnostic and value-oriented too. Return
  owned names and display lists, use `Result<T>` for every SDL query with a
  documented failure sentinel, and capture SDL diagnostics in `SdlDisplayBackend`.
  Preserve `BackendDisplayOrientation::Unknown` as valid unavailable data rather
  than treating it as a failure.
- Apply the same result boundary to `IPlatformRuntimeBackend`: reserve plain
  `bool` for infallible state, capability, and event-availability queries; use
  `VoidResult` for fallible commands and `Result<T>` for fallible value-producing
  operations. `SdlRuntimeBackend` must capture a documented SDL error immediately
  after observing failure so callers never need SDL-specific diagnostics.
- Keep `SdlRuntimeBackend`, `SdlWindowBackend`, and `SdlDisplayBackend` declarations
  and implementations in their dedicated private files. Put only genuinely shared SDL
  conversion and context helpers in `SdlCommon`.
- Permit only one logical `PlatformRuntime` per process. Create runtime and
  windows through fallible factories returning `pond::core::Result<T>`.
- Treat SDL lifecycle ownership as exclusive. Reject creation when an SDL
  subsystem is already initialized; otherwise platform's `SDL_Quit()` could
  invalidate another owner. Before teardown, verify that no subsystem outside
  runtime-owned video/events appeared while the runtime was live.
- Use checked `SDL_SetAppMetadataProperty` calls rather than the convenience
  metadata function and apply descriptor metadata before video initialization.
  Clear absent optional descriptor properties and do not snapshot or restore
  prior metadata. After `SDL_Quit()`, leave the resulting process state to SDL.
- Manage the curated SDL hint catalog exclusively through the public, strongly
  typed `HintManager`; never expose SDL names or an arbitrary string-hint map.
  Keep an independent value stack per hint, enforce value and initialization
  constraints, and invoke descriptor hint configuration before SDL initialization.
  The runtime must not apply implicit hint policies; applications opt into every
  managed hint explicitly. Restore every managed prior effective value after
  `SDL_Quit()`. SDL cannot report priority or
  provenance, so do not claim to preserve either one.
- Keep runtime and resource owners non-copyable and movable. Define moved-from
  behavior and keep child lifetime safe in release builds.
- Treat SDL-backed APIs as runtime-owner-thread APIs unless explicitly documented
  otherwise. Call `PONDER_VERIFY` before SDL for wrong-thread, moved-from, and
  teardown invariants that must remain active in release builds.
- Require runtime creation on the startup thread captured during module
  initialization and independently verify SDL's main-thread predicate. Treat
  that check as defense in depth; executable entry remains responsible for the
  portable process-entry-thread guarantee.
- `SDL_IsMainThread()` cannot identify the process-entry thread before first SDL
  initialization on non-Apple hosts. The desktop must create the first runtime
  on its entry thread; platform captures and enforces that thread afterward.
- Give runtime-owned objects strong, zero-invalid, monotonic IDs that are not
  reused during the runtime lifetime. Ignore stale backend events
  deterministically.
- Use `pond::core::Result<T>` and `pond::core::VoidResult` for recoverable
  failures. Capture SDL errors through one private helper immediately after a
  documented SDL failure; never parse error text for control flow.
- Use public `PlatformErrorCode` values for machine-actionable failures and keep
  their numeric mappings stable once published.
- Use project assertion and logging macros. Log lifecycle messages with the
  `platform` category and keep ignored-event logging at trace level or silent.
- Use a `std::variant` of typed, owned event payloads. Do not add a redundant
  event-kind enum or raw SDL escape hatch. Give every event the original SDL
  monotonic nanosecond timestamp; never resample it with `Now()`.
- Map keyboard input to project-owned physical keys, closed logical
  Unknown/Unicode/named values, and side-specific modifier flags. Never expose
  SDL keycodes, scancodes, raw platform scan values, or keyboard device IDs.
- Keyboard, text-input, and composition events may have no `WindowId` when SDL
  reports backend window ID zero. Drop a nonzero stale or unresolved target.
  Copy and validate UTF-8 text before SDL event storage expires.
- Composition selections are optional Unicode-character-index ranges, not UTF-8
  byte ranges. Preserve a present zero-length range; any negative backend start
  or length means the selection is unavailable.
- Mouse motion, button, and wheel events use optional targets under the same
  zero/missing and nonzero/stale rules as keyboard and text input. Preserve the
  floating logical position on every mouse event and relative movement on
  motion. Drop an event if any position, relative-movement, or wheel component
  is not finite.
- Drag-and-drop events use the same optional-target rule: backend window ID zero
  becomes targetless, while a stale nonzero target drops the event. `DropBegin`
  has no position; dropped-file, dropped-text, drop-position, and drop-complete
  require finite logical positions. Copy and validate source-application text,
  dropped UTF-8 text, and file-name bytes before SDL event storage expires;
  file names become owned `std::filesystem::path` values. Treat every SDL drop
  event independently and do not retain global sequence state just to match
  begin/complete pairs.
- Clipboard text and external-URI APIs are owner-thread `PlatformRuntime`
  services. Validate null-free UTF-8 before set/open calls, copy incoming views
  to null-terminated storage before SDL, and keep all SDL allocation/free and
  `SDL_OpenURL` details private.
- Clipboard reads are the one SDL error-snapshot exception: clear SDL error
  state immediately before `SDL_GetClipboardText()`, copy the error string
  immediately afterward, then copy/free the returned text. Treat empty text plus
  a captured error as failure, and never parse error text for capability
  decisions.
- Automated external-URI tests must use the private backend seam and must not
  open a browser or other host application.
- Map mouse buttons to the closed project vocabulary, preserving unsupported
  backend values as `MouseButton::Unknown`. Normalize flipped wheel input
  exactly once so positive horizontal means right and positive vertical means
  up; do not guess how to normalize a future unknown wheel direction.
- Keep one private `TranslateSdlEvent` production function for PLAT-008 and
  polling. Resolve required backend IDs through injected private callbacks.
  Preserve an unresolved destination display only on
  `WindowDisplayChangedEvent`; drop events whose required identity is stale.
- Make `PlatformRuntime::PollEvent()` consume SDL events on the runtime owner
  thread until it can return one translated event or the queue is empty. Never
  let an unsupported, malformed, or stale event masquerade as an empty queue,
  and never destroy a window in response to a close request.
- Update display routing directly around lifecycle events: connect or allocate
  an added backend display before translation, and translate a removal through
  its retained project ID before finalizing the disconnected tombstone.
  Reconcile a previously unseen non-removal display event against the live
  topology, but never revive a tombstone without an added event.
- Treat window/display scale, display move, content-scale, and usable-bounds
  events as re-query notifications. Use optional display-mode extents because
  SDL's public event contract does not guarantee mode dimensions. Preserve
  zero window sizes, reject negative window sizes, and map unknown display
  orientations to `DisplayOrientation::Unknown`.
- Keep window text-input activation live and idempotent rather than caching it.
  Validate IME areas before SDL, round logical coordinates to the nearest
  backend integer, and return contextual failures from fallible text-input
  operations.
- Keep mouse-grab and relative-mode state live on `Window`; do not cache their
  infallible backend queries. Keep global capture, global mouse position,
  system-cursor selection, and cursor visibility on `PlatformRuntime`.
- Always forward `SetMouseGrab()` requests to SDL. A hidden window can retain
  a pending grab request while SDL's live grab query reports false, so using the
  query to suppress a release would leave stale pending state.
- Return `Unsupported` without parsing error text when the active driver cannot
  provide explicit global capture or a meaningful desktop-relative mouse
  position. Do not invent a capture-state query or cache capture state because
  SDL may release capture on focus loss. Keep capture disabling available as
  an idempotent cleanup operation.
- Lazily create and cache standard SDL system cursors in runtime state. Cursor
  selection never transfers ownership and never changes visibility; show/hide
  remain separate operations. Destroy every cached cursor before SDL shutdown,
  including rollback and teardown paths.
- Keep display content scale, window pixel density, window display scale,
  logical size, and pixel size as distinct concepts.
- Copy display names and all other snapshot data before backend-owned storage
  expires. Current refresh rate zero means unavailable; present refresh rates
  and all scale values must be finite and greater than zero. Reject negative
  rectangle extents as malformed backend data while preserving signed origins
  and valid zero-sized observations.
- Reconcile connected backend displays before runtime display queries. Keep
  project IDs stable across reordering, retain disconnected ID tombstones, and
  allocate a new monotonic ID when a backend display is observed after an
  absence. Invalid zero IDs are `InvalidArgument`; unknown and stale nonzero
  IDs are `NotFound`.
- Resolve a window's current display, pixel density, and display scale on
  demand. Use the runtime display registry for identity, and never substitute a
  display snapshot's base content scale for the window display-scale query.
- Keep presentation, decoration, minimized/maximized state, visibility,
  resizability, focus, and always-on-top as orthogonal window concepts.
- Query SDL's current and pending window flags on demand. Treat input focus as
  focus and hidden state as the inverse of visibility; do not infer either from
  minimization, presentation, or another property.
- Make window-state mutators idempotent and do not cache visible asynchronous
  requests as observed state. Retain the last accepted visible request only as
  private mutator-routing metadata: if live flags still show the old target, an
  immediate opposite request must still reach SDL. SDL merges current and
  pending flags for hidden windows, so retain the last successful hidden state
  request until `Show()` to disambiguate a stale current bit from the new pending
  bit. Before each state mutation, synchronize private request markers to the
  live hidden/visible flag and clear a visible marker once observation catches
  up. Use the marker before observed state when deciding idempotence so an
  accepted duplicate is suppressed and an immediate opposite still reaches SDL.
  Transfer markers across successful show/hide requests, and process a `SHOWN`
  event only after live flags confirm the window is currently visible without
  overwriting newer visible intent. Reject simultaneous minimized/maximized
  flags without the hidden marker as `BackendFailure`.
- Select the null SDL fullscreen mode before entering desktop fullscreen. Keep
  exclusive fullscreen absent until platform owns display-mode selection.
- Classify known unavailable state transitions as `Unsupported` without parsing
  SDL error text. Detect silently ignored decoration, resizability, and
  always-on-top setters by comparing the project-relevant backend flag before
  and after the successful synchronous SDL setter.
- Keep frame-delta calculation, fixed timestep, frame limiting, event-wait idle
  policy, rendering, UI behavior, project policy, chemistry, IO,
  workflow, compute, plugins, and desktop application policy out of platform.
- Provide project-owned primitives for `ponder_ui`; UI must not declare a direct
  SDL dependency, inherit SDL compile usage, or consume SDL types.
- `WindowGraphicsCompatibility` contains `Default`, `Vulkan`, and `Metal`.
  Map `Vulkan` to SDL's Vulkan flag on Windows/Linux only. Map `Metal` to SDL's
  Metal flag only for the later native macOS backend. Store the exact project
  selection in each window; never reconstruct it from SDL flags.
- Do not translate a Vulkan request on macOS into Metal-layer behavior. Vulkan on
  macOS is unsupported by the render contract.
- Stage native windows hidden until minimum-size setup, backend-ID routing, and
  runtime child registration have committed; show only at the end when the
  descriptor requests visibility. Remove routing before native destruction and
  unregister the child after native destruction completes.
- Reject window dimensions that are zero or not representable by SDL's signed
  integer API, and reject screen coordinates that collide with SDL's encoded
  centered/undefined sentinels before calling SDL.
- Keep native Vulkan interop to the approved Win32, X11, and Wayland payloads
  from ADR 0008. Values are tagged, borrowed, narrow, and OS-header-free.
- Return `InvalidArgument` for native-handle queries on non-Vulkan windows,
  `Unsupported` for video drivers outside `"windows"`, `"x11"`, and
  `"wayland"`, and `BackendFailure` for malformed or missing approved backend
  data. Do not return a partially populated handle.
- No Cocoa or Metal-layer payload is part of the current Vulkan native-handle
  contract. The future Metal backend must define its own exact macOS payload
  before platform exposes it. Consumers re-query borrowed native snapshots after
  window show/hide, presentation/state changes, display migration, or renderer
  presentation teardown/rebuild boundaries where stale state would matter.
- Never create a Vulkan surface in platform. Render owns every `VkSurfaceKHR` and
  destroys presentation state before its platform window. This ordering is
  caller-enforced because the platform registry cannot observe render resources.
- Copy all asynchronous dialog inputs and callback outputs. Marshal completion
  to the runtime owner thread and distinguish selection, cancellation, and
  failure. Keep callback fallback storage and FIFO links request-owned so the C
  callback can enqueue without allocating after a failure and owner-thread
  polling can test/pop completions in O(1) without scanning every request.
- Keep process launch shell-free. Process destruction must not terminate the
  child or block the caller; abandoned live process handles stay privately
  waitable until cleanup can reap them.
- Start the shared abandoned-process cleanup worker before creating a child and
  preallocate its cleanup entry before launch. Destruction may enqueue that
  entry but must not create a thread or allocate. Poll every abandoned handle
  fairly and retain it across every inconclusive or failed nonblocking wait;
  destroy backend tracking only after exit is confirmed. Keep the worker in
  constant-initialized, deliberately process-lifetime storage so late or
  static-storage `Process` destruction cannot re-enter a destroyed singleton or
  synchronously join a worker. Block the worker on its atomic queue while idle;
  poll at the short cadence only while abandoned handles remain active.
- Treat process exit translation as host-aware backend work. Preserve normal exit
  statuses as unsigned 32-bit values, and translate POSIX signal/unknown
  conventions only in the host-specific backend adapter.
- Process creation does not require `PlatformRuntime`. Bind each `Process` to its
  launching thread and require its operations and public destruction on that
  thread without concurrent access. The internal abandoned-process reaper may
  wait and destroy after public ownership has ended.
- Treat `Process::Wait()` as a blocking operation. Do not call it from the
  desktop event loop or any UI/platform/render pumping thread; use a worker or a
  higher-level orchestration point when waiting for long-running children.
- Add tests with every implementation task. Late roadmap test tasks are coverage
  audits, not a substitute for incremental tests.

## Verification

Keep `HeadlessAndHostVerification.md` current when CTest labels, live SDL skip
rules, host-local commands, or manual dialog verification steps change.

For intermediate implementation tasks:

- Reuse one configured supported Debug developer preset. Reconfigure only when
  CMake inputs or source lists change.
- Build `ponder_platform`, `ponder_platform_header_tests`,
  `ponder_platform_tests`, and `ponder_platform_backend_tests` as required by
  the affected public and private behavior. Run the complete affected
  deterministic test executables.
- Build and run `ponder_platform_integration_tests` through one CTest invocation
  only when the task changes applicable live SDL behavior. Include the process
  helper and its owning tests when process support is affected.
- Do not repeat a PCH-off build: the platform production and public-header
  targets are already explicitly PCH-free.
- Defer Release, sanitizer, full formatting, clang-tidy, broad whitespace, and
  portability checks to the roadmap gates unless the task changes that tooling.

At the renderer and completion gates:

- PLAT-013 runs the complete renderer-ready platform target/test subset on the
  normal host Debug preset.
- PLAT-021 runs the full host-local Debug, Release, test, formatting, clang-tidy,
  whitespace, dependency-boundary, and applicable manual checks. Clang-tidy uses
  a configuration that provides `compile_commands.json`: the Windows Ninja
  analysis preset or a normal supported single-configuration Linux/macOS
  preset.
- `future.txt` records the Windows, Linux, and macOS compiler/sanitizer matrix.
- Serialize integration tests that touch process-global SDL state or the system
  clipboard, record capability-based skips, and describe local success as
  host-local until the portability matrix is actually run.
