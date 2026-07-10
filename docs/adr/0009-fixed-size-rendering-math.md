# ADR 0009: Fixed-Size Rendering Math

## Status

Accepted.

## Context

The initial renderer needs a shared math foundation before rendering, culling, and picking code can
stabilize around one convention. The library should resemble DirectXMath and DirectXCollision in
capability, quality, and testability, but not in public API compatibility or production dependency.

These choices affect multiple libraries. Renderer code, import/export code, and tests all need a
single durable contract for coordinates, algebra, storage, projection, numerical behavior, and the
initial collision subset.

## Decision

Build the first `ponder_math` implementation as a fixed-size, concrete `float` library for
3D-rendering math.

### Scope And Dependencies

`ponder_math` owns scalar helpers, strong angle values, fixed-size vectors, fixed-size matrices,
quaternions, transform helpers, camera/projection helpers, viewport mapping, project/unproject, and
the selected collision queries. It does not own renderer resources, backend types, UI concepts,
project persistence, file formats, or chemistry operations.

Public code uses `pond::math`, public headers live under `include/ponder/math/`, and consumers
include them as `<ponder/math/...>`. The target is `ponder_math` with alias `ponder::math`.

The library may publicly depend on `ponder::core` for `pond::core::Result<T>` and stable error
codes. It must not depend on renderer, platform, UI, chemistry, Vulkan, SDL, DirectX, or public
instruction-set-specific headers.

DirectXMath and DirectXCollision are Windows-only unit/reference-test inputs. They are capability,
correctness, and performance references only. They are not production dependencies and do not create
a source, naming, layout, or numerical-compatibility promise.

### Public Vocabulary And Type Policy

The initial public vocabulary uses `Vector2`, `Vector3`, `Vector4`, `Matrix3x3`, `Matrix4x4`,
`Quaternion`, `Radians`, and `Degrees`. Collision and screen-mapping tasks add project-owned values
such as `Viewport`, `Ray`, `Plane`, `Sphere`, `AxisAlignedBox`, `Triangle`, and `Frustum`.

The initial API uses concrete, unsuffixed `float` types only. Arbitrary-size math, public scalar
templates, `double`, integer-vector, and half-precision families are deferred.

Named free functions are preferred. Operators are added only when their meaning is unambiguous.
Component-wise vector multiplication and division, dot products, and cross products use named
functions.

Positions, displacements, directions, and normals use `Vector3`. Named `TransformPoint`,
`TransformVector`, and `TransformNormal` helpers encode their different behavior. Dedicated TRS and
affine value types, decomposition, and interpolation are deferred.

### Algebra And Coordinates

The canonical world convention is right-handed, Y up, and -Z forward. The library exposes this as
the stable convention rather than a global switch.

Transforms are active transforms applied to column vectors. Values are multiplied as
`matrix * vector`; `B * A` applies A first and B second. Quaternion multiplication follows the same
reading order. Matrix translation occupies the final column.

Matrices use column-major contiguous storage. The algebra convention and storage convention are both
explicit parts of the public contract.

### Layout And Interchange

`Vector2`, `Vector3`, and `Vector4` are standard-layout, trivially copyable, naturally aligned, and
compact 8-, 12-, and 16-byte values with public named components and checked indexed access.

`Matrix3x3` and `Matrix4x4` have contiguous `float` scalars in column-major order with exact 36- and
64-byte natural layouts. `Quaternion` is a 16-byte trivially copyable unit-rotation value with
invariant-preserving accessors.

These CPU layout guarantees do not promise GPU packing compatibility. Renderer-owned packing values
and conversions handle vertex attributes, storage buffers, uniform buffers, push constants, and
backend-specific layout rules.

### Projection, NDC, And Viewport

Projection helpers produce right-handed clip space looking down -Z. NDC has +Y up and depth
`[0, 1]`. Every projection construction call explicitly selects forward Z or reverse Z. The first
implementation includes finite and infinite-far perspective projection and finite orthographic
projection.

Viewport mapping and project/unproject are included in the initial renderer-ready math library.
Viewport space uses continuous coordinates with a top-left origin and +Y down. NDC `(-1, +1)` maps
to the top-left viewport edge; NDC `(+1, -1)` maps to the bottom-right viewport edge. No half-pixel
or pixel-center correction is applied. NDC depth `[0, 1]` maps linearly into an explicit ordered
viewport depth interval.

Renderer code owns Vulkan viewport-Y adaptation and any backend-specific viewport structures.

### Angles And Quaternions

Angles use strong `Radians` and `Degrees` values with explicit conversion. Rotation APIs consume
`Radians`.

`Quaternion` stores components as `(x, y, z, w)` and represents a unit rotation. Checked factories
normalize finite nonzero input and reject zero or non-finite input. Composition and interpolation
restore normalization. Slerp uses unit shortest-path behavior. Euler construction and extraction
are deferred until a consumer selects an explicit axis order and intrinsic or extrinsic convention.

### Numerical And Failure Contract

Vectors and matrices default to zero. Quaternions default to identity. Exact `operator==` means
exact component or representation equality. Near comparisons require a caller-provided validated
absolute-and-relative `Tolerance`; the library provides no universal epsilon. Quaternion
same-rotation comparison is separate because `q` and `-q` represent the same rotation.

Recoverable invalid input and mathematically unavailable results use `pond::core::Result<T>` with
stable math error codes. Assertions are for internal invariants, not caller-reachable failures.
Ordinary unchecked scalar arithmetic follows IEEE behavior.

Default algorithms favor accuracy and documented tolerance stability on supported hosts. The
contract does not promise bit-identical cross-compiler or cross-architecture results. Fast-math,
reassociation, reciprocal and square-root estimates, and accuracy-reducing shortcuts are not
default behavior. Intentional `std::fma` is allowed only when documented as part of an algorithm and
kept within the tolerance contract.

### Collision

The initial collision matrix contains exactly these query rows:

- Visibility culling: `Frustum` against `AxisAlignedBox`, returning `Disjoint`, `Intersects`, or
  `Contains`.
- Visibility culling: `Frustum` against `Sphere`, returning `Disjoint`, `Intersects`, or
  `Contains`.
- Picking: `Ray` against `AxisAlignedBox`, returning the nearest entry and exit world-distance
  interval.
- Picking: `Ray` against `Triangle`, returning the nearest world-distance hit with barycentric
  coordinates.

Boundary contact counts as intersection, and containment is inclusive. Zero-radius spheres,
zero-extent boxes, and zero-area triangles are valid finite values with documented behavior. Valid
finite nonzero ray directions and plane normals are normalized during checked construction.
Visibility culling conservatively retains boundary or numerically uncertain objects rather than
falsely rejecting them.

## Consequences

Renderer and import/export code can cite one unambiguous math convention without introducing a
global convention switch.

Adding another coordinate convention, scalar family, public bulk API, handwritten SIMD path,
unchecked operation family, GPU packing representation, or collision pair requires a concrete
consumer and an update to the roadmap or a later ADR.

Tests must include independent expectations for project semantics. Windows DirectX reference tests
are useful differential checks, but they never replace project-owned tests and never define
project-specific failure, boundary, uncertainty, or convention semantics by themselves.
