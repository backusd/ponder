# Codex Guidance: Render

Read the root `AGENTS.md` before this file.

## Scope

Work here on renderer backend lifetime, presentation foundations, and later
visualization data flow.

## Local Rules

- Keep public headers under `include/ponder/render/`.
- Use the `pond::render` namespace.
- Keep platform windows in `ponder_platform` and UI widgets in `ponder_ui`.
- The first backend is Vulkan on Windows and Linux under ADR 0008. macOS Vulkan
  is unsupported; a later macOS backend will use native Metal.
- `ponder_render` intentionally does not build on macOS until the Metal backend
  exists. Do not add temporary backend-neutral runtime stubs just to make render
  compile there.
- Keep Vulkan, Volk, VMA or allocator types, validation types, OS types, and
  future Metal types out of general public APIs and private to backend
  implementation files.
- Target Vulkan 1.2 as the runtime floor for bootstrap. Build against the pinned
  current unified Vulkan headers, query loader and device versions separately,
  and opt into Vulkan 1.3 or 1.4 functionality only through explicit capability
  checks and fallbacks.
- The desktop bootstrap boundary supports create, clear, present, resize,
  minimize/restore, multiple independent windows, and orderly shutdown. Do not
  broaden that support claim with drawing, UI, scene, or visualization features
  until their own milestones are implemented and verified.
- Consume only the closed `ponder_platform` native-window variants needed for
  Vulkan on Windows/Linux: Win32, X11, and Wayland. Create and own
  `VkSurfaceKHR`; never consume `SDL_Window` or call `SDL_Vulkan_CreateSurface`.
- No Cocoa or Metal-layer native payload belongs to the Vulkan path. Let the
  future native Metal backend define its own exact platform payload.
- Obtain the native variant and create the `VkSurfaceKHR` synchronously on the
  platform owner thread. Do not store the borrowed native snapshot, pass it to a
  render thread, retain a long-lived `Window` borrow, or key renderer state by
  `Window*`.
- Use `RenderBootstrap` as the owner-thread root for loader and instance state.
  Allow only completed opaque surface handoffs and copied project-owned window
  state to cross to the caller-selected render thread.
- Keep Vulkan device mutation, command recording, submission, presentation, and
  target destruction affine to the caller-selected render thread. Do not create a
  hidden render thread.
- Treat renderer-before-window destruction as a caller-enforced contract;
  platform cannot observe renderer-owned surfaces. Destroy targets and
  unconsumed prepared surfaces before the device, destroy the device before
  `RenderBootstrap`, and destroy platform windows before `PlatformRuntime`.
- Implement `GetRequiredWindowGraphicsCompatibility() noexcept` as a
  parameterless build-selected query. A Vulkan build returns
  `WindowGraphicsCompatibility::Vulkan`; do not add runtime backend selection.
- Declare `ponder::core` and `ponder::platform` as public target dependencies.
  Keep Vulkan/loader/allocator/OS dependencies private. No UI implementation
  library is a render dependency, and render must not depend on `ponder_ui`.
- Do not declare a direct SDL dependency or inherit SDL compile usage. Do not
  depend on `ponder-desktop`, project/domain libraries, workflow, compute, or
  plugins.
- A later narrow generic 2D contract accepts only owning project-defined packets;
  UI owns semantic paint commands and tessellation before crossing that boundary.
  Do not add retained UI types, logical UI units, producer callbacks, native
  command escapes, UI widgets, or arbitrary GPU commands.
- Dear ImGui is not part of the planned architecture, dependency boundary, or
  compatibility surface. The UI roadmap owns the project-defined paint compiler,
  while render will own only its producer-neutral 2D packet consumer.
- Do not put chemistry-specific drawing, project loading, UI widgets, or
  application main-loop ownership in this library.

## Verification

- Configure and build a supported CMake preset.
- Follow `libs/render/docs/Verification.md` for the render verification matrix, including deterministic labels, live labels, install-consumer coverage, and validation-mode commands.
- For normal deterministic checks, run CTest with `-L render -LE live`. For live Windows render presentation checks, run `-L live` with the `ponder_platform_sdl` resource lock and an explicit timeout.
- Do not call the bootstrap milestone validation-clean when the required Khronos
  validation layer is absent. Optional Default-mode success is not a substitute
  for a passing required Standard-validation run.
