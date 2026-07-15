# Examples

User-facing examples live here.

Examples should be small, readable, and useful to someone learning or
demonstrating `ponder`. They are part of the product experience, not just test
inputs.

## Layout

- `platform/` contains runnable platform-library teaching examples.
- `render/` contains runnable renderer/platform integration teaching examples.
- `projects/` contains sample `.ponder` projects once the native project format
  exists.

## Example Indexes

- [Platform examples](platform/README.md)
- [Render examples](render/README.md)

## Rules

- Keep examples intentionally versioned.
- Prefer human-readable files while the project format is still evolving.
- Avoid large assets unless a later project decision allows them.
- Do not use examples as hidden regression tests. If a sample is important for
  automated coverage, also add or mirror it under `tests/fixtures/`.
- Document the purpose of each non-trivial example near the example itself.
