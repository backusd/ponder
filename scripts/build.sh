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
Usage: scripts/build.sh [--preset <name>] [--configure-only] [-- <cmake-build-args>]
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
configure_only=0
build_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            preset="$2"
            shift 2
            ;;
        --configure-only)
            configure_only=1
            shift
            ;;
        --help | -h)
            usage
            exit 0
            ;;
        --)
            shift
            build_args+=("$@")
            break
            ;;
        *)
            build_args+=("$1")
            shift
            ;;
    esac
done

cd "$repo_root"
run cmake --preset "$preset"

if [[ "$configure_only" -eq 0 ]]; then
    run cmake --build --preset "$preset" "${build_args[@]}"
fi