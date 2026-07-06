# Codex Guidance: Workflow

Read the root `AGENTS.md` before this file.

## Scope

Work here on high-level operations that coordinate multiple reusable systems.

## Local Rules

- Keep public headers under `include/ponder/workflow/`.
- Use the `pond::workflow` namespace.
- Keep workflows independent from desktop UI and platform event loops.
- Model long-running operations so remote compute can be added later.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching workflow test executable.
