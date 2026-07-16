# ADR 0008: Vulkan Renderer Backend

## Status

Accepted.

## Related Decisions

- [ADR 0010: Project-Owned UI Rendering](0010-project-owned-ui-rendering.md)

## Context

The first durable desktop shell needs a renderer that can clear and present a
platform window on Windows and Linux without putting graphics ownership in
`ponder_platform` or backend setup in `ponder-desktop`.

The project vision identifies Vulkan as the intended first rendering API. Selecting it now fixes
the SDL window-creation contract, the native values that platform must expose, and the ownership
boundary between render and UI before those types become broader public contracts.

macOS is deliberately not part of the Vulkan renderer contract. The later macOS renderer will use
Metal directly. Treating Vulkan as a portability layer on macOS would force platform to expose a
Metal-layer payload for a backend that `ponder_render` will not support there.

SDL can create Vulkan surfaces from `SDL_Window`, but exposing that type would make SDL a render
and UI contract. Having platform accept Vulkan objects and call `SDL_Vulkan_CreateSurface` would
instead give platform graphics-surface responsibility. Both choices conflict with ADR 0007.

## Decision

Use Vulkan as the first `ponder_render` backend on Windows and Linux.

### Backend And Dependency Boundary

`ponder_render` owns Vulkan loader and instance lifetime, physical/logical device selection,
queues, `VkSurfaceKHR`, swapchains, image views, synchronization, resize recovery, present mode,
and presentation lifetime. Vulkan and operating-system declarations stay in renderer-private
implementation files.

`ponder::core` and `ponder::platform` are public target dependencies of `ponder_render` because the
public renderer contract uses their types. Vulkan headers, Volk, allocator support, validation
layer integration, and operating-system dependencies stay private. A future Metal backend is a
separate private implementation concern and is not part of this ADR.

The initial backend targets Vulkan 1.2 as the runtime floor. It builds against pinned current
unified Vulkan headers, queries the loader and physical-device API versions independently, and
enables only the functionality actually supported by the selected device. Vulkan 1.3 and 1.4
features may be used only through explicit capability checks and documented fallbacks.

The required bootstrap path needs `VK_KHR_surface`, the active Windows/X11/Wayland WSI instance
extension, and device extension `VK_KHR_swapchain`. It requires no optional device feature bit for
the create/clear/present milestone.

Windows and Linux use the system Vulkan loader opened dynamically through private Volk
integration, compatible with SDL's normal loader path for `SDL_WINDOW_VULKAN`. A
Vulkan SDK, import-library link, or statically linked loader is not a runtime or
consumer requirement. Developer validation
configurations request `VK_LAYER_KHRONOS_validation` and `VK_EXT_debug_utils` and fail clearly if
explicitly requested support is absent.

`ponder_render` intentionally does not build on macOS until the native Metal backend exists. A
macOS configure with render enabled fails with an actionable message; configuring the repository
with render disabled remains supported.

### Window Creation And Backend Selection

`pond::platform::WindowGraphicsCompatibility` has these project-owned alternatives:

- `Default` requests no graphics-specific SDL creation flag.
- `Vulkan` requests the native state needed by the Vulkan backend on Windows and Linux.
- `Metal` is reserved for the later native macOS backend.

`Vulkan` maps to `SDL_WINDOW_VULKAN` on Windows and Linux only. Vulkan-compatible windows are not
supported on macOS. `Metal` maps to `SDL_WINDOW_METAL` for the future macOS backend, but no Metal
renderer or Metal native payload exists yet.

Each `Window` stores the exact project compatibility selected in `WindowDesc`. Platform never
infers it from SDL window flags, which are host-dependent and not an unambiguous representation of
the project value.

Before creating a platform window, the application calls
`pond::render::GetRequiredWindowGraphicsCompatibility() noexcept`. In a Vulkan build, this
parameterless query returns `Vulkan`. Backend choice is selected by the build, not by a runtime
backend selector, and the application does not hardcode SDL flags or infer the requirement from the
host.

### Closed Native Window Contract

The current Vulkan `NativeWindowHandle` contract contains exactly these payloads:

- `NativeWin32Window`: opaque `instance` (`HINSTANCE`) and `window` (`HWND`) pointers.
- `NativeX11Window`: opaque `display` (`Display*`) and integer-sized `window` (X11 `Window`).
- `NativeWaylandWindow`: opaque `display` (`wl_display*`) and `surface` (`wl_surface*`) pointers.

