# Golden Files

Golden files live here.

Keep golden data small, readable where possible, and tied to explicit migration
or format-version expectations.

## Rules

- Use golden files only when exact output matters.
- Keep golden files deterministic and reviewable.
- Store golden files near a README or test name that explains what owns them.
- Update golden files in the same change as the behavior or format update that
  requires them.
- Avoid large or opaque binary golden data unless a specific project decision
  allows it.

Prefer fixtures under `tests/fixtures/` when a test needs input data but not an
exact expected output file.