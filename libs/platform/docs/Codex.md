# Codex Guidance: Platform

Read the root `AGENTS.md` before this file.

## Scope

Work here on operating-system and desktop-platform integration.

## Local Rules

- Keep public headers under `include/ponder/platform/`.
- Use the `pond::platform` namespace.
- Keep SDL and OS-specific types behind project-owned boundaries where possible.
- Avoid pulling platform concerns into core domain libraries.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching platform test executable.
