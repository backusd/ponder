# UI Library Boundary

`ponder_ui` owns reusable retained-mode user-interface behavior and project-owned UI paint
semantics. The long-term target is a durable scientific desktop UI; the current milestone only
establishes the internal foundation for rendering a rectangle.

## Public Contract

- Public headers live under `include/ponder/ui/` and use `pond::ui`.
- Public types describe ponder-owned UI concepts only and remain independent of GPU backends,
  platform implementation types, and third-party UI libraries.
- The rectangle-facing surface in the first milestone is explicitly experimental and temporary. It
  must not constrain the later retained tree, layout, styling, input, text, or widget APIs.
- UI-core data and behavior remain buildable, testable, installable, and usable without render.

## Rendering Boundary

- UI owns semantic paint commands, logical coordinate and color rules, clipping, and CPU
  tessellation.
- UI compiles immutable paint data into owning, backend-neutral 2D draw packets only after exact
  per-frame metrics are known.
- Render consumes generic packets and owns shaders, pipelines, GPU resources, command recording,
  submission, presentation, and completion-based retirement.
- Render does not interpret retained elements or UI paint commands and never depends on
  `ponder_ui`.
- UI receives no Vulkan, Metal, SDL, native-window, descriptor, render-pass, command-buffer, or
  other backend handle. No native or third-party type crosses the boundary in either direction.

## Current Scope

The rectangle foundation is specified by [open-questions.txt](open-questions.txt) and ordered by
[roadmap.txt](roadmap.txt). Text, layout, retained-tree design, input, styling, controls, docking,
accessibility, and the stable public UI API belong to later milestones listed in
[milestones.txt](milestones.txt).

UI must not own application command policy, native project data, OS window or GPU lifetimes, domain
workflows, or desktop-specific panels. Domain and foundation libraries must not depend on UI.
