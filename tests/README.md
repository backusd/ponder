# Tests

Tests live here.

The project uses GoogleTest through CTest. Tests are enabled by default with
`PONDER_BUILD_TESTS=ON`.

## Layout

- `tests/unit/` contains fast library-level tests.
- `tests/integration/` contains cross-library workflows and live OS/backend behavior tests.
- `tests/fixtures/` contains small test inputs.
- `tests/golden/` contains intentionally versioned expected outputs.

Run the configured test suite with:

```text
ctest --preset windows-msvc-release
```
