# Desktop Application Boundary

`ponder-desktop` is the main user-facing desktop executable. It owns application
orchestration, not reusable platform, renderer, UI, project, chemistry, IO,
workflow, compute, or plugin behavior.

Status: dependency direction reviewed for the desktop foundation phase.

## Public Surface

- The executable target is `ponder_desktop`; the output name is
  `ponder-desktop`.
- Source code lives under `apps/desktop/src/`.
- Local guidance lives in `apps/desktop/docs/Codex.md`.

## Responsibilities

- Application startup and shutdown ordering.
- High-level app state and command routing.
- Main-loop orchestration across platform events, renderer frames, and UI frames.
- Call `pond::render::GetRequiredWindowGraphicsCompatibility()` before creating
  the main platform window; never hardcode SDL or Vulkan flags.
- Startup/shutdown logging and build metadata display.
- User-facing shell composition using reusable platform, render, and UI APIs.
- Later project-opening commands, once `ponder_project` has durable project
  loading APIs.

## Non-Responsibilities

- SDL3 window lifecycle, native handles, event translation, or platform dialogs;
  those belong in `ponder_platform`.
- Renderer backend/device/surface ownership; that belongs in `ponder_render`.
- Reusable UI state, paint semantics, layout, text, input, and widgets; those
  belong in `ponder_ui`. Generic GPU 2D packet execution belongs in
  `ponder_render`.
- Project format, project validation, or project persistence; those belong in
  `ponder_project`.
- Chemistry data ownership, IO/import handlers, workflow graphs, compute jobs,
  or plugin SDK behavior.

## Dependencies

For the first durable desktop shell, `ponder-desktop` may depend on
`ponder_core`, `ponder_platform`, `ponder_render`, and `ponder_ui`.

It should not depend on `ponder_project`, `ponder_chemistry`, `ponder_io`,
`ponder_workflow`, `ponder_compute`, or `ponder_plugin_sdk` until those systems
have durable APIs that the application can consume without owning their
semantics.
