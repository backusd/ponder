# Project Structure Decisions

Status: implemented decision record for the initial project-structure phase.

Source: answers provided for `docs/project-structure/open-questions.txt` on
2026-07-05.

The decisions in this file drove the initial repository and CMake scaffold. They
remain the baseline for project-structure work unless superseded by a later
roadmap update or ADR.

`docs/project-structure/open-questions.txt` is now historical context only.
Codex should use this file, the project-structure roadmap, the local
verification record, and relevant ADRs when touching project-structure code or
documentation.

## How To Use This File

- Treat these decisions as settled for the completed project-structure phase.
- If implementation reveals that a decision is technically harmful, pause and
  propose a roadmap or ADR update instead of silently changing direction.
- Prefer updating this file or adding an ADR when a project-structure decision
  changes.
- Keep the high-level roadmap flexible, but record structural changes when they
  happen.

## Durable ADRs

Some project-structure decisions are durable enough to have standalone ADRs:

- [ADR 0001: Dependency Management](../adr/0001-dependency-management.md)
- [ADR 0002: C++23 Language Standard](../adr/0002-cpp23-language-standard.md)
- [ADR 0003: Hybrid Plugin ABI Boundary](../adr/0003-hybrid-plugin-abi-boundary.md)
- [ADR 0004: Static Library First](../adr/0004-static-library-first.md)
- [ADR 0005: Native Project Extension](../adr/0005-native-project-extension.md)

## Implementation Status

The PS-000 through PS-018 project-structure tasks are complete.

Local verification is recorded in
[local-verification.md](local-verification.md). Windows MSVC debug and release
build/test verification passed on 2026-07-05.

No unresolved project-structure decisions block the next roadmap phase. Remaining
known gaps are follow-up work outside the initial scaffold:

- Install or document local `clang-format` and `clang-tidy` versions.
- Rerun format and clang-tidy checks once those tools are available.
- Run Linux/macOS and Clang/GCC sanitizer presets on suitable hosts.
- Define the concrete `.ponder` project format in the project-format phase.
- Revisit CI only after local workflows are stable enough to automate.

## Project Identity And Governance

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-ID-001 | Canonical project name is `ponder`. | Use `ponder` for repository-facing names, CMake targets, executables, include paths, project options, docs, and public references. |
| Q-ID-002 | C++ namespace is `pond::`. This is the only intended use of shortened `pond`. | Source namespaces use `pond`; everything else uses full `ponder`. |
| Q-ID-003 | Public includes use `#include <ponder/{library}/{file}>`, such as `#include <ponder/core/Result.hpp>`. | Public headers live under `include/ponder/{library}/`. |
| Q-ID-004 | Initial executables are `ponder-desktop`, `ponder-project-inspect`, and unit-test executables needed for libraries. | Executable names use full `ponder` naming. |
| Q-ID-005 | Native project extension is `.ponder`. | Project-format docs, examples, fixtures, and UI text should use `.ponder`. |
| Q-ID-006 | License is Apache-2.0. | Add Apache-2.0 `LICENSE` during repository metadata setup. |
| Q-ID-007 | Require a Developer Certificate of Origin. | Document DCO expectations in `CONTRIBUTING.md`. |
| Q-ID-008 | Start as a monorepo. | Keep platform libraries, desktop app, built-in plugins, tools, tests, examples, third-party policy, and docs in this repo unless a later decision changes that. |
| Q-ID-009 | Initially support Windows, Linux, and macOS. | CMake, scripts, dependencies, and source layout should account for all three platforms. |
| Q-ID-010 | Codex may choose practical minimum OS versions. Slightly older OS versions are nice, but not if they compromise implementation quality. | Prefer dropping an old OS version over weakening architecture or build quality. |
| Q-ID-011 | Optimize initially for developer source builds. | Local CMake presets, scripts, and build docs matter more than installers for the first phase. |
| Q-ID-012 | Future web, remote compute, and headless-worker use cases are architectural requirements from day one. | Reusable libraries must not depend on desktop UI callbacks or app-only state. |

