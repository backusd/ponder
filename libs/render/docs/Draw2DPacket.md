# Private Draw2D Packet Contract

`Draw2DPacket` is ponder's private, producer-neutral transaction for ordered solid-color 2D
triangle drawing. The implemented contract lives in
[`Draw2DPacket.hpp`](../private/include/ponder/render/draw2d/Draw2DPacket.hpp) and follows
[ADR 0010](../../../docs/adr/0010-project-owned-ui-rendering.md).

The packet is the boundary between a CPU producer that has already tessellated its content and the
render-owned code that validates, uploads, and records it. It is not a UI paint list, retained UI
tree, public canvas, render graph, arbitrary GPU command stream, serialized format, or resource
database.

## Ownership And Dependency Boundary

The packet owns its exact pixel extent, vertices, 32-bit indices, draw records, schema fingerprint,
and derived statistics. It stores no borrowed producer data. `Draw2DPacket` and
`Draw2DPacketBuilder` are move-only, and a sealed packet exposes its arrays only as immutable
`std::span<const T>` views.

The views are valid only while the owning packet remains alive and unchanged. The private
`Draw2DLayer` records a sealed packet by const reference during one synchronous call. It validates
the packet and active frame compatibility immediately. Future upload/pipeline work must copy any
required bytes into renderer-owned storage during that call and must not retain a packet span
afterward. Success and failure leave packet ownership with the producer.

The contract is provided by the non-installed `ponder::render_draw2d_packet` CMake interface target.
It is intentionally absent from the public package and application API. Render tests construct
packets without including or linking UI, and render has no `ponder_ui` dependency.

## Exact Schema

### Coordinates And Extent

`Draw2DPixelExtent` contains positive 32-bit `width` and `height` values. It is the exact
framebuffer pixel extent for which the packet was compiled. It is the packet's only target
compatibility metadata; the packet contains no target, window, swapchain, device, revision, or
native identity.

Vertex positions are finite `float` values in top-left-origin framebuffer pixel-edge coordinates:
positive X points right and positive Y points down. Positions may be fractional or outside the
packet extent. The producer is responsible for exact geometry clipping; the scissor supplies a
coarse bounded rasterization region. Render's vertex shader later converts these pixel coordinates
to the active backend's clip-space convention without a half-pixel offset.

### Vertex And Color

`Draw2DVertex` is a standard-layout, trivially copyable 12-byte value with float alignment:

| Offset | Field | Representation |
| ---: | --- | --- |
| 0 | `x` | 32-bit framebuffer-pixel float |
| 4 | `y` | 32-bit framebuffer-pixel float |
| 8 | `color` | Four bytes in R, G, B, A order |

`Draw2DPackedLinearPremultipliedRgba8` stores one byte per channel in explicit RGBA order. Each byte
is an 8-bit UNORM linear-light value, and RGB is premultiplied by alpha. A valid value therefore
requires `red <= alpha`, `green <= alpha`, and `blue <= alpha`. No machine-endian interpretation of
a packed integer is required.

The first schema has no UV coordinate, texture, sampler, descriptor, material, target, or dummy
white-image field.

### Indices, Draw Records, And Scissors

`Draw2DIndex` is exactly `std::uint32_t`, and the fixed topology is a triangle list.
`Draw2DDrawRecord` contains:

- `firstIndex` and `indexCount` in index elements, not bytes;
- a signed 32-bit `baseVertex`; and
- one `Draw2DScissor`.

Each draw has a nonzero index count divisible by three. Its checked half-open index range must fit
the packet's index array. Every stored raw index must name an existing vertex, and adding the draw's
signed base vertex must still produce an existing vertex for every referenced index.

`Draw2DScissor` stores unsigned half-open `[left, top, right, bottom)` edges. It must have positive
area and must already be clamped to the packet extent. Draw records retain insertion order as total
rendering order. Records may overlap or share index and vertex ranges; render does not sort or
otherwise reinterpret them.

### Schema Fingerprint

The authoritative compile-time schema description is:

```text
ponder.draw2d.packet.v1|extent:u32x2-positive|vertex:position-f32x2-top-left-pixel-edge@0,rgba8-unorm-linear-premul@8,stride12|index:u32|draw:first-index-u32,index-count-u32,base-vertex-i32,scissor-u32-ltrb-half-open|topology:triangle-list
```

`ComputeDraw2DSchemaFingerprint` hashes that text with standard 64-bit FNV-1a into
`kDraw2DSchemaFingerprint`, which every sealed packet carries. Compile-time known-value assertions
lock both the hash algorithm and the current schema value. Packet validation requires exact
equality. Render-owned shader artifacts must use and verify the same description/fingerprint. This
is a build-internal compatibility check, not runtime schema negotiation or a persistent format
version.

## Builder, Sealing, And Reuse

