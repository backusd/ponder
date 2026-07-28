# ADR 0007: SDL3 Platform Backend

## Status

Accepted. The core namespace and failure policy are amended by
[ADR 0012: Core Namespace And Failure Policy](0012-core-namespace-and-failure-policy.md)
and [ADR 0013: Platform Failure Policy](0013-platform-failure-policy.md). The
combined decision, the 2026-07-23 platform namespace/timing migration, and the
2026-07-24 flat runtime consolidation are reflected below.

## Related Decisions

- [ADR 0008: Vulkan Renderer Backend](0008-vulkan-renderer-backend.md)
- [ADR 0012: Core Namespace And Failure Policy](0012-core-namespace-and-failure-policy.md)
- [ADR 0013: Platform Failure Policy](0013-platform-failure-policy.md)

## Context

`ponder` needs a durable desktop foundation before project loading, rendering,
retained UI, chemistry data, workflow, compute, or plugin systems are built on
top of it.

The platform layer must provide reusable operating-system integration without
turning the desktop executable into the owner of windowing and host-environment
behavior. It must remain usable by future executables and tests without leaking
backend-specific types through public headers.

SDL3 is already an allowed dependency in the repository. It provides the initial
implementation path for lifecycle, windows, events, display and high-density
data, clipboard access, file and folder dialogs, process launching,
drag-and-drop, timing, and native window data.

SDL initialization and the event queue are process-global. SDL video and most
window APIs also have main-thread requirements. These constraints must be
represented explicitly rather than hidden behind apparently independent runtime
or window objects.

The renderer and future retained UI need platform data, but exposing
`SDL_Window`, `SDL_Event`, or native OS declarations would make SDL3 part of the
public project contract. Renderer and UI integration therefore need
project-owned interoperability types and primitives.

## Decision

Use SDL3 as the private backend for the initial `ponder_platform` library.

### Dependency And Public Boundary

`ponder_platform` has a public dependency on `ponder_core` and private
dependencies on `ponder_io` and SDL3. Public platform headers expose only
project-owned types and standard-library types. SDL headers, SDL types, and
direct OS-specific types remain in implementation files or private headers.

`ponder_platform` must not depend on `ponder_render`, `ponder_ui`, project or
domain libraries, or `ponder-desktop`. The desktop executable owns application
policy and main-loop orchestration. The renderer owns graphics devices,
surfaces, swapchains, and drawing behavior. The UI library owns retained UI and
paint behavior.

### Runtime And Resource Ownership

There is at most one live logical `Runtime` per process. Runtime creation is
fallible and uses an explicit factory that returns `Runtime` directly and
throws a platform exception on failure.

`Runtime` and native resource owners are non-copyable and movable. `Runtime`
owns exactly one heap-stable `detail::RuntimeImpl` through `std::unique_ptr`;
production resolves that implementation directly to `SdlRuntime`. Moving the
public owner therefore cannot invalidate windows or other child resources.
Moved-from owners are valid only for destruction or move assignment.

`SdlRuntime` directly owns SDL lifecycle, hint stacks, clipboard
synchronization, dialog requests, event/display topology, cursor caches, and
child/window registries. The public API is a flat, subsystem-prefixed surface;
there are no manager service facades, intermediate runtime-state object,
runtime-backend aggregate, or virtual dialog backend. Child objects borrow the
heap-stable implementation under an enforced lifetime contract. Windows and
outstanding platform requests must finish before runtime destruction. Before
SDL shutdown, the runtime uses release-active verification to require empty
child, window, and dialog registries. Because verification does not return
normally on failure, SDL cannot shut down while a child can still use it. Every
child operation verifies its state and owner thread before calling SDL.

Platform owns the SDL lifecycle exclusively and refuses creation while an SDL
subsystem is already initialized. Before process-global `SDL_Quit()`, teardown
verifies that no subsystem outside the runtime-owned video/event set appeared.
Runtime creation verifies the startup thread captured during module
initialization as well as `SDL_IsMainThread()`, then records that thread as the
owner. The module check is defense in depth; the executable remains responsible
for invoking creation from process entry because a library cannot prove that
identity portably under deferred initialization or dynamic loading. SDL-backed
APIs are owner-thread APIs unless a specific API is documented otherwise.
Internal dialog callbacks may execute on another thread, but they may only copy
completion data into synchronized private state; public completion delivery
occurs on the owner thread.

Application name, version, and identifier are applied with checked metadata
property calls before video initialization. Absent optional properties are
cleared. The runtime does not snapshot or restore prior metadata; after
`SDL_Quit()`, the resulting process state is left to SDL.

### Windows, Displays, And Renderer Interop

The first implementation supports multiple windows. Each window has a
runtime-local, project-owned 64-bit `WindowId`. Zero is invalid, IDs increase
monotonically, and an ID is not reused during a runtime lifetime. Backend window
IDs are translated privately. `DisplayId` follows the same zero-invalid,
monotonic, non-reused runtime-local rules. Events for destroyed or unknown
backend resources are ignored deterministically.

