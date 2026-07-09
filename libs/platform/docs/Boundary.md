# Platform Library Boundary

`ponder_platform` owns reusable operating-system and desktop-platform
integration.

Status: platform contracts revised on 2026-07-08.

## Decision Records

- [ADR 0007: SDL3 Platform Backend](../../../docs/adr/0007-sdl3-platform-backend.md)
- [ADR 0008: Vulkan Renderer Backend](../../../docs/adr/0008-vulkan-renderer-backend.md)

## Public API

- Public headers live under `include/ponder/platform/`.
- Source code uses the `pond::platform` namespace.
- The CMake target is `ponder_platform`; the alias is `ponder::platform`.
- Public APIs expose project-owned and standard-library types only.
- Public text is UTF-8. Borrowed text uses `std::string_view`; owned text uses
  `std::string`.
- Filesystem locations use `std::filesystem::path`. Platform owns conversion
  between paths and SDL's UTF-8 representation.
- Native resource owners use PIMPL or an equivalent private representation. SDL
  and OS declarations do not appear in public headers.

## Lifetime And Threading

- At most one logical `PlatformRuntime` may be live in a process.
- `PlatformRuntime::Create()` is a fallible factory returning
  `pond::core::Result<PlatformRuntime>`.
- Windows are created through the runtime with a fallible `CreateWindow()`
  operation.
- Runtime and resource owners are non-copyable and movable. Moved-from owners
  may only be destroyed or assigned another valid value.
- Runtime state is heap-stable and runtime-owned. Children borrow it under an
  enforced lifetime contract, so moving the public runtime cannot invalidate
  them.
- Windows and pending runtime services must complete before runtime destruction.
  Release-active verification requires an empty child/request registry before
  SDL shutdown and does not return normally on violation.
- Platform owns SDL's process-global lifecycle exclusively and rejects runtime
  creation if any SDL subsystem is already initialized. Teardown verifies that
  no subsystem outside the runtime-owned video/event set appeared before using
  process-global `SDL_Quit()`.
- Runtime creation verifies both the startup thread captured during module
  initialization and `SDL_IsMainThread()`, then captures that thread as the
  owner. This is defense in depth: the executable still invokes creation from
  process entry because deferred initialization and dynamic loading prevent a
  library-only check from proving that identity portably. SDL-backed public APIs
  verify the captured owner in every build.
- SDL dialog callbacks are an internal exception: they may copy completion data
  into synchronized private storage from another thread, but public delivery
  occurs only on the runtime owner thread.
- Process objects are a documented exception to the main-thread rule. A process
  is bound to the thread that launches it, and its operations and
  destruction occur on that thread without concurrent access.

## Responsibilities

### Runtime And Windows

- SDL3 video/event initialization and shutdown behind explicit RAII ownership.
- Apply the UI-required SDL mouse policy privately: focus-gaining clicks pass
  through, and SDL automatic capture is disabled in favor of explicit project
  capture. Restore each prior effective nullable hint value after `SDL_Quit()`;
  SDL does not expose enough information to restore its former priority or
  provenance.
- Apply owned, validated application metadata through checked SDL property
  operations before initializing video. Restore the prior effective nullable
  metadata values after failed creation and after `SDL_Quit()` at shutdown.
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
  all other state remains live backend data. Simultaneous minimized and
  maximized flags without that marker are `BackendFailure`.
- Window-manager presentation and state requests may be asynchronous. Success
  means the request was accepted, not that the observed state already changed.
  `Restore()` requests the OS restore behavior and may restore a minimized
  window to its prior maximized state.
- Mutators are idempotent. Maximizing a non-resizable window and positively
  identified unavailable backend operations return `Unsupported`. Decoration
  and resizability cannot change while fullscreen. Drivers that silently ignore
  a requested decoration, resizability, or always-on-top change are also
  reported as `Unsupported` when the backend flag does not latch.
- Window close requests are events. Platform never destroys a window merely
  because the OS requested close.

### Displays And Coordinates

- `PlatformRuntime::EnumerateDisplays()` and `GetDisplayInfo(DisplayId)` return
  project-owned `DisplayInfo` snapshots. Each snapshot contains a runtime-local
  `DisplayId`, owned name, `ScreenRectangle` bounds and usable bounds, optional
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
  same ID space as runtime snapshots. `GetPixelDensity()` reports the ratio of
  window pixel coordinates to logical coordinates, while `GetDisplayScale()`
  reports the window's current content scale. Neither is substituted with the
  display snapshot's base content scale.
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
- Required `WindowId` values only on events that are necessarily window-scoped;
  input events may represent a missing target when SDL supplies no window.
