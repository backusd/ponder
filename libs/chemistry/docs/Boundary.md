# Chemistry Library Boundary

`ponder_chemistry` owns chemical domain models and chemistry-specific
algorithms.

## Public API

- Public headers live under `include/ponder/chemistry/`.
- Source code uses the `pond::chemistry` namespace.
- The CMake target is `ponder_chemistry`; the alias is `ponder::chemistry`.

## Responsibilities

- Molecular, atomic, bond, residue, and chemical-system concepts.
- Chemistry-specific validation and transformations.
- Domain abstractions that can be reused by project, workflow, compute, and UI.

## Non-Responsibilities

- General numeric primitives.
- File-format parsing and serialization.
- Rendering resources or platform windows.

## Dependencies

Prefer dependencies on `ponder::core` and `ponder::math` only when needed.
Avoid exposing third-party chemistry engines through public headers.
