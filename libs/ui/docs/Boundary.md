# UI Library Boundary

`ponder_ui` owns reusable retained-mode user-interface behavior, project-owned paint semantics, and
the UI side of the backend-neutral rendering boundary. The long-term target is a durable scientific
desktop UI; the current milestone only establishes the internal foundation for rendering a
rectangle through production-shaped layers.

The durable architecture decision is
[ADR 0010: Project-Owned UI Rendering](../../../docs/adr/0010-project-owned-ui-rendering.md).

## Public Contract

- Public headers live under `include/ponder/ui/` and use `pond::ui`.
- Public types describe ponder-owned UI concepts only and remain independent of GPU backends,
  platform implementation types, and third-party UI libraries.
- The rectangle-facing surface in the first milestone is explicitly experimental and temporary. It
  must not constrain the later retained tree, layout, styling, input, text, focus, or widget APIs.
- UI-core data and behavior remain buildable, testable, installable, and usable without render.
- No Dear ImGui source, target, adapter, option, compatibility flag, type, name, or behavior is part
  of the UI architecture.

## Internal Paint Ownership

The exact implemented paint-list/compiler behavior is documented in
[PaintCompiler.md](PaintCompiler.md). UI owns semantic paint commands, logical coordinate rules,
color rules, clipping, paint-list validation, and CPU tessellation. Semantic paint data is the
canonical CPU representation; compiled triangles are only one backend-ready output. Do not make GPU
triangles the only internal UI representation or expose private paint-list storage as stable
application API.

The first rectangle milestone follows this temporary development path:

1. Temporary rectangle facade records semantic UI paint.
2. A sealed UI paint list owns ordered commands and clip state.
3. The UI paint compiler lowers commands after exact frame metrics are known.
4. The compiler emits an owning, backend-neutral `Draw2DPacket`.
5. The render integration records that packet through a device-scoped `Draw2DLayer` and active
   `RenderFrame`.
6. Render owns upload, shaders, pipelines, GPU command recording, submission, presentation, and
   completion retirement.

## Render Boundary

Render sees only owning generic 2D draw packets and never depends on `ponder_ui`. It does not
interpret retained elements, widgets, themes, layout results, logical UI units, paint commands such
as `FillRectangle`, or producer object identity.

The `Draw2DPacket` and `Draw2DLayer` names describe a private producer-neutral contract, not a
public canvas. The packet does not select a target. The destination is selected by the explicit
active `RenderFrame`; render validates compatibility, copies packet bytes into renderer-owned
memory, and never retains producer-owned storage after the synchronous record call returns.

UI receives no Vulkan, Metal, SDL, native-window, descriptor, render-pass, command-buffer,
allocator, shader, pipeline, or other backend handle. No native or third-party type crosses the
boundary in either direction.

## Current Scope

The rectangle foundation is specified by [open-questions.txt](open-questions.txt) and ordered by
[roadmap.txt](roadmap.txt). Long-range sequencing lives in [milestones.txt](milestones.txt), and
intentional complexity deferrals live in [future.txt](future.txt). Text, layout, retained-tree
design, input, styling, controls, docking, accessibility, and the stable public UI API belong to
later milestones.

UI must not own application command policy, native project data, OS window or GPU lifetimes, domain
workflows, or desktop-specific panels. Domain and foundation libraries must not depend on UI.
