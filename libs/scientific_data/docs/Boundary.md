# Scientific Data Library Boundary

`ponder_scientific_data` owns reusable scientific reference data and metadata
models.

## Public API

- Public headers live under `include/ponder/scientific_data/`.
- Source code uses the `pond::scientific_data` namespace.
- The CMake target is `ponder_scientific_data`; the alias is
  `ponder::scientific_data`.

## Responsibilities

- Reference data models shared across chemistry, compute, and project systems.
- Units, constants, and metadata abstractions when they are not chemistry-only.
- Small versioned data tables if a later decision approves checking them in.

## Non-Responsibilities

- Large datasets or generated binary assets.
- Project-file persistence.
- Chemistry engine integrations.

## Dependencies

Keep this library lightweight. It may later depend on `ponder::core`, but it
should avoid UI, rendering, platform, and workflow dependencies.
