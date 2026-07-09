# Codex Guidance: UI

Read the root `AGENTS.md` before this file.

## Scope

Work here on reusable user-interface models, widgets, and presentation logic.

## Local Rules

- Keep public headers under `include/ponder/ui/`.
- Use the `pond::ui` namespace.
- Keep UI implementation details out of domain libraries.
- Prefer wrappers around ImGui-facing code when crossing subsystem boundaries.
- During the desktop foundation phase, own ImGui context lifetime, frame
  helpers, project `PlatformEvent` translation, renderer draw-data hookup,
  style, fonts, dockspace/menu/status/log-panel primitives, and small testable
  UI state helpers.
- Follow [PlatformAdapter.md](PlatformAdapter.md). Do not declare a direct SDL
  dependency, include SDL headers, expose SDL types, or compile
  `imgui_impl_sdl3`.
- Install clipboard, IME, cursor, pointer, and external-URI behavior through
  project platform APIs; disable ImGui's default OS hooks.
- Keep docking in the main OS window initially. Do not enable ImGui platform
  multi-viewports, gamepad navigation, or pointer warping without a later
  project-owned platform contract.
- `ponder_ui` may depend on `ponder_core`, `ponder_platform`, `ponder_render`,
  and Dear ImGui. It must not declare a direct SDL dependency or inherit SDL
  compile usage requirements. A final executable may still receive SDL through
  platform's link-only static dependency.
  It must not depend on `ponder-desktop`, project/domain libraries, workflow,
  compute, or plugins.
- Do not add project-specific panels or application command policy until the
  desktop shell is stable and the project APIs exist.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching UI test executable.
- Verify UI targets have no SDL compile usage or direct SDL target dependency.
