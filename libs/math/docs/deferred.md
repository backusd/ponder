# Deferred Math Features

## Cross-Platform Portability Matrix

This validation remains deferred until dedicated host, compiler, architecture, and CI capacity is
available. Record unverified rows honestly; primary-host completion does not imply portability
evidence.

Collect actual evidence for supported hosts, compilers, and architectures.

Work:

- Configure, build, and run public-header, documentation-example, unit, and renderer-shaped tests
  on:
  - Windows with MSVC Debug/Release and clang-cl Debug where supported.
  - Linux with Clang Debug/Release, GCC Debug, and Clang ASAN/UBSAN.
  - macOS with AppleClang Debug/Release.
- Run Windows DirectXMath and DirectXCollision reference tests in their isolated target.
- Exercise x86-64 and ARM64 on supported available hosts without changing public types or semantics
  for either instruction set.
- Use matching configure, build, and test presets where defined. When no test preset exists, build
  first and run CTest directly against the configured build tree.
- Record unrun host/compiler/architecture rows as unverified rather than passed.
- Add CI jobs when CI exists; until then maintain dated manual evidence with exact OS, compiler,
  architecture, configuration, commands, and results.
- File scoped follow-up tasks for real numerical, compiler, or architecture differences rather than
  weakening the documented contract or adding undocumented conditionals.
- Reconfirm that DirectX headers never enter production or non-Windows test compilation.

Done when:

- Windows, Linux, and macOS rows contain actual successful configure, build, public-header, unit,
  and renderer-shaped results for the required compiler configurations.
- Applicable x86-64 and ARM64 rows contain actual evidence or remain explicitly unverified.
- The Windows reference suite passes without changing the independent project test expectations.
- Remaining skips, unavailable rows, and tolerated cross-host numerical differences are explicit,
  justified, and linked to follow-up work.

This register records capabilities excluded from the initial math implementation.
It is derived from `Boundary.md` and ADR 0009. A deferred candidate should enter the public
contract only when a concrete consumer establishes its semantics, constraints, and verification
requirements.

## Included In The Initial Contract

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
  unit-rotation invariant is part of the initial contract. A future consumer needing such
  arithmetic should motivate a separate type rather than weakening `Quaternion`.

### Transforms, Rotations, And Conventions

- Dedicated TRS and compact affine types are deferred. Before adding either, define its invariants
  and its behavior for non-uniform scale, negative scale, and shear.
- Transforming bounds, planes, frustums, and batches of points is deferred. Add these only after a
  renderer or importer identifies the required operand set, normalization behavior, and performance
  shape.
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
- Convenience screen-ray construction helpers are deferred. The initial API exposes the primitives
  needed to build a ray from `Unproject`; a future helper should wait for an input/picking consumer
  to define viewport, depth, and miss-policy details.

### Projection, Camera, And Viewport Variants

- Off-center perspective, asymmetric frusta, oblique clip planes, jittered projections, and
  infinite orthographic projection are deferred until a renderer or camera consumer defines exact
  clip-space and failure semantics.
- Backend-specific projection or viewport adaptation helpers, including Vulkan Y inversion, remain
  outside math. Renderer code owns those policies.
- Pixel-center, texel-center, and half-pixel viewport conventions are deferred. The initial
  viewport contract maps continuous NDC edges directly to viewport edges.

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
- Direct frustum construction from explicit planes, frustum transformation, and frustum/frustum
  queries are deferred until a caller requires those exact operations.
- Plane/plane, plane/ray, plane/triangle, triangle area, triangle normal, and triangle adjacency
  helpers are deferred until a geometry consumer defines their degeneracy and precision contracts.
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
