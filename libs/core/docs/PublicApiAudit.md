# Core Public API Audit

Status: current after TASK CORE-018.

This document records the current `ponder_core` public API surface after the core
foundation implementation pass. Earlier audit findings from CORE-001 through
CORE-017 have either been implemented, documented as boundary policy, or deferred
to the owning subsystem.

## Scope

Reviewed public headers:

- `include/ponder/core/Assert.hpp`
- `include/ponder/core/BuildInfo.hpp`
- `include/ponder/core/Library.hpp`
- `include/ponder/core/Log.hpp`
- `include/ponder/core/PonderException.hpp`
- `include/ponder/core/Result.hpp`
- `include/ponder/core/ScopeExit.hpp`
- `include/ponder/core/StackTrace.hpp`
- `include/ponder/core/String.hpp`
- `include/ponder/core/Uuid.hpp`

Reviewed implementation and verification boundaries:

- `libs/core/CMakeLists.txt`
- `libs/core/src/`
- `tests/unit/core/`
- `tests/unit/core/HeaderSelfContainment/`

## Decision Baseline

Core stays narrow and foundational. It owns error/result conventions,
exception/throw helpers, assertions, logging, source-location and stacktrace
conventions, build metadata, UUIDs, and tiny dependency-free utilities that most
libraries can safely reuse.

Core does not own domain models, IO, platform inspection, filesystem policy,
threading/job systems, workflow progress/cancellation, runtime configuration,
custom allocators, or domain typed IDs.

## Current API Snapshot

### Error, Result, Source Locations, And Stacktraces

`Result.hpp` defines the recoverable error path:

- `ErrorCategory : std::uint8_t` names broad failure categories.
- `ErrorCode` stores an `ErrorCategory` plus an integer value.
- `Error` stores `ErrorCode`, human-readable message, `std::source_location`,
  and a `StackTrace` fallback.
- `Result<T>` is a `[[nodiscard]]` project wrapper around
  `std::expected<T, Error>`.
- `Result<void>` is specialized and aliased as `VoidResult`.
- `MakeUnexpected(...)` builds explicit unexpected errors for propagation.

`StackTrace.hpp` defines the shared source-location and stacktrace convention:

- Public APIs use `std::source_location` directly when source capture is needed.
- Display code uses `FormatSourceLocation` and
  `FormatSourceLocationWithFunction`.
- `CaptureStackTrace` is best-effort and may return an empty trace on platforms
  where useful stack capture is not available yet.

### PonderException And Throw Helpers

`PonderException.hpp` defines the exceptional failure path:

- `PonderException` is a standalone final project type.
- It does not derive from `std::exception` or any other type.
- It stores a message, source location, and best-effort stacktrace.
- It deliberately does not store `Error`.
- `MakePonderException`, `MakeFormattedPonderException`, `ThrowPonderException`,
  and `PONDER_EXCEPTION(...)` preserve source-location capture and support
  std::format-style messages.

### Assertions And Verify

`Assert.hpp` defines assertion and verification diagnostics:

- `AssertionFailureKind : std::uint8_t` distinguishes assertion, verify, and
  unreachable failures.
- `AssertionFailure` stores failure kind, expression, message, and source
  location.
- `PONDER_ASSERT` and `PONDER_ASSERT_MESSAGE` are debug-only and compile out in
  release builds.
- `PONDER_VERIFY` remains active in release builds and throws `PonderException`
  on failure.
- `PONDER_UNREACHABLE` routes through the assertion handler, debug breaks, and
  aborts.
- Scoped assertion and verify handlers make global handler changes testable and
  restorable.

### Logging

`Log.hpp` defines logging frontend and test hooks:

- `LogLevel : std::uint8_t` defines trace, debug, info, warning, error, and
  fatal ordering.
- `LogEntry` captures level, category, message, timestamp, and source location.
- `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`, and
  `LOG_FATAL` provide std::format-style logging macros.
- Each logging macro has a `_CATEGORY` variant.
- Logging formatting failures are captured as logging diagnostics instead of
  escaping the no-throw logging path.
- The backend uses spdlog/fmt and moodycamel privately. Public headers expose
  only standard-library and project-owned types.
- `FlushLog` provides deterministic draining; `ShutdownLogging` is for explicit
  application shutdown paths.
