#!/usr/bin/env sh
set -eu

strict=0
if [ "${1:-}" = "--strict" ]; then
    strict=1
fi

required_failures=0
recommended_failures=0

check_tool() {
    label="$1"
    command_name="$2"
    level="$3"

    if command -v "$command_name" >/dev/null 2>&1; then
        tool_path="$(command -v "$command_name")"
        if version_output="$("$command_name" --version 2>&1)"; then
            first_line="$(printf "%s\n" "$version_output" | sed -n '1p')"
            printf '[OK] %s (%s)\n' "$label" "$level"
            printf '     path: %s\n' "$tool_path"
            printf '     check: %s\n' "$first_line"
            return
        fi

        printf '[BROKEN] %s (%s)\n' "$label" "$level"
        printf '         path: %s\n' "$tool_path"
    else
        printf '[MISSING] %s (%s)\n' "$label" "$level"
        printf '          path: <not found>\n'
    fi

    if [ "$level" = "required" ]; then
        required_failures=$((required_failures + 1))
    else
        recommended_failures=$((recommended_failures + 1))
    fi
}

check_tool "Git" "git" "required"
check_tool "CMake" "cmake" "required"
check_tool "CTest" "ctest" "required"
check_tool "Ninja" "ninja" "recommended"
check_tool "clang-format" "clang-format" "recommended"
check_tool "clang-tidy" "clang-tidy" "recommended"
check_tool "clang++" "clang++" "recommended"
check_tool "GCC" "gcc" "recommended"
check_tool "G++" "g++" "recommended"

printf '\nNotes:\n'
printf '- Linux and macOS presets require a C++23-capable compiler.\n'
printf '- clang-format and clang-tidy are needed for local formatting/static analysis.\n'
printf '- The Windows PowerShell script performs extra Visual Studio LLVM discovery.\n'

if [ "$required_failures" -gt 0 ]; then
    printf '\n%s required tool check(s) failed.\n' "$required_failures" >&2
    exit 1
fi

if [ "$strict" -eq 1 ] && [ "$recommended_failures" -gt 0 ]; then
    printf '\n%s recommended tool check(s) failed.\n' "$recommended_failures" >&2
    exit 1
fi

if [ "$recommended_failures" -gt 0 ]; then
    printf '\n%s recommended tool check(s) failed.\n' "$recommended_failures"
fi
