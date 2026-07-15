# Render Library Boundary

`ponder_render` owns renderer backend lifetime and renderer-facing presentation foundations.

Status: bootstrap implementation audited for REND-020 on 2026-07-14.

## Decision Records

- [ADR 0008: Vulkan Renderer Backend](../../../docs/adr/0008-vulkan-renderer-backend.md)

## Public API

- Public headers live under `include/ponder/render/`.
- Source code uses the `pond::render` namespace.
- The CMake target is `ponder_render`; the alias is `ponder::render`.
- Public headers expose only project-owned, standard-library, `ponder_core`, and
  `ponder_platform` types.
- `GetRequiredWindowGraphicsCompatibility() noexcept` is callable before a
  platform window exists. In a Vulkan build, it returns the build-selected
  `WindowGraphicsCompatibility::Vulkan` value.
- Backend choice is selected by the build. The bootstrap API has no runtime
  backend selector.

## Bootstrap Responsibilities

The first milestone validates create, clear, present, resize, minimize/restore,
and shutdown across multiple platform windows. It does not implement scenes,
public GPU resources, shaders, meshes, textures, picking, offscreen targets,
readback, Dear ImGui, or 3D visualization.

The foundation owns:

- Dynamic Vulkan loader and instance lifetime.
- Physical/logical device selection and queue ownership.
- Renderer-owned `VkSurfaceKHR` creation from platform native snapshots.
- Swapchain, image view, clear-pass, synchronization, and presentation lifetime.
- Per-target resize, suspension, recreation, and loss handling.
- Backend-neutral diagnostics and recoverable errors.

`RenderDevice` lifetime is separate from window-target lifetime. One device can
serve multiple independent targets when its adapter can present to their
surfaces. No process-global renderer singleton and no hidden render thread are
introduced.

## Vulkan Platform Contract

The initial backend builds on Windows and Linux only. `ponder_render` does not
build on macOS until a native Metal backend exists. A render-enabled macOS
configuration fails clearly; a render-disabled repository configuration remains
supported.

Vulkan 1.2 is the runtime floor for integrated and discrete GPUs. Render builds
against pinned current Vulkan headers, queries loader and physical-device API
versions independently, and enables Vulkan 1.3 or 1.4 functionality only when
capability checks prove it is available. The required bootstrap path needs
`VK_KHR_surface`, the active Windows/X11/Wayland WSI instance extension, and
`VK_KHR_swapchain`.

Render uses a private dynamic Vulkan loader through Volk. A Vulkan SDK,
import-library link, or statically linked Vulkan loader is not a consumer or
runtime requirement. Vulkan, Volk, allocator, validation, and operating-system
implementation types remain private.

## Owner-Local Vulkan Dispatch

Only dynamic-loader initialization is process-wide. It is cached with `std::call_once`, so
overlapping renderer bootstraps do not concurrently mutate Volk's loader state. Population of
Volk instance and device tables is serialized because the pinned Volk implementation temporarily
uses mutable global loader pointers while filling those tables; device-table population explicitly
selects the owning instance's `vkGetDeviceProcAddr`.

Every live Vulkan instance owner retains its own `VolkInstanceTable`, and every live logical-device
owner retains its own `VolkDeviceTable`. Their runtime dispatch is derived from those retained
tables. Surfaces, adapter queries, device creation, device state, targets, and frame operations use
the dispatch belonging to their owning instance or device; no one-instance or one-device process
restriction is imposed. VMA receives the owning instance's `vkGetInstanceProcAddr` and
`vkGetDeviceProcAddr` explicitly rather than reading mutable process-global device dispatch.

Deterministic fake-dispatch tests keep overlapping instance and device owners alive across moves
and verify that destruction and allocator calls remain bound to the correct owner-local tables.

## Semantic Presentation Contract

