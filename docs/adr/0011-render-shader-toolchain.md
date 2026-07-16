# ADR 0011: Render Shader Toolchain

## Status

Accepted.

## Related Decisions

- [ADR 0008: Vulkan Renderer Backend](0008-vulkan-renderer-backend.md)
- [ADR 0010: Project-Owned UI Rendering](0010-project-owned-ui-rendering.md)

## Context

The rectangle UI foundation needs renderer-owned shaders, but the shader decision must be wider
than the first rectangle. Future 2D, 3D, and visualization shaders should not each invent their own
source language, validation path, reflection format, embedding strategy, or stale-artifact checks.

The current runtime renderer target is Vulkan 1.2. A future macOS renderer is expected to use native
Metal, so source semantics should avoid becoming Vulkan-only even though the first generated runtime
artifact is SPIR-V. Runtime shader compilation, working-directory shader lookup, checked-in opaque
binaries, and UI-owned shader compilation are intentionally excluded.

## Decision

`ponder_render` owns shader source, generated backend artifacts, reflection metadata, validation,
embedding, and provenance. `ponder_ui` never compiles shaders and never selects a native shader
module.

Author portable shader source in HLSL 2021 for now. Generate Vulkan artifacts with a
DirectXShaderCompiler `dxc` executable built with SPIR-V CodeGen enabled. Validate generated SPIR-V
with `spirv-val` from SPIRV-Tools targeting Vulkan 1.2. Generate interface/reflection metadata with
SPIRV-Cross JSON reflection. This gives the project one source-language path for Vulkan now and a
credible future route to Metal artifacts through SPIR-V/MSL translation or a later explicitly
recorded shader-source evolution.

Shader artifacts are generated only in the build tree. The generator records source hash, tool paths,
tool versions, command-line options, entry point, profile, target environment, schema fingerprint,
SPIR-V hash, and reflection output. Generated C++ embeds SPIR-V words into non-installed render
implementation targets. Build-time checks compare the manifest, generated header, reflection JSON,
and actual SPIR-V hash so stale metadata fails at the shader target boundary.

CMake must not discover shader tools during UI-core-only or render-disabled configure. Tool discovery
happens when an explicit render shader target is built or verified. The cache variables
`PONDER_RENDER_SHADER_DXC`, `PONDER_RENDER_SHADER_SPIRV_VAL`, and
`PONDER_RENDER_SHADER_SPIRV_CROSS` may be set to pin exact executable paths; otherwise the generator searches `%VULKAN_SDK%/Bin`,
then installed `C:/VulkanSDK/*/Bin` directories newest first, and then the build environment `PATH`
at build time.

## Consequences

A normal render-disabled/UI-core configure remains independent of shader tools. A shader-enabled
build fails early and explicitly if the required toolchain is absent or if the discovered `dxc` lacks
SPIR-V CodeGen support.

Windows SDK `dxc` builds are not sufficient unless they were built with SPIR-V CodeGen. The expected
local development setup is a Vulkan SDK or equivalent pinned tool bundle that provides SPIR-V-capable
DXC, SPIRV-Tools, and SPIRV-Cross.

Generated shader C++ is mechanically isolated from the source tree and from public install/export
surfaces. Handwritten project code keeps the normal C++23 and warnings-as-errors policy; generated
blob targets explicitly avoid project PCH coupling.

Rejected alternatives:

- GLSL-first source: closest to Vulkan but weaker for the future Metal path and more likely to make
  shader semantics Vulkan-shaped too early.
- Runtime shader compilation or runtime shader files: violates deterministic startup, packaging, and
  stale-artifact requirements.
- Checked-in SPIR-V blobs: hides provenance and lets source/tool/schema drift go stale.
- A UI-local shader toolchain: violates the render-owned shader boundary.

## References

- [Render Boundary](../../libs/render/docs/Boundary.md)
- [UI Boundary](../../libs/ui/docs/Boundary.md)
- [UI open questions](../../libs/ui/docs/open-questions.txt)
- [UI roadmap](../../libs/ui/docs/roadmap.txt)