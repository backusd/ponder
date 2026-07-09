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
- Declare `ponder::core` as a public target dependency and `ponder::SDL3` as a
  private target dependency. Do not add another production dependency without a
  boundary decision.
- `ponder_platform` intentionally uses no precompiled header. Every header and
  source includes its direct dependencies. Do not enable a platform PCH unless
  platform-only measurements demonstrate a material benefit and this policy is
  deliberately revised.
- Keep SDL and OS-specific helpers under `src/`. Use PIMPL or another
  heap-stable private representation for native resource owners.
- Permit only one logical `PlatformRuntime` per process. Create runtime and
  windows through fallible factories returning `pond::core::Result<T>`.
- Treat SDL lifecycle ownership as exclusive. Reject creation when an SDL
  subsystem is already initialized; otherwise platform's `SDL_Quit()` could
  invalidate another owner. Before teardown, verify that no subsystem outside
  runtime-owned video/events appeared while the runtime was live.
- Use checked `SDL_SetAppMetadataProperty` calls rather than the convenience
  metadata function, apply metadata before video initialization, and restore
  prior effective nullable metadata values on rollback and after shutdown.
- Apply the two UI mouse hints at override priority and restore their prior
  effective values after `SDL_Quit()`. Do not claim to preserve priority or
  provenance because SDL cannot report either one.
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
  requests as observed state. SDL merges current and pending flags for hidden
  windows, so retain only the last successful hidden state request until
  `Show()` to disambiguate a stale current bit from the new pending bit. Reject
  simultaneous minimized/maximized flags without that marker as
  `BackendFailure`.
- Select the null SDL fullscreen mode before entering desktop fullscreen. Keep
  exclusive fullscreen absent until platform owns display-mode selection.
- Classify known unavailable state transitions as `Unsupported` without parsing
  SDL error text. Detect silently ignored decoration, resizability, and
  always-on-top setters by comparing the project-relevant backend flag before
  and after the successful synchronous SDL setter.
- Keep frame-delta calculation, fixed timestep, frame limiting, event-wait idle
  policy, rendering, Dear ImGui behavior, project policy, chemistry, IO,
  workflow, compute, plugins, and desktop application policy out of platform.
- Provide project-owned primitives for `ponder_ui`; UI must not declare a direct
  SDL dependency, inherit SDL compile usage, or compile the bundled SDL ImGui
  backend.
- `WindowGraphicsCompatibility` contains only `Default` and `Vulkan`. Map Vulkan
  to SDL's Vulkan flag on Windows/Linux and Metal flag on macOS. Store the exact
  project selection in each window; never reconstruct it from SDL flags.
- Stage native windows hidden until minimum-size setup, backend-ID routing, and
  runtime child registration have committed; show only at the end when the
  descriptor requests visibility. Remove routing before native destruction and
  unregister the child after native destruction completes.
- Reject window dimensions that are zero or not representable by SDL's signed
  integer API, and reject screen coordinates that collide with SDL's encoded
  centered/undefined sentinels before calling SDL.
- Keep native interop to the approved Win32, X11, Wayland, and Cocoa Metal-layer
  payloads from ADR 0008. Values are tagged, borrowed, narrow, and OS-header-free.
- On macOS, each Vulkan-compatible window owns at most one cached SDL Metal view;
  repeated queries expose the same borrowed layer, and the view is destroyed
  before that SDL window.
- Never create a Vulkan surface in platform. Render owns every `VkSurfaceKHR` and
  destroys presentation state before its platform window. This ordering is
  caller-enforced because the platform registry cannot observe render resources.
- Copy all asynchronous dialog inputs and callback outputs. Marshal completion
  to the runtime owner thread and distinguish selection, cancellation, and
  failure.
- Keep process launch shell-free. Do not make process destruction implicitly
  wait for or terminate a child.
- Process creation does not require `PlatformRuntime`. Bind each `Process` to its
  launching thread and require its operations and destruction on that thread
  without concurrent access.
- Add tests with every implementation task. Late roadmap test tasks are coverage
  audits, not a substitute for incremental tests.

## Verification

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
- PLAT-022 records the Windows, Linux, and macOS compiler/sanitizer matrix.
- Serialize integration tests that touch process-global SDL state or the system
  clipboard, record capability-based skips, and describe local success as
  host-local until the portability matrix is actually run.
