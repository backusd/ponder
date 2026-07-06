# Render Library Boundary

`ponder_render` owns renderer-facing scene abstractions and visualization
interfaces.

## Public API

- Public headers live under `include/ponder/render/`.
- Source code uses the `pond::render` namespace.
- The CMake target is `ponder_render`; the alias is `ponder::render`.

## Responsibilities

- Renderable scene descriptions and visualization abstractions.
- Renderer-independent data flow from domain models to visual output.
- Future backend integration boundaries.

## Non-Responsibilities

- Platform windows and input.
- UI widgets.
- Chemistry data ownership.

## Dependencies

Keep backend-specific APIs behind private implementation boundaries until a
renderer backend decision is recorded.
