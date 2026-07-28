# ADR 0012: Core Namespace And Failure Policy

## Status

Accepted.

## Related Decisions

- [ADR 0006: Core Foundation Contracts](0006-core-foundation-contracts.md)
- [ADR 0009: Fixed-Size Rendering Math](0009-fixed-size-rendering-math.md)

## Context

Core originally used the shortened `pond::core` C++ namespace and treated
`Result<T>` as the default return type for nearly every operation that could
fail. That made expected failures explicit, but it also forced callers to check
and propagate errors from operations that should never or only rarely fail and
for which local recovery is not realistic.

The public project name, include paths, and CMake aliases already use `ponder`.
Using `ponder::core` also makes the C++ namespace consistent with those public
surfaces.

## Decision

Core source and public APIs use the `ponder::core` namespace. The legacy
`pond::core` namespace is not retained as a compatibility alias. Public include
paths remain `<ponder/core/...>`, and the CMake implementation target and alias
remain `ponder_core` and `ponder::core`.

Choose the failure mechanism according to the nature of the failure and where
useful handling can occur:

- Return `Result<T>` for failures expected from caller input, external state, or
  other normal operating conditions, especially when a caller can realistically
  handle the failure locally.
- Throw `Exception` when an operation should never or only rarely fail and local
  recovery is not realistic. Such failures propagate to a higher-level handler.

The standalone project exception root is renamed from `PonderException` to
`Exception` and is defined by the header-only `Exception.hpp`. It does not derive
from `std::exception`. `ExceptionWithData<T>` derives from
`Exception` and owns a typed payload for exceptional cases that benefit from
specific catch handling. Its formatter and stream insertion operator remain
available even when `T` does not support the corresponding operation; the data
is rendered as `<unprintable>` for that unsupported channel.

`MakeException`, `MakeFormattedException`, and `PONDER_EXCEPTION` preserve source
locations and formatted messages.
`PONDER_EXCEPTION_WITH_DATA(data, ...)` provides the corresponding typed-data
construction path.

The assertion and programming-error contracts change as follows:

- One optional-message `PONDER_ASSERT(expression, ...)` replaces the separate
  `PONDER_ASSERT` and `PONDER_ASSERT_MESSAGE` macros. Assertions remain
  debug-only.
- `PONDER_VERIFY` remains release-active and throws `Exception` on failure.
- `PONDER_UNREACHABLE` debug-breaks in debug builds and throws `Exception` in
  release builds. Functions that may reach it must not promise `noexcept`.
- `GetErrorCategoryName()` treats an unknown category as unreachable
  instead of returning a fabricated `"unknown"` value, and is therefore no
  longer `noexcept`.

Initial core APIs adopting the revised distinction are:

- `Tolerance::Create` returns `Tolerance` directly and throws `Exception` for
  non-finite or negative values, which violate its programming contract.
- `UuidEntropySource` returns `void`. `GenerateUuidV4` returns `Uuid` directly,
  is `noexcept`, and requires a non-null injected source; a debug
  `PONDER_ASSERT` enforces that precondition. Entropy-source exceptions terminate
  under the `noexcept` contract. UUID text parsing remains a recoverable
  `Result<Uuid>`.

This ADR amends the failure and exception clauses in ADR 0006 and the
`Tolerance` failure clause in ADR 0009. Their other decisions remain in force.

## Consequences

Core callers no longer need result-propagation boilerplate for failures that are
programming errors or realistically handled only at a higher boundary. APIs for
expected input and environmental failures retain explicit `Result<T>` values.

The namespace, exception type and header, assertion macro, tolerance
factory, UUID entropy source, and UUID generation signatures are source-breaking
changes. Downstream libraries and examples may migrate separately, but new code
must use the revised contracts.

Code must not allow an `Exception` to escape a `noexcept` function or destructor.
External plugin ABI boundaries remain C-style and must not expose project C++
exceptions or templated exception payloads.
