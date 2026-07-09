# Render Library Roadmap

Status: implementation plan after the Vulkan backend decision on 2026-07-08.

This roadmap implements the smallest durable renderer needed by the desktop
shell. It follows [ADR 0008](../../../docs/adr/0008-vulkan-renderer-backend.md)
and keeps SDL, UI widgets, application policy, and domain data outside render.

## REND-000: Record The Initial Backend

Status: completed on 2026-07-08.

Vulkan is the first backend. The platform creation requirement is
`WindowGraphicsCompatibility::Vulkan`; native surface creation uses the closed
Win32, X11, Wayland, and Cocoa Metal-layer payloads.

## REND-001: Establish Vulkan Dependencies

Select and pin the Vulkan headers/loader strategy, memory allocator, and
MoltenVK packaging under ADR 0001. Add each dependency as a repository-managed
CMake target with license inventory updates. Do not add shader compiler tools
until a concrete shader pipeline requires them.

Freeze these bootstrap requirements:

- Vulkan 1.2, `VK_KHR_surface`, the active host WSI instance extension, and
  device extension `VK_KHR_swapchain`.
- A Windows/Linux dynamic loader path compatible with SDL's automatic default
  Vulkan-loader use for Vulkan-compatible windows; do not statically link or
  independently choose an incompatible loader.
- MoltenVK on macOS, including portability enumeration and
  `VK_KHR_portability_subset` when advertised.
- Developer validation through `VK_LAYER_KHRONOS_validation` and
  `VK_EXT_debug_utils`, with clear failure when explicitly requested support
  is unavailable.
- Private Linux Xlib/Wayland development headers and conditional macOS
  Objective-C++/QuartzCore build support required by the approved WSI branches.

Done when Windows, Linux, and macOS builds have a documented loader path, the
macOS app can package MoltenVK, dependency licenses are inventoried, and no
Vulkan usage requirement leaks through unrelated public targets.

## REND-002: Define Renderer Bootstrap And Lifetime

Replace the scaffold with a fallible, move-only renderer owner and
`GetRequiredWindowGraphicsCompatibility() noexcept`, returning
`WindowGraphicsCompatibility::Vulkan` before window creation. Keep device and
backend state behind PIMPL. Configure `ponder::core` and `ponder::platform` as
`PUBLIC_DEPS`; Vulkan, loader, allocator, MoltenVK, OS, and later
`ponder::imgui` dependencies remain private.

Done when desktop can obtain the compatibility requirement without hardcoding
the backend, and public headers expose neither Vulkan nor OS types.

## REND-003: Create Vulkan Instance, Device, And Surface

Obtain the approved `NativeWindowHandle` before Vulkan instance creation and
use its active alternative to select only the required WSI instance extension.
Create the Vulkan instance, then create renderer-owned `VkSurfaceKHR` through:

- `vkCreateWin32SurfaceKHR` for `NativeWin32Window`.
- `vkCreateXlibSurfaceKHR` for `NativeX11Window`.
- `vkCreateWaylandSurfaceKHR` for `NativeWaylandWindow`.
- `vkCreateMetalSurfaceEXT` for `NativeCocoaWindow`.

Only after the surface exists, enumerate/select the physical device and
graphics/present queues using surface-support queries, then create the logical
device with `VK_KHR_swapchain`. Handle MoltenVK portability enumeration and the
borrowed platform-owned `CAMetalLayer` on macOS.

Done when the active host creates and destroys the surface before its platform
window, the caller-enforced renderer-before-window lifetime is documented,
unsupported variants fail explicitly, and no SDL API appears in render.

## REND-004: Implement Swapchain And Clear Frames

Own swapchain images/views, synchronization, frame begin/end, clear pass,
presentation, vsync policy, minimized-window suspension, resize/out-of-date
recovery, and deterministic teardown.

Done when the desktop can clear and present every frame, survive resize and
minimize/restore, and shut down without validation errors.

## REND-005: Add The Dear ImGui Draw Bridge

Add the narrow bridge through which `ponder_ui` supplies borrowed ImGui draw
data. Render owns all Vulkan descriptors, pipelines, command recording,
synchronization, and submission used for that data. UI receives no Vulkan
handles and render does not depend on `ponder_ui`. Add `ponder::imgui` as a
private target dependency for this bridge only.

Done when ImGui draw data renders inside the normal frame lifecycle and backend
resource lifetime remains renderer-owned.

## REND-006: Verify Supported Hosts

Build and test the Win32, X11, Wayland, and Cocoa/MoltenVK paths on their owning
hosts. Record compiler, Vulkan loader/runtime, driver, validation-layer output,
and any capability-based skips. An unrun host remains unverified.