Window creation includes `WindowGraphicsCompatibility` with exactly `Default`,
`Vulkan`, and `Metal`. `Default` requests no graphics-specific flag. `Vulkan`
maps to `SDL_WINDOW_VULKAN` on Windows and Linux only. macOS Vulkan presentation
is unsupported and is not translated through Metal-layer semantics. `Metal` is
reserved for a later native macOS renderer and maps to `SDL_WINDOW_METAL` only
for that backend. This value does not transfer surface or device ownership into
platform.
`Window` stores the exact project compatibility selected in its descriptor and
never reconstructs it from host-dependent SDL flags.

Native window data is a closed tagged variant with `NativeWin32Window`
(`HINSTANCE`, `HWND`), `NativeX11Window` (`Display*`, X11 `Window`),
and `NativeWaylandWindow` (`wl_display*`, `wl_surface*`) payloads represented
only by opaque pointers and integer-sized values. It is not a generic bag of
fields. No Cocoa or Metal-layer payload is part of the current Vulkan interop
contract; the future native Metal backend must define its own exact payload
before platform exposes it.

On Vulkan hosts, renderer-private code performs OS casts and creates and owns
`VkSurfaceKHR`. Native values are borrowed owner-thread snapshots valid while
their window and native state remain alive.

Display data uses project-owned snapshot values. Display content scale, window
pixel density, and window display scale are scalar values with distinct names.
Logical window size and pixel size are separate types. Disconnected display IDs
become stale; reconnecting creates a new project ID.

### Events, Input, And Time

Public events use a `std::variant` of small typed event structs with owned
payloads. There is no redundant event-kind enum. Global quit requests are
distinct from window close requests, and close requests never destroy a window
automatically.

Event timestamps use the core-owned `Timestamp` nanosecond representation and
preserve SDL's monotonic tick domain. `Runtime::TimeNow()` samples that same
SDL domain so applications can correlate observations with queued events.
`ponder::core::Timestamp::Now()` is a separate steady-clock source whose epoch
need not match SDL. Only differences between timestamps from the same source are
semantically meaningful. Public platform declarations spell
`ponder::core::Timestamp` and `ponder::core::Duration` directly; platform does
not provide a timing forwarding header or aliases.
Stateful frame-delta calculation, fixed-step simulation, frame limiting, and
idle policy belong to the application layer.

Event polling skips unknown, unsupported, and stale backend events until it can
return one translated project event or the backend queue is genuinely empty.

`ponder_ui` consumes project-owned platform events and primitives and has no
direct SDL dependency or compile usage requirement. Platform therefore provides
the required low-level primitives, including standard system cursors, clipboard
text, text-input activation, IME input-area control, pointer capture/position,
and external-URI opening, without owning retained UI behavior.

### Errors And Asynchronous Services

The exception-first policy in ADR 0013 reserves `ponder::core::Result<T>` and
`ponder::core::VoidResult` for exactly nine public operations whose expected
failures can be handled at the immediate orchestration layer. Other ordinary
platform failures throw `ponder::core::Exception`, with `PLATFORM_EXCEPTION`
embedding the formatted `PlatformErrorCode` in the diagnostic message.
Retained-result operations may also throw for programming, lifecycle, threading,
or unexpected backend-contract failures. Private adapters copy SDL error text
immediately, add operation context, and map failures to stable core categories
and public `PlatformErrorCode` values.

Dialog cancellation is a normal outcome rather than an error. Synchronous
submission returns a request ID directly and throws on submission failure.
Dialog descriptors and filters are copied into owned request state. Asynchronous
selection, cancellation, or `DialogFailure` completion is marshalled to the
owner-thread event stream exactly once. No exception escapes a callback,
destructor, or worker-thread entry point.

The initial process API is shell-free and intentionally small. Process creation
does not require `Runtime`. A `Process` is bound to its launching thread;
all operations and destruction occur on that thread without concurrent
access. This is the documented exception to the runtime-owner-thread rule.
Destroying a process releases public ownership but does not wait for, kill, or
terminate the child. A still-running handle transfers to a prestarted private
cleanup worker, which retains and polls it until the child can be reaped without
blocking the caller. The worker uses deliberately process-lifetime storage, so
late or static-storage destruction never re-enters an already destroyed singleton
or joins a worker. The OS owns its final process-exit teardown. Explicit `Wait()` remains
blocking and must not run on an event-pumping thread. Compute scheduling, process
supervision, streaming IO, cancellation trees, and remote execution remain
outside platform.

## Consequences

The desktop app can start from reusable platform primitives instead of temporary
windowing code in the executable.

SDL3 can be upgraded or replaced behind the platform boundary without changing
most consumers, provided public types and behavior remain project-owned.

The single-runtime and owner-thread contracts are intentional constraints.
Tests and applications must structure resource lifetime accordingly.

ADR 0008 finalizes Vulkan as the first renderer on Windows and Linux and
explicitly excludes Vulkan on macOS. A later native Metal backend requires
another deliberate decision before the macOS interop payload is added.

Pure value and translation tests are separate from live SDL integration tests.
SDL-free public-header self-containment and consumer targets are explicitly
PCH-free; they verify direct include completeness and that SDL compile usage does
not leak through the public target. The platform implementation also remains
PCH-free unless measurements justify deliberately enabling one. Integration
tests may skip only after positively detecting a missing GUI capability;
unexpected initialization or window failures remain test failures.
