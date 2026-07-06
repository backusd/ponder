# Codex Guidance: IO

Read the root `AGENTS.md` before this file.

## Scope

Work here on file-format import/export and conversion boundaries.

## Local Rules

- Keep public headers under `include/ponder/io/`.
- Use the `pond::io` namespace.
- Do not make IO own the native project model.
- Keep external parser details out of broad public APIs.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching IO test executable.
