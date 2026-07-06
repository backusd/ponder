# Codex Guidance: Chemistry

Read the root `AGENTS.md` before this file.

## Scope

Work here on chemical domain models and chemistry-specific behavior.

## Local Rules

- Keep public headers under `include/ponder/chemistry/`.
- Use the `pond::chemistry` namespace.
- Keep file parsing in `ponder_io` and persistence in `ponder_project`.
- Do not bind core chemistry types directly to renderer or UI implementation.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching chemistry test executable.
