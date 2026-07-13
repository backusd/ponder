# Headless And Host Verification

Status: platform verification guidance current as of 2026-07-10.

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
  - text-input activation and IME rectangle storage where SDL reports success.
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

- `PlatformRuntimeIntegrationTests.ReportsNativeHandleUnsupportedUnderDummyDriver`
  may skip when the dummy driver cannot create a Vulkan-compatible hidden window
  on this host. It must fail when the default-window `InvalidArgument` check
  fails, when a Vulkan-compatible dummy window is created but native-handle query
  does not return `Unsupported`, or when runtime/window creation fails before the
  documented capability check.

Do not add broad `try`/`catch` or "if SDL failed, skip" paths. Unexpected
failures should remain failures with the SDL diagnostic preserved in the test
output.

## Manual Dialog Smoke

Automated dialog tests exercise the deterministic callback-marshalling seam.
They do not open native OS dialogs. Native presentation is an opt-in manual
check on a capable GUI host.

Manual smoke procedure:

1. Start from a clean GUI session where opening native dialogs is acceptable.
2. Create `PlatformRuntime` on the process-entry thread.
3. Create one visible parent `Window`.
4. For each dialog API, open and cancel once without a parent and once with
   `parentWindowId = parent.GetId()`:
   - `ShowOpenFileDialog(OpenFileDialogDesc)`
   - `ShowSaveFileDialog(SaveFileDialogDesc)`
   - `ShowOpenFolderDialog(OpenFolderDialogDesc)`
5. Poll until the matching `DialogCompletedEvent` arrives.
6. Confirm the outcome is `DialogCancellation`.
7. Do not destroy the parent window or runtime until every requested completion
   has been consumed.
8. Record host OS, compiler/configuration, SDL video driver, dialog kind,
   parented/unparented status, and result.

On a headless or remote host where native dialogs cannot be presented, record
dialog presentation as unverified. Do not count the automated callback tests as
native dialog presentation coverage.

If a PLAT-021 host-local gate cannot run this smoke procedure, label the result
as automated host-local verification with manual native dialog presentation
pending. Do not call the complete host-local gate fully closed until the
open/cancel matrix above is performed and recorded on a capable GUI host.

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

### Renderer-Ready Gate

PLAT-013 uses one normal supported Debug developer preset and records the result
as renderer-ready only for that host.

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target `
    ponder_platform_header_tests `
    ponder_platform_tests `
    ponder_platform_backend_tests `
    ponder_platform_integration_tests
build\windows-msvc-debug\bin\Debug\ponder_platform_tests.exe
build\windows-msvc-debug\bin\Debug\ponder_platform_backend_tests.exe
ctest --test-dir build\windows-msvc-debug -C Debug -L integration --output-on-failure
```

Use the matching Debug preset on Linux or macOS when that is the current host:

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug --target \
    ponder_platform_header_tests \
    ponder_platform_tests \
    ponder_platform_backend_tests \
    ponder_platform_integration_tests
ctest --test-dir build/linux-clang-debug -L platform --output-on-failure
```

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
