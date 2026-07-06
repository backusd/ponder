# ADR 0004: Static Library First

## Status

Accepted.

## Context

The initial project phase is focused on source builds, clean target boundaries,
tests, and a desktop application scaffold. Stable public APIs, binary
distribution, and installable SDK packaging are not mature yet.

Choosing shared libraries too early would force the project to solve symbol
visibility, runtime search paths, ABI stability, and packaging details before the
library boundaries have earned that weight.

## Decision

Build project libraries as static libraries initially.

CMake targets should still be written cleanly with implementation targets and
`ponder::*` aliases so that install/export and linkage changes can be added later
without rewriting the project structure.

## Consequences

The initial developer workflow is simpler and easier to keep portable across
Windows, Linux, and macOS.

The project must not treat static linkage as a permanent architectural
assumption. External plugin ABI work, binary distribution, or SDK packaging may
justify shared libraries later.

Public headers should still avoid unnecessary third-party exposure because static
linkage does not remove long-term API maintenance costs.
