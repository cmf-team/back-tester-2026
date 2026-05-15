#!/usr/bin/env bash
set -euo pipefail

# Profile the Standard runner using:
#   1) macOS built-in /usr/bin/sample (always; produces a textual report)
#   2) samply (if installed; opens an interactive flamegraph in the browser)
#
# Usage:
#   ./profile_standard.sh                          # uses bundled sample input
#   ./profile_standard.sh path/to/file.ndjson      # custom input
#
# Environment overrides:
#   SAMPLE_OUT          Output report path (default: /tmp/ingest_standard.sample.txt)
#   SAMPLE_DURATION     Max sampling seconds (default: 60; ends early when process exits)
#   SAMPLE_INTERVAL_MS  Sample interval in ms (default: 1)
#   BUILD=1             Build before profiling (default: skip; use ./build_profiling.sh to build)
#   SKIP_SAMPLY=1       Skip the samply pass even if samply is installed

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
INPUT="${1:-$ROOT_DIR/data/sample.ndjson}"
SAMPLE_OUT="${SAMPLE_OUT:-/tmp/ingest_standard.sample.txt}"
SAMPLE_DURATION="${SAMPLE_DURATION:-60}"
SAMPLE_INTERVAL_MS="${SAMPLE_INTERVAL_MS:-1}"

if [[ ! -e "$INPUT" ]]; then
    echo "Input not found: $INPUT" >&2
    exit 1
fi

if [[ "${BUILD:-0}" == "1" ]]; then
    if ! command -v cmake >/dev/null 2>&1; then
        echo "cmake not found on PATH. Try one of:" >&2
        echo "  export PATH=\"/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin:\$PATH\"" >&2
        echo "  brew install cmake" >&2
        exit 1
    fi

    echo "[1/4] Building (RelWithDebInfo, -O3 -g -fno-omit-frame-pointer)..."
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O3 -g -fno-omit-frame-pointer -DNDEBUG" \
        >/dev/null
    cmake --build "$BUILD_DIR" >/dev/null
else
    echo "[1/4] Using existing binary (set BUILD=1 to rebuild; or run ./build_profiling.sh)"
fi

INGEST_BIN="$BUILD_DIR/ingest"
if [[ ! -x "$INGEST_BIN" ]]; then
    echo "Binary not found or not executable: $INGEST_BIN" >&2
    exit 1
fi

INGEST_PID=""
cleanup() {
    if [[ -n "$INGEST_PID" ]] && kill -0 "$INGEST_PID" 2>/dev/null; then
        kill "$INGEST_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

echo "[2/4] Profiling Standard run with /usr/bin/sample on $INPUT"
"$INGEST_BIN" --mode standard --input "$INPUT" --print-events 0 >/dev/null &
INGEST_PID=$!

# Tiny delay so /usr/bin/sample can attach before short runs finish.
sleep 0.05

# /usr/bin/sample exits early if the target process terminates first.
/usr/bin/sample "$INGEST_PID" "$SAMPLE_DURATION" "$SAMPLE_INTERVAL_MS" \
    -file "$SAMPLE_OUT" >/dev/null

wait "$INGEST_PID" || true
INGEST_PID=""

echo "[3/4] Wrote textual report: $SAMPLE_OUT"

if [[ "${SKIP_SAMPLY:-0}" == "1" ]]; then
    echo "[4/4] Skipping samply pass (SKIP_SAMPLY=1)"
elif command -v samply >/dev/null 2>&1; then
    echo "[4/4] Profiling again with samply (browser will open with flamegraph)"
    echo "      Close the browser tab and Ctrl-C samply when done."
    # samply records its own profile and auto-launches the Firefox Profiler UI;
    # blocks until the user closes the tab + Ctrl-C samply.
    samply record -- \
        "$INGEST_BIN" --mode standard --input "$INPUT" --print-events 0 >/dev/null
else
    echo "[4/4] samply not installed; skipping interactive flamegraph pass."
    echo "      Install with:  brew install samply"
fi

echo
echo "View the textual report:"
echo "  less $SAMPLE_OUT"
echo
echo "Find the heaviest hot leaf frames (top of stack, what's actually running):"
echo "  awk '/^Sort by top of stack/,/^[[:space:]]*\$/' $SAMPLE_OUT | head -40"
echo
echo "Find the heaviest call-tree branches (what called the hot frames):"
echo "  awk '/^Call graph:/,/^Total number/' $SAMPLE_OUT | head -80"
