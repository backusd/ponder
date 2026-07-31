# Platform Examples

These are manual teaching programs for the `ponder_platform` API. They are
intended to be read, built, and run by people learning how the platform layer is
supposed to be used.

Each runtime-owning example uses the explicit lifecycle: call
`Runtime::Create()`, configure required typed hints, then call
`Runtime::Initialize()` with application metadata before using platform services.
`Create()` owns the backend object immediately but leaves it uninitialized;
typed hints configure that object, and `Initialize()` alone activates it.

## Available Now

- `1-window-display-lab`: runtime lifecycle, multi-window ownership, display
  snapshots, window state, and typed window/display events.
- `2-input-drop-monitor`: routed keyboard, text, IME, mouse, cursor, and
  inbound drag-and-drop events across two windows.
- `3-desktop-services-workbench`: clipboard text, explicit URI launch, and
  asynchronous native file/folder dialogs through one parent window, including
  explicit handling of every fallible dialog `Result` and direct dialog-state
  observation.
- `4-responsive-process-runner`: shell-free child launch, worker-thread waits,
  termination commands, abandonment, and headless process use.

All examples use `ponder::core::Timestamp` in the core steady-clock domain for
runtime and event observations; platform does not expose SDL's native tick
epoch.

## Planned

The full platform example plan lives in
[`libs/platform/docs/examples-roadmap.txt`](../../libs/platform/docs/examples-roadmap.txt).
The platform boundary contract lives in
[`libs/platform/docs/Boundary.md`](../../libs/platform/docs/Boundary.md).
