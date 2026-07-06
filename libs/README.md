# Libraries

Reusable platform libraries live here.

Library code should remain usable without the desktop application so future
command-line, web, and remote-worker use cases can share the same foundation.

Each library is a static CMake target with public headers under
`include/ponder/{library}/`, implementation files under `src/`, and local
documentation under `docs/`.

## Initial Targets

- `ponder_core` -> `ponder::core`
- `ponder_math` -> `ponder::math`
- `ponder_chemistry` -> `ponder::chemistry`
- `ponder_scientific_data` -> `ponder::scientific_data`
- `ponder_project` -> `ponder::project`
- `ponder_workflow` -> `ponder::workflow`
- `ponder_compute` -> `ponder::compute`
- `ponder_io` -> `ponder::io`
- `ponder_render` -> `ponder::render`
- `ponder_platform` -> `ponder::platform`
- `ponder_ui` -> `ponder::ui`
- `ponder_plugin_sdk` -> `ponder::plugin_sdk`

For subsystem work, read the root `AGENTS.md` and the local
`libs/{library}/docs/Codex.md` file.