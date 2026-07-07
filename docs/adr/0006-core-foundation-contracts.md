# ADR 0006: Core Foundation Contracts

## Status

Accepted.

## Context

`ponder_core` is the bottom of the project dependency graph. Every future library
will rely on its conventions for recoverable errors, exceptional failures,
assertions, diagnostics, source locations, logging, build metadata, stable
identifiers, and tiny shared utilities.

These contracts need to be durable enough that project, workflow, compute, IO,
rendering, UI, plugin, and chemistry code can grow without redefining basic
failure and diagnostic behavior in each subsystem.

## Decision

Keep `ponder_core` narrow and foundational.

Core owns these project-wide contracts:

- Recoverable failures use `Error`, `ErrorCode`, `ErrorCategory`, and
  `Result<T>`.
- Exceptional failures use standalone `PonderException`; it must not derive from
  `std::exception` or any other type.
- Throw helpers preserve source locations and support std::format-style
  messages.
- Assertions and verification use the project macros in `Assert.hpp`.
- Diagnostics use the logging frontend in `Log.hpp`; spdlog/fmt and moodycamel
  remain private implementation details.
- Source-location display and stacktrace capture use the conventions in
  `StackTrace.hpp`, with empty stacktraces accepted as the portable fallback.
- Configure-time build metadata lives in `BuildInfo.hpp` and is generated into
  the build tree.
- Core owns only generic stable identifier primitives such as `Uuid`; domain IDs
  belong in the owning library.
- Core may own tiny dependency-free utilities, currently `ScopeExit` and UTF-8
  string conversion helpers, when they are broadly useful and well tested.

Core does not own domain models, workflow orchestration, IO, rendering, UI,
platform environment access, filesystem/path utilities, job systems,
cancellation/progress contracts, custom allocator policy, or domain-specific
identifier types.

## Consequences

Future libraries can depend on one consistent failure and diagnostics model.

Core public headers must remain self-contained and avoid exposing third-party
implementation types. If a new public core header is added, it must be listed in
`libs/core/CMakeLists.txt` and covered by a header self-containment source under
`tests/unit/core/HeaderSelfContainment/`.

Subsystems should resist adding convenience utilities to core unless the utility
is genuinely project-wide, dependency-free at the public boundary, and small
enough to test thoroughly.

Threading, cancellation, filesystem access, configuration, and domain-specific
IDs should be routed to the owning subsystem first. Move those concepts into
core only after a later architecture decision explicitly changes the boundary.
