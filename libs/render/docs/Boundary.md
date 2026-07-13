# Render Library Boundary

`ponder_render` owns renderer backend lifetime and renderer-facing presentation foundations.

Status: bootstrap contracts revised on 2026-07-13.

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

Shutdown order is explicit: finish or abandon active frame tokens, destroy render
targets and unconsumed prepared surfaces, destroy the render device, destroy
`RenderBootstrap`, destroy platform windows, and finally destroy
`PlatformRuntime`.

Platform pixel size is authoritative for swapchain extent. Minimized, hidden, or
zero-pixel targets are normal suspended states rather than errors. Surface loss
requires a fresh owner-thread preparation call; device loss is fatal to the
selected device for the bootstrap milestone.

## Future Dear ImGui Boundary

Dear ImGui is not implemented in the bootstrap milestone. The frame lifecycle
must leave a later overlay insertion point where a renderer-owned ImGui bridge
can record after the clear pass and before final submission.

`ponder_ui` owns the ImGui context, widgets, and platform adapter. Render may
later consume borrowed ImGui draw data through a narrow private bridge, but UI
receives no Vulkan handles and render does not depend on `ponder_ui`.

## Non-Responsibilities

- Owning platform windows, platform event polling, or input translation.
- UI widgets, Dear ImGui context ownership, or application command routing.
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

`ponder_render` must not depend directly on SDL, `ponder-desktop`,
`ponder_project`, `ponder_chemistry`, `ponder_io`, `ponder_workflow`,
`ponder_compute`, `ponder_plugin_sdk`, or `ponder_ui`.