#!/usr/bin/env bash
# Benchmark the hard-task merge strategies on a folder dataset.
#
# Usage:
#   ./scripts/benchmark.sh <folder>
#   ./scripts/benchmark.sh <folder> [single_file]
#
# Standard mode reads one file, so it is only included when a matching single-file
# dataset is supplied. Flat and hierarchy are timed by the built-in --benchmark mode.

set -euo pipefail

usage() {
  cat <<USAGE
Usage:
  ./scripts/benchmark.sh <folder>
  ./scripts/benchmark.sh <folder> [single_file]
USAGE
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage >&2
  exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build"
BIN="$BUILD_DIR/ingest"

FOLDER="$1"
SINGLE="${2:-}"

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" -j >/dev/null

[[ -d "$FOLDER" ]] || { echo "missing benchmark folder: $FOLDER" >&2; exit 1; }
if [[ -n "$SINGLE" && ! -f "$SINGLE" ]]; then
  echo "missing standard input file: $SINGLE" >&2
  exit 1
fi

print_standard_row() {
  local input_file="$1"
  local summary
  local messages
  local violations
  local seconds
  local throughput

  "$BIN" --mode standard --input "$input_file" --print-events 0 >/dev/null
  summary="$("$BIN" --mode standard --input "$input_file" --print-events 0 --verbose 2>/dev/null)"
  messages="$(awk -F= '/^total_messages_processed=/ { print $2; exit }' <<<"$summary")"
  violations="$(awk -F= '/^chronological_violations=/ { print $2; exit }' <<<"$summary")"

  if [[ -z "$messages" || -z "$violations" ]]; then
    echo "failed to parse Standard-mode summary for: $input_file" >&2
    exit 1
  fi

  TIMEFORMAT='%R'
  seconds=$({ time "$BIN" --mode standard --input "$input_file" --print-events 0 >/dev/null; } 2>&1)
  throughput="$(awk -v n="$messages" -v t="$seconds" 'BEGIN { if (t > 0) printf "%.2f", n / t; else printf "0.00" }')"

  echo "standard,$messages,$violations,$seconds,$throughput"
}

echo "Strategy,Messages,ChronologicalViolations,WallClockSeconds,ThroughputMessagesPerSecond"
if [[ -n "$SINGLE" ]]; then
  print_standard_row "$SINGLE"
fi

"$BIN" --benchmark "$FOLDER" | tail -n +3
