# Workflow Library Boundary

`ponder_workflow` owns user-facing operations and process orchestration that
span multiple domain systems.

## Public API

- Public headers live under `include/ponder/workflow/`.
- Source code uses the `pond::workflow` namespace.
- The CMake target is `ponder_workflow`; the alias is `ponder::workflow`.

## Responsibilities

- High-level commands and workflows that coordinate project, compute, and IO.
- Long-running operation descriptions that can later run locally or remotely.
- Workflow state that should not live inside UI widgets.

## Non-Responsibilities

- Low-level chemistry data structures.
- Compute engine implementation.
- Desktop-specific event handling.

## Dependencies

This library may later depend on domain and project libraries. Keep UI,
platform, and renderer dependencies out of workflow code.
