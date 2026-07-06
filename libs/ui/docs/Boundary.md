# UI Library Boundary

`ponder_ui` owns reusable user-interface models and widgets that are not tied to
a single executable.

## Public API

- Public headers live under `include/ponder/ui/`.
- Source code uses the `pond::ui` namespace.
- The CMake target is `ponder_ui`; the alias is `ponder::ui`.

## Responsibilities

- Reusable UI-facing models and editor widgets.
- ImGui integration that can be shared by desktop tools.
- Presentation logic that should not live in domain libraries.

## Non-Responsibilities

- Native project data ownership.
- Renderer backend resources.
- OS window lifecycle.

## Dependencies

Keep UI dependencies out of core, chemistry, math, project, and compute
libraries. Prefer adapters around ImGui-facing implementation details.
