# Plugin SDK Library Boundary

`ponder_plugin_sdk` owns the public extension boundary for external plugins.

## Public API

- Public headers live under `include/ponder/plugin_sdk/`.
- Source code uses the `pond::plugin_sdk` namespace.
- The CMake target is `ponder_plugin_sdk`; the alias is `ponder::plugin_sdk`.

## Responsibilities

- Stable external plugin ABI declarations when plugin work begins.
- C++ SDK wrappers over the stable C-style plugin boundary.
- Public extension contracts that can survive compiler and platform differences.

## Non-Responsibilities

- Built-in plugin implementation details.
- UI widgets for managing plugins.
- Arbitrary third-party engine execution.

## Dependencies

Keep the external plugin boundary conservative. Avoid exposing STL-heavy,
exception-heavy, or third-party-heavy ABI surfaces in SDK-facing types.
