# Headless And Host Verification

Status: platform verification guidance current as of 2026-07-24.

This document explains how to interpret platform tests that depend on SDL video
drivers, hidden windows, host services, or native desktop capabilities. It also
records the command shapes used by platform implementation tasks and gates.

Host-local success means the current host, compiler, SDL build, and selected
video driver passed the recorded checks. It is not cross-platform certification.
The portability matrix in `future.txt` remains unverified until each row is
run and recorded.

## Test Categories

Platform tests are intentionally split so deterministic checks can run without
live desktop services.

- Public/value tests:
  - Target: `ponder_platform_tests`
  - Labels: `platform`, `unit`
  - Purpose: SDL-free public types, values, headers, and coverage audits.
- Private backend tests:
  - Target: `ponder_platform_backend_tests`
  - Labels: `platform`, `backend`
  - Purpose: fake-backend SDL seams, event translation, threading, and service
    policy.
- Isolated mock-runtime tests:
  - Target: `ponder_platform_runtime_mock_tests`
  - Labels: `platform`, `backend`
  - Purpose: deterministic `Runtime` forwarding, lifecycle, window, display,
    event, and desktop-service contracts without initializing SDL.
  - Build rule: compile every `Runtime.hpp` consumer with
    `PONDER_PLATFORM_USE_MOCK_RUNTIME`; do not link `ponder::platform`.
- Live integration tests:
  - Target: `ponder_platform_integration_tests`
  - Labels: `platform`, `integration`, `live`
  - Purpose: real SDL runtime, hidden windows, process, clipboard, and
    host-service wiring.

The SDL-free public-header target is an object target named
`ponder_platform_header_tests`. It is built directly rather than discovered by
CTest.

Run pure platform tests without live desktop services:

```powershell
ctest --test-dir build\windows-msvc-debug -C Debug -L platform -LE live --output-on-failure
```

```sh
ctest --test-dir build/linux-clang-debug -L platform -LE live --output-on-failure
```

Run only live platform tests:

```powershell
ctest --test-dir build\windows-msvc-debug -C Debug -L integration --output-on-failure
```

```sh
ctest --test-dir build/linux-clang-debug -L integration --output-on-failure
```

`ponder_platform_integration_tests` uses the `ponder_platform_sdl` CTest
resource lock. That serializes tests touching process-global SDL state and the
system clipboard inside one CTest invocation. It does not coordinate two
independent CTest invocations launched at the same time.

## Hidden Windows And Headless Drivers

Most live integration tests create hidden SDL windows by setting
`WindowDesc::visible` to `false`. Hidden windows are still real SDL windows:
they initialize the video subsystem, receive backend IDs, own window state, and
exercise SDL routing. They are not pure unit-test fakes.

The headless-friendly tests select SDL's dummy driver internally with
`SDL_HINT_VIDEO_DRIVER`. For manual or ad hoc runs, set the SDL3 video-driver
environment variable before runtime creation:

```powershell
$env:SDL_VIDEO_DRIVER = "dummy"
ctest --test-dir build\windows-msvc-debug -C Debug -L integration --output-on-failure
Remove-Item Env:\SDL_VIDEO_DRIVER
```

```sh
SDL_VIDEO_DRIVER=dummy ctest --test-dir build/linux-clang-debug \
    -L integration --output-on-failure
```

SDL also accepts the legacy `SDL_VIDEODRIVER` environment variable for
compatibility. Project documentation and tests use the SDL3 hint spelling
`SDL_VIDEO_DRIVER`.

The dummy driver is useful for CI-like hidden-window coverage, but it is not a
full desktop. Treat these capabilities as follows:

- Supported and must pass under `dummy`:
  - runtime creation, metadata, hint rollback, and shutdown;
  - multiple hidden windows, move/lifetime behavior, IDs, logical size, pixel
    size, and state;
  - synthetic event polling and multi-window routing;
  - text-input activation and IME rectangle storage where SDL reports success;
  - forced-unsupported native-dialog callback translation into an
    asynchronous `DialogFailure` event. This verifies the live callback and
    completion handoff, not native presentation.
- Supported through dummy/offscreen observations:
  - display snapshots, scale values, and window display identity. Values must
    still satisfy platform invariants.
- Expected to return `Unsupported`, not skip:
  - mouse capture and global mouse position.
