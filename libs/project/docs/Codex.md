# Codex Guidance: Project

Read the root `AGENTS.md` before this file.

## Scope

Work here on the native `.ponder` project model and project-level state.

## Local Rules

- Keep public headers under `include/ponder/project/`.
- Use the `pond::project` namespace.
- Keep external import/export details in `ponder_io`.
- Preserve room for stable project-file versioning and migration logic.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching project test executable.
