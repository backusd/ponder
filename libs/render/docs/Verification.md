# Render Verification Matrix

Status: active for the desktop bootstrap milestone. Deterministic, install-consumer, and default
live Windows evidence passes, but the milestone is not validation-clean or closed while the
required validation layer and remaining REND-020 evidence are unavailable. The render milestone is
limited to Vulkan on Windows and Linux; macOS Vulkan is intentionally out of scope.

## Coverage Groups

| Group | CTest labels | Scope | Expected host behavior |
| --- | --- | --- | --- |
| Public/value contract | `render;unit;public;value;deterministic` | Header-visible values, validation helpers, errors, and API traits. | Runs everywhere render builds. |
| Fake-dispatch backend lifecycle | `render;backend;fake-dispatch;lifecycle;rollback;deterministic` | Vulkan loader/instance/device/WSI success and controlled rollback paths without live windows. | Runs everywhere render builds. |
| Public header self-containment | `render;headers;public;no-pch;deterministic` | One PCH-free translation unit per public render header. | Runs everywhere render builds. |
| Public build consumer | `render;consumer;public;no-pch;deterministic` | Representative code that consumes only public `ponder::core`, `ponder::platform`, and `ponder::render` targets/includes. | Runs everywhere render builds. |
| Install-tree consumer | `render;install;consumer;public;deterministic` | A standalone CMake project finds an installed `ponder` package and builds against public targets only. | Runs when configured with `PONDER_ENABLE_INSTALL_RULES=ON`. |
| Live Vulkan/platform integration | `render;integration;live;vulkan;platform` | Real platform windows, Vulkan surfaces, adapter/device creation, swapchains, clear/present, resize, minimize/restore, teardown. | Serialized by `ponder_platform_sdl`; skips on general jobs without live support unless required. |

Use `-L render -LE live` for deterministic coverage and `-L vulkan` for render live presentation coverage.

## Deterministic Local Commands

From a Visual Studio Developer Command Prompt on Windows:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target ponder_render_tests ponder_render_backend_tests ponder_render_header_tests ponder_render_public_consumer_tests
ctest --preset windows-msvc-debug -L render -LE live --output-on-failure --no-tests=error
```

The same shape applies on Linux with the existing Linux debug presets when running on a Linux host:

```bash
cmake --preset <linux-debug-preset>
cmake --build --preset <linux-debug-preset> --target ponder_render_tests ponder_render_backend_tests ponder_render_header_tests ponder_render_public_consumer_tests
ctest --preset <linux-debug-preset> -L render -LE live --output-on-failure --no-tests=error
```

Linux live presentation remains unverified until CI or a developer host runs it with X11/Wayland
surface support. Linux configure/compile coverage should be enabled as soon as CI has a Linux job.

## Install-Tree Consumer Commands

The install consumer is gated by `PONDER_ENABLE_INSTALL_RULES=ON` because it intentionally installs
the current build into a temporary prefix and configures a standalone consumer from that prefix.

```powershell
cmake -S . -B build/render-install-smoke -DPONDER_ENABLE_INSTALL_RULES=ON
ctest --test-dir build/render-install-smoke -C Debug -L install --output-on-failure --no-tests=error
```

The consumer must compile without private include paths, precompiled headers, Vulkan SDK assumptions,
or private third-party targets in user code.

## Live Windows Validation Commands

General-purpose jobs may skip live tests when the Vulkan loader, validation layer, presentation
surface, or suitable adapter is unavailable. A render-support environment must require live support:

```powershell
cmake -S . -B build/render-live-standard -DPONDER_RENDER_REQUIRE_LIVE_TESTS=ON -DPONDER_RENDER_LIVE_VALIDATION_MODE=Standard
cmake --build build/render-live-standard --target ponder_render_integration_tests --config Debug
ctest --test-dir build/render-live-standard -C Debug -L vulkan --output-on-failure --timeout 300 --no-tests=error
```

For the owner Windows laptop, build and run every validation-mode candidate because configure
success does not prove that the installed validation layer supports a mode. Use a separate build
tree for each candidate:

```powershell
cmake -S . -B build/render-live-gpu-assisted -DPONDER_RENDER_REQUIRE_LIVE_TESTS=ON -DPONDER_RENDER_LIVE_VALIDATION_MODE=GpuAssisted
cmake --build build/render-live-gpu-assisted --target ponder_render_integration_tests --config Debug
ctest --test-dir build/render-live-gpu-assisted -C Debug -L vulkan --output-on-failure --timeout 300 --no-tests=error

cmake -S . -B build/render-live-best-practices -DPONDER_RENDER_REQUIRE_LIVE_TESTS=ON -DPONDER_RENDER_LIVE_VALIDATION_MODE=BestPractices
cmake --build build/render-live-best-practices --target ponder_render_integration_tests --config Debug
ctest --test-dir build/render-live-best-practices -C Debug -L vulkan --output-on-failure --timeout 300 --no-tests=error