Public presentation requests use `VSync`, `LowLatencyVSync`, and `Uncapped`; native present-mode
names are backend implementation details. `VSync` is the default. Every policy request and
maximum-queued-frame request explicitly declares `Preferred` or `Required` strength. An unavailable
requirement fails target creation or recreation with `UnsupportedSurface`. An unavailable preference
selects the closest supported semantic result and reports both the actual value and a typed fallback
reason. `Uncapped` prefers uncapped presentation, then low-latency synchronized presentation, then
ordinary synchronized presentation; `LowLatencyVSync` falls back only to ordinary synchronized
presentation.

Queued latency is a semantic maximum, not a swapchain image-count or synchronization-topology
setting. Its supported public range remains one through three frames. A preferred request may be
reduced to the target maximum and reports `TargetMaximumExceeded`; a required request is never
silently reduced.

Adapter snapshots expose only semantic presentation policies, the supported queued-latency range,
window-presentation support, and the opaque SDR sRGB guarantee. Selected presentation results expose
the requested and actual semantic policy, requested and actual queued latency, typed fallback
reasons, output guarantee, and pixel extent. Native formats, color spaces, present modes,
queue-family indices, allocator state, and Vulkan presentation-completion extensions remain private.
When a native value is relevant to a failure, it is carried by `BackendDiagnostic`, not a general
consumer capability snapshot.

## Diagnostics Contract

`RenderBootstrap::GetDiagnostics()`, `RenderDevice::GetDiagnostics()`, and
`RenderTarget::GetDiagnostics()` return owning, immutable-by-copy snapshots. Bootstrap diagnostics
record requested and enabled validation, negotiated API version, implemented debug hooks, and the
last failure. Device diagnostics retain the selected adapter's backend-neutral capabilities,
lifecycle counters, loss state, and last failure. Target diagnostics add its `WindowId`, stable
`target/window:<WindowId>` label, presentation/recreation state, and last failure.

Native failures are propagated as structured `BackendDiagnostic` values at the Vulkan translation
boundary. Native result code, symbolic name, exact operation, render error code, validation context,
and target identity are never recovered by parsing a human-readable `core::Error` message. Display
messages remain actionable context, but they are not a machine-readable transport.

Every target-specific Vulkan object name begins with
`pond.render.target/window:<WindowId>/`, matching the public diagnostic label. Target creation and
surface-recovery failures carry the same identity in both their diagnostic snapshot and actionable
error context. Debug instrumentation reports object names and command labels only when the enabled
debug-utils path and owner-local dispatch actually provide them. Timing markers and capture regions
remain false until those facilities are implemented.

Stable render log event names are `validation_configuration`, `adapter_selected`,
`device_requirements`, `presentation_requirements`, `presentation_settings`,
`target_recreation_requested`, `target_recreation_completed`, `surface_lost`,
`surface_recovered`, `target_failure`, and `lifecycle_misuse`. Their fields identify the
selected/requested state, recreation reason, or misuse operation without requiring prose parsing.

## Initial Output Contract

The bootstrap milestone presents opaque SDR sRGB output. Swapchain negotiation
accepts only `VK_FORMAT_B8G8R8A8_SRGB` or `VK_FORMAT_R8G8B8A8_SRGB`, paired with
`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`, and requires
`VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR`. BGRA is preferred deterministically when
both pixel formats are available. A surface that cannot provide this exact
contract fails with `RenderErrorCode::UnsupportedSurface`; render does not
silently substitute a UNORM, HDR, wide-gamut, undefined, arbitrary, or
non-opaque presentation mode.

Clear colors are linear floating-point values. Writing the clear value to the
selected sRGB attachment performs the required linear-to-sRGB framebuffer
conversion. HDR, wide gamut, shader-based output conversion, and compositable
alpha targets remain later, explicitly negotiated extensions.

## Presentation Completion And Retirement

Swapchain retirement distinguishes graphics-submission completion from release
by the presentation engine. A graphics fence alone is not evidence that a
presentation wait semaphore or an old swapchain generation is no longer in use.

