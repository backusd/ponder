# Dear ImGui Platform Adapter Contract

Status: integration requirements frozen by PLAT-002 on 2026-07-08.

`ponder_ui` owns a custom Dear ImGui platform adapter over project-owned
`ponder_platform` APIs. It does not compile `imgui_impl_sdl3`, declare a direct
`ponder::SDL3` dependency, include SDL headers, or consume
`SDL_Window`/`SDL_Event`.

## Ownership And Dependencies

- UI owns one ImGui context and one platform adapter borrowing the main
  `PlatformRuntime` and `Window`.
- The adapter and context are destroyed before the borrowed window/runtime.
- UI may depend on core, platform, render, and Dear ImGui. It receives no SDL
  compile usage requirements. Final executables may still receive SDL as
  platform's link-only static implementation dependency.
- Render owns Vulkan resources and submission for ImGui draw data. UI owns the
  context, platform adapter, and frame orchestration and receives no Vulkan
  handles.

## Dear ImGui Build Contract

Before UI implementation, pin the Dear ImGui submodule to a reviewed docking
branch commit so docking inside the main OS window is available. Platform
multi-viewports remain disabled initially.

Compile the project ImGui target with:

- `IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS`.
- `IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS`.
- `IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS`.

The adapter installs project callbacks for clipboard, IME, and external URI
opening. This prevents ImGui core from bypassing platform with direct OS calls.

## Initialization And Frame Contract

- Set project backend name/user data. Leave both main-viewport `PlatformHandle`
  and `PlatformHandleRaw` null. Never store `SDL_Window*`, a native payload, or
  the address of a movable `Window` object in either field.
- Advertise `ImGuiBackendFlags_HasMouseCursors`.
- Do not advertise `HasSetMousePos` or `HasGamepad`.
- Desktop supplies frame delta. The adapter queries logical size, pixel size,
  scale, focus, minimized state, relative mode, and global mouse position
  through platform.
- Global-position fallback runs only while the adapter window is focused, the
  pointer is not hovering it, no mouse button is held, relative mode is off, and
  the driver supports global position. Subtract the window's screen position
  from the global position before submitting window-logical coordinates.
- Explicit project capture supports drags outside the window where the active
  SDL driver supports it. Unsupported global positioning/capture degrades
  without inventing coordinates.
- Cursor updates map every ImGui cursor to `SystemCursorShape`; ImGui `None` or
  software-drawn cursor mode hides the system cursor.
- IME callbacks start/stop text input and update the project text-input area.

## Event Mapping

Recognized events are always submitted to ImGui for the adapter's `WindowId`.
The application consults `WantCaptureMouse` and `WantCaptureKeyboard` afterward;
the adapter's event function does not decide application routing.

| Platform input | Dear ImGui input |
| --- | --- |
| Pointer enter/leave | Track hover and delayed leave clearing |
| Mouse motion | `AddMousePosEvent` |
| Left/right/middle/X1/X2 buttons | `AddMouseButtonEvent` |
| Wheel, positive X right and positive Y up | `AddMouseWheelEvent` unchanged |
| Physical/logical keys and modifiers | Project mapping to `AddKeyEvent` |
| UTF-8 text input | `AddInputCharactersUTF8` |
| Window focus gain/loss | `AddFocusEvent` |

Unknown keys are ignored. Raw native scancodes are not forwarded. Global quit,
window close, display changes, drag-and-drop, and dialog completion remain
application concerns unless a later UI feature explicitly consumes them.

A pointer-leave event becomes pending. One frame later, if no button is held and
no enter/motion event canceled it, submit ImGui's off-screen mouse-position
sentinel. This preserves drags that cross the window boundary.

Each platform key event emits at most one ImGui key event: prefer a known
project logical/named key, otherwise use the known physical key, and ignore the
event if neither maps. Never submit both fields as duplicate key events.

## Callback And Error Contract

- Clipboard reads store owned scratch text valid until the next clipboard
  callback. Clipboard writes use the platform text API.
- External URI callbacks call `PlatformRuntime::OpenExternalUri`.
- ImGui callback signatures cannot propagate `Result`; failures are logged
  through core diagnostics and callbacks return their documented failure value.
- IME data is converted to project logical coordinates before calling platform.

## Initial Scope

The initial adapter supports docking within one OS window. Secondary ImGui OS
viewports, pointer warping, gamepad navigation, touch/pen source
discrimination, custom cursors, and monitor callbacks for viewport creation are
deferred. Adding any of them requires a project-owned platform contract first.

## Shutdown And Tests

Shutdown stops adapter-owned text input, releases capture, restores a visible
default cursor, clears only callbacks/user data/flags installed by the adapter,
and destroys the context before its platform owners.

Tests cover pure `PlatformEvent` to ImGui-input mapping, window filtering,
cursor mapping, callback success/failure with project seams, and shutdown state.
A compile/dependency check proves UI has no SDL include path, compile definition,
direct target dependency, or public type.