- Not verified by `dummy`:
  - native Vulkan/window handles. Unsupported-driver behavior is verified only
    when a Vulkan-compatible dummy window can be created.
  - native file/folder dialog presentation. Use the manual smoke procedure on a
    GUI host.
- Host/system service:
  - clipboard. Do not assume dummy covers every real desktop clipboard branch.
- Deterministic seam only:
  - external URI opening. Automated tests must not launch host applications.

## Skip Rules

Live tests should fail on normal GUI hosts when SDL initialization, hidden-window
creation, routing, state, or service behavior regresses. A skip is allowed only
after the test has positively identified an unsupported driver, missing
capability, or intentionally selected no-display mode.

The current permitted integration skip is:

- `PlatformRuntimeIntegrationTests.ReportsExpectedNativeHandleFailuresUnderDummyDriver`
  may skip when the dummy driver cannot create a Vulkan-compatible hidden window
  on this host. It must fail when the default-window `InvalidArgument` check
  fails, when a Vulkan-compatible dummy window is created but native-handle query
  does not return `Unsupported`, or when runtime/window creation fails before the
  documented capability check.

Do not add broad `try`/`catch` or "if SDL failed, skip" paths. Unexpected
failures should remain failures with the SDL diagnostic preserved in the test
output.

## Manual Dialog Smoke

Automated dialog tests exercise the deterministic callback-marshalling seam and
the live synchronous callback from a deliberately unsupported file-dialog
driver. They do not open native OS dialogs. Native presentation is an opt-in
manual check on a capable GUI host.

Manual smoke procedure:

1. Start from a clean GUI session where opening native dialogs is acceptable.
2. Create `Runtime` on the process-entry thread.
3. Create one visible parent `Window`.
4. For each dialog API, open and cancel once without a parent and once with
   `parentWindowId = parent.GetId()`:
   - `runtime.DialogShowOpenFile(OpenFileDialogDesc)`
   - `runtime.DialogShowSaveFile(SaveFileDialogDesc)`
   - `runtime.DialogShowOpenFolder(OpenFolderDialogDesc)`
5. Confirm `DialogGetPendingCount()` and `DialogGetPending()` describe every
   accepted request, then call `DialogPollCompletion()` until the matching
   `DialogCompletedEvent` arrives.
6. Confirm `event.request` preserves the ID, kind, parent, request timestamp,
   filter count, and multiple-selection setting, and that the outcome is
   `DialogCancellation`.
7. Confirm the runtime removes each request only when its completion is polled.
   Do not destroy the parent window or runtime until every requested completion
   has been consumed.
8. Record host OS, compiler/configuration, SDL video driver, dialog kind,
   parented/unparented status, and result.

On a headless or remote host where native dialogs cannot be presented, record
dialog presentation as unverified. Do not count the automated callback tests as
native dialog presentation coverage.

If a PLAT-EH-018 or PLAT-021 host-local gate cannot run this smoke procedure,
label the result as automated host-local verification with manual native dialog
presentation pending. Do not call the complete host-local gate fully closed
until the open/cancel matrix above is performed and recorded on a capable GUI
host.

## Command Catalog

Use a Visual Studio Developer Command Prompt or Developer PowerShell for Windows
MSVC and `windows-ninja-analysis` commands. A plain shell may miss MSVC include
or Windows SDK library paths.

### Intermediate Platform Tasks

Reconfigure only when CMake inputs or source lists changed.

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target `
    ponder_platform_header_tests `
    ponder_platform_tests `
    ponder_platform_backend_tests
build\windows-msvc-debug\bin\Debug\ponder_platform_tests.exe
build\windows-msvc-debug\bin\Debug\ponder_platform_backend_tests.exe
ctest --test-dir build\windows-msvc-debug -C Debug -L integration --output-on-failure
```

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug --target \
    ponder_platform_header_tests \
    ponder_platform_tests \
    ponder_platform_backend_tests
