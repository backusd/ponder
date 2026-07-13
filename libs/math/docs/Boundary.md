# Math Library Boundary

`ponder_math` owns fixed-size, reusable numeric and geometric primitives for the initial
3D-rendering foundation. ADR 0009 records the cross-library contract behind these rules.
`Usage.md` is the contributor-facing guide for applying this boundary without reading tests.

## Public API

- Public headers live under `include/ponder/math/`.
- Source code uses the `pond::math` namespace.
- The CMake target is `ponder_math`; the alias is `ponder::math`.
- Public includes use `#include <ponder/math/...>`.

## Responsibilities

- Concrete `float` angle types, fixed-size vectors, fixed-size matrices, and quaternions.
- Math-specific overloads built on the project-wide number helpers owned by `ponder_core`.
- Named transform helpers for points, vectors, and normals.
- View, projection, clip/NDC, viewport, project, and unproject helpers.
- The initial renderer visibility and picking collision subset.
- Deterministic, documented math behavior that is independent of renderer resources, UI,
  persistence, file formats, and chemistry semantics.

## Non-Responsibilities

- Chemistry-specific operations or semantic chemistry types.
- Renderer resources, Vulkan or other backend types, shader interfaces, or GPU packing structs.
- UI concepts, project persistence, workflow orchestration, and application policy.
- Arbitrarily sized vectors or matrices, public scalar templates, or non-`float` type families.
- Public bulk-processing APIs, handwritten SIMD contracts, estimate variants, or unchecked hot-path
  variants in the initial implementation.
- DirectXMath or DirectXCollision source compatibility, public API compatibility, or production
  dependencies.

## Dependencies

`ponder_math` may publicly depend on `ponder::core` for `pond::core::Result<T>`, stable error
codes, and `ponder/core/Numbers.hpp`. It must not depend on chemistry, rendering, platform, UI,
Vulkan, SDL, DirectX, or instruction-set-specific public headers.

DirectXMath and DirectXCollision are Windows-only unit/reference-test inputs. They must never be
included by production math code, public math headers, ordinary unit tests, or consumer compile
tests.

## Public Vocabulary And Type Families

The initial public vocabulary is:

- `Vector2`, `Vector3`, and `Vector4`.
- `Matrix3x3` and `Matrix4x4`.
- `Quaternion`.
- `Radians` and `Degrees`.
- Renderer-facing values such as `Viewport`, `Ray`, `Plane`, `Sphere`, `AxisAlignedBox`,
  `Triangle`, and `Frustum`.

The initial API uses concrete, unsuffixed `float` types only. Future `double`, integer,
half-precision, or templated families must use distinct public names and require a concrete
consumer before entering the public contract.

## Algebra And Coordinates

The canonical world convention is right-handed, Y up, and -Z forward. The library exposes this as
the stable convention rather than a global switch.

Transforms are active transforms applied to column vectors:

- Values are multiplied as `matrix * vector`.
- `B * A` applies A first and B second.
- Quaternion multiplication follows the same reading order.
- Matrix translation occupies the final column.

If a future importer, exporter, or backend needs another convention, add explicitly named
conversion helpers. Passive basis-change operations must use names distinct from the active
transform API.

## CPU Layout And Interchange

CPU layout is part of the public contract for the selected values:

- `Vector2`, `Vector3`, and `Vector4` are standard-layout, trivially copyable, naturally aligned,
  compact 8-, 12-, and 16-byte values with public named components and checked indexed access.
- `Matrix3x3` and `Matrix4x4` store contiguous `float` scalars in column-major order with exact
  36- and 64-byte natural layouts and checked row, column, and element access.
- `Quaternion` is a 16-byte trivially copyable unit-rotation value with invariant-preserving
  accessors rather than public mutable fields.

These CPU guarantees do not imply vertex-attribute, storage-buffer, uniform-buffer, or
push-constant packing compatibility. Renderer-owned packing values and conversions must handle
GPU layout rules.

## Projection, NDC, And Viewport

Projection helpers produce right-handed clip space looking down -Z. NDC has +Y up and depth
`[0, 1]`. Every projection construction call explicitly selects forward Z or reverse Z. The first
implementation includes finite and infinite-far perspective projection and finite orthographic
projection.

Viewport helpers are part of the first renderer-ready math library. Viewport space uses continuous
coordinates with a top-left origin and +Y down. NDC `(-1, +1)` maps to the top-left viewport edge;
NDC `(+1, -1)` maps to the bottom-right viewport edge. No half-pixel or pixel-center correction is
applied. NDC depth `[0, 1]` maps linearly into an explicit ordered viewport depth interval.

`Unproject` accepts world-to-clip and performs checked inversion. Repeated callers may cache that
inverse and use `UnprojectFromClipToWorld`, whose explicit name records the required matrix
direction.

Renderer code owns Vulkan viewport-Y adaptation and any backend-specific viewport structures.

## Numerical Contract

Vectors and matrices default to zero. Quaternions default to identity. Exact `operator==` means
exact component or representation equality. Near comparisons require a caller-provided validated
absolute-and-relative `pond::core::Tolerance`; the library provides no universal epsilon.
Quaternion same-rotation comparison is separate because `q` and `-q` represent the same rotation.

Recoverable invalid input and mathematically unavailable results use `pond::core::Result<T>` with
stable math error codes. Assertions are for internal invariants, not caller-reachable failures.
Ordinary unchecked scalar arithmetic follows IEEE behavior.

Default algorithms favor accuracy and documented tolerance stability on supported hosts. The
contract does not promise bit-identical cross-compiler or cross-architecture results. Fast-math,
reassociation, reciprocal and square-root estimates, and accuracy-reducing shortcuts are not
default behavior. Intentional `std::fma` is allowed only when documented as part of an
algorithm and kept within the tolerance contract.

## Collision Scope

The first collision matrix contains exactly these query rows:

- Visibility culling: `Frustum` against `AxisAlignedBox`, returning `Disjoint`, `Intersects`, or
  `Contains`.
- Visibility culling: `Frustum` against `Sphere`, returning `Disjoint`, `Intersects`, or
  `Contains`.
- Picking: `Ray` against `AxisAlignedBox`, returning the nearest entry and exit world-distance
  interval.
- Picking: `Ray` against `Triangle`, returning the nearest world-distance hit with barycentric
  coordinates.

Boundary contact counts as intersection, and containment is inclusive. Valid finite nonzero ray
directions and plane normals are normalized during checked construction. Returned ray
parameters are world distances. Visibility culling must conservatively retain boundary or
numerically uncertain objects rather than falsely reject them.

Frustum uncertainty is scale-aware and based on each plane-test magnitude; uncertain culling
results are `Intersects`. Ray/triangle queries treat near-parallel or near-degenerate,
ill-conditioned configurations as no hit while accepting and clamping boundary values inside their
scale-aware error bounds. Internal uncertainty multipliers are algorithm details, not public
tolerances.

Picking results are supported only when every returned distance and barycentric is finite and
representable as `float`. The optional hit APIs report `std::nullopt` when a geometric result is
outside that representation domain.