## Build System

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-BLD-001 | Use a modern CMake version for presets and clean dependency integration. | Roadmap recommends CMake 3.28 minimum unless implementation finds a concrete reason to raise it. |
| Q-BLD-002 | Use C++23 from day one. | Compiler versions and dependencies must support C++23 cleanly. |
| Q-BLD-003 | Support MSVC and clang-cl on Windows, Clang and GCC on Linux, and AppleClang on macOS. | Presets should reflect these compiler families. |
| Q-BLD-004 | Codex may choose compiler minimums, prioritizing modern best practices over old compiler support. | Roadmap recommends MSVC 19.38 / VS 2022 17.8, Clang/clang-cl 17, GCC 13, and AppleClang 15 as initial minimums. |
| Q-BLD-005 | Everything should be a static library initially. | Use static project libraries until plugin or distribution requirements justify changing linkage. |
| Q-BLD-006 | Use a hybrid plugin model: stable C-style binary boundary for real external plugins, with C++ SDK wrappers on top. Built-in plugins can start simpler. | Avoid exposing unstable C++ ABI assumptions in external plugin boundaries. |
| Q-BLD-007 | Implementation targets use names like `ponder_core`; aliases use names like `ponder::core`. | Do not abbreviate targets to `pond_*`. |
| Q-BLD-008 | Project options always use the `PONDER_*` prefix. | All CMake options should follow this prefix. |
| Q-BLD-009 | Warnings-as-errors are always enabled for every project build. | Apply to project code; avoid forcing project warning policy onto third-party submodules. |
| Q-BLD-010 | RTTI is enabled intentionally. | Editor-like systems and plugins may rely on RTTI when useful. |
| Q-BLD-011 | Exceptions are allowed only for truly exceptional, usually unrecoverable cases. Project-thrown exceptions use `PonderException`. | Recoverable errors should prefer `std::expected` or an expected-like project wrapper. |
| Q-BLD-012 | Use precompiled headers with modern best practices. | PCH should be target-scoped, private to implementation targets, and not required by public APIs. |
| Q-BLD-013 | Unity/jumbo builds are not supported yet. | Do not add unity build assumptions to the scaffold. |
| Q-BLD-014 | Do not support C++ modules yet and do not assume future support. | Keep ordinary header/source structure. |
| Q-BLD-015 | Prepare for installable CMake package exports now, polish later after APIs stabilize. | Write clean target-oriented CMake without over-investing in package exports during the scaffold. |
| Q-BLD-016 | Generated files stay in the build tree unless intentionally checked in and documented. | Avoid generated source churn in the repo. |
| Q-BLD-017 | Explicitly reject in-source builds. | Root CMake should fail clearly for in-source configuration. |
| Q-BLD-018 | Use descriptive preset names such as `windows-msvc-debug`, `windows-msvc-release`, `linux-clang-debug`, `linux-gcc-debug`, and `linux-clang-asan`. | Presets should favor clarity over brevity. |
| Q-BLD-019 | Support developer-local overrides. | Ignore `CMakeUserPresets.json`. |
## Dependencies

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-DEP-001 | Use Git submodules plus `add_subdirectory`. | Add third-party dependencies under `third_party/` and wire them through CMake. |
| Q-DEP-002 | Not relevant because vcpkg is not selected. | No vcpkg registry decision needed. |
| Q-DEP-003 | Not relevant because vcpkg is not selected. | No vcpkg baseline decision needed. |
| Q-DEP-004 | Not relevant because vcpkg is not selected. | No vcpkg triplet decision needed. |
| Q-DEP-005 | Third-party patches should be isolated, documented, and easy to reapply. | Use `third_party/patches/` or equivalent patch documentation. |
| Q-DEP-006 | First allowed dependencies are spdlog, GoogleTest, nlohmann/json, SDL3, and Dear ImGui. | Do not add Python, cloud, renderer-backend, or chemistry-engine dependencies during the scaffold. |
| Q-DEP-007 | Use nlohmann/json. | Project metadata and early structured files should use this dependency. |
| Q-DEP-008 | Use GoogleTest. | Unit-test targets should use GoogleTest. |
| Q-DEP-009 | Allow permissive licenses by default: MIT, BSD, Apache-2.0, ISC, Zlib, and Boost. MPL/EPL are case-by-case. Do not link GPL/AGPL into core. GPL tools may be external user-installed engines. | Document the license policy before adding third-party code. |
| Q-DEP-010 | Track dependency license information from the beginning. | Start with a simple dependency/license note and automate later if needed. |
| Q-DEP-011 | Offline and air-gapped builds are best-effort initially. | Pin dependencies and support pre-populated submodules, but do not require perfect offline builds in the first scaffold. |
| Q-DEP-012 | Use dependency caches in CI later, keyed carefully. | CI is deferred, but future CI docs should include dependency-cache expectations. |
| Q-DEP-013 | Avoid large assets at first. | Keep examples and fixtures small unless a later decision adds large asset handling. |

