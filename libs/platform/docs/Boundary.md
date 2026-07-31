# Platform Library Boundary

`ponder_platform` owns reusable operating-system and desktop-platform
integration.

Status: platform contracts revised through 2026-07-30 for the mixed
exception/Result failure policy, the exception-shielded dialog surface, the
`ponder::platform` namespace, direct core timing types, and the single
compile-selected runtime implementation.

## Decision Records

- [ADR 0007: SDL3 Platform Backend][adr-0007]
- [ADR 0008: Vulkan Renderer Backend][adr-0008]
- [ADR 0012: Core Namespace And Failure Policy][adr-0012]
- [ADR 0013: Platform Failure Policy][adr-0013]

[adr-0007]: ../../../docs/adr/0007-sdl3-platform-backend.md
[adr-0008]: ../../../docs/adr/0008-vulkan-renderer-backend.md
[adr-0012]: ../../../docs/adr/0012-core-namespace-and-failure-policy.md
[adr-0013]: ../../../docs/adr/0013-platform-failure-policy.md

## Public API

- Public headers live under `include/ponder/platform/`.
- Source code uses the `ponder::platform` namespace.
- The CMake target is `ponder_platform`; the alias is `ponder::platform`.
- Public APIs expose project-owned and standard-library types only.
- Public text is UTF-8. Borrowed text uses `std::string_view`; owned text uses
  `std::string`.
- Filesystem locations use `std::filesystem::path`. Platform uses `ponder_io`
  path encoding helpers when converting between paths and SDL's UTF-8
  representation.
- Native resource owners use PIMPL or an equivalent private representation. SDL
  and OS declarations do not appear in public headers.
- `Runtime` is the sole platform-service facade. Its flat methods begin with the
  owning subsystem name (`Hint*`, `Clipboard*`, `Dialog*`, `Event*`, `Window*`,
  `Display*`, `Mouse*`, `Time*`, and `Uri*`); it does not return manager objects.

## Lifetime And Threading

- At most one logical `Runtime` may be live in a process, including an
  uninitialized owner returned by `Runtime::Create()`.
- A private atomic 0/1 reservation is acquired before fallible `RuntimeImpl`
  construction and remains held until that owner is destroyed. It rejects
  reentrant creation during explicit hint configuration, partial initialization,
  normal operation, or teardown, then is released so a later runtime can be
  created sequentially. It is not a public lifecycle API or a
  creating/active/destroying state machine.
- `Runtime::Create()` takes no arguments, constructs exactly one heap-stable but
  uninitialized `RuntimeImpl`, returns its `Runtime` owner directly, and throws
  a platform exception on construction failure.
- The caller configures pre-initialization hints through the typed `Hint*` API,
  then calls `Runtime::Initialize(applicationName, applicationVersion,
  applicationIdentifier)`. `Initialize` returns `void`, initializes the backend,
  and throws on invalid metadata or initialization failure. It may succeed only
  once; ordinary non-hint services require successful initialization.
- Windows are created through the runtime with a direct `WindowCreate()`
  operation that throws on construction failure.
- Runtime and resource owners are non-copyable and movable. Moved-from owners
  may only be destroyed or assigned another valid value.
- From a successful `Runtime::Create()` until owner destruction, `Runtime` owns
  exactly one heap-stable, initially uninitialized `detail::RuntimeImpl`;
  children borrow
  that implementation under an enforced lifetime contract, so moving the public runtime cannot invalidate them.
- `Runtime::Create()` acquires the reservation, validates the process-entry
  thread, and constructs the uninitialized `RuntimeImpl`. Its construction
  verifies requirements for SDL main-thread use and lifecycle exclusivity. Only
  typed `Hint*` configuration and `Initialize()` are valid in that explicit
  phase. `Initialize()` validates and owns its application metadata and is the
  only public operation that calls `RuntimeImpl::Initialize()`. There is no callback-taking `Create` overload,
  public runtime descriptor, intermediate runtime-state object, runtime builder,
  backend-provider hierarchy, or virtual runtime-backend interface.
- Windows and pending runtime services must complete before runtime destruction.
  Release-active verification requires an empty runtime-child registry and an
  empty runtime-owned dialog request registry before SDL shutdown and does not return
  normally on violation.
- Platform owns SDL's process-global lifecycle exclusively. `RuntimeImpl`
  construction in `Runtime::Create()` rejects any
  already initialized SDL subsystem. Teardown verifies that
  no subsystem outside the runtime-owned video/event set appeared before using
  process-global `SDL_Quit()`.
