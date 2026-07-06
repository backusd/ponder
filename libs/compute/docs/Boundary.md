# Compute Library Boundary

`ponder_compute` owns computation job abstractions and execution-facing
interfaces.

## Public API

- Public headers live under `include/ponder/compute/`.
- Source code uses the `pond::compute` namespace.
- The CMake target is `ponder_compute`; the alias is `ponder::compute`.

## Responsibilities

- Job, task, and result abstractions for computational work.
- Boundaries between local execution, remote execution, and external engines.
- Scheduling-neutral models that workflow and project code can reference.

## Non-Responsibilities

- Specific chemistry engine implementations.
- UI progress widgets.
- Renderer resources.

## Dependencies

Keep external engines behind narrow interfaces. Do not link GPL or AGPL engines
into core platform libraries.
