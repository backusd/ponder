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