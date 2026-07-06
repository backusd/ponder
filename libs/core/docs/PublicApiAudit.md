# Core Public API Audit

Status: implemented for TASK CORE-001.

## Scope

This audit reviewed the current public core headers and the matching source/test
files where needed to understand behavior:

- `include/ponder/core/Assert.hpp`
- `include/ponder/core/Library.hpp`
- `include/ponder/core/Log.hpp`
- `include/ponder/core/PonderException.hpp`
- `include/ponder/core/Result.hpp`
- `src/Assert.cpp`
- `src/Log.cpp`
- `src/PonderException.cpp`
- `src/Result.cpp`
- `tests/unit/core/SmokeTests.cpp`

## Decision Baseline

Core should stay narrow and foundational. The public API should avoid
third-party types, use `Result<T>` for recoverable errors, reserve
`PonderException` for exceptional failures, and provide project-owned behavior
for diagnostics, assertions, logging, source locations, stacktraces, and small
utilities.

The most important resolved decision for this audit is that `PonderException`
must be a standalone project type and must not derive from `std::exception` or
any other type.

## Summary

The existing public API is a useful scaffold, but it is not yet the settled core
surface. The highest-priority follow-up work is:

- Replace `PonderException` with the standalone design and update tests away
  from `what()`.
- Expand `Error` beyond message/location so it can carry category/code and
  stacktrace information where practical.
- Add the missing assertion and verify contracts, including test-scoped handler
  overrides.
- Keep logging third-party-free in public headers, then add the planned async
  backend, test capture hooks, fatal handling, and explicit failure policy.
- Add header hygiene/self-containment checks as the API becomes real.

## Header Findings

### `Assert.hpp`

CORE-005 update:

- Assertions now expose scoped handler overrides, `PONDER_VERIFY`,
  `PONDER_UNREACHABLE`, and `PONDER_DEBUG_BREAK`.
- The findings below are retained as historical audit context for why the
  assertion work was added.

Current behavior:

- Provides `PONDER_ASSERT` and `PONDER_ASSERT_MESSAGE`.
- Debug builds call `HandleAssertionFailure`.
- Release builds compile both assertion macros out.
- `HandleAssertionFailure` logs, calls `DebugBreak`, then aborts.

Mismatches and gaps:

- Missing `PONDER_VERIFY`, `PONDER_UNREACHABLE`, and public
  `PONDER_DEBUG_BREAK` macro.
- Assertion messages do not support std::format-style arguments yet.
- There is no scoped handler override for tests.
- Failure behavior is process-terminating today, which makes direct unit tests
  awkward until handler overrides exist.
- `PONDER_VERIFY` needs different release behavior than debug-only assertions:
  it should remain active and throw on failure.

Roadmap coverage:

- CORE-005 should replace the current direct failure path with the resolved
  handler-driven design.
- CORE-017 should add focused assertion and verify tests.

### `Library.hpp`

Current behavior:

- Exposes `GetLibraryName()` returning `"core"`.

Mismatches and gaps:

- This is fine as a smoke-test API, but it should not become the long-term build
  metadata API by accident.
- The planned build/version metadata should probably live in a more explicit API
  rather than expanding this small library-name helper indefinitely.

Roadmap coverage:

- CORE-010 should introduce the real build information API.
- CORE-018 should decide whether `GetLibraryName()` remains useful after build
  metadata exists.

### `Log.hpp`

Current behavior:

- Public header defines `LogLevel`, `LogMessage`, `SetMinimumLogLevel`, and
  `LogFormatted`.
- Logging macros capture `std::source_location`.
- Public header uses only standard library types.
- Source file uses spdlog privately.

Mismatches and gaps:

- The public header currently performs formatting and catches all formatting
  failures silently. The final design should make formatting/failure behavior
  intentional and testable.
- Logging is synchronous today; the planned queued backend and dedicated logging
  thread do not exist yet.
- There is no category support yet.
- There is no test sink, scoped log capture, deterministic flush, or fatal
  handler hook.
- Current spdlog usage is private, which is good and should remain true when the
  async backend is added.

Roadmap coverage:

- CORE-006 should add the internal queue dependency.
- CORE-007 should refine the frontend/backend and preserve the no-third-party
  public-header rule.
- CORE-008 should add deterministic logging test infrastructure.
- CORE-015 should keep checking public header hygiene.

### `PonderException.hpp`

CORE-004 update: `PonderException` has since been refined into a standalone
project type with project-owned message access, source location, stacktrace
fallback storage, and source-location-aware throw helpers.


Current behavior:

- `PonderException` derives from `std::runtime_error`.
- Message access currently comes from `std::runtime_error::what()`.
- Stores a source location.
- Existing smoke tests call `exception.what()`.

Mismatches and gaps:

- This directly violates the resolved standalone-type decision.
- The public header includes `<stdexcept>` only because of the current
  inheritance design.
- There is no project-owned message accessor yet.
- There is no stacktrace support or fallback behavior yet.
- There are no throw helpers/macros with std::format-style messages and source
  location capture.
- Existing tests will need to be updated when inheritance is removed.

Roadmap coverage:

- CORE-004 should replace this type and update existing tests/consumers.
- CORE-009 should provide the shared stacktrace/source-location convention used
  here.
- CORE-017 should cover exception construction and throw helpers.

### `Result.hpp`

CORE-003 update: `Result<T>` has since been refined into a `[[nodiscard]]`
project-owned wrapper around `std::expected<T, Error>`. The alias finding below is
retained as historical audit context.

Current behavior:

- `Error` carries message and source location.
- `Result<T>` is an alias around `std::expected<T, ErrorType>`.
- `VoidResult` aliases `Result<void>`.
- `MakeUnexpected` is a small helper.

Mismatches and gaps:

- `Error` does not yet carry category/code.
- Stacktrace capture and fallback behavior are not present.
- The expected-like surface is currently just an alias. That may remain
  acceptable, but the next pass should confirm whether a wrapper is useful for
  `[[nodiscard]]`, helper consistency, and stable public conventions.
- Tests only cover a basic value result and one error result.

Roadmap coverage:

- CORE-002 should define the full `Error` model.
- CORE-003 should confirm the final `Result<T>` shape.
- CORE-017 should expand tests beyond smoke coverage.

## Cross-Cutting Findings

- Source-location use is already present, but stacktrace policy is still
  undefined. CORE-009 should settle the shared convention once for Error,
  PonderException, assertions, and logging.
- The current tests are intentionally small smoke tests. They should be split
  into focused files as soon as Error, exceptions, assertions, and logging become
  more than scaffolding.
- Public headers are self-contained enough for the current scaffold, but
  self-containment should become explicit test coverage before more libraries
  depend on core.
- Existing implementation files use spdlog privately, which matches the
  dependency boundary decision.

## Roadmap Changes From This Audit

- Mark CORE-001 as implemented and reference this document.
- Make CORE-004 explicitly update the current smoke test and any consumers that
  rely on `PonderException::what()`.
- Make CORE-005 explicitly replace the current direct abort/debug-break path
  with scoped, testable handlers.
- Make CORE-007 explicitly preserve the current no-spdlog-public-header boundary
  while adding async logging.
- Make CORE-015 use this audit as a checklist for header hygiene and
  self-containment.
