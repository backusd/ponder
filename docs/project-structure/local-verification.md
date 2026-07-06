# Local Scaffold Verification

Status: PS-017 verification record.

Date: 2026-07-05.

Host: Windows, using the Windows MSVC CMake presets.

## Commands Run

```powershell
.\scripts\build.ps1 -Preset windows-msvc-release
```

Result: passed.

This configured and built the release scaffold, including the initial project
libraries, desktop app, project inspection tool, and all unit-test executables.

```powershell
.\scripts\test.ps1 -Preset windows-msvc-release -SkipBuild
```

Result: passed.

CTest ran 12 tests and all passed.

```powershell
.\scripts\test.ps1 -Preset windows-msvc-debug
```

Result: passed.

This configured, built, and tested the default Windows developer preset. CTest
ran 12 tests and all passed.

```powershell
.\scripts\format.ps1 -Check
```

Result: blocked by local tool availability.

`clang-format` was not found on `PATH`. The wrapper failed before making changes
with a clear missing-command error.

```powershell
where.exe clang-format
where.exe clang-tidy
where.exe clang
where.exe clang++
where.exe gcc
where.exe g++
```

Result: none of these tools were found on `PATH`.

```powershell
cmake --graphviz=build/windows-msvc-release/ponder-targets.dot `
    --preset windows-msvc-release
```

Result: passed.

The generated CMake graph was used for an informal target dependency inspection.

## Target Graph Notes

- Application and tool targets depend downward on reusable libraries.
- `ponder_desktop` currently depends on `ponder_core`.
- `ponder_project_inspect` currently depends on `ponder_core`.
- Each unit-test executable depends on its matching library and `gtest_main`.
- Reusable libraries do not depend on the desktop app, tools, or tests.
- GUI/platform dependencies do not leak into `ponder_core`.
- Third-party dependencies are added through CMake under `third_party/` and are
  isolated with `SYSTEM` and `EXCLUDE_FROM_ALL` where practical.
- Project warnings-as-errors are applied through project target helpers, not by
  globally forcing warning policy onto third-party targets.
- Build outputs and generated files stay under the selected build tree.

## Local Limitations

- Format checking could not be completed because `clang-format` is not installed
  or not on `PATH`.
- clang-tidy could not be completed because `clang-tidy` is not installed or not
  on `PATH`.
- The Linux `linux-clang-asan` preset could not be run from this Windows host.
- Clang, GCC, and their C++ drivers were not available on `PATH` on this host.
- Sanitizers are intentionally not configured for MSVC in the current scaffold.
- SDL reported missing optional `PkgConfig` and `LibUSB` during configure. This
  did not block the Windows MSVC configure, build, or test runs.

## Current Conclusion

The scaffold configures, builds, and tests cleanly on the Windows MSVC debug and
release presets available on this machine.

The remaining verification gaps are local toolchain availability gaps, not
source-level scaffold failures. Future verification should rerun format,
clang-tidy, and Clang/GCC sanitizer checks after those tools are installed or on
a Linux/macOS host with the matching presets available.
