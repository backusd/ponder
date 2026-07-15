# Codex Guidance: UI

Read the root `AGENTS.md` before this file.

## Scope

Work here on reusable retained-mode UI, project-owned paint semantics, and the UI side of the
backend-neutral rendering boundary.

## Rules

- Keep public headers under `include/ponder/ui/` and use `pond::ui`.
- Do not add Dear ImGui source, targets, adapters, compatibility flags, types, names, or behavior.
- UI owns semantic paint commands, logical coordinates, color and clipping rules, and CPU
  tessellation. Do not make GPU triangles its only internal UI representation.
- Render sees only owning generic 2D draw packets and never depends on UI. Do not put widgets,
  retained elements, logical UI units, or paint-command interpretation in render.
- Keep Vulkan, Metal, SDL, native-window, descriptor, render-pass, command-buffer, and other backend
  handles out of UI.
- Preserve UI-core configure, build, test, install, and consumption when render is disabled. Isolate
  the temporary render integration in an explicit target or source component.
- For the current milestone, treat [open-questions.txt](open-questions.txt) as the accepted contract
  and follow [roadmap.txt](roadmap.txt) for implementation order. The rectangle-facing API is
  experimental; do not design the retained tree, layout, styling, text, input, focus, widgets, or
  stable public frame API in this work.
- Keep public headers lightweight and keep private paint/compiler representation non-installed.
- Keep application command policy, domain data, desktop-specific panels, workflows, compute, and
  plugin behavior outside this library.

## Verification

- Test paint recording and compilation deterministically without a GPU or native window.
- Test generic render packets independently without linking UI.
- Build public-header and install-tree consumers, including UI core with render disabled.
- Run the bounded Windows/Vulkan rectangle gate when changing the paint-to-render boundary.
- Verify UI core has no direct SDL, Vulkan, or render dependency and render has no UI dependency.