- Event timestamps expressed as a strong chrono nanosecond value in SDL's
  monotonic tick domain, with the same epoch as `SDL_GetTicksNS()`. Only
  timestamp differences are semantically meaningful.
- A monotonic `Now()` primitive in the same clock domain. Frame deltas and frame
  pacing remain application policy.
- One private production translator owns SDL-to-project event conversion.
  Unrepresentable timestamps, malformed required data, unresolved required
  identities, and unsupported SDL events produce no project event. Future
  display-orientation values map to `DisplayOrientation::Unknown`.
- Polling that skips unknown, unsupported, and stale SDL events until it returns
  one translated event or the SDL queue is genuinely empty.
- Display additions enter the runtime identity registry before their event is
  returned. Removals retain their prior project identity through translation
  and become disconnected tombstones afterward. A previously unseen
  non-removal display event reconciles the live topology before translation;
  it does not revive a disconnected tombstone.
- Project-owned physical and logical key values, modifiers, repeat state, UTF-8
  text, and IME composition ranges.
- Text-input start/stop, active-state, composition clearing, and IME input-area
  control.
- Mouse motion, buttons, and wheel events. Window grab/relative mode remain on
  `Window`; global capture, global position, cursor visibility, and standard
  system cursor selection live on `PlatformRuntime`. Custom cursor images are
  deferred.
- Window-scoped pointer-enter and pointer-leave events. Wheel values use positive
  X for right and positive Y for up.
- System cursors for default, text input, move, north-south/east-west and both
  diagonal resizes, pointer, wait, progress, and not-allowed. A UI request for
  no cursor maps to cursor hiding rather than another shape.

### Renderer And Dear ImGui Interop

- `WindowGraphicsCompatibility` has exactly `Default` and `Vulkan`. `Default`
  requests no graphics-specific SDL flag. `Vulkan` maps to
  `SDL_WINDOW_VULKAN` on Windows/Linux and `SDL_WINDOW_METAL` on macOS.
- Each `Window` stores the exact project compatibility selected at creation. It
  is never reconstructed from SDL flags, whose meaning is host-dependent.
- `NativeWindowHandle` is a closed tagged variant of:
  - `NativeWin32Window`: `void* instance` and `void* window`.
  - `NativeX11Window`: `void* display` and `std::uintptr_t window`.
  - `NativeWaylandWindow`: `void* display` and `void* surface`.
  - `NativeCocoaWindow`: `void* metalLayer`.
- Native interop is available only for Vulkan-compatible windows using SDL video
  drivers `"windows"`, `"x11"`, `"wayland"`, and `"cocoa"`. A `Default`
  window is incompatible; every other driver returns `Unsupported`.
- Native data is a borrowed snapshot. It is valid only on the runtime owner
  thread while the owning window and relevant native state remain valid; callers
  re-query it after documented native-window state changes.
- On macOS, each Vulkan-compatible `Window` lazily owns at most one cached
  `SDL_MetalView`. Repeated queries expose the same borrowed `CAMetalLayer*`, and
  platform destroys the view before that window.
- `ponder_render` interprets native payloads privately and owns devices,
  contexts, surfaces, swapchains, and rendering.
- The caller destroys renderer presentation state before its platform window.
  Platform's child registry cannot observe or diagnose renderer-owned surfaces.
- `ponder_ui` implements its Dear ImGui platform adapter over project-owned
  events, cursor, clipboard, text-input, display, and window APIs. Platform does
  not expose an `SDL_Window` or `SDL_Event` bridge for the bundled SDL ImGui
  backend.
- `ponder_ui` has no direct SDL dependency or compile usage requirement. It uses
  a custom single-OS-viewport adapter; pointer warping, gamepad navigation,
  touch/pen sources, and secondary ImGui platform windows are not required
  initially.

### Clipboard, External URIs, Dialogs, And Processes

- Owner-thread UTF-8 text clipboard get/set operations on `PlatformRuntime`.
  Rich MIME and binary clipboard data are deferred.
- Owner-thread `OpenExternalUri(std::string_view)` on `PlatformRuntime`, backed
  privately by SDL and returning a contextual `VoidResult`.
