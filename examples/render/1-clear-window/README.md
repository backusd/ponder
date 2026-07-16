# 1 Clear Window

This is the minimal recommended platform/render integration example. It creates
one initially hidden compatible platform window, prepares its presentation
surface on the platform owner thread, creates renderer state, and then shows a
window that is continuously cleared and presented.

## Behavior And Controls

The clear color is opaque linear `(0.035, 0.10, 0.22, 1.0)`; the renderer's
opaque SDR sRGB presentation target performs the display encoding.

Resize, minimize, restore, cover, and expose the window through the operating
system controls. Suspended frames while the window is initially hidden,
minimized, or has no drawable pixel extent are normal; the polling loop backs
off instead of busy-spinning. Close the window through its normal
operating-system control to exit cleanly.

The example coalesces relevant platform events into the latest copied target
snapshot before handing that state to the renderer. It does not retain a native
window handle or a borrowed `Window` reference in renderer-facing state.

## Requirements

The Vulkan backend is supported on Windows and Linux. Windows is the currently
live-tested host; Linux presentation behavior has not yet been validated by the
project. Vulkan on macOS is intentionally unsupported because a future macOS
backend will use Metal.

Running the example requires a Vulkan 1.2-or-newer loader and a present-capable
GPU/driver for the active desktop window system. A Vulkan SDK is not required at
runtime because `ponder_render` loads Vulkan dynamically.

## Validation

The example requests the renderer's default validation policy. Developer builds
configured with `PONDER_RENDER_ENABLE_VALIDATION=ON` use standard Khronos
validation when the layer and debug utilities are available; otherwise the
default policy continues without validation. The renderer reports its selected
validation mode and any recoverable startup or presentation failure through the
project logging system.

## Lifetime Order

The source keeps the ownership order visible: an active frame finishes or is
abandoned before the render target, followed by the render device and
`RenderBootstrap`, then the platform window, and finally `PlatformRuntime`.
Renderer work and renderer-owned destruction stay on the selected render thread;
this minimal example deliberately uses the platform owner thread as that thread.

## Build And Run

Configure and build a supported preset with examples and render enabled:

```powershell
cmake --preset windows-msvc-debug -DPONDER_BUILD_EXAMPLES=ON -DPONDER_BUILD_RENDER=ON
cmake --build --preset windows-msvc-debug --target ponder_render_clear_window
```

Run the example:

```powershell
.\build\windows-msvc-debug\bin\ponder-render-clear-window.exe
```

The CTest entry `ponder_render_clear_window_build_smoke` builds the target but
does not launch this interactive program.

## Intentional Scope

This example renders one clear-only window. It uses no SDL, Vulkan, native-window,
shader, geometry, scene, UI toolkit, or backend-selection API directly. Keeping
that scope narrow makes the renderer lifecycle, window-state handoff, error
handling, and teardown sequence a readable template for future applications.