When available, `VK_KHR_swapchain_maintenance1` presentation fences are the
authoritative asynchronous resource-release proof. Render prefers the promoted
KHR path when its required `VK_KHR_surface_maintenance1` instance prerequisite
and device extension feature are both available. The functionally equivalent
`VK_EXT_surface_maintenance1` and `VK_EXT_swapchain_maintenance1` pair remains a
compatibility fallback. Every queued presentation carries a fence from the
selected maintenance pair, and old swapchain generations remain alive until
their graphics work and all associated presentation fences complete.

If the maintenance path is unavailable but both `VK_KHR_present_id` and
`VK_KHR_present_wait` are enabled, render assigns monotonic IDs per swapchain and
waits for the latest accepted presentation before the native swapchain is
retired. `vkWaitForPresentKHR` is never used after a swapchain has entered the
retired state. A bounded present-wait timeout falls back to the successor-history
proof described below.

On the unextended Vulkan 1.2 path, creating a successor swapchain does not make
the old generation immediately destroyable. Render retains the old generation
until it has presented from the successor, reacquired an image previously
presented by that successor, and completed the graphics submission that waits on
the reacquire signal. That ordered successor event proves completion of the
earlier presentation work; a graphics fence or acquire event without this
history does not.

If surface loss or final shutdown leaves no usable successor from which to
establish that proof, render uses a counted presentation-queue or device-idle
practical compatibility fallback. This can stall other targets sharing the
queue or device and is not described as a formal presentation-completion proof.
Final device shutdown may use `vkDeviceWaitIdle` after explicit and successor
completion paths have been handled.

Render-finished binary semaphores are owned by swapchain image, so reacquiring an
image supplies the presentation-completion proof required before its semaphore is
signaled again.

Frame-slot acquisition uses a dedicated acquire-completion fence alongside the
binary image-available semaphore. The acquire fence remains independently pending
after graphics submission because its signal is ordered separately from the acquire
semaphore signal. Retirement polls every acquire and submission fence whose
operation is actually pending; an unused, reset, or never-submitted
fence is not treated as work that must complete. If any fallible operation after a
successful image acquisition cannot leave the frame synchronization reusable, the
entire presentation generation is marked poisoned. It is retained until its real
pending acquire, graphics, and presentation work completes, while the next
presentable frame lazily builds a fresh generation. A signaled image-available
semaphore, partially recorded command buffer, or reset-but-unsubmitted graphics
fence is never returned to ordinary frame-slot reuse.

Swapchain replacement has a two-phase commit boundary. Capability queries,
selection, and other preflight work run before native replacement; a preflight
failure preserves the current usable generation. The call to
`vkCreateSwapchainKHR` is irreversible when `oldSwapchain` is non-null: Vulkan
retires that old swapchain even if creation of the successor fails, as specified by
[`VkSwapchainCreateInfoKHR`](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainCreateInfoKHR.html).
Render records that retirement at the native call boundary and never supplies the
retired handle as `oldSwapchain` again.
Storage for that mandatory state transition is reserved before entering the native
commit boundary, so recording the retired generation cannot fail afterward. Each
new native handle is adopted by RAII before any later fallible host allocation;
allocation failures follow the same recoverable no-swapchain transition.

Ordinary resize, suspension, surface-loss, and replacement paths reserve retirement ownership
storage before moving a live presentation generation. A host allocation failure returns
`RenderErrorCode::OutOfMemory` while the pre-commit target state and owned native resources remain
intact and retryable. Target destruction transfers its current and already-retired generations to
the device without first appending to the target retirement vector. If the device cannot allocate
that terminal ownership record, destruction uses a counted exceptional `vkDeviceWaitIdle`
fallback before releasing the resources; it does not terminate or expose partially moved owners.
This exceptional destructor fallback is not used by ordinary frame, resize, minimize, restore, or
replacement work.

Swapchain-image enumeration retries `VK_INCOMPLETE`; a partial image set is never
published. The successor is published only after its swapchain-dependent objects,
frame resources, and presentation tracker all succeed. Failure at or after the native
commit boundary leaves a recoverable target whose window status remains `Active`
and whose recreation request remains pending, but which exposes no usable
swapchain, selected presentation configuration, or swapchain generation. The next
frame retries from `VK_NULL_HANDLE`. The outgoing and partial generations remain
owned or are rolled back through RAII until their real pending work permits safe
destruction. Failure to drain an older generation after the successor commits does
not roll back the usable successor.