cmake -S . -B build/render-live-sync -DPONDER_RENDER_REQUIRE_LIVE_TESTS=ON -DPONDER_RENDER_LIVE_VALIDATION_MODE=Synchronization
cmake --build build/render-live-sync --target ponder_render_integration_tests --config Debug
ctest --test-dir build/render-live-sync -C Debug -L vulkan --output-on-failure --timeout 300 --no-tests=error
```

Record each runtime result and designate the highest passing candidate as the strongest supported
mode. An unsupported mode must fail as a required live job; it is not evidence against the modes
that pass.

## Required Live Scenarios

The live integration label covers the first milestone scenarios:

- bootstrap instance/device creation and shutdown;
- owner-thread surface creation and transfer to the caller-selected render thread;
- swapchain negotiation for one and two independent windows;
- repeated clear/submit/present loops;
- resize-triggered swapchain recreation;
- minimize/zero-size suspension and restore recreation;
- asymmetric two-window lifecycles where one window resizes/minimizes/destroys while the other keeps presenting;
- close/hidden states without assuming immediate platform destruction;
- renderer target/device/bootstrap teardown in the documented order.

## Current Windows Host Evidence

Recorded on 2026-07-14 from Windows 10.0.26200 with MSVC 19.51.36248.0. The
installed Vulkan loader reports instance version 1.4.335. The live adapters are
an NVIDIA GeForce RTX 3070 Laptop GPU (Vulkan 1.4.325) and AMD Radeon Graphics
(Vulkan 1.3.212).

| Gate | Result |
| --- | --- |
| Deterministic render matrix | Passed, 147/147. |
| Clean install-tree consumer | Passed, 1/1; the standalone executable linked and ran out-of-line core, platform, and render APIs. |
| Default-mode live scenarios | Passed, 3/3. |
| Required Standard validation | Built and ran, 0/3: required failure because `VK_LAYER_KHRONOS_validation` is not installed. |
| Required GPU-assisted candidate | Built and ran, 0/3 for the same missing validation layer. |
| Required best-practices candidate | Built and ran, 0/3 for the same missing validation layer. |
| Required synchronization candidate | Built and ran, 0/3 for the same missing validation layer. |
| Strongest supported validation mode | None on this host while the Khronos validation layer is absent. |
| UI-000 renderer prerequisite gate | Not closed: required validation and the remaining REND-020 closeout evidence are still outstanding. |

`vulkaninfo --summary` confirms that only the AMD switchable-graphics and NVIDIA
Optimus/presentation vendor layers are discoverable. The required jobs failed
rather than skipping, which verifies the required-job policy but does not
satisfy the validation-clean release gate. Install the Khronos validation layer
and rerun Standard plus all three candidates before claiming required validation
coverage.

### Acceptance Disposition

The source and deterministic renderer architecture are suitable for extension. Formal REND-018,
REND-020, and UI-000 acceptance remains open. Default-mode live success must not be described as
validation-clean success, and a correctly failing required job proves gate enforcement rather than
gate completion.

## Failure And Rollback Matrix

The deterministic fake-dispatch backend tests are the authority for failure coverage. They should
cover every host without a live Vulkan dependency and must exercise rollback for:

- loader and global dispatch failures;
- instance extension, validation layer, and debug messenger failures;
- surface creation and WSI capability failures;
- queue-family and presentation-support mismatches;
- adapter rejection and missing required swapchain capability;
- logical device and allocator creation failures;
- swapchain/image/view/command/synchronization failures;
- acquire, submit, present, surface-loss, device-loss, and out-of-memory paths.

Live tests should report backend diagnostics for unexpected failures, but they should not be the only
place a rollback path is tested.

## Sanitizers And Lifetime Permutations

Current Windows MSVC coverage is compile/run with warnings-as-errors only. The project sanitizer
helper deliberately enables no sanitizer for MSVC, and no Windows AddressSanitizer run may be
claimed until a supported configuration is implemented and executed. Windows sanitizer coverage is
therefore unavailable.

Linux Clang ASan/UBSan is an unexecuted future gate until a Linux configuration and CI job build and
run the deterministic render matrix with both sanitizers enabled.

Lifetime permutations to keep covered in deterministic or live tests:

- move construction and move assignment of bootstrap/device/surface/target/frame wrappers;
- unconsumed prepared-surface destruction;
- target destruction before device destruction;
- device destruction before bootstrap destruction;
- failure rollback after partial instance, device, swapchain, and frame setup;
- recovery after surface loss using a fresh owner-thread surface.

## Deferrals

Pixel readback, golden images, timing/performance thresholds, broad GPU/driver matrices, Metal, and
higher-level 3D or UI rendering are intentionally deferred. The implemented bootstrap has the
source architecture needed by the project-owned rectangle and generic 2D work. Formal REND-020 and
UI-000 readiness is not claimed until the required validation and closeout evidence above pass; no
visual-fidelity claim extends beyond successful clear/present.
