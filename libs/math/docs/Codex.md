# Codex Guidance: Math

Read the root `AGENTS.md` before this file.

## Scope

Work here on reusable numeric and geometric primitives only. The first implementation is a
fixed-size, concrete `float` math library for 3D rendering support.

Use `Boundary.md`, `roadmap.txt`, `deferred.md`, and ADR 0009 as the durable contract. Do not rely
on retired question files for implementation choices.

## Local Rules

- Keep public headers under `include/ponder/math/`.
- Use the `pond::math` namespace.
- Avoid chemistry, rendering, UI, project, SDL, Vulkan, DirectX, and instruction-set-specific public
  dependencies in production math code.
- Prefer deterministic APIs with explicit units, coordinate conventions, and failure behavior.
- Keep DirectXMath and DirectXCollision isolated to Windows-only reference tests.
- Preserve the canonical contract: right-handed, Y up, -Z forward, active column-vector transforms,
  column-major matrices, NDC +Y up, NDC depth `[0, 1]`, and explicit forward/reverse-Z projection
  selection.
- Treat CPU layout promises as math-value promises only; renderer-owned packing types handle GPU
  layout rules.

## Fast Intermediate Validation

For ordinary implementation tasks, validate the behavior just introduced without running the final
matrix every time:

- Documentation-only changes: run no build unless examples, public headers, CMake, or generated
  documentation inputs changed.
- CMake, source-list, or target changes: reconfigure the active Debug build before compiling the
  affected math targets.
- Public API or value changes: build `ponder_math_header_tests` and `ponder_math_tests`, then run
  the complete `ponder_math_tests` executable.
- Windows reference-test changes: build and run `ponder_math_reference_tests` only when the task
  adds or changes DirectXMath or DirectXCollision comparison coverage, or when a milestone task
  asks for it explicitly.
- Milestone tasks such as MATH-020 and MATH-022 may add focused integration or renderer-shaped tests
  as described in the roadmap.
- Defer duplicate configurations, Release, sanitizers, clang-tidy, repository-wide formatting,
  whitespace checks, and the portability matrix until the final validation gates.

## Final Validation

At MATH-024 and MATH-025, validate the completed library substantially more aggressively:

- Configure clean Debug and Release builds for the primary host.
- Build the library, PCH-free public-header tests, unit tests, documentation examples,
  renderer-shaped tests, and Windows reference tests where applicable.
- Run every applicable math test executable or CTest label in Debug and Release.
- Run the supported sanitizer preset, currently Linux Clang ASAN/UBSAN, when that host is available.
- Run clang-format in check mode, clang-tidy over math production and test translation units, and
  `git diff HEAD --check` plus equivalent whitespace checks for intended untracked text files.
- Audit public dependencies, public layout assertions, DirectX isolation, deferred feature absence,
  generated artifacts, logs, and local environment files.
- Run the cross-platform matrix from MATH-025 and record exact OS, compiler, architecture,
  configuration, commands, results, and justified skips.
