# Codex Guidance: Math

Read the root `AGENTS.md` before this file.

## Scope

Work here on reusable numeric and geometric primitives only.

## Local Rules

- Keep public headers under `include/ponder/math/`.
- Use the `pond::math` namespace.
- Avoid chemistry, rendering, UI, and project concepts in this library.
- Prefer deterministic APIs with explicit units or coordinate conventions.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching math test executable.
