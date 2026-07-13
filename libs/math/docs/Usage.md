# Math Usage Guide

This guide documents the renderer-ready `ponder_math` contract. It is the public usage companion
to `Boundary.md`; implementation tests should not be required to infer coordinate conventions,
failure behavior, or layout assumptions.

## Target And Includes

Use the CMake alias target:

```cmake
target_link_libraries(my_target PRIVATE ponder::math)
```

Public includes use `ponder/math/...`:

```cpp
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Vector3.hpp>
```

`ponder::math` publicly inherits `ponder::core` because checked math operations return
`pond::core::Result<T>`. Production math headers do not expose renderer, Vulkan, platform, UI,
chemistry, DirectX, or instruction-set-specific dependencies.

## Coordinates And Algebra

The canonical convention is:

- right-handed world space;
- Y up;
- -Z forward;
- active transforms applied to column vectors;
- NDC +Y up;
- NDC depth `[0, 1]`.

Matrices multiply vectors on the left side of the expression:

```cpp
pond::math::Vector3 worldPoint = pond::math::TransformPoint(world, localPoint);
```

Composition reads right-to-left. For `B * A`, `A` is applied first and `B` second:

```cpp
const pond::math::Matrix4x4 world =
    pond::math::Matrix4x4::Translation({1.0F, 2.0F, 3.0F}) *
    pond::math::Matrix4x4::Scale({2.0F, 2.0F, 2.0F});
```

Quaternion multiplication follows the same application order. If `combined = b * a`, then `a`
rotates first and `b` rotates second.

Passive basis changes, importer/exporter convention conversion, and backend-specific coordinate
adaptation are intentionally separate concepts. Add explicitly named conversion helpers when a
concrete consumer needs them; do not reinterpret the active transform API.

## CPU Layout And GPU Packing

The selected CPU value layouts are public math-value contracts:

| Type | Layout contract |
| --- | --- |
| `Vector2` | standard-layout, trivially copyable, 8 bytes, natural `float` alignment |
| `Vector3` | standard-layout, trivially copyable, 12 bytes, natural `float` alignment |
| `Vector4` | standard-layout, trivially copyable, 16 bytes, natural `float` alignment |
| `Matrix3x3` | standard-layout, trivially copyable, 36 bytes, column-major scalar order |
| `Matrix4x4` | standard-layout, trivially copyable, 64 bytes, column-major scalar order |
| `Quaternion` | standard-layout, trivially copyable, 16 bytes, unit-rotation invariant |
| `Ray` | standard-layout, trivially copyable, normalized direction |
| `Plane` | standard-layout, trivially copyable, normalized normal |
| `Sphere` | standard-layout, trivially copyable, non-negative radius |
| `AxisAlignedBox` | standard-layout, trivially copyable, ordered inclusive bounds |
| `Triangle` | standard-layout, trivially copyable, finite vertices |

Matrix member names are logical row/column names, but storage is contiguous column-major `float`
order. For example, `Matrix4x4::row3Column0` is the fourth scalar in memory.

Vectors expose public named components and checked `At(index)` access. They intentionally do not
provide `operator[]`; caller-controlled invalid indices remain recoverable `Result` errors.

These guarantees do not mean the same values can be copied directly into a vertex buffer, uniform
buffer, storage buffer, push constants, or shader parameter block. Renderer code owns GPU packing
types and conversions. Vulkan viewport flipping and negative-height viewport setup also belong in
renderer code, not in `ponder_math`.

## Scalars, Angles, And Comparisons

The first implementation uses concrete `float` math types. There are no public scalar templates,
`double` families, integer-vector families, or half-precision families.

Project-wide constants, `Tolerance`, scalar `Clamp`, scalar `Lerp`, scalar
`IsNear`, and finite-number helpers are owned by
`<ponder/core/Numbers.hpp>`. Math adds only overloads for its own value types.

Angles are explicit strong values:

```cpp
const pond::math::Radians fovY =
    pond::math::ToRadians(pond::math::Degrees{60.0F});
```

