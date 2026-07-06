# Deferred CI, Packaging, And Release Requirements

Status: deferred planning note for project-structure implementation.

This document records CI, packaging, dependency-cache, vulnerability-scan, and
release requirements that are intentionally not implemented in the initial
scaffold.

## Current Position

CI is intentionally deferred.

Do not add GitHub Actions, Azure Pipelines, packaging jobs, release jobs,
artifact publishing, or branch-protection assumptions during the project
structure scaffold.

The repository should still be shaped so those systems can be added later
without rewriting the build, test, dependency, or documentation model.

## Future CI Requirements

Future CI should build and test the project on:

- Windows.
- Linux.
- macOS.

The first CI implementation should start from the existing CMake presets and
developer workflow scripts instead of creating a second build system.

The future matrix should cover at least:

- Configure.
- Build.
- Test.
- Format check.
- clang-tidy, once the local checks are stable enough.
- Sanitizers on Clang/GCC where supported.

Formatting and clang-tidy should tighten over time. Early CI should avoid noisy
checks that slow down useful development, but it should make real regressions
visible.

## Dependency Caches

Future CI may use dependency caches to keep builds fast.

Cache keys must be specific enough that stale dependencies do not hide problems.
Good cache inputs include:

- Operating system.
- Compiler family and version.
- CMake preset.
- Dependency submodule SHAs.
- Relevant CMake dependency options.

CI must also have a way to run without a warm cache. A clean-cache build should
remain part of the maintenance story.

## Vulnerability And License Scanning

Dependency vulnerability scanning is deferred with CI.

When added, scanning should understand that dependencies are Git submodules under
`third_party/`. It should complement, not replace, the dependency and license
policy in `third_party/README.md` and `third_party/licenses.md`.

License checks should preserve the current policy:

- Permissive licenses are allowed by default.
- MPL and EPL require case-by-case review.
- GPL and AGPL must not be linked into the core platform.
- GPL tools may only be supported as separate external user-installed engines.

## Packaging And Install Exports

Do not produce release artifacts during the scaffold.

CMake should remain target-oriented and structured for future packaging. In
particular:

- Project libraries should use clean implementation targets and `ponder::*`
  aliases.
- Public headers should remain under `include/ponder/{library}/`.
- Generated files should stay in the build tree unless intentionally checked in
  and documented.
- Install/export rules may be prepared lightly, but polished package exports can
  wait until APIs stabilize.

Future packaging work should decide:

- Source archive policy.
- Binary package formats per platform.
- CMake package export stability expectations.
- Runtime dependency bundling.
- How desktop app packaging differs from SDK or library packaging.

## Release Signing

Signed releases are a future requirement.

When releases begin:

- Start with signed git tags.
- Record the expected release-signing identity and verification instructions.
- Add Windows code signing before broad public Windows binary distribution.
- Add macOS code signing and notarization before broad public macOS binary
  distribution.

Release signing should be documented before users are asked to trust downloaded
binary artifacts.

## Reproducibility

The project should aim for reproducible builds, but not at the cost of more
important design goals in the initial scaffold.

Useful early steps are:

- Pin dependencies.
- Keep generated files out of the source tree unless documented.
- Keep configure and build steps auditable.
- Avoid executing plugins, scripts, or external tools during configure/build
  unless there is a clear reason.

Perfect reproducibility can be revisited when packaging and release automation
are active work.

## Revisit Triggers

Revisit this document before:

- Opening the project to broad public contribution.
- Adding the first CI workflow.
- Publishing binary artifacts.
- Creating installable SDK packages.
- Adding dependency vulnerability scanning.
- Requiring signed release artifacts.
