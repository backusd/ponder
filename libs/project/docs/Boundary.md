# Project Library Boundary

`ponder_project` owns the native `.ponder` project model and project-level
state.

## Public API

- Public headers live under `include/ponder/project/`.
- Source code uses the `pond::project` namespace.
- The CMake target is `ponder_project`; the alias is `ponder::project`.

## Responsibilities

- The in-memory model for `.ponder` projects.
- Project identity, versioning, and high-level document state.
- Boundaries between project data and import/export adapters.

## Non-Responsibilities

- Low-level parsing for external scientific file formats.
- Desktop UI state.
- Remote compute execution.

## Dependencies

Prefer narrow dependencies on domain libraries. Keep serialization libraries
behind project-owned APIs instead of exposing third-party JSON types broadly.
