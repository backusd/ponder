# Codex Guidance: Plugin SDK

Read the root `AGENTS.md` before this file.

## Scope

Work here on external plugin SDK boundaries and extension contracts.

## Local Rules

- Keep public headers under `include/ponder/plugin_sdk/`.
- Use the `pond::plugin_sdk` namespace.
- Keep real external plugin boundaries C-style and stable.
- Put ergonomic C++ wrappers on top of the stable boundary, not in place of it.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching plugin SDK test
  executable.
