# Core Library Boundary

`ponder_core` owns the narrow project-wide foundation code that is safe for most
other libraries to depend on.

## Public API

- Public headers live under `include/ponder/core/`.
- Source code uses the `pond::core` namespace.
- The CMake target is `ponder_core`; the alias is `ponder::core`.
- Public APIs may use stable standard library vocabulary types when they improve
  clarity, such as `std::string_view`, `std::span`, `std::source_location`, and
  `std::chrono` types.
- Public APIs should not expose third-party types.
- Do not expose `std::filesystem::path` from core APIs for now.

## Responsibilities

- `Error` for recoverable failure details. It should carry category/code,
  human-readable message, source location, and stacktrace if practical.
- `Result<T>` for recoverable operations, implemented as a thin `[[nodiscard]]`
  project-owned wrapper around `std::expected<T, Error>` and supporting `Result<void>`.
- `PonderException` for truly exceptional, usually unrecoverable failures. It is
  a standalone project type and must not derive from `std::exception` or any
  other type. It should carry a human-readable message, source location, and
  stacktrace if practical, but it should not carry `Error`.
- Throw helper macros/functions for `PonderException`, including
  `PONDER_EXCEPTION` and `ThrowPonderException`, with std::format-style
  messages and automatic source-location capture.
- `PONDER_ASSERT`, `PONDER_ASSERT_MESSAGE`, `PONDER_VERIFY`,
  `PONDER_UNREACHABLE`, and `PONDER_DEBUG_BREAK`.
- Logging macros and diagnostics for trace, debug, info, warning, error, and
  fatal levels.
- Async logging internals using a queue and dedicated logging thread. This is a
  logging implementation detail, not a general job system.
- Build/version information generated into the build tree.
- UUID/stable identifier primitives that are not tied to a domain concept.
- Tiny dependency-free helpers such as `ScopeExit`.
- Minimal string utilities, initially limited to `std::string` and
  `std::wstring` conversion unless a strong shared need appears.

## Non-Responsibilities

- Chemistry domain modeling.
- Application workflow orchestration.
- Desktop, renderer, or plugin-specific behavior.
- General threading, thread pools, job queues, or schedulers.
- Cancellation or progress contracts for now.
- Runtime configuration or feature flags.
- Environment variable access.
- Filesystem/path utilities.
- Custom memory allocation, memory tracking, or allocator infrastructure.
- Non-copyable or non-movable helper base types.
- Domain typed IDs such as `AssetId`, `NodeId`, or `MoleculeId`; those belong in
  the library that owns the domain concept.

## Dependencies

Keep this library at the bottom of the dependency graph. All project libraries
may depend on core, but core must not depend on any project library.

`ponder_core` may use third-party libraries privately. Current or planned private
implementation dependencies include:

- spdlog/fmt for logging implementation details.
- moodycamel ConcurrentQueue for queued logging internals.

Do not expose spdlog, fmt, moodycamel, or other third-party types from public
headers.
