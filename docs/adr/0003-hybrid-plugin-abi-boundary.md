# ADR 0003: Hybrid Plugin ABI Boundary

## Status

Accepted.

## Context

`ponder` should eventually support external plugins while remaining usable across
different compilers, standard-library implementations, operating systems, and
release builds. Exposing a broad C++ ABI as the real plugin boundary would make
that goal fragile.

At the same time, plugin authors should not be forced to write low-level C-style
code for every interaction. The project can provide ergonomic C++ wrappers above
a stable binary boundary.

## Decision

Use a hybrid plugin model.

Real external plugins should use a stable C-style binary boundary. The C++ plugin
SDK may provide wrappers on top of that boundary, but those wrappers are not a
replacement for the stable ABI surface.

Built-in plugins can start with simpler project-internal integration because they
are built with the repository and do not need the same binary compatibility
guarantees.

## Consequences

The plugin SDK must avoid exposing STL-heavy, exception-heavy, or third-party
types across the external binary boundary.

The `ponder_plugin_sdk` library owns the external extension contract. Other
libraries should not invent independent plugin ABI surfaces.

The first scaffold does not need a complete plugin runtime, but it should keep
public SDK-facing types conservative and ready for ABI-stability patterns.
