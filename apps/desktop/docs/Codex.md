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

## Verification

- Configure and build a supported CMake preset with `PONDER_BUILD_APPS=ON`.
- Run `ponder-desktop` from the build output when behavior changes.