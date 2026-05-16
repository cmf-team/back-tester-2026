#!/usr/bin/env bash
# Benchmark the hard-task merge strategies on a folder dataset.
#
# Usage:
#   ./scripts/benchmark.sh <json-folder> [feather-folder]
#
# Prints two paste-ready report blocks:
#   1. ingestion/merge with the logging processor
#   2. ingestion/merge plus LOB reconstruction

set -euo pipefail

usage() {
  cat <<USAGE
Usage:
  ./scripts/benchmark.sh <json-folder> [feather-folder]
USAGE
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage >&2
  exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JSON_FOLDER="$1"
FEATHER_FOLDER="${2:-}"

if [[ -n "$FEATHER_FOLDER" ]]; then
  BUILD_DIR="$ROOT/build-arrow"
else
  BUILD_DIR="$ROOT/build"
fi
BIN="$BUILD_DIR/ingest"

cmake_args=(-S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release)
if [[ -n "$FEATHER_FOLDER" ]]; then
  cmake_args+=(-DENABLE_ARROW=ON)
else
  cmake_args+=(-DENABLE_ARROW=OFF)
fi

cmake "${cmake_args[@]}" >/dev/null
cmake --build "$BUILD_DIR" -j >/dev/null

[[ -d "$JSON_FOLDER" ]] || { echo "missing JSON benchmark folder: $JSON_FOLDER" >&2; exit 1; }
if [[ -n "$FEATHER_FOLDER" ]]; then
  [[ -d "$FEATHER_FOLDER" ]] || { echo "missing Feather benchmark folder: $FEATHER_FOLDER" >&2; exit 1; }
fi

run_single_format() {
  "$BIN" --benchmark "$JSON_FOLDER" "$@"
}

run_json_feather_comparison() {
  local json_output
  local feather_output
  json_output="$(mktemp)"
  feather_output="$(mktemp)"

  "$BIN" --benchmark "$JSON_FOLDER" "$@" >"$json_output"
  "$BIN" --benchmark "$FEATHER_FOLDER" --input-format feather "$@" >"$feather_output"

  sed -n '1,2p' "$json_output"
  sed -n '3,$p' "$json_output"
  sed -n '3,$p' "$feather_output"

  rm -f "$json_output" "$feather_output"
}

if [[ -n "$FEATHER_FOLDER" ]]; then
  run_json_feather_comparison
  echo
  run_json_feather_comparison --lob
else
  run_single_format
  echo
  run_single_format --lob
fi
