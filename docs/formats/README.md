# Formats

File format and schema documentation lives here.

The native project format uses the `.ponder` extension.

This directory will hold versioned format notes, schema descriptions, migration
rules, and compatibility expectations once the project format exists.

Keep examples and fixtures separate from the format specification:

- User-facing sample projects belong under `examples/projects/`.
- Test project fixtures belong under `tests/fixtures/projects/`.
- Exact expected outputs belong under `tests/golden/`.

The `.ponder` extension is accepted in
`docs/adr/0005-native-project-extension.md`. The concrete file or directory
layout still needs a later format decision.
