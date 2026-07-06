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
Usage: scripts/format.sh [--check] [--tidy] [--preset <name>]
EOF
}

run()
{
    printf '+'
    printf ' %q' "$@"
    printf '\n'
    "$@"
}

require_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command '$1' was not found on PATH." >&2
        exit 1
    fi
}

collect_source_files()
{
    local roots=()
    local root

    for root in apps libs plugins tests tools; do
        if [[ -d "$root" ]]; then
            roots+=("$root")
        fi
    done

    if [[ "${#roots[@]}" -eq 0 ]]; then
        return 0
    fi

    find "${roots[@]}" -type f \
        \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \
        -o -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' \) |
        sort
}

preset="$(default_preset)"
check=0
tidy=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            preset="$2"
            shift 2
            ;;
        --check)
            check=1
            shift
            ;;
        --tidy)
            tidy=1
            shift
            ;;
        --help | -h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

cd "$repo_root"
require_command clang-format

mapfile -t source_files < <(collect_source_files)
if [[ "${#source_files[@]}" -eq 0 ]]; then
    echo "No source files found."
    exit 0
fi

for file in "${source_files[@]}"; do
    if [[ "$check" -eq 1 ]]; then
        run clang-format --dry-run --Werror "$file"
    else
        run clang-format -i "$file"
    fi
done

if [[ "$tidy" -eq 1 ]]; then
    require_command clang-tidy

    build_dir="build/${preset}"
    compile_database="${build_dir}/compile_commands.json"
    if [[ ! -f "$compile_database" ]]; then
        echo "clang-tidy requires '${compile_database}'." >&2
        echo "Configure a preset that exports compile_commands.json first." >&2
        exit 1
    fi

    compile_files=()
    for file in "${source_files[@]}"; do
        case "$file" in
            *.cc | *.cpp | *.cxx) compile_files+=("$file") ;;
        esac
    done

    for file in "${compile_files[@]}"; do
        run clang-tidy "$file" -p "$build_dir"
    done
fi