`operator==` is exact component or representation equality. Near comparisons require an explicit
validated tolerance:

```cpp
auto tolerance = pond::core::Tolerance::Create(1.0e-5F, 1.0e-5F);
if (!tolerance.HasValue())
{
    return tolerance.GetError();
}

const bool close = pond::math::IsNear(a, b, tolerance.GetValue());
```

Quaternions have two comparison concepts:

- `IsNear(lhs, rhs, tolerance)` compares stored components.
- `IsSameRotation(lhs, rhs, tolerance)` also treats `q` and `-q` as the same rotation.

`Slerp` requires a finite amount but does not clamp it. Amounts in `[0, 1]` interpolate and
amounts outside that interval extrapolate along the same shortest-path rotation. Exact amounts zero
and one return the exact input endpoints.

`ToQuaternion(Matrix4x4)` inspects and validates only the upper-left 3x3 linear part. Translation
and the matrix bottom row are intentionally ignored; the extracted linear part must still be a
proper rotation.

## Checked Failure Behavior

Recoverable invalid input and mathematically unavailable results return
`pond::core::Result<T>` with stable math error codes from `MathErrorCode`.

Common examples:

- invalid finite domains, such as a negative sphere radius, return `InvalidArgument`;
- non-finite inputs return `NonFiniteInput`;
- zero-length normalization inputs return `DegenerateInput`;
- singular or finite-`float`-unrepresentable matrix inversion returns `SingularMatrix`;
- zero or non-finite homogeneous `w`, or a perspective-division result outside the finite `float`
  range, returns `UndefinedHomogeneousCoordinate`.

Callers must handle or propagate these results. Assertions are reserved for internal invariants,
not caller-reachable invalid inputs.

`PerspectiveDivide` accepts any finite nonzero `w`, including subnormal values, when all three
quotients convert to finite `float` NDC coordinates. It rejects zero or non-finite `w`, non-finite
clip coordinates, and quotients outside the finite `float` result range.

## Points, Vectors, And Normals

Use distinct transform helpers for distinct semantics:

```cpp
const pond::math::Vector3 transformedPoint =
    pond::math::TransformPoint(world, localPoint);
const pond::math::Vector3 transformedVector =
    pond::math::TransformVector(world, localDirection);
auto transformedNormal = pond::math::TransformNormal(world, localNormal);
```

`TransformPoint` uses homogeneous `w = 1` and applies translation. `TransformVector` uses
`w = 0` and ignores translation. `TransformNormal` uses the inverse-transpose of the linear part so
normals remain perpendicular under non-uniform scale and shear.

`TransformNormal` is fully checked: the complete matrix and input normal must be finite, the linear
part must be invertible, and the transformed result must remain finite and representable. It does
not silently normalize the output; normalize explicitly when your caller needs a unit normal.

## Camera, Projection, NDC, And Viewport

View helpers build right-handed matrices looking down -Z:

```cpp
auto view = pond::math::Matrix4x4::LookAt(
    pond::math::Vector3{0.0F, 1.5F, 6.0F},
    pond::math::Vector3{0.0F, 0.0F, 0.0F},
    pond::math::Vector3{0.0F, 1.0F, 0.0F});
```

Every projection call selects an explicit depth direction:

```cpp
auto projection = pond::math::Matrix4x4::Perspective(
    pond::math::ToRadians(pond::math::Degrees{60.0F}),
    16.0F / 9.0F,
    0.1F,
    1000.0F,
    pond::math::ProjectionDepth::ReverseZ);
```

Supported projection forms are:

- finite perspective, forward Z;
- finite perspective, reverse Z;
- infinite-far perspective, forward Z;
- infinite-far perspective, reverse Z;
- finite orthographic, forward Z;
- finite orthographic, reverse Z.

NDC uses +Y up and depth `[0, 1]`.

`Viewport` uses continuous screen coordinates with top-left origin and +Y down. NDC `(-1, +1)`
maps to the top-left viewport edge. NDC `(+1, -1)` maps to the bottom-right viewport edge. No
half-pixel or pixel-center offset is applied. Viewport depth maps linearly into the explicit
ordered viewport depth interval.

