# Dependency License Inventory

This inventory records the third-party dependencies currently vendored under
`third_party/`. It is a project-maintenance aid, not legal advice.

## Policy Summary

Permissive licenses are allowed by default: MIT, BSD, Apache-2.0, ISC, Zlib,
and Boost Software License.

MPL and EPL dependencies require case-by-case review before they are added.

GPL and AGPL code must not be linked into core platform libraries. GPL tools may
be supported only as external user-installed engines kept separate from project
libraries.

Dependencies are pinned as Git submodules. Floating branches are not allowed.

## Inventory

### spdlog

- Purpose: logging backend for future project logging wrappers.
- Usage: runtime dependency, but it should sit behind project logging APIs.
- License: MIT.
- License file: `third_party/spdlog/LICENSE`.
- Pinned commit: `8671ca4d492c8ee1cdfd3dd88afb9f88dd268178`.
- Notes: includes bundled fmt under MIT. Prefer project logging wrappers over
  direct app-wide use.

### moodycamel ConcurrentQueue

- Purpose: lock-free queue for the future async logging backend.
- Usage: private runtime dependency behind core logging implementation details.
- License: BSD-2-Clause or Boost Software License 1.0; zlib applies to the
  embedded `lightweightsemaphore.h` implementation used by the blocking queue.
- License file: `third_party/moodycamel/LICENSE.md`.
- Pinned commit: `1e2def448e43fb3362123ab5ff039c39e1ba5cfd`.
- Notes: keep this dependency out of public core headers. Prefer the non-blocking
  queue for logging unless blocking behavior is explicitly needed.

### GoogleTest

- Purpose: unit and integration test framework.
- Usage: test-only dependency.
- License: BSD-3-Clause.
- License file: `third_party/googletest/LICENSE`.
- Pinned commit: `973323ed64a05b128418e7eab67016db5ba049df`.
- Notes: do not link into shipping project libraries or executables.

### nlohmann/json

- Purpose: JSON parsing and serialization for early project metadata and
  fixtures.
- Usage: runtime dependency behind project file and serialization APIs.
- License: MIT.
- License file: `third_party/nlohmann_json/LICENSE.MIT`.
- Pinned commit: `83c87cb9e087d81717c5ac7c50fac694bcb82ad7`.
- Notes: avoid exposing this header from broad public APIs unless a boundary
  document allows it.

### SDL3

- Purpose: windowing, input, and platform integration for the first desktop
  application.
- Usage: runtime dependency for desktop-facing targets.
- License: Zlib; bundled third-party notices may apply to optional components.
- License file: `third_party/SDL3/LICENSE.txt`.
- Pinned commit: `e0b323890be80097a7ec094117c278c8217590b2`.
- Notes: keep SDL usage out of core libraries. Review bundled notices before
  binary releases.

### Vulkan-Headers

- Purpose: unified Vulkan API header definitions for the render Vulkan backend.
- Usage: private compile-time dependency for `ponder_render` when Vulkan render
  support is enabled.
- Upstream URL: https://github.com/KhronosGroup/Vulkan-Headers.git
- License: Apache-2.0 OR MIT.
- License file: `third_party/Vulkan-Headers/LICENSE.md`; full texts are under
  `third_party/Vulkan-Headers/LICENSES/`.
- Pinned commit: `8d6039a455a7ecc7d2a592ff97f62db4e59b70bf`.
- Local CMake options: `VULKAN_HEADERS_ENABLE_TESTS=OFF`,
  `VULKAN_HEADERS_ENABLE_INSTALL=OFF`, and
  `VULKAN_HEADERS_ENABLE_MODULE=OFF`.
- Notes: compile against the pinned current unified headers while keeping Vulkan
  1.2 as the runtime floor through renderer capability checks.

### Volk

- Purpose: dynamic Vulkan loader and dispatch table support for the render
  Vulkan backend.
- Usage: private runtime dependency for `ponder_render` when Vulkan render
  support is enabled.
- Upstream URL: https://github.com/zeux/volk.git
- License: MIT.
- License file: `third_party/volk/LICENSE.md`.
- Pinned commit: `3b00554371e801bf4f708ec6f0fc78d4274b5720`.
- Local CMake options: `VULKAN_HEADERS_INSTALL_DIR` points at the repository
  `third_party/Vulkan-Headers` checkout, `VOLK_PULL_IN_VULKAN=ON`,
  `VOLK_INSTALL=OFF`, `VOLK_HEADERS_ONLY=OFF`, and `VOLK_NAMESPACE=OFF`.
- Notes: keep Volk private to the render backend. Do not require
  `find_package(Vulkan)`, a Vulkan SDK import library, or a statically linked
  Vulkan loader.

### Vulkan Memory Allocator

- Purpose: Vulkan memory allocation helper for the render Vulkan backend.
- Usage: private runtime dependency behind project-owned render RAII and
  `Result`-based error handling.
- Upstream URL: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
- License: MIT.
- License file: `third_party/VulkanMemoryAllocator/LICENSE.txt`.
- Pinned commit: `3aa921224c154a0d2c43912bc88e1c42ce1f7607`.
- Local CMake options: `VMA_ENABLE_INSTALL=OFF`,
  `VMA_BUILD_DOCUMENTATION=OFF`, and `VMA_BUILD_SAMPLES=OFF`. The project VMA
  target also carries `VMA_VULKAN_VERSION=1002000`,
  `VMA_STATIC_VULKAN_FUNCTIONS=0`, and `VMA_DYNAMIC_VULKAN_FUNCTIONS=1` for the
  Vulkan C API and Volk-owned dynamic dispatch.
- Notes: do not expose VMA declarations or allocator handles through public
  render headers.
### Dear ImGui

- Purpose: immediate-mode UI for early desktop tooling.
- Usage: runtime dependency for desktop/editor-facing targets.
- License: MIT.
- License file: `third_party/imgui/LICENSE.txt`.
- Pinned commit: `776bf2ab0d61e719e58f2c6d27d109ab5dcf2af1`.
- Notes: built through the local `ponder_imgui` target because upstream does
  not provide a root CMake target.

## Review Checklist For New Dependencies

- Confirm the dependency is necessary and cannot be cleanly avoided.
- Confirm the license category is allowed by this policy.
- Record the dependency in this file before or with the build integration.
- Pin the submodule to a specific commit or tag.
- Keep third-party options isolated in `cmake/Dependencies.cmake`.
- Prefer project-owned wrapper APIs at public boundaries.
- Avoid executing dependency-provided scripts during configure or build unless
  there is a documented reason.
- Keep large assets out of the repository unless a specific decision approves
  them.
- Document any generated checked-in files with the generator version and
  regeneration command.