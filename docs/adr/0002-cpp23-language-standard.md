# ADR 0002: C++23 Language Standard

## Status

Accepted.

## Context

`ponder` is intended to be a long-lived modern C++ codebase. The project should
start with conventions that can support high-quality scientific, desktop, and
future distributed systems work without immediately accumulating compatibility
workarounds for older compilers.

The initial supported compiler families are MSVC and clang-cl on Windows, Clang
and GCC on Linux, and AppleClang on macOS. Compiler minimums may be raised when
needed to preserve clean implementation practices.

## Decision

Use C++23 from day one for project source code.

CMake targets must request C++23 through target-oriented configuration. Public
APIs may use C++23 standard-library facilities when they are supported cleanly by
the agreed compiler matrix.

Do not add C++ modules support during the scaffold. The project will use ordinary
headers and source files until a later decision explicitly changes that.

## Consequences

The codebase can use modern language and library facilities early, including
`std::expected` where compiler support is acceptable.

Contributor toolchains must be reasonably current. If an older operating system
or compiler blocks a clean C++23 implementation, the project should prefer
raising the requirement over weakening the codebase.

Future dependency choices must be checked against the C++23 compiler matrix.
