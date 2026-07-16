# ADR 0010: Project-Owned UI Rendering

## Status

Accepted.

## Related Decisions

- [ADR 0007: SDL3 Platform Backend](0007-sdl3-platform-backend.md)
- [ADR 0008: Vulkan Renderer Backend](0008-vulkan-renderer-backend.md)

## Context

`ponder` needs a durable scientific desktop UI, but the first UI rendering milestone is deliberately
small: render a rectangle through production-shaped internals. That rectangle must not force the
future retained widget API, but it should establish the ownership, coordinate, color, clipping,
frame, and dependency boundaries that later panels, tables, docking, text, and property editors can
reuse.

Dear ImGui has been removed from the architecture. Keeping a third-party immediate-mode layer as an
adapter would make the new retained UI design answer to another toolkit's data flow. It would also
make the renderer boundary blurrier by encouraging UI concepts to be lowered into an implementation
that neither `ponder_ui` nor `ponder_render` truly owns.

The renderer already owns window presentation, devices, swapchains, frame recording, submission, and
completion retirement. The UI library must own UI semantics without receiving Vulkan, Metal, SDL,
native-window, command-buffer, descriptor, render-pass, or allocator handles. The shared rendering
boundary therefore needs to be producer-neutral: useful for UI, testable without UI, and not a
general public GPU encoder.

## Decision

### UI Owns Paint Semantics

`ponder_ui` owns retained UI behavior, semantic paint commands, logical coordinate rules, clipping
rules, color semantics, paint-list validation, and CPU tessellation. The current rectangle entry
point is only a temporary facade over that internal path. It must not become the future retained
tree, layout, styling, text, input, focus, or widget API.

The canonical CPU input is an owning semantic paint list. The paint compiler lowers that list after
exact per-frame metrics are known. UI may use indexed triangles as a compiled output, but triangles
are not the only UI representation and must not replace semantic paint data as the source of truth.

### Render Owns Generic 2D Execution

`ponder_render` consumes producer-neutral 2D draw packets and owns the GPU execution of those
packets: validation at the render boundary, upload memory, shaders, pipelines, command recording,
submission, presentation, diagnostics, and completion-based retirement.

The private cross-library names should be precise and producer-neutral, such as `Draw2DPacket` for
the sealed geometry/state payload and `Draw2DLayer` for the move-only device-scoped renderer owner.
The contract is project-private and non-installed for this milestone. Applications do not receive a
public generic canvas, arbitrary GPU command API, callback escape hatch, native encoder, or backend
handle.

Render must not interpret widgets, retained elements, themes, layout results, logical UI units,
paint commands such as `FillRectangle`, or producer object identity. It accepts only closed
project-owned values and containers with explicit ownership.

### Packet And Frame Ownership

The producer owns a sealed packet for the synchronous record call. Render validates it, copies the
required bytes into renderer-owned mapped memory, and never retains borrowed producer storage.
After the call returns, the producer may reset and reuse its exclusive builders; only
renderer-owned upload generations remain tied to frame completion.

The destination target is selected only by the explicit active `RenderFrame`. The packet does not
select a window, target, swapchain, or native surface. There is no ambient current target, global
render context, process-wide UI renderer, or pointer-identity lookup.

### First Geometry Strategy

The first rectangle path uses dynamic indexed 2D geometry with explicit draw batches, scissors, and
premultiplied linear color. This is a foundation strategy, not a promise that every future UI
primitive remains dynamic triangles. Future text, images, cached meshes, atlases, path rendering,
and specialized materials may add typed packet/resource variants when their semantics are known.

### Build And Dependency Shape

`ponder_ui` core must remain configurable, buildable, testable, installable, and consumable when
`ponder_render` is disabled. The temporary GPU join belongs in an explicit integration target or
source component gated by render. `ponder_render` must not depend on `ponder_ui`; only UI-facing
integration code may depend on both sides of the private packet contract.

No Dear ImGui source, target, adapter, option, compatibility flag, type, name, or behavior is part
of this architecture.

## Consequences

Rendering one rectangle requires more structure than a direct backend draw call, but that structure
protects the later retained UI from backend details and protects render from UI concepts.

The first milestone can claim only a tested project-owned rectangle paint path on the documented
Windows/Vulkan configuration, with backend-neutral CPU and packet contracts. It cannot claim a
retained widget API, a complete 2D canvas, a public generic draw API, native Metal runtime support,
or a finished scientific desktop UI.

Future milestones may replace the temporary facade, extend paint commands, add text and image
resources, define input routing, and promote a public drawing API only after new requirements
justify
those decisions. Version control remains the history for removed third-party UI integration plans.

## References

- [UI Boundary](../../libs/ui/docs/Boundary.md)
- [Render Boundary](../../libs/render/docs/Boundary.md)
- [UI open questions](../../libs/ui/docs/open-questions.txt)
- [UI roadmap](../../libs/ui/docs/roadmap.txt)
- [UI milestones](../../libs/ui/docs/milestones.txt)
- [UI future decisions](../../libs/ui/docs/future.txt)