- `Create()` verifies the startup thread captured during module initialization,
  constructs `RuntimeImpl`, verifies `SDL_IsMainThread()`, and captures that thread as the
  backend owner. This is defense in depth: the executable still invokes creation from
  process entry because deferred initialization and dynamic loading prevent a
  library-only check from proving that identity portably. SDL-backed public APIs
  verify the captured owner in every build. Wrong-thread public use normally
  throws a `PlatformErrorCode::WrongThread` diagnostic before SDL is called;
  fallible dialog submissions and shutdown contain it and return
  `BackendFailure`, preserving the original formatted message and its embedded
  `WrongThread` code as diagnostic text, while direct dialog observers assert
  the owner-thread precondition. Ordinary service use before successful
  `Initialize()` is a programmer error guarded by `PONDER_ASSERT`; typed hint
  operations are the deliberate pre-initialization exception. Runtime
  forwarding also asserts its non-null implementation; other moved-from and impossible ownership/lifecycle violations
  use release-active verification.
- SDL dialog callbacks are an internal exception: they may copy completion data
  into synchronized private storage from another thread, but public delivery
  occurs only on the runtime owner thread.
- Process objects are a documented exception to the main-thread rule. A process
  is bound to the thread that launches it, and its operations and
  destruction occur on that thread without concurrent access.

## Responsibilities

### Runtime And Windows

- SDL3 video/event initialization and shutdown behind explicit RAII ownership.
- Production resolves `detail::RuntimeImpl` directly to `SdlRuntime`.
  `PONDER_PLATFORM_USE_MOCK_RUNTIME` selects the API-compatible `MockRuntime`
  while compiling an isolated runtime source variant;
  production never defines it. `Runtime.hpp` consumers in such a variant must
  all observe the same selector.
- `SdlRuntime` has the same complete callable surface as `Runtime` and directly
  owns SDL lifecycle, hint stacks, clipboard synchronization, dialog state,
  event/display topology, cursor caches, and child/window registries. Its
  private runtime types are co-located in `SdlRuntime.hpp`; there is no separate
  SDL runtime-types configuration header. Narrow
  `SdlWindowBackend` and `SdlDisplayBackend` helpers remain resource adapters,
  not service facades. There are no `*Manager`, state-facade, backend-aggregate,
  or virtual dialog-backend layers.
- Initialization arms no-throw SDL rollback immediately before video
  initialization. Teardown requires all windows and dialogs to be gone, destroys
  cached cursors, deactivates the dialog callback handoff, calls `SDL_Quit()`,
  restores managed hints, destroys `RuntimeImpl`, and releases the process
  reservation last.
- Production code has no `*ForTesting` stage-hook registry, mutable global
  backend override, test provider, or other testing entry point. Deterministic
  backend tests use fixture-owned `MockRuntime` state in an isolated
  mock-runtime executable and verify rollback through observable contracts. Such
  an executable compiles the required runtime source variant directly and does
  not link `ponder::platform`.
- Expose the curated SDL hint catalog through explicitly specialized strongly
  typed `Runtime::HintPush<T>`, `HintPop<T>`, `HintClear<T>`, and `HintGet<T>`
  templates. Keep the primary templates deleted so unsupported hint types fail
  at compile time. Each supported specialization uses independent value stacks,
  validated values, initialization-phase checks, and explicit configuration
  between `Runtime::Create()` and `Runtime::Initialize()`. Runtime applies no implicit hint policy;
  applications opt into each managed value. Restore every managed prior
  effective nullable value after `SDL_Quit()`; SDL does not expose enough
  information to restore its former priority or provenance.
- Hint push, pop, and clear operations return `void`. Hint lookup returns
  `std::optional<T>`: an unset hint is ordinary absence. If SDL supplies an
  empty, malformed, or unsupported value, lookup logs an error in the `platform`
  category and returns `std::nullopt`; it does not throw for third-party data.
  Wrong-thread use remains a programming error and throws before SDL is queried.
- Apply owned, validated `Runtime::Initialize()` metadata through checked SDL property
  operations before initializing video. Clear absent optional properties and
  do not snapshot or restore prior metadata on failed initialization or shutdown.
  Leave the post-`SDL_Quit()` process state to SDL.
- Multiple windows from the first implementation.
- Runtime-local 64-bit `WindowId` values. Zero is invalid; valid IDs increase
  monotonically and are never reused during a runtime lifetime.
