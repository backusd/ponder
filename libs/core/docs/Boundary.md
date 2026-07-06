# Core Library Boundary

`ponder_core` owns project-wide foundation code that is safe for most other
libraries to depend on.

## Public API

- Public headers live under `include/ponder/core/`.
- Source code uses the `pond::core` namespace.
- The CMake target is `ponder_core`; the alias is `ponder::core`.

## Responsibilities

- `PonderException` for truly exceptional, usually unrecoverable failures.
- `Result<T>` and `Error` for recoverable failures.
- `PONDER_ASSERT*` macros for debug-only invariant checks.
- `LOG_*` macros and severity levels for project diagnostics.
- Small utility types that are broadly useful and do not create domain coupling.
- Cross-platform primitives that should not depend on UI, rendering, chemistry,
  or project-file code.

## Non-Responsibilities

- Chemistry domain modeling.
- Application workflow orchestration.
- Desktop, renderer, or plugin-specific behavior.

## Dependencies

Keep this library at the bottom of the dependency graph. It should avoid
third-party types in public headers unless a later boundary decision allows it.

`ponder_core` may use spdlog privately as the logging backend. Do not expose
spdlog types from public headers.