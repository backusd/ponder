# Codex Guidance: Render

Read the root `AGENTS.md` before this file.

## Scope

Work here on rendering abstractions and visualization data flow.

## Local Rules

- Keep public headers under `include/ponder/render/`.
- Use the `pond::render` namespace.
- Keep platform windows in `ponder_platform` and UI widgets in `ponder_ui`.
- The first backend is Vulkan under ADR 0008. Keep Vulkan, MoltenVK, and OS types
  out of general public APIs and private to backend implementation files. Target
  Vulkan 1.2 with only the required WSI extensions for bootstrap.
- During the desktop foundation phase, focus on backend/device lifetime,
  presentable surface or swapchain lifetime, frame begin/end, clear pass, resize
  handling, and presentation policy.
- Consume only the closed `ponder_platform` native-window variant. Create and
  own `VkSurfaceKHR`; never consume `SDL_Window` or call
  `SDL_Vulkan_CreateSurface`.
- Obtain the native variant and select its instance extensions, create the
  instance and `VkSurfaceKHR`, then select physical device and graphics/present
  queues using surface-support queries before creating the logical device.
- Treat renderer-before-window destruction as a caller-enforced contract;
  platform cannot observe renderer-owned surfaces. Use a Windows/Linux dynamic
  Vulkan loader compatible with SDL's default loader path.
- Implement `GetRequiredWindowGraphicsCompatibility() noexcept` so desktop can
  obtain the selected backend's requirement before window creation.
- Declare `ponder::core` and `ponder::platform` as public target dependencies.
  Keep Vulkan/loader/allocator/MoltenVK/OS dependencies private. The ImGui draw
  bridge may privately depend on `ponder::imgui`, but render must not depend on
  `ponder_ui`.
- Do not declare a direct SDL dependency or inherit SDL compile usage. Do not
  depend on
  `ponder-desktop`, project/domain libraries, workflow, compute, or plugins.
- Renderer-owned Vulkan code submits borrowed ImGui draw data through a narrow
  bridge; UI owns the ImGui context and widgets and receives no Vulkan handles.
- Do not put chemistry-specific drawing, project loading, ImGui widgets, or
  application main-loop ownership in this library.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching render test executable.
