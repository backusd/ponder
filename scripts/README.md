# Scripts

Developer convenience scripts live here.

Scripts should be thin wrappers around canonical tools such as CMake, CTest,
clang-format, and clang-tidy. They should not become a second build system.

## Tool Check

``powershell
.\scripts\check-tools.ps1
``

``sh
scripts/check-tools.sh
``

The PowerShell tool check also searches Visual Studio LLVM install directories
for Clang tools that are installed but not on PATH.

## Build

```sh
scripts/build.sh --preset linux-clang-debug
```

```powershell
.\scripts\build.ps1 -Preset windows-msvc-debug
```

The build scripts run:

1. `cmake --preset <preset>`
2. `cmake --build --preset <preset>`

Pass extra build arguments after `--`.

## Test

```sh
scripts/test.sh --preset linux-clang-debug
```

```powershell
.\scripts\test.ps1 -Preset windows-msvc-debug
```

The test scripts configure, build, and then run `ctest --preset <preset>`.
Pass extra CTest arguments after `--`.

## Format

```sh
scripts/format.sh --check
```

```powershell
.\scripts\format.ps1 -Check
```

By default, the format scripts run `clang-format -i` on project source files.
Use check mode for verification without edits.

The format scripts also support an optional clang-tidy pass:

```sh
scripts/format.sh --check --tidy --preset linux-clang-debug
```

```powershell
.\scripts\format.ps1 -Check -Tidy -Preset windows-msvc-debug
```

`clang-tidy` requires a `compile_commands.json` file in the selected build tree.
On Windows, use `windows-ninja-analysis` from a Visual Studio Developer shell
when you need a Ninja-generated compile database for analysis.
