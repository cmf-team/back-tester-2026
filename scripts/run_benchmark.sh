#!/usr/bin/env bash
# run_benchmark.sh — benchmark runner for the back-tester Hard task.
#
# Runs the binary N times on the given directory and reports per-run
# throughput plus per-strategy min/median/max aggregates.
#
# Usage:
#   scripts/run_benchmark.sh <directory>
#                            [--runs=N]
#                            [--strategy={flat|hierarchy|both}]
#                            [--warmup]
#
# Notes:
#   * First run after boot is cold-cache; subsequent runs read from page
#     cache. Use --warmup to discard the first (cold) run from aggregates.
#   * Throughput is reported in messages per second (msg/s).

set -euo pipefail

usage() {
    cat >&2 <<EOF
Usage: $0 <directory> [--runs=N] [--strategy={flat|hierarchy|both}] [--warmup]

  <directory>          Directory containing *.mbo.json NDJSON files.
  --runs=N             Number of runs (default: 3).
  --strategy=<kind>    flat | hierarchy | both (default: both).
  --warmup             Run once before measured runs; drop it from stats.
EOF
    exit 1
}

[[ $# -ge 1 ]] || usage
DIR="$1"; shift

RUNS=3
STRAT=both
WARMUP=0
for a in "$@"; do
    case "$a" in
        --runs=*)     RUNS="${a#--runs=}" ;;
        --strategy=*) STRAT="${a#--strategy=}" ;;
        --warmup)     WARMUP=1 ;;
        -h|--help)    usage ;;
        *) echo "unknown flag: $a" >&2; usage ;;
    esac
done

case "$STRAT" in flat|hierarchy|both) ;; *) echo "bad strategy: $STRAT" >&2; exit 1;; esac
[[ "$RUNS" =~ ^[0-9]+$ ]] || { echo "bad runs: $RUNS" >&2; exit 1; }

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build/bin/back-tester"

# Build if necessary.
if [[ ! -x "$BIN" ]]; then
    echo "[build] $BIN not found; configuring + building..." >&2
    (cd "$ROOT" && cmake -B build -S . >/dev/null && cmake --build build -j >/dev/null)
fi
[[ -d "$DIR" ]] || { echo "error: directory not found: $DIR" >&2; exit 1; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

file_count=$(find -L "$DIR" -maxdepth 1 -name "*.mbo.json" | wc -l | awk '{print $1}')

echo "=== back-tester benchmark ==="
echo "Directory: $DIR"
echo "Files:     $file_count"
echo "Runs:      $RUNS$([[ $WARMUP -eq 1 ]] && echo ' (+1 warmup, dropped)')"
echo "Strategy:  $STRAT"
echo "Binary:    $BIN"
echo

if [[ $WARMUP -eq 1 ]]; then
    echo "[warmup] prefilling page cache..."
    "$BIN" "$DIR" --strategy="$STRAT" >/dev/null
    echo
fi

for i in $(seq 1 "$RUNS"); do
    log="$TMP/run_$i.log"
    printf "[run %d/%d]\n" "$i" "$RUNS"
    "$BIN" "$DIR" --strategy="$STRAT" > "$log"
    grep -E "^(Flat|Hierarchy):" "$log" | sed 's/^/    /'
done

echo
echo "=== Aggregate throughput (msg/s) ==="
for strat in Flat Hierarchy; do
    case "$STRAT" in
        flat)      [[ "$strat" == "Flat"      ]] || continue ;;
        hierarchy) [[ "$strat" == "Hierarchy" ]] || continue ;;
    esac
    vals=$(for i in $(seq 1 "$RUNS"); do
        grep "^$strat:" "$TMP/run_$i.log" 2>/dev/null \
            | sed 's/.*throughput=\([0-9]*\).*/\1/'
    done | sort -n)
    [[ -n "$vals" ]] || continue
    min=$(echo "$vals" | head -1)
    max=$(echo "$vals" | tail -1)
    n=$(echo "$vals" | wc -l | awk '{print $1}')
    mid=$(( (n + 1) / 2 ))
    median=$(echo "$vals" | sed -n "${mid}p")
    # Render with thousands separators via printf %'d (POSIX-2024; fallback OK).
    printf "%-10s  min=%12d   median=%12d   max=%12d\n" \
        "$strat:" "$min" "$median" "$max"
done
