# Platform Library Boundary

`ponder_platform` owns operating-system and desktop-platform integration.

## Public API

- Public headers live under `include/ponder/platform/`.
- Source code uses the `pond::platform` namespace.
- The CMake target is `ponder_platform`; the alias is `ponder::platform`.

## Responsibilities

- Windowing, input, filesystem, and process abstractions when they are shared.
- Integration points for SDL or platform-specific APIs.
- Platform services that apps can reuse without depending on UI widgets.

## Non-Responsibilities

- Domain models.
- Rendering backend implementation.
- Application workflow policy.

## Dependencies

SDL usage should stay behind platform-owned APIs unless a local boundary
document explicitly allows exposing it.