build/linux-clang-debug/bin/ponder_platform_tests
build/linux-clang-debug/bin/ponder_platform_backend_tests
ctest --test-dir build/linux-clang-debug -L integration --output-on-failure
```

Build and run only the affected rows from the roadmap's verification cadence.
Documentation-only tasks do not need a rebuild unless they change CMake, build
commands, generated interfaces, or public headers.

### Error-Handling Convergence Gate

PLAT-EH-013 uses a new, otherwise unused build directory with examples, render,
and UI/render integration disabled. Build every platform target named below,
then use one platform-labelled CTest invocation so the live SDL tests retain
their configured resource lock. Record the result as host-local Debug evidence.

```powershell
$platformBuild = "build\windows-msvc-debug-plat-eh013"
cmake --fresh --preset windows-msvc-debug -B $platformBuild `
    -DPONDER_BUILD_TESTS=ON `
    -DPONDER_BUILD_EXAMPLES=OFF `
    -DPONDER_BUILD_RENDER=OFF `
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build $platformBuild --config Debug --parallel 4 --target `
    ponder_platform `
    ponder_platform_header_tests `
    ponder_platform_tests `
    ponder_platform_backend_tests `
    ponder_platform_integration_tests `
    ponder_platform_process_helper
ctest --test-dir $platformBuild -C Debug -L "^platform$" --parallel 4 `
    --output-on-failure --no-tests=error
```

Use the matching Debug preset on Linux or macOS when that is the current host:

```sh
platform_build=build/linux-clang-debug-plat-eh013
cmake --fresh --preset linux-clang-debug -B "$platform_build" \
    -DPONDER_BUILD_TESTS=ON \
    -DPONDER_BUILD_EXAMPLES=OFF \
    -DPONDER_BUILD_RENDER=OFF \
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build "$platform_build" --target \
    ponder_platform \
    ponder_platform_header_tests \
    ponder_platform_tests \
    ponder_platform_backend_tests \
    ponder_platform_integration_tests \
    ponder_platform_process_helper
ctest --test-dir "$platform_build" -L '^platform$' --parallel 4 \
    --output-on-failure --no-tests=error
```

Host-local PLAT-EH-013 record for 2026-07-22:

- Windows 10.0.26200, MSVC 19.51.36248.0, Debug.
- The fresh cache had tests enabled and examples, render, and UI/render
  integration disabled. All six required targets built successfully.
- The single platform-labelled CTest invocation selected 297 tests: 296 passed,
  zero failed, and the native-handle integration probe recorded the one
  permitted skip because the dummy driver could not create a Vulkan-compatible
  hidden window.
- Automated dummy-driver dialog callback coverage passed. No native dialog was
  presented as part of this headless gate.

### Error-Handling Final Verification Gate

PLAT-EH-018 uses otherwise unused Debug, Release, and analysis build
directories. Tests, all four examples, and the process helper are enabled;
render and UI/render integration remain disabled. The Debug and Release test
runs each use one platform-labelled CTest invocation so live tests retain their
configured resource lock.

Windows command shape:

```powershell
$debugBuild = "build\windows-msvc-debug-plat-eh018"
$releaseBuild = "build\windows-msvc-release-plat-eh018"
$platformTargets = @(
    "ponder_platform"
    "ponder_platform_header_tests"
    "ponder_platform_tests"
    "ponder_platform_backend_tests"
    "ponder_platform_integration_tests"
    "ponder_platform_process_helper"
    "ponder_platform_1_window_display_lab"
    "ponder_platform_2_input_drop_monitor"
    "ponder_platform_3_desktop_services_workbench"
    "ponder_platform_4_responsive_process_runner"
)

cmake --fresh --preset windows-msvc-debug -B $debugBuild `
    -DPONDER_BUILD_TESTS=ON `
    -DPONDER_BUILD_EXAMPLES=ON `
    -DPONDER_BUILD_RENDER=OFF `
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build $debugBuild --config Debug --parallel 4 --target $platformTargets
ctest --test-dir $debugBuild -C Debug -L "^platform$" --parallel 4 `
    --output-on-failure --no-tests=error

cmake --fresh --preset windows-msvc-release -B $releaseBuild `
    -DPONDER_BUILD_TESTS=ON `
    -DPONDER_BUILD_EXAMPLES=ON `
    -DPONDER_BUILD_RENDER=OFF `
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build $releaseBuild --config Release --parallel 4 --target $platformTargets
ctest --test-dir $releaseBuild -C Release -L "^platform$" --parallel 4 `
    --output-on-failure --no-tests=error
```

Configure the analysis tree from an x64 Visual Studio Developer environment:

