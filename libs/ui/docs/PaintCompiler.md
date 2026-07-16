# Private UI Paint Compiler Contract

The UI paint compiler is the private CPU boundary between an immutable semantic UI paint list and
an owning, producer-neutral `Draw2DPacket`. It implements the decisions accepted in
[open-questions.txt](open-questions.txt) and follows
[ADR 0010](../../../docs/adr/0010-project-owned-ui-rendering.md).

This is an internal milestone contract. It is not an installed API, a public canvas, a retained
widget API, a serialization format, or an ABI promise.

## Ownership And Inputs

`PaintCompiler::Compile()` accepts only:

- a `const SealedPaintList&` containing ordered `FillRectangle`, `PushClipRectangle`, and `PopClip`
  commands;
- a copied, immutable `UiTargetMetrics` value; and
- the compiler's exclusively owned clip scratch and reusable `Draw2DPacketBuilder` workspace.

The paint list remains the canonical semantic representation. Compilation never mutates or consumes
it. The compiler owns no device, render target, platform window, backend handle, or global mutable
state. Its implementation is part of the render-gated private UI integration target; UI paint
recording itself remains available when render is disabled.

The metrics value must have valid target identity and revisions, a positive logical extent, and a
positive exact framebuffer extent. Compilation validates the copied metrics but does not compare
them with an active render frame. The private UI/render metrics rendezvous owns that exact match,
and the experimental facade added by UI-015 must invoke it before calling the compiler.

## Compilation Stages

Compilation is a synchronous CPU transaction with these observable stages:

1. `PaintValidation` checks input budgets and all sealed paint-list invariants again.
2. `MetricsValidation` validates drawable metrics and derives independent X and Y scale ratios.
3. `ScratchPreflight` checks and reserves the clip-stack workspace.
4. `GeometryPreflight` performs a no-output pass that resolves clips, elision, merging, counts, and
   byte requirements.
5. `PacketBuild` sets the packet extent, reserves exact output counts, and repeats compilation to
   emit geometry and draw records.
6. `PacketSeal` runs the render-owned packet validator and creates the immutable owning packet.

The emission summary must equal the preflight summary. Any divergence is a compilation failure;
partial output is never published.

## Coordinate And Clipping Rules

Semantic geometry uses finite logical UI units with a top-left origin, positive X to the right, and
positive Y downward. Rectangle bounds are half-open: `[left, top, right, bottom)`.

The compiler derives scale independently on each axis:

```text
scaleX = exactFramebufferWidth  / logicalWidth
scaleY = exactFramebufferHeight / logicalHeight
```

Scaling and fractional clipping use `double` intermediates. Packet positions are narrowed only
after root clipping and remain top-left framebuffer-pixel `float` values. There is no pixel
snapping, uniform-scale assumption, half-pixel offset, or clip-space transformation. Narrowed zero
coordinates are canonicalized to positive zero.
A nonempty clipped interval that collapses while narrowing to packet floats is rejected as a
compilation failure rather than emitting degenerate geometry.

The clip stack starts with the fractional root target:

```text
[0, 0, exactFramebufferWidth, exactFramebufferHeight)
```

Each pushed logical clip is scaled and intersected with the current effective clip. A disjoint
intersection becomes an empty clip; nested commands still execute and a later pop restores the
parent. A fill is intersected with the effective fractional clip before tessellation, so fractional
clip edges are represented by geometry rather than approximated by the integer scissor.

Every visible draw receives a conservative half-open integer scissor derived from its effective
clip, not from the individual fill bounds:

- minimum edges use `floor`;
- maximum edges use `ceil`; and
- all edges are clamped to the exact unsigned framebuffer extent.

This keeps the scissor coarse and conservative while exact geometry prevents leakage outside a
fractional clip.

## Rectangle Tessellation

Every visible rectangle emits four vertices in this order:

```text
0: top-left
1: top-right
2: bottom-right
3: bottom-left
```

It emits two triangle-list primitives with indices:

```text
0, 1, 2, 0, 2, 3
```

Indices are absolute 32-bit indices into the packet's shared vertex stream. Draw records therefore
use `baseVertex = 0`, which allows adjacent rectangle index ranges to be merged without changing
geometry or order. With positive Y downward, the documented first triangle has positive signed
screen-space area. Render disables culling, but tests still lock this exact sequence.

## Color Pipeline

The recorder validates straight-alpha sRGB authoring values, converts RGB channels to linear light,
and premultiplies them by alpha. The sealed paint list therefore stores finite, higher-precision
`LinearPremultipliedColor`; the compiler does not perform a second sRGB conversion.

During compilation, the semantic color is validated again and each linear-premultiplied channel is
quantized to UNORM8 with explicit round-half-up behavior:

```text
floor(clamp(channel, 0, 1) * 255 + 0.5)
```

