# Math Library Boundary

`ponder_math` owns reusable numeric and geometric primitives.

## Public API

- Public headers live under `include/ponder/math/`.
- Source code uses the `pond::math` namespace.
- The CMake target is `ponder_math`; the alias is `ponder::math`.

## Responsibilities

- Numeric helper types and algorithms needed by multiple libraries.
- Geometry, vector, matrix, and transform primitives when they become needed.
- Deterministic math behavior that is independent of UI and file formats.

## Non-Responsibilities

- Chemistry-specific semantics.
- Rendering backend resources.
- Project persistence or workflow orchestration.

## Dependencies

Keep dependencies minimal. This library may later depend on `ponder::core`, but
it should not depend on chemistry, rendering, UI, or project libraries.