- Window creation with UTF-8 title, logical size, visibility, resizability,
  optional minimum size, high-pixel-density support, and a project-owned
  graphics compatibility value selected at creation.
- Basic window operations are owned-title get/set, signed screen-position
  get/set, logical-size get/set, pixel-size observation, and show/hide. Visible
  windows are staged hidden until creation commits, so callers never receive a
  partially registered native window.
- `WindowPresentation` contains `Windowed` and `DesktopFullscreen` only.
  Desktop fullscreen explicitly uses SDL's null fullscreen mode; exclusive
  fullscreen remains deferred until platform owns a display-mode selection
  contract.
- `WindowDecoration` contains `System` and `Borderless`. `WindowState` contains
  `Normal`, `Minimized`, and `Maximized`. These remain separate from
  presentation, visibility, input focus, resizability, and always-on-top state;
  there is no compound `WindowMode`.
- `Window` exposes independent live queries and mutators for those properties.
  `IsVisible()` means the window is not hidden, so a minimized window remains
  visible. `IsFocused()` observes input focus, not mouse focus. Focus has no
  platform mutator, and `Show()`/`Hide()` remain the visibility mutators.
- Window state queries include SDL's current and pending flags. SDL merges those
  sources and can retain an old current state bit while a hidden window stages
  the opposite pending state. Platform therefore retains only the last
  successful hidden state request as a disambiguation marker until `Show()`;
  all observed state remains live backend data. A separate accepted-request
  marker is used only by asynchronous presentation/state mutators so stale live
  flags cannot discard an immediate opposite request; it is never returned as
  observed state. Simultaneous minimized and maximized flags without the hidden
  marker are `BackendFailure`.
- Window-manager presentation and state requests may be asynchronous. Success
  means the request was accepted, not that the observed state already changed.
  Before a state mutator decides whether to call SDL, platform moves its private
  accepted-request marker into the live hidden/visible domain and clears a
  visible marker when the observed state has caught up. Repeating the accepted
  target is idempotent; requesting its opposite still reaches SDL while live
  flags lag.
  `Restore()` requests the OS restore behavior and may restore a minimized
  window to its prior maximized state.
- A delayed `SDL_EVENT_WINDOW_SHOWN` clears/transfers hidden-state intent only
  when a live flag query confirms the window is currently shown. A historical
  event observed after the window was hidden again cannot erase newer intent,
  and an older hidden marker cannot overwrite a newer visible request.
- Mutators are idempotent. Maximizing a non-resizable window and positively
  identified unavailable backend operations throw an `Unsupported` platform
  exception. Decoration and resizability cannot change while fullscreen.
  Drivers that silently ignore a requested decoration, resizability, or
  always-on-top change also throw `Unsupported` when the backend flag does not
  latch.
- Window close requests are events. Platform never destroys a window merely
  because the OS requested close.
- Window queries return direct values and window commands return `void`,
  including text-input and window-local mouse operations. The only window
  `Result` operations are `GetNativeHandle()` and `GetDisplayId()` under their
  narrow contracts below. Direct operations throw platform exceptions on
  validation, capability, and backend failure.

### Displays And Coordinates

- `Runtime::DisplayEnumerate()` returns project-owned `DisplayInfo` snapshots
  directly. `DisplayGetInfo(DisplayId)` returns a
  `Result` only so a stale nonzero ID can produce locally actionable
  `NotFound`. Zero IDs throw `InvalidArgument`, and malformed topology/backend
  data throws with its platform code embedded in the message. Each snapshot
  contains a runtime-local `DisplayId`, owned name, `ScreenRectangle` bounds and
  usable bounds, optional
  current refresh rate in hertz, current orientation, and scalar display
  content scale.
- `DisplayOrientation` contains `Unknown`, `Landscape`, `LandscapeFlipped`,
  `Portrait`, and `PortraitFlipped`. It describes the current orientation, not
  the panel's natural orientation; unavailable and future backend values map to
  `Unknown`.
- A successful current-mode refresh rate of zero maps to `std::nullopt`.
  Present refresh rates and every display/window scale observation are finite
  and greater than zero; malformed backend values are `BackendFailure`.
- Display snapshots use runtime-local 64-bit `DisplayId` values. Zero is
  invalid; IDs increase monotonically and are never reused during a runtime
  lifetime. Reordering does not change identity. A disconnected ID is retained
  as a tombstone and queries return `NotFound`; observing a display again after
  an absence creates a new project ID.
