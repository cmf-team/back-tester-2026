#!/usr/bin/env bash
#
# run.sh - one-shot helper for the HW-1 ingestion driver.
#
# Configures (once), builds, runs unit tests, and then runs back-tester
# on the daily NDJSON file passed as the only argument.
#
# Usage:
#   scripts/run.sh <daily-ndjson-file>
#
# Can be invoked from any working directory — the script resolves its own
# location and operates relative to the project root.

set -euo pipefail

usage() {
    cat >&2 <<EOF
usage: $(basename "$0") <daily-ndjson-file>

Configures (if needed), builds the project, runs unit tests, and then
executes build/bin/back-tester on the given file. Only the last 50 lines
of the back-tester output are shown.
EOF
    exit 2
}

if [[ $# -ne 1 ]]; then
    usage
fi

INPUT="$1"
if [[ ! -f "$INPUT" ]]; then
    echo "error: not a regular file: $INPUT" >&2
    exit 1
fi

# Resolve to an absolute path BEFORE we cd into the project root.
INPUT_ABS="$(cd "$(dirname "$INPUT")" && pwd)/$(basename "$INPUT")"

# scripts/ lives inside the project; jump up one level to the project root.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# Prefer cmake/ctest from PATH, fall back to ~/.local/bin (the location used
# by the portable Kitware tarball install).
pick_tool() {
    local name="$1"
    local from_path
    from_path="$(command -v "$name" || true)"
    if [[ -n "$from_path" ]]; then
        echo "$from_path"
        return
    fi
    if [[ -x "$HOME/.local/bin/$name" ]]; then
        echo "$HOME/.local/bin/$name"
        return
    fi
    echo ""
}

CMAKE="${CMAKE:-$(pick_tool cmake)}"
CTEST="${CTEST:-$(pick_tool ctest)}"

if [[ -z "$CMAKE" || -z "$CTEST" ]]; then
    echo "error: cmake/ctest not found in PATH (or ~/.local/bin)" >&2
    echo "       see SolutionDescription.md / BuildSystemNotes.md for setup" >&2
    exit 1
fi

section() {
    printf '\n\033[1;36m== %s ==\033[0m\n' "$*"
}

if [[ ! -f build/CMakeCache.txt ]]; then
    section "Configure"
    "$CMAKE" -B build -S .
fi

section "Build"
"$CMAKE" --build build -j

section "Tests"
"$CTEST" --test-dir build -j --output-on-failure

section "Run on $INPUT_ABS (tail -50)"
build/bin/back-tester "$INPUT_ABS" | tail -50
