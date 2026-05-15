#!/usr/bin/env bash
set -euo pipefail

# Build ingest with profiling-friendly flags:
#   -O3                       full release optimizations
#   -g                        DWARF debug info (symbol-resolved profiles)
#   -fno-omit-frame-pointer   accurate stack walking on aarch64
#   -DNDEBUG                  strip asserts (matches release semantics)
#
# Output: build/ingest, ready to be sampled by /usr/bin/sample, samply, etc.
#
# Usage: ./build_profiling.sh

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake not found on PATH. Try one of:" >&2
    echo "  export PATH=\"/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin:\$PATH\"" >&2
    echo "  brew install cmake" >&2
    exit 1
fi

echo "Configuring (RelWithDebInfo, -O3 -g -fno-omit-frame-pointer)..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O3 -g -fno-omit-frame-pointer -DNDEBUG" \
    >/dev/null

echo "Building..."
cmake --build "$BUILD_DIR" >/dev/null

INGEST_BIN="$BUILD_DIR/ingest"
if [[ ! -x "$INGEST_BIN" ]]; then
    echo "Build did not produce expected binary: $INGEST_BIN" >&2
    exit 1
fi

echo "Done: $INGEST_BIN"
echo
echo "Verify debug info is present:"
echo "  dwarfdump --debug-info $INGEST_BIN | head -5"
echo
echo "Next step: profile a run without rebuilding"
echo "  SKIP_BUILD=1 ./profile_standard.sh"