- Scoped sink, fatal-handler, and minimum-level guards make tests deterministic.

### Build Metadata

`BuildInfo.hpp` exposes configure-time build metadata through `GetBuildInfo()`.
The implementation source is generated in the build tree and is not checked in.
Reconfigure CMake when Git commit or toolchain metadata needs to refresh.

### Stable Identifiers

`Uuid.hpp` defines the generic core-owned UUID primitive:

- 16-byte value storage.
- Nil UUID support.
- Canonical lowercase formatting.
- Canonical parse support for lowercase and uppercase hexadecimal input.
- RFC 4122 version/variant helpers.
- Hashing and comparison support.
- Deterministic and entropy-backed v4 generation.

Domain-specific IDs belong in their owning libraries.

### Small Utilities

`ScopeExit.hpp` defines a tiny move-only no-throw scope guard for local cleanup
and temporary state restoration.

`String.hpp` defines dependency-free conversion between project UTF-8
`std::string` values and platform-width `std::wstring` values. Invalid encodings
are recoverable parse errors returned through `Result`.

`Library.hpp` currently exposes `GetLibraryName()` and remains useful as a small
smoke-test API.

## Header Hygiene Findings

Status: implemented.

- Public core headers are self-contained and compile when included first in a
  translation unit.
- Public core headers do not expose spdlog, fmt, moodycamel, or other
  third-party implementation types.
- Every public core header is listed in `libs/core/CMakeLists.txt`.
- Every public core header has a corresponding compile-only self-containment
  source under `tests/unit/core/HeaderSelfContainment/`.
- Heavier standard headers that remain in public headers are tied to template
  helpers or public value semantics and cannot be moved to `.cpp` files without
  changing the exposed API shape.

Future public headers should be added to both `libs/core/CMakeLists.txt` and the
header self-containment source list in `tests/unit/core/CMakeLists.txt`.

## clang-tidy And Formatting Findings

Status: implemented for high-signal core findings.

- Small public scoped enums use `std::uint8_t` underlying storage:
  `AssertionFailureKind`, `ErrorCategory`, and `LogLevel`.
- Logging implementation catch-all handlers call an explicit internal suppression
  helper instead of using empty catch blocks in no-throw diagnostic paths.
- Internal assertion handler globals use the project camelCase variable style.
- `.clang-tidy` treats local `const` variables as normal camelCase locals so the
  project can keep using `const` generously without false naming noise.
- `portability-avoid-pragma-once` is disabled because the repository
  intentionally uses `#pragma once` across the scaffold. Revisit this only if the
  project chooses to migrate headers to include guards.

A scoped library/source tidy pass has no enum-size, empty-catch, or core
implementation naming findings remaining.

## Test Coverage Snapshot

`ponder_core_tests` is now a focused suite rather than a smoke-only target. It
covers:

- `Result<T>`, `VoidResult`, `Error`, `ErrorCode`, and error formatting.
- `PonderException`, throw helpers, source locations, and stacktrace fallback.
- Assertions, verify behavior, unreachable handling, scoped handler restoration,
  and source-location capture.
- Logging levels, category/message formatting, formatting failure diagnostics,
  async queue flushing, fatal handlers, minimum level filtering, and scoped RAII
  overrides.
- Build information metadata.
- UUID parse/format/hash/generation behavior.
- `ScopeExit` cleanup, dismissal, move transfer, and no-throw contract.
- UTF-8 and wide-string conversion, including invalid encoding failures.
- Public header self-containment for every public core header.

The local Windows MSVC debug suite currently runs 86 tests.

## Deferred Work

These items are intentionally not core responsibilities right now:

- General job systems, schedulers, and worker pools.
- Cancellation and progress contracts.
- Runtime configuration and feature flags.
- Environment and filesystem/path utilities.
- Custom allocator or memory tracking infrastructure.
- Domain-specific typed IDs.
- Chemistry, IO, rendering, UI, project format, workflow, compute, and plugin
  domain behavior.

Route those systems to the owning library or application layer before adding new
core API.

## Roadmap State

- CORE-001 through CORE-017 are implemented.
- CORE-018 records this final documentation snapshot.
- CORE-019 verified the completed core phase locally on 2026-07-07.
