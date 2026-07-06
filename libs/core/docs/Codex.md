# Codex Guidance: Core

Read the root `AGENTS.md` before this file.

## Scope

Work here only on project-wide foundation code that can be shared without
pulling in domain, UI, rendering, or application dependencies.

## Local Rules

- Keep public headers under `include/ponder/core/`.
- Use the `pond::core` namespace.
- Avoid third-party types in public APIs unless a boundary decision allows them.
- Prefer small, boring primitives with clear tests.
- Use `Result<T>` for recoverable errors.
- Use `PonderException` only for truly exceptional failures.
- Use `PONDER_ASSERT` or `PONDER_ASSERT_MESSAGE` for internal invariants.
- Use `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`, and
  `LOG_FATAL` for diagnostics.

## Verification

- Configure and build a supported CMake preset.
- When tests exist for this library, run the matching core test executable.