## Presentation State And Recovery

Suspension, a stale swapchain, surface loss, and device loss are distinct states.
`TargetStatus::Hidden`, `TargetStatus::Minimized`, and `TargetStatus::Suspended` are
ordinary no-work states. `TargetStatus::SurfaceLost` requires a fresh owner-thread
prepared surface, while `TargetStatus::DeviceLost` is terminal for the selected device.
Neither loss state is reported as ordinary suspension.

`VK_ERROR_OUT_OF_DATE_KHR` leaves the existing surface valid, reports
`FrameStatus::RecreationPending`, and schedules an internal target-local swapchain retry.
The caller must not prepare or recover a surface for that result. Genuine surface loss
sets `TargetStatus::SurfaceLost`, reports `RenderErrorCode::SurfaceLost`, and makes
`RecoverSurface()` the accepted recovery operation. Device loss sets
`TargetStatus::DeviceLost` and continues to report `RenderErrorCode::DeviceLost`.
`FrameStatus::Recreated` is reserved for a frame after replacement has actually
committed; it never means merely that recreation was requested.

`RenderTargetSnapshot` carries the platform's faithful `WindowState` value rather than separate
minimized/restored event flags. The application-side window-state aggregator that consumes platform
events owns its per-window snapshot sequence: it assigns a nonzero, monotonically increasing
snapshot revision for every newer coalesced snapshot and never reuses a revision for different
state. Render only validates that sequence; it never invents or increments a window revision.

The same aggregator also owns a nonzero, monotonically increasing
`PresentationEnvironmentRevision` for each window lifetime. It increments that typed revision when
a display migration, display-scale event, window-presentation transition, or other color/present
environment change requires surface capabilities and presentation selection to be queried again,
even when the pixel extent did not change. Ordinary visibility, size, and window-state snapshots may
retain the current presentation-environment revision. A regression in either sequence is rejected.

`TargetRecreationInfo` revisions describe those application-owned platform snapshots, not backend
event sequence numbers. Size, restore, and presentation-environment changes carry actual increasing
snapshot revisions. Backend-only presentation-change and surface-loss events leave both optional
snapshot revisions absent, so render never fabricates a window revision.

Render consumes only the platform native-window alternatives needed by Vulkan on
Windows and Linux:

- `NativeWin32Window` maps privately to `VK_KHR_win32_surface` and
  `vkCreateWin32SurfaceKHR`.
- `NativeX11Window` maps privately to `VK_KHR_xlib_surface` and
  `vkCreateXlibSurfaceKHR`.
- `NativeWaylandWindow` maps privately to `VK_KHR_wayland_surface` and
  `vkCreateWaylandSurfaceKHR`.

No Cocoa or Metal-layer native payload is part of the Vulkan contract. The future
native Metal backend must define its own macOS payload before platform exposes
one.

## Lifetime And Threading

Native window data is a borrowed owner-thread snapshot. `RenderBootstrap`
acquires and consumes it synchronously on the platform owner thread while
creating a renderer-owned opaque surface. Render never stores a native snapshot,
passes it to a render thread, keeps a long-lived `Window` borrow, or keys state
by `Window*`.

Only the completed opaque surface handoff and copied project-owned window-state
snapshot may cross to the caller-selected render thread. Device mutation,
command recording, submission, presentation, and target destruction are affine to
that thread, which may also be the platform owner thread.

An owner-thread surface may be transferred immediately with the identical valid snapshot used for
preparation. A same-revision snapshot with any conflicting field and every older snapshot are
rejected. A genuinely newer coalesced snapshot is accepted when both its window snapshot revision
and presentation-environment revision remain monotonic. Target updates after transfer remain
strictly newer operations.