- `Window::GetDisplayId()` resolves the current display on demand and uses the
  same ID space as runtime snapshots. It retains `Result` only for a transiently
  unresolved or disconnected display; invalid state and malformed backend data
  throw. `GetPixelDensity()` reports the ratio of window pixel coordinates to
  logical coordinates, while `GetDisplayScale()` reports the window's current
  content scale. Neither is substituted with the display snapshot's base
  content scale.
- Shared geometry consists of signed `ScreenPosition`, non-negative
  `ScreenExtent`, `ScreenRectangle`, floating `LogicalPoint` and
  `LogicalExtent`, `LogicalRectangle`, and distinct non-negative `LogicalSize`
  and `PixelSize`.
- Floating logical values use `IsValid` at public boundaries to reject
  infinities, NaNs, and negative extents. Zero extents and sizes are valid
  observations; feature descriptors may require positive sizes.
- Window logical size, pixel size, pixel density, and display scale remain
  distinct values with explicit names.

### Events, Input, And Time

- A `std::variant` of typed, project-owned event structs with owned payloads.
  There is no parallel event-kind enum.
- Global quit, window, display, keyboard, text/composition, mouse, inbound
  drag-and-drop, and dialog-completion events. The core window vocabulary
  distinguishes close, move, logical-size, pixel-size, focus, visibility,
  minimized/maximized state, presentation, display, display-scale, and
  pointer-boundary changes.
- Window display changes preserve an optional destination `DisplayId` when the
  backend reports no current display or the destination is not yet resolvable.
  Display-scale, display-position, content-scale, and usable-bounds events are
  re-query notifications because SDL provides no corresponding value.
  Desktop/current display-mode changes carry an optional `ScreenExtent` when
  SDL supplies positive dimensions.
- Required `WindowId` values only on events that are necessarily window-scoped.
  Keyboard, text/composition, and mouse input events use
  `std::optional<WindowId>`: backend window ID zero becomes no target, while a
  nonzero stale or unresolved target causes the event to be dropped.
- Event timestamps are strong chrono nanosecond observation timestamps sampled
  with `ponder::core::Timestamp::Now()` when a backend event is translated.
  They deliberately do not expose the backend event's native epoch.
- `Runtime::TimeNow()` samples the same core steady-clock domain as event and
  dialog timestamps, so callers can correlate runtime observations. Platform
  never calls `SDL_GetTicksNS()`; only differences between timestamps from the
  core domain are semantically meaningful.
- Public platform declarations use `ponder::core::Duration` and
  `ponder::core::Timestamp` directly. Platform defines no timing forwarding
  header or aliases. Frame deltas and frame pacing remain application policy.
- One private production translator owns SDL-to-project event conversion.
  Unrepresentable timestamps, malformed required data, unresolved required
  identities, and unsupported SDL events produce no project event. Future
  display-orientation values map to `DisplayOrientation::Unknown`.
- Polling that skips unknown, unsupported, and stale SDL events until it returns
  one translated event or the SDL queue is genuinely empty.
- Owner-thread event waiting with a nonnegative `ponder::core::Duration`. It
  preserves polling's dialog and SDL-event processing order, rounds positive
  sub-millisecond waits upward, and returns no event on timeout or an explicit
  wake. The wait primitive supplies blocking mechanics without selecting an
  application frame cadence or main-loop policy.
- A thread-safe `Runtime::EventWake()` command backed by a private event. Wake
  sentinels are consumed internally and never enter the public event variant;
  asynchronous dialog completion wakes an idle event loop after its completion
  becomes observable.
- Display additions enter the runtime identity registry before their event is
  returned. Removals retain their prior project identity through translation
  and become disconnected tombstones afterward. A previously unseen
  non-removal display event reconciles the live topology before translation;
  it does not revive a disconnected tombstone.
- Project-owned physical keys, closed logical Unknown/Unicode/named values,
  side-specific modifiers, and repeat state. Unknown mappings remain explicit;
  raw platform scan values are never exposed.
- Owned UTF-8 input and composition text. Composition selections are optional
  ranges measured in Unicode characters rather than UTF-8 bytes; a present
  zero-length range remains distinct from an unavailable selection.
- Window text-input start/stop, live active-state query, composition clearing,
  and logical IME input-area/cursor control. Logical area values round to the
  nearest backend integer after finite/range validation.
