# Third-Party Dependencies

Third-party dependencies live here as Git submodules. They are wired into the
build from `cmake/Dependencies.cmake` using `add_subdirectory`.

Dependencies must be pinned to specific commits. Do not track floating branches
in `.gitmodules`.

## Policy

Dependencies should earn their place in the repository. Before adding one,
document why the project needs it, whether it is runtime or test-only, which
targets use it, and what license obligations it brings.

Allowed by default:

- MIT
- BSD
- Apache-2.0
- ISC
- Zlib
- Boost Software License

Requires case-by-case review:

- MPL
- EPL

Not allowed in core platform libraries:

- GPL
- AGPL

GPL tools may be supported as external user-installed engines only when they are
kept separate from the core platform and are not linked into project libraries.

## Current Dependencies

- spdlog
  - Path: `third_party/spdlog`
  - License: MIT
  - Commit: `8671ca4d492c8ee1cdfd3dd88afb9f88dd268178`
- GoogleTest
  - Path: `third_party/googletest`
  - License: BSD-3-Clause
  - Commit: `973323ed64a05b128418e7eab67016db5ba049df`
- nlohmann/json
  - Path: `third_party/nlohmann_json`
  - License: MIT
  - Commit: `83c87cb9e087d81717c5ac7c50fac694bcb82ad7`
- SDL3
  - Path: `third_party/SDL3`
  - License: Zlib; see bundled third-party notices
  - Commit: `e0b323890be80097a7ec094117c278c8217590b2`
- Dear ImGui
  - Path: `third_party/imgui`
  - License: MIT
  - Commit: `776bf2ab0d61e719e58f2c6d27d109ab5dcf2af1`

## CMake Targets

Use the project aliases from `cmake/Dependencies.cmake` instead of depending on
third-party target names directly:

- `ponder::spdlog`
- `ponder::gtest`
- `ponder::gtest_main`
- `ponder::nlohmann_json`
- `ponder::SDL3`
- `ponder::imgui`

See `third_party/licenses.md` for the dependency/license inventory.

## Updating Dependencies

Dependency updates should be intentional and reviewable:

1. Update one dependency at a time unless a coordinated update is necessary.
2. Move the submodule to a specific commit or tag.
3. Update `third_party/licenses.md` and this README if the version, license,
   purpose, targets, or notes change.
4. Reapply and update any patches documented under `third_party/patches/`.
5. Configure, build, and test at least one supported local preset.
6. Mention any new build steps, generated files, bundled assets, or license
   obligations in the change summary.

Do not add large assets with a dependency update unless a specific project
decision allows them.

## Offline Builds

Offline and air-gapped builds are best-effort during the initial scaffold. A
source checkout with pre-populated submodules should configure without fetching
new dependencies, but perfect offline packaging is deferred until the project
has stable release requirements.

## Patch Policy

Do not make undocumented local edits inside dependency submodules. If a
dependency needs a local change, store a patch file or patch note under
`third_party/patches/<dependency>/` with the reason for the patch and the
command needed to reapply it.