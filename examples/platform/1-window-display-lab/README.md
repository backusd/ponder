# 1 Window Display Lab

This example is the first platform-library tour. It creates a platform runtime,
opens two native windows, prints display and window snapshots, routes typed
window/display events by strong IDs, and shuts down through normal RAII.

## Features Exercised

- Explicit `ponder::platform::Runtime::Create()`, typed pre-initialization hint
  configuration, `Runtime::Initialize()` with application metadata, `TimeNow()`,
  event polling, and safe reporting of the expected `RuntimeAlreadyActive`
  exception from a deliberate second-runtime probe.
- Move-only `Window` ownership, strong `WindowId`/`DisplayId` values, a released
  probe window that demonstrates IDs are not reused during the runtime, and
  application-owned close-request handling.
- Direct `Runtime::DisplayEnumerate()` plus locally recoverable display
  lookup, bounds, usable bounds, optional refresh rate, orientation, and content
  scale.
- Direct `Runtime::WindowCreate()` and direct title, position, logical
  size, pixel size, pixel density, display scale, graphics compatibility,
  presentation, decoration, state, visibility, resizability, focus,
  always-on-top, show, and hide operations. The current display ID remains a
  locally recoverable query.
- Typed `PlatformEvent` visitation for quit, window, pointer-boundary, and
  display event families.

## Controls And Output

The windows have blank client areas because this example intentionally does not
depend on the renderer. Watch the console output and window titles.

Close any window through the operating system window controls. The example
prints the close request, destroys that window owner, and keeps running until no
windows remain or the platform sends a quit event.

Pass `--auto-close-ms <milliseconds>` to exit automatically after a short run.
Pass `--exercise-state` to opt into more intrusive window-manager operations
such as changing resizability, minimize/maximize, borderless decoration,
always-on-top, and desktop fullscreen.

## Lifetime And Error Handling

All platform calls happen on the runtime owner thread from `main()`. The example
uses the explicit Create/Hint*/Initialize sequence: `Create()` constructs the
uninitialized backend object, hints configure it, and `Initialize()` activates
it. The example then uses the direct runtime,
window, geometry, property, and display-enumeration
contracts: successful calls return their values directly, while exceptional
failures propagate to one application boundary in `main()`. That boundary
catches one `ponder::core::Exception` type and prints its message. Platform
exceptions identify their formatted platform error code inside that message,
so the example does not need a typed payload or a platform-specific catch.

Two platform queries deliberately keep local `Result` handling. A window can
move or its display can disconnect before `Window::GetDisplayId()` resolves the
current topology, and a `DisplayId` returned by enumeration can become stale
before `Runtime::DisplayGetInfo()` runs. The example prints either
error and continues so one topology race does not terminate the lab. It does
not add local catches around the direct calls.

The deliberate second-runtime probe is the one expected-exception
demonstration. Because the first runtime is known to remain active, the probe
accepts the expected `ponder::core::Exception` and prints its complete message
without branching on a typed error-code payload.

A close request is only an event. The application chooses when to release the
`Window` object; platform does not destroy it behind the application's back.

## Side Effects And Host Limits

The example creates native windows and may move, resize, hide, show, or change
window-manager state. The optional `--exercise-state` path is deliberately more
visible and may be unsupported on dummy/headless drivers or constrained window
managers.

High-density displays can report different logical and pixel sizes. Display,
content-scale, and state events are notification-style events, so the example
re-queries live state after receiving them.

## Build And Run

Configure and build a supported preset with examples enabled:

```powershell
cmake --preset windows-msvc-debug `
    -DPONDER_BUILD_EXAMPLES=ON `
    -DPONDER_BUILD_RENDER=OFF `
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build --preset windows-msvc-debug --target ponder_platform_1_window_display_lab
```

Run the example:

```powershell
.\build\windows-msvc-debug\bin\Debug\ponder-platform-1-window-display-lab.exe
```

For a short smoke run:

```powershell
.\build\windows-msvc-debug\bin\Debug\ponder-platform-1-window-display-lab.exe --auto-close-ms 1000
```
