# AGENTS.md

This file contains global guidance for Codex and other coding agents working in
this repository. Keep this file limited to rules that apply across the whole
codebase.

For subsystem work, read exactly two guidance files by default:

1. This root `AGENTS.md`.
2. The subsystem-local `<library|executable path>/docs/Codex.md`.

Do not add library-specific or executable-specific guidance here. Put that in
the relevant subsystem `docs/Codex.md` file.

## Global Project Rules

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

- Prefer `std::expected` or an expected-like wrapper for recoverable errors.
- Use `PonderException` only for truly exceptional, usually unrecoverable cases.
- Use project assertion macros once they exist.
- Use project logging macros once they exist.

## Documentation Model

- Root `AGENTS.md` contains global rules only.
- Each library, executable, tool, or plugin should have local guidance at
  `<subsystem path>/docs/Codex.md`.
- Local `docs/Codex.md` files should describe ownership, dependencies,
  invariants, local docs/tests, and verification steps for that subsystem.
- Do not create a separate Codex routing index or repo-local skill-pack layer
  unless the project explicitly revisits this decision.
