# Example Projects

Small user-facing `.ponder` example projects live here once the native project
format exists.

These examples should help users understand what a real `ponder` project looks
like. Test-only fixtures belong under `tests/fixtures/projects/`.

## Guidelines

- Keep each project small enough to inspect in a code review.
- Prefer clear names that describe the project scenario.
- Include only assets needed to understand the example.
- Keep project-format versions explicit once the format has version metadata.
- If an example is also used as a test fixture, keep the relationship
  intentional and documented in both places.

Do not add large molecules, trajectories, grids, render captures, or binary
assets here without a specific project decision.