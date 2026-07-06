# Applications

User-facing applications live here.

The initial application is `ponder-desktop`. Application code should compose
reusable libraries rather than owning domain logic directly.

## Targets

- `apps/desktop/` builds the `ponder_desktop` CMake target with output name
  `ponder-desktop`.