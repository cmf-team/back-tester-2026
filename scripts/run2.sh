#!/usr/bin/env bash
# run2.sh: build, test and run the HW-2 (multi-instrument) back-tester
# pipeline.  Mirror of scripts/run.sh, but invokes the back-tester2
# binary and forwards extra options through to it.
#
# Usage:
#   scripts/run2.sh <input> [back-tester2 options...]
#
# <input> is either a single NDJSON file or a directory of NDJSON files.
# Examples:
#   scripts/run2.sh data/2025-04-01.json --merger=hierarchy
#   scripts/run2.sh data/             --workers=4 --snapshot-every=100000

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <input> [back-tester2 options...]" >&2
    exit 2
fi

INPUT="$1"
shift

# Resolve input path (file or directory) to an absolute one before we cd.
if [[ ! -e "$INPUT" ]]; then
    echo "input does not exist: $INPUT" >&2
    exit 1
fi
INPUT_ABS="$(cd "$(dirname "$INPUT")" && pwd)/$(basename "$INPUT")"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

pick_tool() {
    local tool="$1"
    if command -v "$tool" >/dev/null 2>&1; then
        command -v "$tool"
    elif [[ -x "$HOME/.local/bin/$tool" ]]; then
        echo "$HOME/.local/bin/$tool"
    else
        return 1
    fi
}

CMAKE="${CMAKE:-$(pick_tool cmake || true)}"
CTEST="${CTEST:-$(pick_tool ctest || true)}"
if [[ -z "${CMAKE:-}" || -z "${CTEST:-}" ]]; then
    echo "cmake/ctest not found. Install CMake or set CMAKE=/path/to/cmake CTEST=..." >&2
    exit 1
fi

section() { printf '\n=== %s ===\n' "$*"; }

if [[ ! -f build/CMakeCache.txt ]]; then
    section "Configure"
    "$CMAKE" -B build -S .
fi

section "Build"
"$CMAKE" --build build -j

section "Tests"
"$CTEST" --test-dir build -j --output-on-failure

section "Run on $INPUT_ABS"
build/bin/back-tester2 "$INPUT_ABS" "$@"