- `MouseMotionEvent`, `MouseButtonEvent`, and `MouseWheelEvent` preserve
  floating logical positions. Motion also preserves floating relative
  movement; button events distinguish left, right, middle, X1, X2, and
  `Unknown`; wheel events expose horizontal and vertical floating values.
  Every position, relative-movement component, and wheel value must be finite
  or the event is dropped. Motion coordinates follow window-logical axes,
  including positive Y downward. Wheel values are normalized separately so
  positive X means right and positive Y means up.
- Inbound drag-and-drop uses `DropBeginEvent`, `DroppedFileEvent`,
  `DroppedTextEvent`, `DropPositionEvent`, and `DropCompleteEvent`. Window
  targets are optional using the same zero/stale rules as keyboard, text, and
  mouse input. Begin events carry no position; the other drop events carry a
  finite logical position. File payloads are owned `std::filesystem::path`
  values, text payloads are owned UTF-8 strings, and available source
  application text is copied. Platform does not keep global state merely to
  validate begin/complete ordering; malformed payloads are dropped.
- `Window::SetMouseGrab()` and `Window::SetRelativeMouseMode()` are direct
  window-scoped commands that throw on failure. `IsMouseGrabbed()` and
  `IsRelativeMouseModeEnabled()` are live, infallible observations and are not
  cached by platform. A hidden window's pending grab request is not necessarily
  visible through SDL's live grab query, so every mouse-grab setter request is
  forwarded to the backend.
- `Runtime::MouseSetCapture()` controls explicit global capture.
  `MouseGetGlobalPosition()` returns a floating desktop-relative
  `LogicalPoint`. Both retain `Result` for unsupported or unavailable host
  behavior that permits local fallback. Enabling capture or querying a
  meaningful global position returns `Unsupported` when the active video driver
  cannot provide the capability; disabling capture remains an idempotent
  cleanup operation. Wrong-thread use and malformed backend data throw.
  Platform exposes no capture-state query because SDL has no direct,
  authoritative global query and capture may be released asynchronously on
  focus loss.
- `Runtime::MouseSetSystemCursor()` selects a standard shape without changing
  visibility. `MouseShowCursor()`, `MouseHideCursor()`, and
  `MouseIsCursorVisible()` control and observe visibility separately. Selection,
  show, and hide return `void` and throw platform exceptions on failure;
  visibility remains a direct observation. Runtime lazily creates and caches
  each selected SDL system cursor; selection does not transfer ownership, and
  every cached cursor is destroyed before SDL shutdown. Custom cursor images
  are deferred.
- Window-scoped pointer-enter and pointer-leave events.
- System cursors for default, text input, move, north-south/east-west and both
  diagonal resizes, pointer, wait, progress, and not-allowed. A UI request for
  no cursor maps to cursor hiding rather than another shape.

### Renderer And UI Interop

- `WindowGraphicsCompatibility` has exactly `Default`, `Vulkan`, and `Metal`.
  `Default` requests no graphics-specific SDL flag. `Vulkan` maps to
  `SDL_WINDOW_VULKAN` on Windows/Linux only. `Metal` is reserved for the later
  native macOS backend and maps to `SDL_WINDOW_METAL` only for that backend.
- Vulkan-compatible windows are unsupported on macOS. Platform must not translate
  a Vulkan request into Metal-layer semantics.
- Each `Window` stores the exact project compatibility selected at creation. It
  is never reconstructed from SDL flags, whose meaning is host-dependent.
- `NativeWindowHandle` for the current Vulkan interop contract is a closed tagged
  variant of:
  - `NativeWin32Window`: `void* instance` and `void* window`.
  - `NativeX11Window`: `void* display` and `std::uintptr_t window`.
  - `NativeWaylandWindow`: `void* display` and `void* surface`.
- No Cocoa or Metal-layer payload is part of the current Vulkan native-handle
  contract. The future Metal backend must define its own macOS payload before
  platform exposes it.
- `Window::GetNativeHandle()` is available only for Vulkan-compatible windows
  using SDL video drivers `"windows"`, `"x11"`, and `"wayland"`. A `Default`
  window returns `InvalidArgument`; a `Metal` window has no native-handle
  payload until the Metal backend defines one; every other driver returns
  `Unsupported`. Those are the only value-based failures. Missing or malformed
  native properties and unexpected backend failures throw.
- Native data is a borrowed snapshot. It is valid only on the runtime owner
  thread while the owning window and relevant native state remain valid. Callers
  re-query after window destruction/recreation, show/hide, presentation,
  minimized/maximized restore, display migration, or any renderer-owned surface
  teardown/rebuild boundary where stale native state would matter.
