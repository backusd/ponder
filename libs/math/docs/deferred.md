# Deferred Math Features

This register records capabilities excluded from the initial implementation in `roadmap.txt`.
It is derived from `Boundary.md`, ADR 0009, and the roadmap. A deferred candidate should enter the
roadmap only when a concrete consumer establishes its semantics, constraints, and verification
requirements.

## Included In The Initial Roadmap

The following are not deferred. They are part of the first renderer-ready math library:

- Viewport mapping and project/unproject helpers.
- Frustum/AABB and frustum/sphere classification for renderer visibility.
- Ray/AABB nearest entry and exit world-distance intervals for picking.
- Ray/triangle nearest world-distance hits with barycentric coordinates for picking.

## Deferred Candidates

### Scalar And Type Families

- Arbitrarily sized vectors and matrices remain outside the fixed-size initial API. Reconsider
  them only for a concrete workload that cannot use fixed dimensions effectively.
- `Matrix2x2` and fixed dimensions other than the selected vector and matrix types are deferred.
  Add one only when a consumer defines its required operations and interchange constraints.
- Public scalar templates and `double`, integer-vector, and half-precision families are deferred.
  A consumer must first establish precision, conversion, layout, and arithmetic semantics. Any
  future double family must use distinct names rather than redefine unsuffixed `float` types.
- Strong position, displacement, direction, and normal types are deferred. Revisit them only if
  named operations on `Vector3` prove insufficient to prevent real integration bugs.
- An unrestricted four-component quaternion-like numeric type is not part of `Quaternion`, whose
  unit-rotation invariant is part of the initial contract. A future consumer needing such arithmetic
  should motivate a separate type rather than weakening `Quaternion`.

### Transforms, Rotations, And Conventions

- Dedicated TRS and compact affine types are deferred. Before adding either, define its invariants
  and its behavior for non-uniform scale, negative scale, and shear.
- Matrix decomposition and transform interpolation are deferred. Revive them with a workflow that
  defines failure behavior, decomposition uniqueness, and interpolation semantics.
- Euler-angle construction and extraction are deferred. A consumer must select an explicit axis
  order and intrinsic or extrinsic convention before either API is designed.
- Dual-quaternion skinning and other animation-specific math are deferred. Add them only with an
  animation or skinning consumer that defines representation and blending requirements.
- Alternative world-coordinate conversions are deferred. Add explicitly named conversions only for
  a concrete importer, exporter, or backend; do not add a global switch.
- Passive basis-change operations are deferred. A future need must receive names distinct from the
  initial active-transform API.

### Performance, SIMD, And Bulk Work

- Public bulk and span-based APIs are deferred. Candidate workloads include point and bounds
  transforms and frustum culling. Require representative workload data and a stable public use case
  before choosing an API.
- Handwritten SIMD paths are deferred until after renderer and collision correctness. Profiling
  must justify each x86-64 or ARM64 path, and public types must remain ISA-independent.
- Estimate operations and other accuracy-reducing shortcuts are deferred. Add only
  explicitly named variants when measurements show that accurate default operations miss a
  concrete performance budget.
- Explicitly unchecked variants are deferred. Add one only for a measured hot path
  whose preconditions can be maintained and documented by its caller.

### Geometry And Collision

- Oriented boxes, capsules, and segments are deferred until a consumer names a query that requires
  the primitive and its edge semantics.
- Swept tests, closest-distance queries, and bounds-overlap pairs are deferred until a concrete
  workflow specifies operands and required result detail.
- Every collision pair other than the four rows listed in `Boundary.md` and ADR 0009 is deferred.
  Do not infer a pairwise query matrix merely because its operand types exist.
- Other 2D or 3D geometry queries are deferred until a consumer specifies their inputs, outputs,
  degeneracy behavior, numerical contract, and milestone.

### Numeric Utilities

- Random and noise generation and curve APIs are deferred until a consumer defines distribution,
  reproducibility, curve-family, and precision requirements.

### Interchange And Serialization

- Math-value serialization formats are deferred until a persistence or interchange consumer
  defines versioning, endianness, precision, and compatibility requirements.
- Long-term binary ABI guarantees beyond the exact CPU layouts selected for math values are
  deferred. Revisit them only with a supported binary-plugin or cross-module ABI requirement.

## Outside The `ponder_math` Boundary

The following are not deferred candidates for this library. They belong to other subsystems even
if future work requires them:

- Chemistry-specific operations and semantic types.
- Renderer resources, Vulkan or other backend types, and GPU buffer-packing representations.
  Renderer code owns packing types and conversion from the stable CPU math values.
- UI concepts, project persistence, workflow orchestration, and stateful application controllers.
- DirectXMath or DirectXCollision source, public API compatibility, and library dependencies.
  They remain Windows-only unit-test references.
- A global coordinate-convention switch or silently passive transform mode. The canonical
  conventions in ADR 0009 remain explicit and stable.
- Cross-platform bit-identical floating-point behavior. The math contract promises documented
  tolerance stability, not identical results across compilers and architectures.
