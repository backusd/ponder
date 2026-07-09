# Render Library Boundary

`ponder_render` owns renderer backend lifetime and renderer-facing scene or
visualization abstractions.

Status: initial Vulkan backend selected on 2026-07-08.

## Decision Records

- [ADR 0008: Vulkan Renderer Backend](../../../docs/adr/0008-vulkan-renderer-backend.md)

## Public API

- Public headers live under `include/ponder/render/`.
- Source code uses the `pond::render` namespace.
- The CMake target is `ponder_render`; the alias is `ponder::render`.

## Responsibilities

- The initial Vulkan backend selected by ADR 0008.
- Graphics device/context lifetime.
- Swapchain, surface, or equivalent window-presentable target lifetime.
- Frame begin/end, clear pass, resize handling, and presentation/vsync policy.
- Renderer-independent APIs that the desktop app can drive without knowing
  backend details.
- Future renderable scene descriptions and visualization abstractions, once the
  window/render loop foundation is stable.
- `GetRequiredWindowGraphicsCompatibility() noexcept`, callable before a
  platform window exists and returning the selected backend's requirement.
- A narrow Dear ImGui draw bridge that owns Vulkan resources and submission
  without taking ownership of the ImGui context or UI widgets.

## Vulkan Platform Contract

The initial backend targets Vulkan 1.2 and requires `VK_KHR_surface`, the
active host WSI instance extension, and device extension `VK_KHR_swapchain`.
The active `NativeWindowHandle` alternative is obtained before instance
creation and determines the WSI instance extension.

Render consumes the closed platform variant containing `NativeWin32Window`,
`NativeX11Window`, `NativeWaylandWindow`, or `NativeCocoaWindow`. Private WSI
code maps them to:

- `VK_KHR_win32_surface` and `vkCreateWin32SurfaceKHR`.
- `VK_KHR_xlib_surface` and `vkCreateXlibSurfaceKHR`.
- `VK_KHR_wayland_surface` and `vkCreateWaylandSurfaceKHR`.
- `VK_EXT_metal_surface` and `vkCreateMetalSurfaceEXT` through MoltenVK.

Render creates and owns `VkSurfaceKHR`, swapchains, device resources,
synchronization, and presentation. On macOS, the Cocoa payload borrows the
platform-owned `CAMetalLayer` used by MoltenVK. After creating the instance and
surface, render selects a physical device and graphics/present queues using
surface-support queries, then creates the logical device.

Renderer presentation state is destroyed before the platform window. This is a
caller-enforced lifetime rule because platform cannot observe renderer-owned
surfaces. On Windows and Linux, render uses a dynamic Vulkan loader compatible
with SDL's default loader path and releases Vulkan state before window
destruction releases SDL's loader reference.

Render never consumes `SDL_Window`, calls `SDL_Vulkan_CreateSurface`, declares a
direct SDL dependency, or inherits SDL compile usage. Final executables may still
receive SDL through platform's link-only static dependency.

## Non-Responsibilities

- Owning platform windows, platform event polling, or input translation.
- UI widgets.
- Chemistry data ownership.
- Project format, project loading, IO/import, workflow, plugin, or compute
  behavior.
- Application main-loop policy beyond renderer frame lifecycle primitives.

## Dependencies

`ponder::core` and `ponder::platform` are `PUBLIC_DEPS` of
`ponder_render` because its public contract uses their types. Vulkan headers,
the loader, allocator, MoltenVK, OS support, and `ponder::imgui` are private
implementation dependencies. `ponder::imgui` is used only by the
renderer-owned draw bridge; render must not depend on `ponder_ui`.

`ponder_render` must not depend directly on SDL, `ponder-desktop`,
`ponder_project`, `ponder_chemistry`, `ponder_io`, `ponder_workflow`,
`ponder_compute`, or `ponder_plugin_sdk`.

Vulkan and OS types stay behind private implementation boundaries. A dedicated
ImGui draw bridge may mention borrowed ImGui draw data, but must not expose
Vulkan resources to UI.
