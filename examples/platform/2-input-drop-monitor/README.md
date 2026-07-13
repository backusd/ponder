# 2 Input Drop Monitor

This example creates two native windows and prints a structured stream of
keyboard, text, composition, mouse, cursor, and inbound drag-and-drop events.
It is intentionally console-first so it can run before the renderer or UI
libraries exist.

## Features Exercised

- Optional event targets and multi-window `WindowId` routing.
- Physical keys, logical Unknown/character/named keys, side-specific modifier
  flags, repeat detection, logical-key helpers, and modifier predicates.
- Text input start/stop, committed text, composition text, optional
  Unicode-character selection ranges, composition clearing, and IME area
  set/clear.
- Mouse motion, relative movement, buttons, wheel axes, pointer enter/leave,
  window mouse grab, relative mouse mode, global capture, and global mouse
  position.
- Every `SystemCursorShape`, cursor selection separate from cursor visibility,
  and orderly restoration on shutdown.
- `DropBeginEvent`, `DroppedFileEvent`, `DroppedTextEvent`,
  `DropPositionEvent`, and `DropCompleteEvent`, including optional source
  application text and portable file-path display through `pond::io`.

## Controls And Output

The windows have blank client areas. Watch the console and window titles.

- `F1`: print controls.
- `1` / `2`: make a window active.
- `T`: toggle text input for the active window.
- `I`: set a fixed logical IME candidate area.
- `A`: clear the IME candidate area.
- `X`: clear active text composition.
- `G`: toggle mouse grab on the active window.
- `R`: toggle relative mouse mode on the active window.
- `P`: toggle global mouse capture.
- `M`: query global mouse position.
- `S`: cycle system cursor shape.
- `V`: show or hide the cursor.
- `Q` or `Escape`: quit.

Pass `--auto-close-ms <milliseconds>` to exit automatically after a short run.

## Lifetime And Error Handling

All platform calls stay on the runtime owner thread. Every `Result` is checked
and printed with stable error category/code information. `Unsupported` for
global capture, global mouse position, relative mode, or cursor behavior is a
capability result, not a bug in the example.

Text input is active on at most one example window at a time. On shutdown, the
example stops text input, clears the IME area, releases mouse grab, disables
relative mode and global capture, and shows the cursor while the resources still
exist.

## Side Effects And Host Limits

The example creates native windows and may grab the mouse, hide the cursor, or
enable relative mouse mode when explicitly requested by key commands. Some
drivers and desktop environments intentionally do not support global capture or
desktop-relative mouse positions.

Composition selections are Unicode-character ranges, not UTF-8 byte ranges.
`TextInputArea::cursorOffset` is horizontal and relative to
`rectangle.origin.x`. Wheel output is already normalized by platform so positive
horizontal means right and positive vertical means up.

Inbound drops depend on the host desktop and drag source. `DropBeginEvent` has
no position; file, text, position, and complete events carry positions and are
interpreted independently.

## Build And Run

Configure and build a supported preset with examples enabled:

```powershell
cmake --preset windows-msvc-debug -DPONDER_BUILD_EXAMPLES=ON
cmake --build --preset windows-msvc-debug --target ponder_platform_2_input_drop_monitor
```

Run the example:

```powershell
.\build\windows-msvc-debug\bin\ponder-platform-2-input-drop-monitor.exe
```

For a short smoke run:

```powershell
.\build\windows-msvc-debug\bin\ponder-platform-2-input-drop-monitor.exe --auto-close-ms 1000
```
