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