- `ponder_render` interprets native payloads privately and owns devices,
  contexts, surfaces, swapchains, and rendering.
- The caller destroys renderer presentation state before its platform window.
  Platform's child registry cannot observe or diagnose renderer-owned surfaces.
- `ponder_ui` consumes project-owned events, cursor, clipboard, text-input,
  display, and window APIs when its later input milestone is implemented.
  Platform does not expose an `SDL_Window` or `SDL_Event` bridge for UI.
- `ponder_ui` has no direct SDL dependency or compile usage requirement.
  Pointer warping, gamepad navigation, touch/pen sources, and UI-owned native
  multi-window behavior require their own later contracts.
### Clipboard, External URIs, Dialogs, And Processes

- Runtime-owned UTF-8 text clipboard access uses
  `Runtime::ClipboardGetText()` returning `ponder::core::Result<std::string>` and
  `ClipboardSetText()` returning `ponder::core::VoidResult`. `SdlRuntime`
  serializes calls, while SDL access remains owner-thread-only. Invalid input,
  unsupported or unavailable access, malformed external text, and host failures
  are locally actionable result errors; wrong-thread and lifecycle misuse still
  throw. Set validates UTF-8 and embedded nulls before SDL receives a
  null-terminated copy. Empty clipboard text is a successful value. Rich MIME
  and binary clipboard data are deferred.
- Owner-thread `UriOpenExternal(std::string_view)` on `Runtime`, backed
  privately by SDL and returning `ponder::core::VoidResult` for invalid
  user-provided text and host launch/capability failures. Platform validates
  only non-empty null-free UTF-8 input and deliberately does not own
  scheme-specific shell policy. Wrong-thread and lifecycle failures throw.
- Clipboard reads explicitly distinguish successful empty text from SDL failure
  by clearing SDL error state before the ambiguous call and checking it
  immediately afterward before SDL-owned text is freed; error text is still
  never parsed.
- Inbound file and text drag-and-drop with owned payloads. Outbound dragging is
  deferred.
- Asynchronous open-file, save-file, and open-folder requests are direct
  `Runtime::Dialog*` operations with copied descriptors, filters, and default
  paths.
- Public dialog kinds, request descriptors, file filters, and the strong request ID live in
  `Dialogs.hpp` under `ponder::platform::dialogs`; dialog completion events and
  their outcome data remain in `PlatformEvent.hpp` under `ponder::platform`.
- Successful dialog requests return a strong `dialogs::DialogRequestId` inside
  a `ponder::core::Result`. `SdlRuntime` directly owns
  the sole pending-request registry, mutex, request-ID counter, and completion
  FIFO, and exposes its count and deterministic metadata snapshots; runtime child
  tracking does not mirror dialog request identities.
  Exactly one self-describing `DialogCompletedEvent` is delivered on the owner
  thread with the request metadata and one of three outcomes: selected paths plus
  an optional selected-filter index, cancellation, or failure. Cancellation is
  not an error.
- Synchronous open-file, save-file, and open-folder submission returns
  `Result<dialogs::DialogRequestId>`, and `Runtime::DialogShutdown()` returns
  `VoidResult`. Pending counts, pending snapshots, pending-state checks,
  outstanding-request counts, and completion polling return direct `noexcept`
  values under their owner-thread and initialized-runtime preconditions.
- The four fallible dialog methods contain every ordinary C++ exception raised
  in their call stacks. They log caught failures through the core facilities
  with the `platform` category, roll back partial request state when necessary,
  and return `BackendFailure`. A caught `ponder::core::Exception` contributes
  its message, stack trace, and source location to the returned diagnostic, so a
  platform code already embedded in the message remains visible without being
  parsed for control flow. Standard and unknown exceptions receive contextual
  diagnostics as well. No typed platform exception is reintroduced. The five
  direct observers have no recoverable failure channel.
- `SdlRuntime` submits request-owned storage directly to SDL; there is no dialog
  manager, dialog state facade, virtual backend, or backend factory. SDL launch,
  request translation, callback translation, and callback handoff remain
  cohesive inside `SdlRuntime.cpp`. Each callback receives a weak completion
  token, so duplicate or late callbacks cannot retain request state or access a
  destroyed runtime.
  Each parented request owns an RAII parent-window lease until owner-thread
  completion polling removes the request, releases the lease, and returns the
  event.
