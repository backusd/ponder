# ADR 0001: Dependency Management

## Status

Accepted.

## Context

`ponder` is intended to be a long-lived open source C++ project. The project
needs third-party libraries for logging, testing, JSON, desktop platform
integration, and early UI work, but dependency management should remain
auditable and friendly to source builds.

## Decision

Use Git submodules under `third_party/` and wire dependencies into CMake with
`add_subdirectory` from `cmake/Dependencies.cmake`.

Dependencies must be pinned to specific commits or tags. Floating branch
tracking is not allowed.

Permissive licenses are allowed by default: MIT, BSD, Apache-2.0, ISC, Zlib,
and Boost Software License. MPL and EPL require case-by-case review. GPL and
AGPL must not be linked into core platform libraries. GPL tools may be
supported only as external user-installed engines kept separate from project
libraries.

Offline and air-gapped builds are best-effort during the initial scaffold. A
checkout with pre-populated submodules should configure without fetching new
dependencies, but polished offline packaging is deferred.

## Consequences

Contributors can inspect exact dependency source and pinned commits in the
repository. CMake target integration stays local and explicit.

The repository will carry third-party source code. Dependency updates require
submodule updates, license inventory updates, and local verification.

Future CI may add dependency caches, license checks, or vulnerability scanning,
but those systems are intentionally deferred until CI exists.