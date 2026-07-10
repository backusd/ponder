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
- Keep pure low-level value operations constexpr/noexcept when practical. Do not
  force constexpr through APIs that need runtime diagnostics, stacktrace capture,
  entropy, logging, allocation-heavy formatting, or throwing.
- Do not create individual utility documentation files. Put durable core utility
  boundary details in `libs/core/docs/Boundary.md` and implementation guidance in
  this file.
- Use `Result<T>` for recoverable errors.
- `Error` carries category/code, message, source location, and a best-effort
  stacktrace.
- `Result` success paths may be constexpr, but `Error` construction is
  runtime-only. Keep evaluated constexpr failure paths ill-formed with an
  explicit diagnostic rather than weakening `Error` diagnostics.
- Use `PonderException` only for truly exceptional failures. It must be a
  standalone project type and must not derive from `std::exception` or any other
  type.
- `PonderException` does not carry `Error`; it carries a human-readable
  message, source location, and a best-effort stacktrace.
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
- Logging records capture timestamp, level, file, line, function, and optional
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
- Build information is exposed through `BuildInfo.hpp` and implemented by a
  generated source file in the build tree. Reconfigure CMake when Git commit or
  toolchain metadata must be refreshed.
- `Uuid` is the generic stable identifier primitive. Keep domain-specific IDs
  such as `AssetId`, `NodeId`, and `MoleculeId` in the owning domain library.
  Keep UUID parse/format behavior canonical and deterministic.
- Core owns build/version information, UUID/stable identifiers, `ScopeExit`,
  minimal identifier hashing helpers, minimal string conversion/validation
  helpers, and minimal number helpers.
- Use `ScopeExit` for tiny local cleanup and restoration paths. Cleanup
  callbacks must be `noexcept`; create durable RAII types for reusable resource
  concepts.
- Narrow project strings are UTF-8. Use `IsValidUtf8` or
  `IsValidUtf8WithoutEmbeddedNull` for allocation-free validation, and use
  `Utf8ToWideString` and
  `WideStringToUtf8` for core-owned conversion between UTF-8 `std::string` and
  platform-width `std::wstring`.
- String conversion failures are recoverable parse errors. Do not silently
  replace invalid UTF-8, invalid surrogate sequences, or out-of-range Unicode
  code points.
- Keep string conversion dependency-free and avoid Windows-only APIs in public
  headers.
- `LogLevel`, `ErrorCategory`, and `AssertionFailureKind` use compact
  `std::uint8_t` underlying storage; keep future small public enums explicit
  when their value set is intentionally bounded.
- `.clang-tidy` intentionally disables `portability-avoid-pragma-once` because
  the scaffold uses `#pragma once`. Do not churn headers to include guards unless
  the project-level style decision changes first.
- Local `const` variables still use normal camelCase names. Reserve `kPascalCase`
  for true constants rather than every immutable local.
- Precompiled headers are target-scoped build accelerators only. Keep `.cpp` files
  explicitly including the headers they use, keep public headers self-contained,
  and put only stable, widely used, expensive implementation headers in
  `src/Pch.hpp`. Do not rely on PCH contents to satisfy include dependencies.
  Analysis presets may disable PCH so tools see normal include relationships.
- Core must not own runtime configuration, environment access, filesystem/path
  utilities, custom allocators, non-copyable helper base types, cancellation,
  progress reporting, domain typed IDs, or job execution.

## Deferred System Routing

When a task asks for one of these systems while working in core, do not implement
it here unless the roadmap has explicitly changed first:

- Put general threading, job execution, schedulers, and worker pools in
  `ponder_compute` or a dedicated execution library.
- Put cancellation and progress contracts in `ponder_compute`, `ponder_workflow`,
  or the operation-owning library.
- Put runtime configuration, feature flags, command-line policy, and preferences
  in the application layer or a future configuration subsystem.
- Put environment variable access and host environment inspection in
  `ponder_platform`.
- Put filesystem/path utilities, file watching, and directory traversal in
  `ponder_io` or `ponder_platform`.
- Put memory allocation/tracking infrastructure in the subsystem that proves the
  need; do not add project-wide allocator policy speculatively.
- Do not add `NonCopyable`, `NonMovable`, or similar helper base classes to core.
  Delete copy/move operations directly on the type that owns the invariant.
- Put domain typed IDs in the owning domain library. Core owns only generic
  stable identifier primitives such as `Uuid`.

## Header Hygiene

- Public headers must be self-contained and compile when included as the first
  project header in a translation unit.
- Every new public core header must be listed in `libs/core/CMakeLists.txt` and
  added to the header self-containment sources under
  `tests/unit/core/HeaderSelfContainment/`.
- Keep third-party implementation details such as spdlog/fmt and moodycamel out
  of public headers. Standard-library template APIs may include the headers they
  require, but avoid heavy includes that can be moved to `.cpp` files without
  changing the public type surface.
- Prefer forward declarations only when they keep the header self-contained and
  do not obscure ownership, value semantics, or template requirements.

## Verification

- Configure and build a supported CMake preset.
- Run `ponder_core_tests` after changing core behavior.
- Run formatting checks for touched source files.
- Run clang-tidy where available for high-signal core warnings.
- Confirm generated files and local build outputs are not staged.