Frame-slot and presentation-completion waits, image acquisition, fence reset, and
command recording are target-local work and never hold a Vulkan queue lock. Each
externally synchronized queue submission, presentation, or queue-idle call locks only
the mutex keyed by its actual `VkQueue` handle. Graphics and presentation roles share
that mutex when their handles alias; distinct queues remain independently usable.
Device-idle waits and native device destruction take an exclusive device gate that
excludes every queue operation.

Ownership is a checked parent-child hierarchy, not shared-lifetime orphaning.
`RenderBootstrap` outlives every prepared surface, device, target, and frame;
each `RenderDevice` outlives its targets; and each `RenderTarget` outlives every
frame token it issued, including suspended, timed-out, and recreation-pending
tokens. Moves preserve heap-stable state and do not relax this ordering.

`RenderBootstrap::Shutdown()` is an explicit, recoverable release attempt. It
returns `InvalidState` without changing the bootstrap when called off the owner
thread or while any child remains. Ignoring the ordering contract and releasing
or replacing a parent with live children is a release-active verification
failure and terminates. The same enforcement applies when move assignment would
discard a live destination owner.

Destruction follows the thread that owns the native release. `RenderBootstrap`
and an untransferred `PreparedSurface` are released on the platform owner
thread. Once a prepared surface is transferred, it, the device, its targets,
and all frame tokens are released on the selected render thread. A frame may be
abandoned by destroying it on that thread; destroying or replacing its target
first, or releasing any GPU-facing owner from another thread, is a
release-active violation.

Shutdown order is explicit: finish or abandon active frame tokens, destroy render
targets and unconsumed prepared surfaces, destroy the render device, destroy
`RenderBootstrap`, destroy platform windows, and finally destroy
`PlatformRuntime`.

Platform pixel size is authoritative for swapchain extent. Minimized, hidden, or
zero-pixel targets are normal suspended states rather than errors. Surface loss
requires a fresh owner-thread preparation call; device loss is fatal to the
selected device for the bootstrap milestone.

## Future Generic 2D Draw Boundary

The bootstrap milestone does not implement generic 2D drawing. Its frame lifecycle is split into
real ordered phases: frame acquisition begins command recording, `Clear()` records the clear while
leaving frame recording open, one private renderer-owned insertion seam follows the clear, and
`FinishAndPresent()` finalizes, submits, and presents. The acquired frame therefore owns the
recording state needed by a later generic layer; clear work is not deferred into finalization.

A later explicit project-private render contract accepts only owning, project-defined 2D draw
packets. Render owns their shaders, GPU resources, recording, submission, and completion retirement.
`ponder_ui` owns semantic paint commands and CPU tessellation before crossing this boundary; UI
receives no Vulkan handles and render does not depend on `ponder_ui`. The current private insertion
seam is not yet a public draw layer, callback, native command-buffer escape hatch, canvas, or
arbitrary GPU command API. The UI roadmap's render-integration tasks own promotion of this
test-proven seam into the real producer-neutral 2D packet consumer. Dear ImGui is not a successor
layer or compatibility target.

## Non-Responsibilities

- Owning platform windows, platform event polling, or input translation.
- UI widgets, semantic paint-command interpretation, or application command routing.
- Chemistry data ownership, project loading, IO/import, workflow, plugins, or
  compute behavior.
- Application main-loop policy beyond renderer frame lifecycle primitives.
- Scene, camera, mesh, material, texture, shader, offscreen, readback, or picking
  APIs in the bootstrap milestone.

## Dependencies

`ponder::core` and `ponder::platform` are public dependencies of
`ponder_render`. Vulkan headers, Volk, VMA or other allocator support,
validation, operating-system support, and future backend implementation targets
are private dependencies.
No UI implementation library is a render dependency.

`ponder_render` must not depend directly on SDL, `ponder-desktop`,
`ponder_project`, `ponder_chemistry`, `ponder_io`, `ponder_workflow`,
`ponder_compute`, `ponder_plugin_sdk`, or `ponder_ui`.