## Repository Layout

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-LAY-001 | `high-level-roadmap.txt` is guidance, not a rigid requirement. | Use best judgment based on this decision record. |
| Q-LAY-002 | Library directory names always use underscores. | Use names such as `scientific_data` and `plugin_sdk`. |
| Q-LAY-003 | Use `scientific_data`. | Library folder, target, include path, and tests should use `scientific_data`. |
| Q-LAY-004 | Every library has public headers under `include/<project>/<library>/` and implementation files under `src/`. | For this project, use `include/ponder/{library}/`. |
| Q-LAY-005 | Private headers live in `src/`. | Do not create a separate public-looking private include tree. |
| Q-LAY-006 | Use a central `tests` directory. | Tests live under `tests/`, organized by library and type. |
| Q-LAY-007 | Each library has its own `docs/` directory. | Add boundary and design docs under each library as it is scaffolded. |
| Q-LAY-008 | Built-in plugins live under `plugins/builtin/`. | Do not bury built-in plugins inside the desktop app. |
| Q-LAY-009 | Developer tools live under `tools/` when they test or inspect reusable systems. | `ponder-project-inspect` belongs under `tools/project-inspect/`. |
| Q-LAY-010 | Examples live under `examples/` from the beginning. | Add an examples skeleton early. |
| Q-LAY-011 | Sample project files live under both `examples/` and `tests/fixtures/` as appropriate. | User-facing samples and test fixtures can overlap, but should be intentional. |
| Q-LAY-012 | Ignore build trees, CMake user presets, IDE files, generated binaries, dependency caches, logs, and local environment files. | Codex should use best judgment when creating `.gitignore`. |

## Code Style And Engineering Conventions

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-STYLE-001 | Use an LLVM-derived custom style with 4-space indentation and a 100-column limit. | `.clang-format` should encode this. |
| Q-STYLE-002 | Types use PascalCase, functions use PascalCase, variables use camelCase, private members use `m_*`, constants use `kPascalCase`, CMake implementation targets use `project_library`, aliases use `project::library`, and braces use Allman style. | Apply consistently in scaffolded code and CMake. |
| Q-STYLE-003 | Filenames use PascalCase. | Prefer files such as `Result.hpp`, `PonderException.hpp`, and `Main.cpp`. |
| Q-STYLE-004 | Line length is 100 columns. | Configure formatting and review docs/code with this target. |
| Q-STYLE-005 | Start clang-tidy useful but not overwhelming, then tighten over time. | Initial `.clang-tidy` should catch high-signal issues without blocking early work on noise. |
| Q-STYLE-006 | Do not enforce include-what-you-use at first. | Keep headers clean manually, use clang-tidy where helpful, and add IWYU later as advisory. |
| Q-STYLE-007 | Exceptions are for truly exceptional, usually unrecoverable cases. Otherwise prefer `std::expected`. | Core should provide clear exception and result conventions. |
| Q-STYLE-008 | Build custom assertion macros that debug break in debug builds and compile out in release builds. | Add assertion conventions in `ponder_core`. |
| Q-STYLE-009 | Logging supports trace, debug, info, warning, error, and fatal. Logging macros look like `LOG_*`, support `std::format`-style formatting, and capture time, file name, and source location. | Add logging conventions in `ponder_core`, backed by spdlog when dependencies are ready. |
| Q-STYLE-010 | Use selective comments. Comment non-obvious logic, use Doxygen-style comments for important public APIs once stable, and use design docs/ADRs for architectural decisions. | Avoid noisy comments and avoid relying on comments for architecture that belongs in docs. |
| Q-STYLE-011 | Public API headers should avoid heavy third-party includes. | Prefer forward declarations, PIMPL, or narrow wrappers for public APIs. |
| Q-STYLE-012 | Use PIMPL or other ABI-stability patterns in public SDK-facing types when appropriate. | Especially relevant for plugin SDK and public extension APIs. |
## Testing

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-TEST-001 | Use GoogleTest. | Test dependencies and targets should use GoogleTest. |
| Q-TEST-002 | Tests are enabled by default. | Developer presets and root CMake options should build tests unless explicitly disabled. |
| Q-TEST-003 | Every initial library gets at least one smoke test. | Scaffold one minimal test per library target. |
| Q-TEST-004 | Start with one test executable per library, then split as libraries grow. | Use names such as `ponder_core_tests`. |
| Q-TEST-005 | Enable AddressSanitizer and UndefinedBehaviorSanitizer on Clang/GCC; skip sanitizers on Windows for now. | Add sanitizer presets for Clang/GCC only. |
| Q-TEST-006 | Defer code coverage configuration. | Do not add coverage tooling in the scaffold. |
| Q-TEST-007 | Defer benchmark framework setup until there is meaningful code to measure. | Do not add benchmarks in the scaffold. |
| Q-TEST-008 | Golden files and fixtures should be small, readable where possible, and intentionally versioned. | Avoid large or opaque fixture data early. |

