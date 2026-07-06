# Codex Guidance: Render

Read the root `AGENTS.md` before this file.

## Scope

Work here on rendering abstractions and visualization data flow.

## Local Rules

- Keep public headers under `include/ponder/render/`.
- Use the `pond::render` namespace.
- Keep platform windows in `ponder_platform` and UI widgets in `ponder_ui`.
- Do not expose backend-specific types in broad public APIs without a decision.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching render test executable.
