# 3 Desktop Services Workbench

This example creates one native parent window and uses keyboard commands to
exercise host desktop services that do not need a renderer: clipboard text,
explicit external URI opening, and asynchronous native file/folder dialogs.

The program is intentionally inert at startup. It never changes the clipboard,
opens a URI, or shows a native dialog until you press a command key.

## Features Exercised

- Clipboard read/write, empty text, UTF-8 text, and best-effort restoration of
  the clipboard text captured before the first example write.
- `OpenExternalUri()` behind both an explicit `--uri` command-line value and a
  user key press.
- Open-file, save-file, and open-folder dialogs with parented and unparented
  requests, default locations, filters, single selection, and multiple
  selection where supported.
- `DialogRequestId` correlation for multiple outstanding requests.
- All `DialogOutcome` alternatives: selection, cancellation, and failure.
- Portable path display through `pond::io::PathToUtf8()`.
- Window close handling that keeps the runtime and parent window alive until
  all registered dialog completions have been consumed.

## Controls

- `F1`: print controls.
- `C`: copy the configured sample text to the clipboard.
- `E`: copy empty text to the clipboard.
- `R`: read and print clipboard text.
- `B`: restore the clipboard text captured before the first example write.
- `U`: open the URI supplied with `--uri <uri>`.
- `O`: show a parented single-selection open-file dialog.
- `M`: show an unparented multi-selection open-file dialog.
- `S`: show a parented save-file dialog.
- `F`: show an unparented single-selection open-folder dialog.
- `A`: launch a concurrent batch of three dialog requests.
- `Q` or `Escape`: request shutdown.

Useful command-line options:

```powershell
ponder-platform-3-desktop-services-workbench `
    --clipboard-text "Ponder UTF-8 sample: H2O -> ΔG" `
    --uri "https://example.invalid/" `
    --dialog-location "C:\dev\ponder"
```

Pass `--auto-close-ms <milliseconds>` for a short smoke run.

## Side Effects And Host Limits

Clipboard writes modify the host clipboard. The example lazily captures the
previous clipboard text before its first write and restores it on normal exit,
but restoration is best-effort: another application can change the clipboard
while the example is running, and some hosts may reject clipboard access.

Opening an external URI is host policy owned by the application, not by
platform. This example requires both `--uri <uri>` and the `U` key so automated
runs cannot accidentally launch a browser or another application.

Native dialogs are manual host UI. Headless, dummy, remote, or constrained
drivers may fail or decline to present them. Cancellation is a normal outcome;
failure is printed with the stable platform error category/code and diagnostic
message.

Dialog completion timestamps are the callback completion timestamps reported by
the platform layer. They are not the time the request was issued and not the
time this example consumed the event.

## Build And Run

Configure and build a supported preset with examples enabled:

```powershell
cmake --preset windows-msvc-debug -DPONDER_BUILD_EXAMPLES=ON
cmake --build --preset windows-msvc-debug --target ponder_platform_3_desktop_services_workbench
```

Run the example:

```powershell
.\build\windows-msvc-debug\bin\ponder-platform-3-desktop-services-workbench.exe
```

For a short smoke run:

```powershell
.\build\windows-msvc-debug\bin\ponder-platform-3-desktop-services-workbench.exe --auto-close-ms 1000
```
