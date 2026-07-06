# IO Library Boundary

`ponder_io` owns import/export adapters and file-format boundaries.

## Public API

- Public headers live under `include/ponder/io/`.
- Source code uses the `pond::io` namespace.
- The CMake target is `ponder_io`; the alias is `ponder::io`.

## Responsibilities

- External file-format readers and writers.
- Conversion between external data and project/domain models.
- IO diagnostics that can be shown by tools or UI layers.

## Non-Responsibilities

- Native project state ownership.
- Chemistry engine execution.
- Platform file dialogs or desktop UI.

## Dependencies

Keep third-party parsers behind project-owned APIs. Public headers should avoid
heavy third-party includes and direct dependency-specific data types.