The public structs use only `void*` and `std::uintptr_t`; they do not include OS, SDL, Vulkan, or
Metal headers. No HDC, X11 screen, Wayland viewport/xdg object, Cocoa window, Metal layer, generic
extra field, or externally owned-window alternative is included. The native Metal backend must
introduce its own exact macOS payload when it is designed.

The corresponding renderer-private Vulkan WSI paths are:

- Win32: `VK_KHR_win32_surface` and `vkCreateWin32SurfaceKHR`.
- X11: `VK_KHR_xlib_surface` and `vkCreateXlibSurfaceKHR`.
- Wayland: `VK_KHR_wayland_surface` and `vkCreateWaylandSurfaceKHR`.

The initial Linux contract supports SDL's X11 and Wayland drivers. Other SDL drivers return a
platform `Unsupported` result for native Vulkan interop. Requesting the native handle of a
`Default` or future `Metal` window for the Vulkan backend is an invalid-argument or unsupported
capability result, as appropriate to the public API layer.

### Lifetime, Threading, And Multiple Windows

Native payloads are borrowed owner-thread snapshots. Renderer-owned code consumes
the native snapshot synchronously during owner-thread surface preparation, creates
and owns `VkSurfaceKHR`, and never stores or transfers the native snapshot itself.

`RenderBootstrap` is the owner-thread root for dynamic loader and instance state. It prepares
surfaces from platform windows and outlives every prepared surface, device, and target created from
it. Only the completed opaque surface handoff plus copied project-owned state such
as `WindowId`, pixel size, visibility, faithful `WindowState`, and a typed presentation-environment
revision may cross to the caller-selected render thread. The application-side platform-event
aggregator owns monotonically increasing per-window snapshot and presentation-environment
revisions; render validates but never fabricates them. Immediate transfer accepts the identical
preparation snapshot, while older or same-revision conflicting state is rejected.

`RenderDevice` is separate from presentation-target lifetime. A single device can
serve multiple independent window targets when the selected adapter can present to
their surfaces. Device mutation, frame recording, submission, and target
destruction are affine to the caller-selected render thread, which may be the
platform owner thread. Render creates no hidden render thread and no
process-global renderer singleton.

The renderer-to-window lifetime is caller-enforced: platform's child registry cannot observe a
renderer-owned surface. Shutdown destroys or abandons active frame tokens, destroys render targets
and unconsumed prepared surfaces, destroys the render device, destroys `RenderBootstrap`, destroys
platform windows, and finally destroys `PlatformRuntime`.

Ordinary resize, minimize, hide/show, same-size display or scale changes, and swapchain
staleness are
handled from copied state snapshots on the render thread. A newer presentation-environment revision
forces presentation re-evaluation even when pixel size is unchanged. Surface loss requires a fresh
owner-thread preparation call; render must not reuse stored native data. Device loss is fatal to the
selected device for the bootstrap milestone.

### Generic 2D Draw Composition

Generic 2D drawing is not part of the create/clear/present bootstrap milestone. The renderer
foundation leaves a semantic insertion point after preceding color work and before final
submission.

ADR 0010 records the ownership of project-owned UI rendering. `ponder_ui` owns semantic paint
commands, logical coordinates, clipping, color rules, and CPU tessellation into owning
project-defined packets. `ponder_render` consumes only the producer-neutral packet contract and owns
shaders, GPU resources, command recording, submission, and completion retirement. UI receives no
Vulkan handles and render does not depend on UI.

## Consequences

The initial renderer and native interop contract are intentionally Vulkan-on-Windows/Linux only.
Adding Metal requires a later decision before platform exposes a macOS native payload or render
builds on macOS.

Platform can remain independent of Vulkan and renderer code. Render has no direct SDL dependency or
compile usage while creating its own presentation surface; final executables still receive
platform's link-only SDL dependency.

Vulkan 1.2 keeps the runtime floor broad while allowing newer Vulkan 1.3 and 1.4 functionality to
be used opportunistically when queried capabilities prove it is available. Requiring Vulkan 1.4 is
not part of the bootstrap contract.

Windows on the current development laptop is the only initially verified runtime host. Linux X11
and Wayland implementation paths remain unverified until they are configured, built, and exercised
on native hosts.
