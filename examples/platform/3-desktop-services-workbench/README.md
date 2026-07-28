# 3 Desktop Services Workbench

This example creates one native parent window and uses keyboard commands to
exercise host desktop services that do not need a renderer: clipboard text,
explicit external URI opening, and asynchronous native file/folder dialogs.

The program is intentionally inert at startup. It never changes the clipboard,
opens a URI, or shows a native dialog until you press a command key.

## Features Exercised

- Direct runtime and window creation, direct asynchronous dialog submission,
  and explicit `void` `Runtime::DialogShutdown()`.
- `Runtime` clipboard read/write, empty text, UTF-8 text, and best-effort
  restoration of the clipboard text captured before the first example write.
- `Runtime::UriOpenExternal()` behind both an explicit `--uri` command-line value and a
  user key press.
- Open-file, save-file, and open-folder dialogs with parented and unparented
  requests, default locations, filters, single selection, and multiple
  selection where supported.
- Runtime-owned bookkeeping for multiple outstanding requests, including
  pending counts and self-describing completion events.
- Distinct handling for every `DialogOutcome` alternative: selected paths and
  optional filter index, normal cancellation, and asynchronous `DialogFailure`.
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

## Lifetime And Error Handling

All platform operations run on the runtime owner thread. Runtime creation,
window creation, window title updates, and dialog submission use direct
contracts. A successful `Runtime::DialogShow*()` call returns its `DialogRequestId`
after descriptor validation, request registration, and backend invocation.
Synchronous failures from those operations are exceptional and propagate to a
single boundary in `main()`. That boundary catches `ponder::core::Exception`
and prints its message. Platform exceptions embed their formatted platform
error code inside that message instead of exposing a typed payload. Unrelated
failures are not converted into service results or silently ignored.

If a concurrent batch accepts one or more requests before a later submission
throws, the workbench defers rethrowing that exception. It disables new desktop
work, keeps pumping the already accepted requests to completion, performs the
normal parent/clipboard/dialog cleanup, and only then lets the exception reach
`main()`. This preserves the direct exceptional contract without abandoning
runtime-owned requests during stack unwinding.

Clipboard get/set return `Result<std::string>` and `VoidResult`, respectively,
because unavailable or malformed host clipboard access can be handled by the
command that requested it. The example reports failures immediately and remains
responsive; a failed restoration retains the captured text so it can be retried.
`Runtime::UriOpenExternal()` likewise retains `Result` because host capability
and policy failures can be handled by the command that requested them.
Example-local command-line parsing also retains `Result` because invalid input
is immediately reportable to the user.

Dialog completion is a separate asynchronous boundary. The event loop prints
selected paths for `DialogSelection`, reports `DialogCancellation` as a normal
outcome, and reports the stable error carried by `DialogFailure`. Dialog
submission succeeding does not promise a later selection: presentation or
completion can still fail after the initiating stack has returned. Completions
are consumed in callback-enqueue order, which need not match request order or
the ordering of unrelated platform events, so each event's request ID and
metadata provide the correlation.

The title and console show outstanding request counts. Once shutdown is
requested, the workbench rejects new desktop work, continues pumping events,
and keeps the parent window and runtime alive until every registered completion
has been consumed. It then releases the parent, restores the clipboard while
the runtime is still valid, calls the direct `void`
`Runtime::DialogShutdown()` only with no pending requests, and finally lets the
runtime shut down. This ordering prevents callbacks from referring to destroyed
window or runtime state.

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
asynchronous failure is printed with the stable platform error category/code
and diagnostic message. A short headless auto-close run verifies startup,
event pumping, and ordered shutdown without invoking any desktop service.
Clipboard get/set, URI success or policy failure, dialog selection and
cancellation, concurrent completion ordering, and parent-window closure require
manual verification on a desktop host. URI testing should use an intentionally
chosen safe destination because a successful command launches a host
application.
The full native-dialog matrix remains the opt-in
[manual dialog smoke](../../../libs/platform/docs/HeadlessAndHostVerification.md#manual-dialog-smoke).

Each completion reports both the request timestamp stored by `Runtime`
and the later callback completion timestamp. Neither timestamp represents the
time this example consumed the event.

## Manual Desktop Checklist

On a GUI host where these side effects are acceptable:

1. Start with explicit `--clipboard-text`, `--uri`, and `--dialog-location`
   values. Press `R`, `C`, `R`, `E`, `R`, and `B`; confirm the original text is
   captured before the first write, empty and UTF-8 text round-trip, failures
   stay local, and `B` restores the captured text.
2. Press `U` with an approved URI and confirm success. In a separate run, use a
   deliberately unsupported or policy-blocked scheme and confirm the retained
   failure and manual-copy fallback. URI success and failure are host-dependent.
3. Use `O`, `M`, `S`, and `F`; select once and cancel once. Confirm accepted
   request IDs appear immediately, while later events distinguish selection
   paths/filter index from normal cancellation.
4. Press `A`, finish the three dialogs in a different order where the host
   permits it, and confirm each self-describing completion is correlated by its
   own request ID rather than submission order.
5. Leave an unparented dialog pending, refocus the parent, and request shutdown.
   Confirm new work is rejected, the parent remains alive through the final
   completion, and only then is the parent released and the manager shut down.
6. Treat an asynchronous `DialogFailure` from a constrained host as a failure
   outcome, not a thrown submission error. The deterministic headless callback
   check covers this branch without claiming native dialog presentation.

## Build And Run

Configure and build a supported preset with examples enabled:

```powershell
cmake --preset windows-msvc-debug `
    -DPONDER_BUILD_EXAMPLES=ON `
    -DPONDER_BUILD_RENDER=OFF `
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build --preset windows-msvc-debug --target ponder_platform_3_desktop_services_workbench
```

Run the example:

```powershell
.\build\windows-msvc-debug\bin\Debug\ponder-platform-3-desktop-services-workbench.exe
```

For a short smoke run:

```powershell
.\build\windows-msvc-debug\bin\Debug\ponder-platform-3-desktop-services-workbench.exe `
    --auto-close-ms 1000
```