`Draw2DPacketBuilder` begins in the `Open` state with no extent or data. Its configured limits must
all be nonzero. A producer may reserve validated final counts before setting the extent, but it must
set a positive extent before appending vertices, indices, or draw records. The extent cannot change
after any data has been appended.

Append operations synchronously copy their input spans into builder-owned storage. They preflight
the resulting logical counts and byte budgets before growth. `Reserve` rejects counts below current
storage and checks representability, all configured count limits, packet bytes, and upload bytes
before reserving any requested capacity. Packet/upload byte budgets describe live semantic payload,
not allocator bookkeeping or unused reusable vector capacity; speculative capacity remains bounded
by the corresponding element-count limits.

`Seal` validates the complete candidate and copies it into a new owning `Draw2DPacket`. Sealing is
transactional: a validation or allocation failure leaves the builder open with its data intact and
reusable. A successful seal changes the builder to `Sealed`; further extent, reserve, append, or
seal operations fail until `Reset` is called.

`Reset` clears counts and the extent, returns the builder to `Open`, and retains vector capacity.
Previously returned packets remain independently owned and valid. Moving a builder transfers its
complete open/sealed state and resets the source to a reusable open-empty builder with the same
limits. Moving a valid packet transfers its payload and leaves the source as a canonical empty
packet with the same positive extent. Neither builder nor packet provides concurrent mutation; an
exclusively owned instance may be moved between threads when its caller supplies the required
synchronization.

## Canonical Empty Packet

There is exactly one valid empty shape:

- a positive exact pixel extent;
- the current schema fingerprint; and
- empty vertex, index, and draw-record arrays.

This represents successful no-work compilation, such as fully clipped or fully transparent input.
Any partially empty combination is malformed, as is an individual draw with an empty range.

## Validation, Statistics, And Budgets

Validation is pure CPU work and has no Vulkan, allocator, native-window, device, or UI dependency.
It rejects the complete packet rather than truncating data or accepting a valid prefix.

The validation entry points have distinct uses:

- `InspectDraw2DPacketCounts` checks count and byte arithmetic without allocating storage.
- `InspectDraw2DPacketData` checks an extent, fingerprint, and immutable array views.
- `InspectDraw2DPacket` inspects an owning sealed packet.
- `ValidateDraw2DPacket` converts inspection failure into `core::Result<Draw2DPacketStats>`.

`Draw2DPacketValidation` identifies a stable typed issue and carries computed statistics, an
offending element index when applicable, and requested/allowed values for limit failures. The
validator checks:

- nonzero configured limits and a positive extent;
- the exact schema fingerprint;
- checked 64-bit multiplication and addition for every byte count;
- 32-bit addressability of vertex and index counts;
- configured vertex, index, draw-record, packet-byte, upload-byte, and base-vertex validation-work
  budgets;
- the canonical empty shape;
- finite vertex positions and valid premultiplied colors;
- every raw index, draw range, triangle count, base-vertex result, and scissor.

`baseVertex == 0` requires no second index pass because raw-index validation has already proved the
effective vertex range. For a nonzero base vertex, global raw-index extrema first provide a
constant-time sufficient proof when every possible effective index remains in range. Only draw
ranges that cannot use that proof are scanned. Their index counts are accumulated with checked
arithmetic and charged against the configurable base-vertex validation-index budget before each
scan, bounding hostile validation work even when draw records share large index ranges.

`Draw2DPacketStats` reports vertex, index, and draw counts; their individual byte sizes; total
packet bytes; and upload bytes. Upload bytes are vertex bytes plus index bytes. Packet bytes add
draw-record bytes to the upload bytes.

The default internal budgets are:

| Budget | Default |
| --- | ---: |
| Vertices | 4,194,304 |
| Indices | 8,388,608 |
| Draw records | 1,048,576 |
| Packet bytes | 256 MiB |
| Upload bytes | 256 MiB |
| Nonzero-base indices requiring per-range validation | 8,388,608 |

Malformed data, invalid limits, and budget exhaustion map to
`RenderErrorCode::InvalidArgument`. Open/sealed misuse maps to `RenderErrorCode::InvalidState`.
Host allocation and container-length failures map to `RenderErrorCode::OutOfMemory`. Diagnostic
text uses producer-neutral packet, vertex, index, draw, and scissor terminology.

Before a future frame records any Draw2D GPU command or consults a backend allocator, render must
validate the entire packet against its active budgets and verify that the packet extent matches the
active frame. The draw layer then owns pipeline, viewport, scissor, buffer binding, upload,
recording, submission, presentation, and completion retirement; it assumes no previously bound GPU
state.

## Explicit Non-Goals

The current packet contains no logical UI units, paint commands, widgets, retained elements,
producer identity, callbacks, raw pointers, erased payloads, textures, glyphs, paths, transforms,
materials, arbitrary state changes, or backend handles. It is transient in-process data and is not
promised to survive resize, target recreation, device changes, serialization, or a future private
schema revision. Producers should retain or cache higher-level semantic data and recompile a packet
for the exact current frame extent.
