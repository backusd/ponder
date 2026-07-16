# 1 Rectangle

> **Experimental API warning:** this is a rectangle-milestone workbench. It
> uses pond::ui::experimental::RectangleRenderer, which is not the future
> retained-mode UI API and has no source or ABI stability guarantee.

This non-installed contributor example exercises the complete public
platform/render/UI rectangle path. It creates one initially hidden,
high-density-compatible window, prepares a Vulkan render target, creates the
device-scoped experimental rectangle renderer, then shows and continuously
renders the window.

## Scene And Features

The target clears to an opaque, nearly black linear color. One opaque blue
rectangle is painted first, followed by translucent orange and green rectangles
whose bounds overlap each other and the base. UI colors are authored as sRGB
straight alpha; the UI compiler converts them to the renderer's packed linear
premultiplied representation. Their visible overlap therefore demonstrates both
painter ordering and source-over premultiplied blending.

Rectangle bounds are proportional to the current copied logical frame metrics.
The example coalesces logical-size, pixel-size, visibility, window-state,
presentation, display, and display-scale events into a new target snapshot.
Resize and high-density changes consequently rebuild the scene from current
logical and framebuffer extents and allow normal swapchain recreation.

## Controls And Output

Resize, minimize, restore, cover, and expose the window with normal operating
system controls. Close the window to exit interactive mode. A minimized,
initially hidden, or zero-pixel target is a normal typed suspension: the loop
continues polling events with a 50 ms backoff instead of busy-spinning, and
rendering resumes after the target becomes drawable.

The program prints target snapshot and UI metric changes, suspension/recovery
transitions, a final frame/skip summary, renderer child counts, and validation
totals. It intentionally does not print every frame. Upload and pipeline-cache
counters are not exposed by the public APIs and are therefore not inspected.

Options:

~~~text
--auto-close-ms <milliseconds>  Exit after a bounded run.
--smoke                         Use a validation-disabled 3000 ms lifecycle run.
--help                          Print usage without creating a runtime.
~~~

Bounded mode returns success only after at least one ordered rectangle batch was
both recorded and presented. The --smoke tour additionally requires a recorded
batch to be presented after restore. Interactive mode and --auto-close-ms
retain the renderer default validation policy. The explicit --smoke capability probe
disables validation so unrelated host-layer diagnostics do not turn a basic
launch check into a validation gate. It also requests one resize, minimize, and
restore through public platform APIs before exiting. This makes it useful for
capability-gated local automation without turning the workbench into the live
UI acceptance test.

## Lifetime And Errors

Every Result and every RectangleRecordOutcome is checked. Structured errors
are printed through the core::Error formatter and return a nonzero process
status. Surface loss is recovered through the latest copied target snapshot;
other failures propagate after an active frame is explicitly abandoned.

Ownership order is visible in the source: an active frame finishes or is
abandoned first, followed by the experimental rectangle renderer, render
target, render device, validation inspection, RenderBootstrap, platform
window, and finally PlatformRuntime. The renderer never retains a native
window handle or borrowed platform window.

## Side Effects And Host Limits

Running the program opens one native window, creates a Vulkan instance/device
and presentation resources, allocates transient GPU upload storage, and writes
concise diagnostics to standard output or standard error. It creates no files,
opens no network connection, and installs no artifacts.

The Vulkan backend is supported on Windows and Linux. Windows is the currently
live-tested host; Linux presentation behavior has not yet been validated by the
project. A run requires a Vulkan 1.2-or-newer loader, a present-capable
GPU/driver, and an active desktop session. Developer builds may also require
the configured Khronos validation layer. The Vulkan SDK is not required merely
to run an already-built executable.

## Build And Run

From a Visual Studio Developer Command Prompt, configure with examples and the
UI/render integration enabled:

~~~bat
cmake --preset windows-msvc-debug ^
  -DPONDER_BUILD_EXAMPLES=ON ^
  -DPONDER_BUILD_RENDER=ON ^
  -DPONDER_BUILD_UI_RENDER_INTEGRATION=ON
cmake --build --preset windows-msvc-debug --target ponder_ui_1_rectangle
~~~

Run interactively or in bounded mode:

~~~powershell
.\build\windows-msvc-debug\bin\ponder-ui-1-rectangle.exe
.\build\windows-msvc-debug\bin\ponder-ui-1-rectangle.exe --smoke
.\build\windows-msvc-debug\bin\ponder-ui-1-rectangle.exe --auto-close-ms 5000
~~~

The compile-only CTest entry ponder_ui_1_rectangle_build_smoke builds this
target but does not launch it. The bounded launch is supporting evidence only;
the validation-disabled --smoke path does not replace deterministic
packet/backend tests or the later required live Windows/Vulkan validation gate.

## Intentional Scope

The source includes only public core, platform, render, and experimental UI
headers. It does not use private paint/compiler/packet contracts, Vulkan, SDL,
or native-window APIs directly. It does not implement layout, input, text,
widgets, or retained UI state.

The independent renderer lifecycle example remains
[examples/render/1-clear-window](../../render/1-clear-window/README.md); its
clear-only support claim is intentionally separate from this UI workbench.