- The private `WindowRegistry` owns direct indices for both project
  `WindowId` values and backend window IDs. `AcquireDialogLease()` returns a
  lease directly from stable shared lease state and throws when the requested
  parent cannot be leased; the public dialog submission boundary catches that
  failure and returns `BackendFailure` with the original platform diagnostic.
  The lease never retains a `WindowImpl` pointer. Multiple requests may lease
  the same parent independently, and each lease prevents parent destruction
  until consumed.
- Dialog completions are FIFO by callback enqueue order. Request order and
  ordering relative to SDL events are unspecified; the event's `request` record
  carries the correlation contract.
- SDL has no general dialog cancellation operation. A parent window and runtime
  may not be destroyed while their dialog request is registered or its
  completion remains unconsumed; this is a release-active lifetime contract.
- Shell-free child-process launch. The executable becomes `argv[0]` and
  descriptor arguments follow verbatim.
- Move-only process tracking with
  `Wait() -> ponder::core::Result<ProcessExitStatus>`,
  nonblocking `TryWait() -> ponder::core::Result<std::optional<ProcessExitStatus>>`,
  and explicit termination returning `ponder::core::VoidResult`. `LaunchProcess` also retains
  `ponder::core::Result<Process>`. These results represent caller input,
  child-state, wait, launch, and termination failures that immediate
  orchestration can handle; programming, lifecycle, and unexpected internal
  failures throw.
- `Wait()` is a blocking wait for process exit. It must not run from the desktop
  event loop or any thread that must continue pumping UI, platform, or render
  work while the child is running.
- `TryWait()` never blocks. A successful empty optional means the child is still
  running; a present status confirms termination. It is the process-completion
  primitive for event loops and higher-level orchestration.
- Termination modes are `GracefulPreferred` and `Force`; graceful delivery is
  best effort and may fall back to forced termination where SDL requires it.
- `ProcessExitStatus` distinguishes a 32-bit unsigned normal exit code, signal
  termination, and an unknown termination.
- Destroying a process object releases public tracking without killing or
  terminating the child. If the child is still running, platform transfers SDL
  tracking to a prestarted private asynchronous cleanup worker. Cleanup polls
  all abandoned handles fairly, retains every inconclusive wait, and destroys a
  handle only after exit is confirmed; caller destruction itself does not wait.
  The cleanup worker is a deliberately process-lifetime service: its storage is
  constant-initialized before dynamic globals and is not destructed during C++
  static teardown. Late or static-storage `Process` destruction therefore stays
  non-blocking and cannot enqueue through a destroyed singleton; the OS owns the
  worker's final process-exit teardown.
  Standard-IO piping, working-directory control, environment customization,
  process trees, and supervision are deferred.

## Errors And Diagnostics

- Platform is exception-first. Failures that should never or only rarely occur,
  or for which the caller cannot complete useful local recovery, throw.
- Platform failures throw `ponder::core::Exception` and are constructed at the
  failure site with `PLATFORM_EXCEPTION`. The macro embeds the formatted
  `PlatformErrorCode` in the exception message; platform does not use
  `ExceptionWithData<PlatformErrorCode>` or define a public exception alias.
- The non-dialog public `Result` surface contains these operations:
  - `Runtime::ClipboardGetText` and `ClipboardSetText` for locally actionable
    clipboard input, availability, malformed-data, and host failures.
  - `Runtime::DisplayGetInfo` for a stale nonzero display ID.
  - `Runtime::MouseSetCapture` and `MouseGetGlobalPosition` for
    unsupported or unavailable host capabilities.
  - `Runtime::UriOpenExternal` for invalid user input and host
    launch/capability failure.
  - `LaunchProcess`, `Process::Wait`, `Process::TryWait`, and
    `Process::Terminate` for actionable input, child-state, and operating-system
    failures.
  - `Window::GetNativeHandle` for incompatible graphics mode or an unsupported
    native driver.
  - `Window::GetDisplayId` for transiently unresolved display topology.
- The dialog surface adds four `noexcept` Result-bearing operations: the three
  `DialogShow*` submissions and `DialogShutdown`. Its five direct `noexcept`
  observer operations are `DialogGetPendingCount`, `DialogHasPending`,
  `DialogGetPending`, `DialogPollCompletion`, and
  `DialogGetOutstandingRequestCount`.
