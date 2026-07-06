# Developer Setup

This document describes the expected local tools for building and checking
`ponder` from source. The project optimizes first for developer source builds.

## Required Core Tools

All developers need:

- Git with submodule support.
- CMake 3.28 or newer.
- CTest, installed with CMake.
- A C++23-capable compiler.

Recommended additional tools:

- Ninja, especially for compile database generation and local analysis presets.
- clang-format and clang-tidy.
- A POSIX shell on Linux and macOS. On Windows, PowerShell scripts are the
  canonical scripts.

Run the local tool check from the repository root:

```powershell
.\scripts\check-tools.ps1
```

```sh
scripts/check-tools.sh
```

The shell script is intended for Linux and macOS. It may also work under Git
Bash or WSL on Windows, but that is not the primary Windows path.

## Windows

The primary Windows build path uses Visual Studio with MSVC:

```powershell
.\scripts\build.ps1 -Preset windows-msvc-debug
.\scripts\test.ps1 -Preset windows-msvc-debug
```

Install Visual Studio with:

- Desktop development with C++.
- MSVC C++ build tools.
- A Windows SDK.
- C++ Clang tools for Windows.

Visual Studio may install LLVM tools without adding them to `PATH`. On this
machine they were found under:

```text
C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin
```

The tools can be executed directly from that directory, but the existing format
script expects `clang-format` and `clang-tidy` to be on `PATH`. Add the LLVM
`bin` directory to your developer shell `PATH` if you want:

```powershell
.\scripts\format.ps1 -Check
.\scripts\format.ps1 -Check -Tidy -Preset windows-ninja-analysis
```

The `windows-ninja-analysis` preset exists to generate `compile_commands.json`
for tools such as clang-tidy. Run it from a Visual Studio Developer PowerShell
or Developer Command Prompt so `cl`, `link`, the Windows SDK, and MSVC include
and library paths are available. If you prefer not to modify your global
environment, put machine-specific compiler paths in `CMakeUserPresets.json`.

## Linux

Install a C++23-capable Clang or GCC toolchain, CMake, CTest, Git, and Ninja.
The initial Linux presets are:

```sh
scripts/build.sh --preset linux-clang-debug
scripts/test.sh --preset linux-clang-debug
scripts/build.sh --preset linux-gcc-debug
```

The sanitizer preset is intended for Clang/GCC environments:

```sh
scripts/test.sh --preset linux-clang-asan
```

## macOS

Install Xcode or the Xcode command-line tools, CMake, Git, and Ninja. The
initial macOS presets use AppleClang:

```sh
scripts/build.sh --preset macos-appleclang-debug
scripts/test.sh --preset macos-appleclang-debug
```

## Local Overrides

`CMakeUserPresets.json` is intentionally ignored by Git. Use it for:

- Local compiler paths.
- Alternate generators.
- Toolchain files.
- Build directories outside the default `build/<preset>` tree.

Keep shared presets portable. Do not commit machine-specific paths to
`CMakePresets.json`.