```powershell
$analysisBuild = "build\windows-ninja-analysis-plat-eh018"
cmake --fresh --preset windows-ninja-analysis -B $analysisBuild `
    -DPONDER_BUILD_TESTS=ON `
    -DPONDER_BUILD_EXAMPLES=ON `
    -DPONDER_BUILD_RENDER=OFF `
    -DPONDER_BUILD_UI_RENDER_INTEGRATION=OFF
cmake --build $analysisBuild --parallel 4 --target $platformTargets
```

Run clang-tidy once for each unique compile-database source under
`libs/platform`, `tests/unit/platform`, `tests/integration/platform`, and
`examples/platform`. Visual Studio's compile database includes
`/Zc:preprocessor`; clang-tidy 22 diagnoses that otherwise valid MSVC option as
unused and `/WX` promotes the driver diagnostic. Preserve every project
diagnostic while suppressing only that incompatibility:

```powershell
clang-tidy.exe --quiet -p=$analysisBuild `
    --extra-arg=-Wno-unused-command-line-argument <source>
```

Host-local PLAT-EH-018 record for 2026-07-22:

- Host and tools: Windows 10.0.26200, Windows SDK 10.0.26100.0,
  MSVC 19.51.36248.0, MSBuild 18.8.2, CMake/CTest 4.2.3, Ninja 1.13.2,
  and clang-format/clang-tidy 22.1.3. The required tool check passed after this
  API shell loaded the installed Visual Studio x64 tool paths.
- Both fresh caches used tests/examples on and render/UI-render integration off.
  All ten explicit targets built with warnings as errors in Debug and true
  Release; Release used `/O2 /Ob2 /DNDEBUG`.
- Each platform-labelled CTest invocation selected 297 tests: 296 passed, zero
  failed, and one capability probe skipped. The selected labels contained 48
  unit, 234 backend, and 15 live integration tests.
- The permitted skip was
  `PlatformRuntimeIntegrationTests.ReportsExpectedNativeHandleFailuresUnderDummyDriver`
  because this host's dummy driver could not create a Vulkan-compatible hidden
  window. No other test skipped.
- The eight documented side-effect-light example probes passed in both Debug
  and Release: dummy-driver auto-close for Examples 1 through 3; Example 4
  headless normal/nonzero exits, dummy-driver auto-close, contained injected
  worker failure, and platform-exception invalid-driver failure.
- Four additional Debug runs used the real Windows video driver and auto-closed:
  Example 1 completed its intrusive window/display/state tour; Examples 2 and 3
  completed host startup/event-pump/teardown without clipboard, URI, or dialog
  actions; and Example 4 completed its bounded process/event flow. These were
  automated host smoke runs, not human input checks.
- Live clipboard round-trip tests passed in both configurations and restored the
  previous clipboard text. URI automation exercised invalid input without
  launching a host application. Dummy-driver dialog automation verified
  callback failure marshalling but did not present a native dialog.
- The Ninja analysis tree built all ten targets before analysis and again after
  the final dialog-cleanup diagnostic change. Its compile database yielded 71
  unique scoped translation units: 22 library, 44 unit/header/backend test, one
  integration, and four examples.
- The corrected clang-tidy pass analyzed all 71 files with zero nonzero exits,
  zero error diagnostics, and no third-party diagnostics. It emitted 2,713
  advisory warning lines representing 1,562 unique location/message pairs;
  review of the higher-signal production advisories established no defect.
- Scoped clang-format checking passed for 106 files after mechanically fixing
  three platform-header line wraps. The focused coverage audit passed both
  tests. The manifest audit listed all 57 production files, paired all 18
  public headers with SDL-free self-containment translations, preserved the
  public-core/private-IO-and-SDL dependency boundary, and found exactly eleven
  public `Result` operations.
- Tracked changed-line whitespace and all 11 intended untracked scoped text
  files passed. The final scan found no stale `pond::core` uses, removed
  exception names in code or active guidance, stale result-propagation macros,
  or unapproved public results. At that gate, `pond::platform` remained the
  public namespace under the error-handling task's scope; the subsequent
  2026-07-23 namespace migration establishes `ponder::platform` as the current
  contract. Intentional explanatory references in ADR 0013 and explicitly
  historical task records are excluded from removed-name findings.
- The `noexcept`, destructor, callback, and worker-thread boundary audit found
  operational failures contained by no-throw operations or explicit catch/log
  fallbacks. Remaining termination paths are documented impossible-invariant
  `PONDER_VERIFY` contracts; no C++ exception escapes those boundaries.
- No human-driven keyboard, drag/drop, close-button, native dialog, successful
  external-URI launch, clipboard-example hotkey, or process G/F/A termination
  interaction was performed. Those manual paths remain unverified rather than
  being inferred from automated seam/backend coverage.
- This Windows host has no supported sanitizer preset. Linux Clang ASAN/UBSAN,
  other compilers, Linux/macOS behavior, POSIX process semantics, and native
  interop remain future-matrix work in `future.txt`.
- The gate made no render, UI, or associated-example edits. An already-dirty
  `examples/ui/1-rectangle/Main.cpp` remained untouched and outside the scoped
  diff/dependency audit.

This is host-local automated verification with the manual interactions above
still pending; it is not cross-platform certification or native-dialog
presentation coverage.

### Host-Local Completion Gate

PLAT-021 is the complete local gate for implemented platform services. It should
record exact commands, host OS, compiler, configuration, SDL video driver, and
capability-based skips.

Windows:

```powershell
.\scripts\check-tools.ps1
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target `
    ponder_platform_header_tests `
    ponder_platform_tests `
    ponder_platform_backend_tests `
    ponder_platform_integration_tests
