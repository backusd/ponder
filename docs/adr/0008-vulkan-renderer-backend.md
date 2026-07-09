# ADR 0008: Vulkan Renderer Backend

## Status

Accepted.

## Context

The first durable desktop shell needs a renderer that can clear and present a
platform window on Windows, Linux, and macOS without putting graphics ownership
in `ponder_platform` or backend setup in `ponder-desktop`.

The project vision already identifies Vulkan as the intended rendering API.
Selecting it now fixes the SDL window-creation contract, the native values that
platform must expose, the macOS portability strategy, and the ownership boundary
between render and UI before any of those types become public.

SDL can create Vulkan surfaces from `SDL_Window`, but exposing that type would
make SDL a render and UI contract. Having platform accept Vulkan objects and call
`SDL_Vulkan_CreateSurface` would instead give platform graphics-surface
responsibility. Both choices conflict with ADR 0007.

## Decision

Use Vulkan as the first `ponder_render` backend.

### Backend And Dependency Boundary

`ponder_render` owns Vulkan instance, physical/logical device, queues,
`VkSurfaceKHR`, swapchain, image views, synchronization, resize recovery, present
mode, and presentation lifetime. Vulkan and operating-system declarations stay
in renderer-private implementation files.

The Vulkan headers/loader strategy, memory allocator, and MoltenVK packaging are
implementation prerequisites tracked by the renderer roadmap. Those dependencies
must follow ADR 0001: pinned repository dependencies, CMake targets, and license
inventory updates are added together. Shader compilation dependencies wait for
the renderer task that defines the shader pipeline.

`ponder::core` and `ponder::platform` are public target dependencies of
`ponder_render`. Vulkan/loader/allocator/MoltenVK/OS dependencies stay private.
The renderer-owned ImGui draw bridge may also use private `ponder::imgui`, but
render does not depend on `ponder_ui`.

The initial backend targets Vulkan 1.2. It requires `VK_KHR_surface`, the active
host WSI instance extension, and device extension `VK_KHR_swapchain`, but no
optional device feature bit for the bootstrap clear pass. On macOS it also
handles `VK_KHR_portability_enumeration` and its instance flag and enables
`VK_KHR_portability_subset` when advertised.

Windows and Linux use the standard dynamic Vulkan loader path compatible with
SDL's automatic loader use for `SDL_WINDOW_VULKAN`; render must not statically
link or independently select an incompatible loader. macOS packages MoltenVK.
Developer validation configurations request `VK_LAYER_KHRONOS_validation` and
`VK_EXT_debug_utils` and fail clearly if explicitly requested support is absent.

### Window Creation And Bootstrap

`pond::platform::WindowGraphicsCompatibility` has exactly two alternatives:

- `Default` requests no graphics-specific SDL creation flag.
- `Vulkan` requests the native state needed by this backend.

`Vulkan` maps to `SDL_WINDOW_VULKAN` on Windows and Linux. On macOS it maps to
`SDL_WINDOW_METAL`, because MoltenVK presents through a `CAMetalLayer` and
platform must create the SDL Metal view without exposing `SDL_Window`.
High-pixel-density, visibility, resizability, and other window properties remain
orthogonal.

Each `Window` stores the exact project compatibility selected in `WindowDesc`.
Platform never infers it from SDL window flags, which are host-dependent and not
an unambiguous representation of the project value.

Before creating the platform window, `ponder-desktop` calls
`pond::render::GetRequiredWindowGraphicsCompatibility() noexcept`; the initial
query returns `Vulkan`. The application does not hardcode SDL flags or infer the
requirement from the host.

### Closed Native Window Contract

`NativeWindowHandle` is a closed tagged variant with these exact payloads:

- `NativeWin32Window`: opaque `instance` (`HINSTANCE`) and `window` (`HWND`)
  pointers.
- `NativeX11Window`: opaque `display` (`Display*`) and integer-sized `window`
  (X11 `Window`).
- `NativeWaylandWindow`: opaque `display` (`wl_display*`) and `surface`
  (`wl_surface*`) pointers.
- `NativeCocoaWindow`: opaque `metalLayer` (`CAMetalLayer*`) pointer.

The public structs use only `void*` and `std::uintptr_t`; they do not include OS,
SDL, or Vulkan headers. No HDC, X11 screen, Wayland viewport/xdg object, Cocoa
window, generic extra field, or externally owned-window alternative is included.

The corresponding renderer-private Vulkan WSI paths are:

- Win32: `VK_KHR_win32_surface` and `vkCreateWin32SurfaceKHR`.
- X11: `VK_KHR_xlib_surface` and `vkCreateXlibSurfaceKHR`.
- Wayland: `VK_KHR_wayland_surface` and `vkCreateWaylandSurfaceKHR`.
- macOS: `VK_EXT_metal_surface` and `vkCreateMetalSurfaceEXT` through MoltenVK.

The initial Linux contract supports SDL's X11 and Wayland drivers. Other SDL
drivers return a platform `Unsupported` result for native Vulkan interop.
Requesting the native handle of a `Default` window is an invalid-argument
failure.

### macOS Native State

Each macOS `Window` created for `Vulkan` lazily owns at most one cached
`SDL_MetalView`. Repeated native-handle queries return that window's existing
borrowed `CAMetalLayer*`. Platform destroys the view before its SDL window.
Render may configure the borrowed layer as required by MoltenVK but does not own
or destroy it. Render creates and owns the Vulkan surface.

### Lifetime And Destruction

Native payloads are borrowed owner-thread snapshots. Render destroys swapchain
resources and `VkSurfaceKHR` before the platform window. Platform then destroys
its macOS Metal view when present, destroys the window, and finally shuts down
SDL after all children are gone.

This renderer-to-window lifetime is caller-enforced: platform's child registry
cannot observe a renderer-owned surface. `Renderer` destruction must precede
`Window` destruction. On Windows/Linux this also ensures Vulkan state is gone
before SDL destroys the Vulkan-compatible window and releases its loader
reference.

### Dear ImGui Rendering

`ponder_ui` owns the Dear ImGui context and SDL-free platform adapter.
`ponder_render` owns Vulkan resources and command submission used to render
borrowed ImGui draw data. They meet at a narrow renderer-owned ImGui draw bridge;
UI receives no Vulkan handles, and render does not depend on `ponder_ui`.

## Consequences

The initial renderer and native interop contract are intentionally Vulkan-only.
Adding another graphics API requires a later decision and only then may extend
`WindowGraphicsCompatibility` or `NativeWindowHandle`.

Platform can remain independent of Vulkan and renderer code. Render has no
direct SDL dependency or compile usage while creating its own presentation
surface; final executables still receive platform's link-only SDL dependency.

macOS support requires MoltenVK and an SDL-owned Metal view. Linux requires both
X11 and Wayland native branches. These branches remain unverified until their
owning hosts compile and exercise them.