- Clipboard reads explicitly distinguish successful empty text from SDL failure
  by clearing SDL error state before the ambiguous call and checking it
  immediately afterward; error text is still never parsed.
- Inbound file and text drag-and-drop with owned payloads. Outbound dragging is
  deferred.
- Asynchronous `PlatformRuntime` open-file, save-file, and open-folder requests
  with copied descriptors, filters, and default paths.
- Dialog requests return a strong `DialogRequestId`. Exactly one
  `DialogCompletedEvent` is delivered on the owner thread with one of three
  outcomes: selected paths plus an optional selected-filter index, cancellation,
  or failure. Cancellation is not an error.
- Dialog completions are FIFO by callback enqueue order. Request order and
  ordering relative to SDL events are unspecified; IDs and timestamps carry the
  correlation contract.
- SDL has no general dialog cancellation operation. A parent window and runtime
  may not be destroyed while their dialog request is registered or its
  completion remains unconsumed; this is a release-active lifetime contract.
- Shell-free child-process launch. The executable becomes `argv[0]` and
  descriptor arguments follow verbatim.
- Move-only process tracking with
  `Wait() -> pond::core::Result<ProcessExitStatus>` and explicit termination
  returning `pond::core::VoidResult`.
- Termination modes are `GracefulPreferred` and `Force`; graceful delivery is
  best effort and may fall back to forced termination where SDL requires it.
- `ProcessExitStatus` distinguishes a normal exit code, signal termination, and
  an unknown termination. Non-blocking status polling is deferred because SDL's
  documented API cannot distinguish a running process from every failure.
- Destroying a process object releases SDL tracking state but does not
  implicitly wait for, kill, or terminate the child. Standard-IO piping,
  working-directory control, environment customization, process trees, and
  supervision are deferred.

## Errors And Diagnostics

- Recoverable failures use `pond::core::Result<T>` or
  `pond::core::VoidResult`.
- Public `PlatformErrorCode` values provide stable machine-actionable codes and
  convert constexpr to `pond::core::ErrorCode`.
- Platform reserves numeric values `0x0001'0000` through `0x0001'FFFF`.
  Published codes are `InvalidArgument`, `RuntimeAlreadyActive`,
  `BackendFailure`, `NotFound`, and `Unsupported`.
- Public descriptor validation maps to `InvalidArgument`; known unavailable
  capabilities map to `Unsupported`; stale resources map to `NotFound`.
- Other SDL failures use a statically selected category and platform error code.
  Error text is diagnostic data and is never parsed for control flow.
- A single private adapter copies `SDL_GetError()` immediately after a
  documented SDL failure and adds operation and object context. It supplies an
  explicit fallback if SDL returns an empty error string.
- Invalid internal state and violated lifetime or thread contracts use project
  assertions or release-active verification together with state checks that
  prevent unsafe SDL calls.
- Lifecycle diagnostics use the core logging frontend with a `platform`
  category. Unknown events are silent or trace-only.

## Non-Responsibilities

- Domain models, project formats, chemistry data, IO/import policy, workflows,
  plugins, or compute-job behavior.
- Rendering backend implementation or graphics-resource ownership.
- Dear ImGui context, widgets, docking, presentation, or renderer adapters.
- Desktop application workflow, main-loop policy, command routing, menus,
  recent-file lists, or project-specific behavior.
- Frame-delta accumulation, fixed timesteps, frame limiting, or idle policy.
- Direct Win32, Cocoa, X11, Wayland, DBus, or other OS APIs outside SDL3 unless
  a later decision deliberately expands the private backend.
- Pointer warping, gamepad navigation, touch/pen source discrimination, or
  secondary Dear ImGui OS-window management in the first UI adapter.
- Rich clipboard data, outbound drag-and-drop, custom cursors, file watching,
  advanced process IO/supervision, packaging, or release integration.

## Dependencies

`ponder_platform` has `ponder::core` as a public CMake dependency and
`ponder::SDL3` as a private dependency. Because platform is a static library,
SDL may still appear as a link-only dependency of final executables; consumers
must not inherit SDL compile definitions, include paths, or public types.

Platform must not depend on `ponder_render`, `ponder_ui`, `ponder_project`,
`ponder_chemistry`, `ponder_scientific_data`, `ponder_io`, `ponder_workflow`,
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