ctest --preset windows-msvc-debug
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release --target `
    ponder_platform_header_tests `
    ponder_platform_tests `
    ponder_platform_backend_tests `
    ponder_platform_integration_tests
ctest --preset windows-msvc-release
.\scripts\build.ps1 -Preset windows-ninja-analysis -ConfigureOnly
.\scripts\format.ps1 -Check
.\scripts\format.ps1 -Check -Tidy -Preset windows-ninja-analysis
git diff HEAD --check
```

Linux:

```sh
scripts/check-tools.sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug --target \
    ponder_platform_header_tests \
    ponder_platform_tests \
    ponder_platform_backend_tests \
    ponder_platform_integration_tests
ctest --preset linux-clang-debug
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release --target \
    ponder_platform_header_tests \
    ponder_platform_tests \
    ponder_platform_backend_tests \
    ponder_platform_integration_tests
ctest --test-dir build/linux-clang-release --output-on-failure
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan --target \
    ponder_platform_tests \
    ponder_platform_backend_tests \
    ponder_platform_integration_tests
ctest --preset linux-clang-asan
scripts/format.sh --check
scripts/format.sh --check --tidy --preset linux-clang-debug
git diff HEAD --check
```

macOS:

```sh
scripts/check-tools.sh
cmake --preset macos-appleclang-debug
cmake --build --preset macos-appleclang-debug --target \
    ponder_platform_header_tests \
    ponder_platform_tests \
    ponder_platform_backend_tests \
    ponder_platform_integration_tests
ctest --preset macos-appleclang-debug
cmake --preset macos-appleclang-release
cmake --build --preset macos-appleclang-release --target \
    ponder_platform_header_tests \
    ponder_platform_tests \
    ponder_platform_backend_tests \
    ponder_platform_integration_tests
ctest --test-dir build/macos-appleclang-release --output-on-failure
scripts/format.sh --check
git diff HEAD --check
```

Run the manual dialog smoke on a capable GUI host as part of PLAT-021. If the
host cannot present native dialogs, record that limitation rather than treating
the service as manually verified.

### Portability Matrix

`future.txt` records actual results for supported hosts. These rows are not implied
by a successful host-local PLAT-021 run.

| Host row | Presets or command shape |
| --- | --- |
| Windows MSVC Debug/Release | `windows-msvc-debug`, `windows-msvc-release` |
| Windows clang-cl Debug | `windows-clangcl-debug` |
| Linux Clang Debug/Release | `linux-clang-debug`, `linux-clang-release` |
| Linux GCC Debug | `linux-gcc-debug` |
| Linux Clang ASAN/UBSAN | `linux-clang-asan` |
| macOS AppleClang Debug/Release | `macos-appleclang-debug`, `macos-appleclang-release` |

For presets without a CTest preset, build first and run:

```sh
ctest --test-dir build/<preset> --output-on-failure
```

For multi-configuration generators, add the configuration:

```powershell
ctest --test-dir build\<preset> -C Debug --output-on-failure
```

Record unrun rows as unverified, not as passes.