`Project` transforms world points to viewport coordinates. `Unproject` transforms viewport
coordinates back to world coordinates:

```cpp
auto screen = pond::math::Project(worldToClip, viewport, worldPoint);
auto world = pond::math::Unproject(worldToClip, viewport, screen.GetValue());
```

`Unproject` validates and inverts `worldToClip` on each call. It fails when that inverse is
singular or cannot be represented as a finite `Matrix4x4`. When unprojecting more than one point
with the same transform, invert once and use the explicitly named cached path:

```cpp
auto clipToWorld = pond::math::Inverse(worldToClip);
auto nearPoint = pond::math::UnprojectFromClipToWorld(
    clipToWorld.GetValue(), viewport, nearScreenPoint);
auto farPoint = pond::math::UnprojectFromClipToWorld(
    clipToWorld.GetValue(), viewport, farScreenPoint);
```

The cached helper accepts clip-to-world, not world-to-clip; the name is intentionally explicit to
make accidental matrix-direction swaps visible at the call site.

## Collision And Picking

Collision values are checked at construction boundaries:

- `Ray::Create` requires finite origin and finite nonzero direction, then normalizes direction.
- `Plane::Create` requires finite nonzero normal and finite offset, then normalizes both.
- `Sphere::Create` accepts zero radius and rejects negative radius.
- `AxisAlignedBox::Create` accepts point boxes and rejects inverted bounds.
- `Triangle::Create` accepts finite zero-area triangles; they simply have no surface hit.
- `Frustum::FromWorldToClip` builds planes from a finite world-to-clip matrix and represents absent
  infinite-far planes explicitly.

The initial query matrix is exactly:

| Query | Result |
| --- | --- |
| `Frustum::Classify(AxisAlignedBox)` | `CollisionClassification` |
| `Frustum::Classify(Sphere)` | `CollisionClassification` |
| `Intersect(Ray, AxisAlignedBox)` | `std::optional<RayBoxHit>` |
| `Intersect(Ray, Triangle)` | `std::optional<RayTriangleHit>` |

No other pairwise collision matrix is implied.

Boundaries are inclusive. Culling conservatively retains objects that touch a boundary or fall
inside the documented floating-point uncertainty region. Ray hit distances are world distances
because ray directions are normalized. `RayTriangleHit` barycentrics are ordered to match
`Triangle::GetVertex0`, `GetVertex1`, and `GetVertex2`.

Frustum classification uses a scale-aware floating-point error bound derived from the magnitudes in
each plane test. A value confidently outside any plane is `Disjoint`; a boundary or value inside
that error bound is retained as `Intersects`. The bound is an algorithmic accuracy policy, not a
caller-selected tolerance or a frozen public multiplier.

Ray/triangle intersection uses scale-aware determinant, barycentric, and distance error bounds.
Near-parallel and near-degenerate configurations are treated as no hit because their result is
ill-conditioned. Boundary barycentrics and distances inside the uncertainty bound are accepted,
clamped to the exact boundary, and renormalized when needed.

The concrete-float picking API supports hits only when every reported distance and barycentric is
finite and representable as `float`. A geometric hit beyond that result domain returns
`std::nullopt`, indistinguishable from a miss; callers that require larger coordinate spans need a
different scalar or result contract. The deterministic higher-precision collision corpus exercises
characteristic scales `1e-3`, `1`, and `1e6`. Those test scales are evidence, not API limits.

## Compiled Documentation Examples

Small examples live in `docs/examples/RendererExamples.cpp` and are compiled by the PCH-free
`ponder_math_examples` target. The target links only `ponder::math`; any use of `pond::core` comes
from the public dependency inherited through `ponder::math`.

The examples cover camera construction, reverse-Z projection, project/unproject, frustum culling,
and ray picking without renderer resources, Vulkan types, application policy, or chemistry
semantics.
