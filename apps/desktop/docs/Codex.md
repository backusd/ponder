# Codex Guidance: Desktop Application

Read the root `AGENTS.md` before this file.

## Scope

Work here on the `ponder-desktop` executable. The current target is a console
stub until platform, windowing, renderer, and UI integration are ready.

## Local Rules

- Keep reusable behavior in libraries under `libs/`.
- Use the executable target `ponder_desktop` with output name `ponder-desktop`.
- Link only the minimal libraries needed for the current scaffold.
- Do not add desktop-owned chemistry, project, compute, or IO logic.
- Prefer `ponder_core` diagnostics and error conventions for scaffold code.
- For the first durable desktop shell, the executable may orchestrate
  `ponder_core`, `ponder_platform`, `ponder_render`, and `ponder_ui`.
- Call `pond::render::GetRequiredWindowGraphicsCompatibility()` before creating
  the main platform window; do not hardcode a renderer or SDL creation flag.
- Keep SDL3 window/event ownership in `ponder_platform`, renderer backend/device
  ownership in `ponder_render`, and ImGui context/backend/widget ownership in
  `ponder_ui`.
- Do not link `ponder_project`, `ponder_chemistry`, `ponder_io`,
  `ponder_workflow`, `ponder_compute`, or `ponder_plugin_sdk` until those
  libraries expose durable APIs needed by the app.
- The desktop app should compose commands, panels, and high-level application
  state; it should not become the owner of reusable subsystem behavior.

## Verification

- Configure and build a supported CMake preset with `PONDER_BUILD_APPS=ON`.
- Run `ponder-desktop` from the build output when behavior changes.