## CI And Developer Workflow

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-CI-001 | Defer CI right now. | Do not add CI workflows in the scaffold. |
| Q-CI-002 | Defer CI platform matrix right now. | Document future CI expectations instead. |
| Q-CI-003 | Defer CI formatting checks right now. | Local formatting scripts can still be added. |
| Q-CI-004 | Defer CI clang-tidy right now. | Local clang-tidy config can still be added. |
| Q-CI-005 | Defer CI dependency caching right now. | Document future cache expectations. |
| Q-CI-006 | Defer CI merge requirements right now. | No branch protection assumptions in the scaffold. |
| Q-CI-007 | Do not produce release artifacts now, but structure CMake so packaging can be added later. | Keep target/export design clean. |
| Q-CI-008 | Scripts should be thin wrappers, not a second build system. | Scripts should call CMake, CTest, clang-format, or clang-tidy directly. |

## Documentation

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-DOC-001 | Docs should be written as `.md`. | New docs should be Markdown. Existing `.txt` planning files may remain until deliberately converted. |
| Q-DOC-002 | Use lightweight ADRs for durable choices such as dependency management, plugin ABI, project format, C++ standard, and renderer backend. | Add `docs/adr/` and seed it with major decisions when appropriate. |
| Q-DOC-003 | Add `CONTRIBUTING.md`, `SECURITY.md`, and `CODE_OF_CONDUCT.md` before going public. Defer heavier governance docs until there are real contributors. | Repository metadata task should add or prepare these files. |
| Q-DOC-004 | Every major library should have a boundary document. | Add `libs/{library}/docs/Boundary.md` when scaffolding libraries. |
| Q-DOC-005 | Defer generated API documentation. | Do not add Doxygen/Sphinx tooling in the scaffold. |
| Q-DOC-006 | Document dependency and license policy before adding third-party libraries. | Add third-party policy docs before or with submodules. |

## Security, Supply Chain, And Release Posture

| ID | Decision | Implementation Consequence |
| --- | --- | --- |
| Q-SEC-001 | Defer vulnerability scanning with CI. | Record as future CI work. |
| Q-SEC-002 | Signed releases are a future requirement. | Start with signed git tags when releases begin; add Windows/macOS code signing before broad binary distribution. |
| Q-SEC-003 | Avoid executing plugins, scripts, or external tools during configure/build unless there is a clear reason. | Keep configure/build steps auditable and predictable. |
| Q-SEC-004 | Aim for reproducible builds, but not at the cost of more important project goals. | Prefer pinning and clean generated-file policy without over-optimizing early. |
| Q-SEC-005 | If generated code is checked in, document the generator version and regeneration command. | Generated checked-in files require regeneration docs. |

## Codex Support Follow-Up

These were not original open-question IDs, but they should guide upcoming project-structure tasks.

### Two-Document Lookup Model

Decision: each Codex task should normally need only two documentation files:

1. The root `AGENTS.md`, containing truly global rules for the entire codebase.
2. The subsystem-local `<library|executable path>/docs/Codex.md`, containing guidance for the specific library, executable, tool, or plugin being changed.

Implementation consequence: do not add a separate `docs/codex/README.md`, repo-local skill-pack layer, or broad routing tree unless this decision is explicitly revisited.

### Root AGENTS.md

Recommendation: add a root `AGENTS.md` during repository metadata setup.

It should be short, global, and practical. It should include only rules that apply across the whole repository, such as:

- The project is `ponder`, but C++ namespaces use `pond::`.
- New source uses C++23.
- Public includes use `include/ponder/{library}/`.
- CMake targets use `ponder_*` with `ponder::*` aliases.
- Project CMake options use `PONDER_*`.
- Warnings-as-errors are required for project code.
- Dependencies are Git submodules plus `add_subdirectory`.
- Tests use GoogleTest and are enabled by default.
- New docs are Markdown.
- For subsystem work, read the nearest `<library|executable path>/docs/Codex.md` in addition to `AGENTS.md`.

`AGENTS.md` must not contain guidance specific to any one library or executable.

### Subsystem Codex.md Files

Each major library, executable, tool, and plugin should eventually have a local guidance file at:

```text
<subsystem path>/docs/Codex.md
```


These files should include only local guidance:

- What this subsystem owns.
- What it must not depend on.
- Which local docs and tests matter.
- Local invariants and common mistakes to avoid.
- How to verify changes in that subsystem.

Do not duplicate global rules in every local file. Link back to `AGENTS.md` if needed.
