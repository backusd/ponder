# AGENTS.md

This file contains global guidance for Codex and other coding agents working in
this repository.

For subsystem work, read exactly two guidance files by default:

1. This root `AGENTS.md`.
2. The subsystem-local `<library|executable path>/docs/Codex.md`.

## Global Project Rules

- When writing commit messages, NEVER auto-add your agent name as co-author
- When making technical decisions, do not give any weight to development
  cost. Instead, prefer quality, simplicity, robustness, scalability, and
  long term maintainability.
- The canonical project name is `ponder`.
- C++ namespaces use `pond::`. This is the only intended use of shortened
  `pond`.
- Public includes use `#include <ponder/{library}/{file}>`.
- New C++ code uses C++23.
- The build system is CMake.
- Project CMake options use the `PONDER_*` prefix.
- CMake implementation targets use `ponder_<name>`; aliases use
  `ponder::<name>`.
- Project libraries are static libraries initially.
- Project code is built with warnings-as-errors.
- Dependencies are Git submodules wired with `add_subdirectory`.
- Tests use GoogleTest and are enabled by default.
- New documentation should be Markdown.

## Global Style

- Formatting is LLVM-derived with 4-space indentation, 100 columns, and Allman
  braces.
- Types and functions use PascalCase.
- Variables use camelCase.
- Private data members use `m_*`.
- Constants use `kPascalCase`.
- Filenames use PascalCase.
- Public headers should avoid heavy third-party includes.

## Error Handling And Diagnostics

- Prefer `Result` (an expected-like wrapper) defined in
  `libs/core/include/ponder/core/Result.hpp` for recoverable errors.
- Use `PonderException` defined in `libs/core/include/ponder/core/PonderException.hpp`
  only for truly exceptional, usually unrecoverable cases.
- Use project assertion macros defined in `libs/core/include/ponder/core/Assert.hpp`
- Use project logging macros defined in `libs/core/include/ponder/core/Log.hpp`

## C++ Coding Guidelines

- Prefer clear ownership. Use automatic storage, values, references, and smart
  pointers. Do not use raw `new` or `delete` in project code.
- Use `const` wherever a value should not change after initialization.
- Use `constexpr` or `consteval` for values and functions that should be
  evaluated at compile time.
- Use `inline` variables or functions for header-defined entities that require
  a single program-wide definition.
- Use `noexcept` when a function has a stable no-throw contract, especially for
  move operations, destructors, swaps, and low-level utility functions.
- Use `[[nodiscard]]` on functions whose return values carry errors, ownership,
  handles, or other important state.
- Prefer `std::unique_ptr` for exclusive ownership and `std::shared_ptr` only
  when shared ownership is truly required.
- Prefer references for non-null borrowed objects. Use pointers only when
  nullability, reseating, or pointer-like semantics are intentional.
- Prefer `std::span`, `std::string_view`, and other view types for non-owning
  contiguous data and string parameters.
- Prefer `std::array`, `std::vector`, `std::string`, and other standard library
  containers over manually managed buffers.
- Keep constructors explicit when implicit conversion would be surprising.
- Initialize variables at declaration. Avoid uninitialized storage.
- Keep scopes tight. Declare variables as close as possible to their first use.
- Prefer range-based algorithms and standard library algorithms over handwritten
  loops when they improve clarity.
- Prefer strong types and scoped enums over primitive values when the meaning or
  valid range matters.
- Use `enum class` instead of unscoped enums unless interoperability requires
  otherwise.
- Avoid macros for constants, functions, or type aliases. Use language features
  instead, reserving macros for logging, assertions, configuration gates, and
  other cases where a macro is genuinely needed.
- Avoid global mutable state. If shared state is unavoidable, make ownership,
  lifetime, and synchronization explicit.
- Design APIs around invariants. Validate inputs at boundaries and keep invalid
  states difficult or impossible to represent.
- Prefer small, cohesive functions and types. Split code when doing so exposes a
  real concept or reduces meaningful complexity.
- Keep public headers lightweight and stable. Avoid exposing third-party types
  or implementation details unless they are intentionally part of the API.
- Use RAII for resources such as files, handles, memory, locks, and temporary
  state changes.
- Prefer `std::scoped_lock`, `std::lock_guard`, and other RAII synchronization
  primitives over manual lock/unlock pairs.
- Avoid owning raw threads. Prefer higher-level concurrency abstractions when
  available, and make thread ownership and shutdown behavior explicit.
- Treat casts as suspicious. Prefer avoiding casts; when needed, use the narrowest
  C++ cast that expresses the intent.
- Avoid undefined behavior, implementation-defined assumptions, and lifetime
  tricks. Write code that remains understandable under optimization.
- Keep exception use narrow. Use `Result` for recoverable failures and
  `PonderException` for exceptional unrecoverable failures.
- Do not ignore errors. Handle them, propagate them, or deliberately document why
  they are safe to discard.
- Prefer deterministic behavior and reproducible outputs, especially in project
  files, tests, serialization, and computation workflows.
