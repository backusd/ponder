# Codex Guidance: Core

Read the root `AGENTS.md` before this file.

## Scope

Work here only on narrow project-wide foundation code that can be shared without
pulling in domain, UI, rendering, project-format, platform, filesystem, runtime
configuration, environment, job-system, or application dependencies.

## Local Rules

- Keep public headers under `include/ponder/core/`.
- Use the `pond::core` namespace.
- Keep `ponder_core` at the bottom of the dependency graph.
- All project libraries may depend on core; core must not depend on any project
  library.
- Avoid third-party types in public APIs.
- Stable standard library vocabulary types are allowed when they improve API
  clarity, but avoid `std::filesystem::path` in core for now.
- Prefer small, boring primitives with clear tests.
- Use `Result<T>` for recoverable errors.
- `Error` should carry category/code, message, source location, and stacktrace if
  practical.
- Use `PonderException` only for truly exceptional failures. It must be a
  standalone project type and must not derive from `std::exception` or any other
  type.
- `PonderException` should not carry `Error`; it should carry a human-readable
  message, source location, and stacktrace if practical.
- Use `throw PONDER_EXCEPTION(...)` or `ThrowPonderException` for throwing
  `PonderException`. These helpers support std::format-style messages and
  source-location capture.
- Use `PONDER_ASSERT` or `PONDER_ASSERT_MESSAGE` for internal invariants.
- Debug-only assertions should log and debug break, and should compile out in
  release builds.
- Use `PONDER_VERIFY` when the expression must remain active in release builds;
  verify failures should throw and support std::format-style messages.
- Use `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`, and
  `LOG_FATAL` for diagnostics.
- Use `_CATEGORY` logging macros when a stable subsystem category helps filtering
  or later capture assertions.
- Use `ScopedLogSinkHandler`, `ScopedLogFatalHandler`, and
  `ScopedMinimumLogLevel` in tests that modify logging state.
- `ScopedLogSinkHandler` flushes before installing and before restoring its
  handler so async records remain deterministic.
- Use `FlushLog` before process boundaries where queued messages must be visible;
  use `ShutdownLogging` only for explicit application shutdown paths.
- Fatal logs flush and invoke the configured fatal handler. The default fatal
  handler is no-op for now; code that must terminate should still own that
  termination path explicitly.
- Logging should capture timestamp, level, file, line, function, and optional
  category.
- Prefer `std::source_location` directly in public core APIs when source capture
  is needed.
- Use `FormatSourceLocation` for file/line/column display and
  `FormatSourceLocationWithFunction` only when function names are useful.
- `CaptureStackTrace` is best-effort. Use `IsStackTraceCaptureSupported` when
  behavior depends on real frames, and accept an empty stacktrace as the
  cross-platform fallback.
- Use `StackTraceCaptureOptions` to skip known wrapper frames or cap captured
  depth at public boundaries.
- Logging may use spdlog/fmt and moodycamel privately, but public headers must
  not expose those types.
- Async logging may use an internal queue and dedicated logging thread, but core
  must not grow a general threading or job system.
- Core may own build/version information, UUID/stable identifiers, `ScopeExit`,
  and minimal string conversion helpers.
- Core must not own runtime configuration, environment access, filesystem/path
  utilities, custom allocators, non-copyable helper base types, cancellation,
  progress reporting, domain typed IDs, or job execution.

## Verification

- Configure and build a supported CMake preset.
- Run `ponder_core_tests` after changing core behavior.
- Run formatting checks for touched source files.
- Run clang-tidy where available for high-signal core warnings.
- Confirm generated files and local build outputs are not staged.
