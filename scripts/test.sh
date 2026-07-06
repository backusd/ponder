#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

default_preset()
{
    case "$(uname -s)" in
        Darwin*) echo "macos-appleclang-debug" ;;
        Linux*) echo "linux-clang-debug" ;;
        CYGWIN* | MINGW* | MSYS*) echo "windows-msvc-debug" ;;
        *) echo "windows-msvc-debug" ;;
    esac
}

usage()
{
    cat <<'EOF'
Usage: scripts/test.sh [--preset <name>] [--skip-build] [-- <ctest-args>]
EOF
}

run()
{
    printf '+'
    printf ' %q' "$@"
    printf '\n'
    "$@"
}

preset="$(default_preset)"
skip_build=0
test_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            preset="$2"
            shift 2
            ;;
        --skip-build)
            skip_build=1
            shift
            ;;
        --help | -h)
            usage
            exit 0
            ;;
        --)
            shift
            test_args+=("$@")
            break
            ;;
        *)
            test_args+=("$1")
            shift
            ;;
    esac
done

cd "$repo_root"
run cmake --preset "$preset"

if [[ "$skip_build" -eq 0 ]]; then
    run cmake --build --preset "$preset"
fi

run ctest --preset "$preset" "${test_args[@]}"