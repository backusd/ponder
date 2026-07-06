# Codex Guidance: Compute

Read the root `AGENTS.md` before this file.

## Scope

Work here on computation job abstractions and execution-facing interfaces.

## Local Rules

- Keep public headers under `include/ponder/compute/`.
- Use the `pond::compute` namespace.
- Preserve separation between local, remote, and external engine execution.
- Keep GPL tools external and user-installed if they are ever supported.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching compute test executable.
