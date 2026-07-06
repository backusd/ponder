# Codex Guidance: UI

Read the root `AGENTS.md` before this file.

## Scope

Work here on reusable user-interface models, widgets, and presentation logic.

## Local Rules

- Keep public headers under `include/ponder/ui/`.
- Use the `pond::ui` namespace.
- Keep UI implementation details out of domain libraries.
- Prefer wrappers around ImGui-facing code when crossing subsystem boundaries.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching UI test executable.
