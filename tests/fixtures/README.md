# Test Fixtures

Small test fixtures live here.

Fixtures should be readable where possible, intentionally versioned, and scoped
to the behavior being tested.

## Layout

- `projects/` contains `.ponder` project fixtures once the native project format
  exists.

## Rules

- Keep fixtures small and focused.
- Prefer text formats and deterministic ordering.
- Avoid checking in generated files unless the generator version and
  regeneration command are documented.
- Avoid large assets unless a later project decision allows them.
- Use examples for user-facing demonstrations; use fixtures for automated test
  coverage.

The same `.ponder` project may appear as both an example and a fixture when that
is useful, but the duplication should be deliberate and documented.