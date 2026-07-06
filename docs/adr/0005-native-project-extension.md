# ADR 0005: Native Project Extension

## Status

Accepted.

## Context

`ponder` needs a recognizable native project format for desktop workflows,
examples, fixtures, future project inspection tools, and possible remote or
headless execution.

The extension should be explicit, easy to associate with the full project name,
and not depend on the shortened C++ namespace name.

## Decision

Use `.ponder` as the native project extension.

Documentation, examples, fixtures, tools, UI text, and format tests should refer
to native projects as `.ponder` projects.

The exact on-disk shape is not decided by this ADR. A `.ponder` project may later
be defined as a directory, a single file, or another structured container through
a dedicated project-format decision.

## Consequences

Example projects should live under `examples/projects/` when they are intended
for users and under `tests/fixtures/projects/` when they are intended for tests.

The `ponder-project-inspect` tool and future project-format documentation should
use `.ponder` terminology from the beginning.

Future format ADRs or specs must define versioning, migration behavior, and the
specific file or directory structure.