The packet vertex stores ordered red, green, blue, and alpha bytes. It does not contain textures,
UVs, samplers, descriptors, materials, or a dummy white image.

## Elision, Ordering, And Merging

Commands always remain observable through their stable paint-command indices and inspection
counters. A validated fill emits no geometry when, in this order, it is:

1. exactly zero-area;
2. fully transparent; or
3. under an empty effective clip or fully outside its effective clip.

Elision is normal no-work and is not logged as an error. It does not alter clip behavior or the
relative order of later visible fills.

Visible geometry is appended strictly in recorder insertion order. The compiler never sorts by
opacity, color, bounds, or any other key. It merges only adjacent pending draw records when:

- their effective integer scissors are identical;
- their index ranges are contiguous; and
- extending the 32-bit index count is representable.

Merging extends only the draw record. Vertex and index order remain unchanged. A visible fill whose
effective scissor differs flushes the pending record, so returning to an earlier scissor later
cannot merge across an intervening visible draw. Clip changes with no visible fill emit and flush
nothing.

## Preflight And Budgets

All relevant limits are checked before packet growth or output writes:

- source command count, payload bytes, and maximum clip depth;
- compiler scratch bytes;
- compiled vertex, index, and draw-record counts;
- packet bytes; and
- vertex/index upload bytes.

The command-count limit is checked from the sealed list's constant-time span size before any
statistics scan. Scratch accounting is the maximum clip depth multiplied with the private
fractional clip entry size. Output accounting uses four vertices and six indices per visible
rectangle plus the exact post-merge draw count. The compiler checks that projection after each
visible fill, so a failure stops at and identifies the command that first crosses a limit. Every
multiplication, addition, index offset, and 32-bit narrowing is checked.

`InspectPaintCompileCounts()` provides a deterministic, allocation-free projection of count,
representability, byte, and limit validation. A rejected limit records its `UiHardLimitKind`,
requested value, and allowed value. The compiler and render packet builder both enforce their
respective budgets; the more restrictive applicable limit wins.

## Transaction, Reset, And Reuse

On success, compilation returns a move-only `Draw2DPacket` whose extent, vertices, indices, draw
records, schema fingerprint, and statistics are owned independently of the compiler and source
paint list. The compiler's packet builder becomes sealed. Another compile attempt fails with
`InvalidPaintState` until `Reset()` is called.

Once a ready compiler enters a compilation transaction, failure clears the open packet builder and
clip stack, publishes no packet, and enters `Failed`. A misuse attempt while the prior builder is
still sealed instead reports a fresh `StateValidation` failure inspection and leaves that sealed
builder intact. `Reset()` is required before either retry. Reset clears logical state but retains
reasonable vector capacity, allowing warm reuse without invalidating a previously returned packet.

The temporary rectangle facade additionally recycles the prior sealed packet through the private
compiler reset path and seals recorder output into a retained paint-list owner. Once warmed for an
equal-or-smaller rectangle workload, both sides reuse every vector stream instead of allocating new
host containers per frame. Growth reserves all destination streams before publishing new contents;
a failed reserve leaves the prior reusable paint list intact and reports a recoverable error.

The compiler and its outputs are move-only. Moving a compiler transfers its complete state and
leaves the source as a coherent ready-empty compiler with the same limits; an exclusively owned
instance may move between threads, but concurrent mutation is not supported.

## Inspection And Errors

`PaintCompilerInspection` is a copied deterministic projection, not a view of container layout. It
reports:

- `Ready`, `Succeeded`, or `Failed` status and the failure stage;
- last and rejected paint-command indices;
- processed commands and maximum clip depth;
- visible, zero-area, transparent, clipped, and merged counts;
- scratch bytes, planned packet counts/statistics, and unioned visible output bounds;
- rejected limit details when applicable; and
- retained clip-stack and packet-builder capacities/state.

Recoverable failures use the UI error domain:

- `InvalidPaintValue` for malformed sealed paint data or invalid compiler limits;
- `UnbalancedPaintState` for invalid clip-stack structure;
- `InvalidMetrics` for invalid or non-drawable copied metrics;
- `LimitExceeded` for checked input, scratch, count, representability, or byte budgets;
- `CompilationFailure` for non-finite/narrowing, allocation, packet-build, or packet-seal failure;
  and
- `InvalidPaintState` when reuse occurs without reset.

The compiler returns errors and inspection data without logging. Render-owned packet validation
remains producer-neutral and does not mention UI paint commands.

## Deliberate Scope

This compiler currently lowers only solid rectangle fills and rectangular clips. It defines no
retained tree, layout, styling, text, images, paths, transforms, rounded corners, layers, hit
testing, input, focus, widgets, target selection, frame recording, GPU uploads, shaders, or backend
execution. Those concerns extend either semantic paint or render execution in later milestones
without changing this ownership boundary.
