# Codex Guidance: Scientific Data

Read the root `AGENTS.md` before this file.

## Scope

Work here on reusable scientific data models and small reference data.

## Local Rules

- Keep public headers under `include/ponder/scientific_data/`.
- Use the `pond::scientific_data` namespace.
- Avoid adding large assets without a recorded project decision.
- Keep data provenance and versioning explicit when real data is introduced.

## Verification

- Configure and build a supported CMake preset.
- When tests exist, run the matching scientific data test executable.
