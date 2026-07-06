# Codex Guidance: Project Inspect Tool

Read the root `AGENTS.md` before this file.

## Scope

Work here on the `ponder-project-inspect` developer tool. The current target is
a console stub until `.ponder` project loading exists.

## Local Rules

- Keep reusable inspection and project-loading behavior in libraries.
- Use the executable target `ponder_project_inspect` with output name
  `ponder-project-inspect`.
- Link only the minimal libraries needed for the current scaffold.
- Do not make this tool a hidden owner of project-file behavior.
- Prefer `ponder_core` diagnostics and error conventions for scaffold code.

## Verification

- Configure and build a supported CMake preset with `PONDER_BUILD_TOOLS=ON`.
- Run `ponder-project-inspect` from the build output when behavior changes.