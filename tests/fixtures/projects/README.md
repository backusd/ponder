# Project Fixtures

Small `.ponder` project fixtures live here once the native project format
exists.

These fixtures are test data, not user-facing examples. They should support
project-format, IO, migration, and workflow tests.

## Guidelines

- Keep each fixture small enough to review by hand.
- Prefer one scenario per fixture.
- Name fixtures after the behavior they cover.
- Keep version metadata explicit once the `.ponder` format supports it.
- If a fixture mirrors a user-facing example from `examples/projects/`, document
  that relationship in both directories.

Do not add large assets or opaque binary data here without a specific project
decision.