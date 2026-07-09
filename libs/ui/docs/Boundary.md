# UI Library Boundary

`ponder_ui` owns reusable user-interface models and widgets that are not tied to
a single executable.

Status: SDL-free Dear ImGui integration requirements frozen on 2026-07-08.

## Integration Contract

- [Dear ImGui Platform Adapter Contract](PlatformAdapter.md)

## Public API

- Public headers live under `include/ponder/ui/`.
- Source code uses the `pond::ui` namespace.
- The CMake target is `ponder_ui`; the alias is `ponder::ui`.

## Responsibilities

- Dear ImGui context lifetime and frame setup.
- Project-owned `PlatformEvent` translation into Dear ImGui input.
- Renderer backend hookup for ImGui draw data through renderer-owned frame
  boundaries.
- Reusable UI-facing models and editor widgets.
- Shared shell primitives such as menu bars, dockspaces, status areas,
  diagnostics/log panels, style, and font setup.
- Presentation logic that should not live in domain libraries.
- Clipboard, IME, cursor, pointer, and external-URI callbacks implemented only
  through `ponder_platform`.

## Non-Responsibilities

- Native project data ownership.
- Renderer backend resources.
- OS window lifecycle.
- Secondary Dear ImGui OS viewports in the initial adapter.
- Application command policy and desktop-specific workflow ownership.
- Project format, chemistry data, IO/import, workflow, plugin, or compute
  behavior.

## Adapter Boundary

UI owns the ImGui context and custom platform adapter. It does not compile the
bundled SDL platform backend and never consumes `SDL_Window` or `SDL_Event`.
Docking is initially confined to the main OS window.

Render owns Vulkan resources and draw submission for ImGui data. UI drives that
narrow bridge through renderer frame boundaries without receiving Vulkan
handles.

## Dependencies

`ponder_ui` may depend on `ponder_core`, `ponder_platform`, `ponder_render`, and
Dear ImGui. It must not declare a direct SDL dependency or receive SDL compile
usage requirements. Final executables may still receive SDL as platform's
link-only static implementation dependency. UI must not depend on `ponder-desktop`,
`ponder_project`, `ponder_chemistry`, `ponder_io`, `ponder_workflow`,
`ponder_compute`, or `ponder_plugin_sdk`.

Keep UI dependencies out of core, chemistry, math, project, and compute
libraries. Prefer adapters around ImGui-facing implementation details.