- A retained non-dialog Result operation still throws for wrong-thread use,
  programming errors, lifecycle violations, or unexpected backend corruption.
  Fallible dialog operations are the deliberate exception-shielded boundary:
  caught core, standard, and unknown exceptions are logged and converted to
  detailed errors after appropriate rollback. Direct observers require their
  documented preconditions.
- Asynchronous `DialogFailure{ponder::core::Error}` remains event data because
  completion occurs after the initiating call stack has gone. It is distinct
  from a synchronous Result error returned before a request is accepted.
- Public `PlatformErrorCode` values provide stable diagnostic and retained-result
  codes and convert constexpr to `ponder::core::ErrorCode`.
- Platform reserves numeric values `0x0001'0000` through `0x0001'FFFF`.
  Published codes are `InvalidArgument`, `RuntimeAlreadyActive`,
  `BackendFailure`, `NotFound`, `Unsupported`, and `WrongThread`. Names,
  numeric values, `ToErrorCode` behavior, and enum formatting fallbacks remain
  stable.
- Invalid descriptors and forged closed-enum values normally throw
  `InvalidArgument`. Wrong-thread public calls normally throw `WrongThread`.
  Fallible dialog entry points catch those exceptions and return
  `BackendFailure`, retaining the embedded original platform code and message
  diagnostically without parsing them; direct dialog observers assert their
  preconditions. Unsupported direct operations
  and malformed backend data embed their platform codes in exception messages,
  except typed hint lookup, which logs malformed SDL values and returns absence.
  Runtime pass-throughs use debug `PONDER_ASSERT` checks for successful
  initialization where required and for a non-null implementation; other
  moved-from use and impossible ownership/lifecycle invariants remain
  release-active `PONDER_VERIFY` failures.
- Other SDL failures use a statically selected category and platform error code.
  Error text is diagnostic data and is never parsed for control flow.
- Private SDL adapters copy `SDL_GetError()` immediately after a documented
  failure and before any other SDL call, then add operation and object context.
  They supply an explicit fallback if SDL returns an empty error string and
  provide distinct exception and retained-error paths.
- No exception may escape an SDL or operating-system callback, a thread entry
  point, or a destructor. Destructors, move operations, and release paths remain
  `noexcept` and contain operational cleanup failures. Ordinary fallible APIs
  and helpers do not promise `noexcept`.
- Lifecycle diagnostics use the core logging frontend with a `platform`
  category. Unknown events are silent or trace-only.

## Non-Responsibilities

- Domain models, project formats, chemistry data, IO/import policy, workflows,
  plugins, or compute-job behavior.
- Rendering backend implementation or graphics-resource ownership.
- UI retained trees, widgets, docking, presentation, or renderer integration.
- Desktop application workflow, main-loop policy, command routing, menus,
  recent-file lists, or project-specific behavior.
- Frame-delta accumulation, fixed timesteps, frame limiting, or idle policy.
- Direct Win32, Cocoa, X11, Wayland, DBus, or other OS APIs outside SDL3 unless
  a later decision deliberately expands the private backend.
- Pointer warping, gamepad navigation, touch/pen source discrimination, or
  UI-owned native multi-window policy.
- Rich clipboard data, outbound drag-and-drop, custom cursors, file watching,
  advanced process IO/supervision, packaging, or release integration.

## Dependencies

`ponder_platform` has `ponder::core` as a public CMake dependency and
`ponder::io` plus `ponder::SDL3` as private dependencies. Because platform is a
static library, private dependencies may still appear as link-only dependencies
of final executables; consumers must not inherit SDL compile definitions,
include paths, or public types.

Platform must not depend on `ponder_render`, `ponder_ui`, `ponder_project`,
`ponder_chemistry`, `ponder_scientific_data`, `ponder_workflow`,
`ponder_compute`, `ponder_plugin_sdk`, or `ponder-desktop`.

## Testing Boundary

- Pure value, header, and event-translation tests run without a desktop window.
- SDL-free header-self-containment and consumer targets verify that public
  headers do not acquire SDL transitively.
- A separate private translation-test target may include SDL and must exercise
  the exact translator used by production polling.
- Live SDL behavior uses a separate integration target and does not depend on
  `ponder-desktop`.
- Tests touching process-global SDL state or the system clipboard are serialized.
- A GUI-related test skips only after positively identifying an unsupported
  driver or capability. An unexpected initialization or window failure on a
  normal GUI host is a failure.
- Headless, hidden-window, CTest label, resource-lock, host-local command,
  portability, and manual dialog-smoke details live in
  `HeadlessAndHostVerification.md`.
