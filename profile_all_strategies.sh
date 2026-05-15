#!/usr/bin/env bash
set -euo pipefail

# Profile all supported strategies on all local datasets under ./data.
# Expected project layout:
#   project-root/
#     CMakeLists.txt
#     data/<dataset-folder>/*.mbo.json
#     src/...
#
# Usage:
#   ./profile_all_strategies.sh
#
# Optional environment variables:
#   DATA_ROOT=/path/to/data
#   PERF=/path/to/perf
#   BUILD_DIR=/path/to/build-prof
#   OUT_DIR=/path/to/output-dir
#   PRINT_EVENTS=0
#   RUN_STANDARD=1
#   RUN_FLAT=1
#   RUN_HIERARCHY=1
#   SKIP_BUILD=0
#   DO_STAT=1
#   DO_RECORD=1
#   STANDARD_FREQ=99
#   HARD_FREQ=49
#   STANDARD_FILE_SELECTION=random  # random or first
#   STANDARD_RANDOM_SEED=123        # optional reproducible random selection via shuf random source

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -f "$SCRIPT_DIR/CMakeLists.txt" ]]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [[ -f "$SCRIPT_DIR/../CMakeLists.txt" ]]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  PROJECT_ROOT="$(pwd)"
fi

cd "$PROJECT_ROOT"

DATA_ROOT="${DATA_ROOT:-$PROJECT_ROOT/data}"
PERF="${PERF:-$HOME/WSL2-Linux-Kernel/tools/perf/perf}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-prof}"
OUT_DIR="${OUT_DIR:-$PROJECT_ROOT/logs/profile/all_datasets_$(date +%Y%m%d_%H%M%S)}"
PRINT_EVENTS="${PRINT_EVENTS:-0}"
RUN_STANDARD="${RUN_STANDARD:-1}"
RUN_FLAT="${RUN_FLAT:-1}"
RUN_HIERARCHY="${RUN_HIERARCHY:-1}"
SKIP_BUILD="${SKIP_BUILD:-0}"
DO_STAT="${DO_STAT:-1}"
DO_RECORD="${DO_RECORD:-1}"
STANDARD_FREQ="${STANDARD_FREQ:-99}"
HARD_FREQ="${HARD_FREQ:-49}"
STANDARD_FILE_SELECTION="${STANDARD_FILE_SELECTION:-random}"
STANDARD_RANDOM_SEED="${STANDARD_RANDOM_SEED:-}"

if [[ ! -x "$PERF" ]]; then
  if command -v perf >/dev/null 2>&1; then
    PERF="$(command -v perf)"
  else
    echo "Error: perf not found. Set PERF=/path/to/perf" >&2
    exit 1
  fi
fi

if [[ ! -d "$DATA_ROOT" ]]; then
  echo "Error: data root does not exist: $DATA_ROOT" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

echo "========== profiler configuration =========="
echo "project_root=$PROJECT_ROOT"
echo "data_root=$DATA_ROOT"
echo "perf=$PERF"
echo "build_dir=$BUILD_DIR"
echo "out_dir=$OUT_DIR"
echo "print_events=$PRINT_EVENTS"
echo "run_standard=$RUN_STANDARD"
echo "run_flat=$RUN_FLAT"
echo "run_hierarchy=$RUN_HIERARCHY"
echo "do_stat=$DO_STAT"
echo "do_record=$DO_RECORD"
echo "standard_file_selection=$STANDARD_FILE_SELECTION"
if [[ -n "$STANDARD_RANDOM_SEED" ]]; then
  echo "standard_random_seed=$STANDARD_RANDOM_SEED"
fi
"$PERF" --version

# Discover dataset folders containing Databento MBO JSON files.
declare -A SEEN_DIRS=()
while IFS= read -r -d '' file; do
  dir="$(dirname "$file")"
  SEEN_DIRS["$dir"]=1
done < <(find "$DATA_ROOT" -type f \( -iname '*.mbo' -o -iname '*.mbo.json' \) -print0)

if [[ ${#SEEN_DIRS[@]} -eq 0 ]]; then
  echo "Error: no .mbo or .mbo.json files found under $DATA_ROOT" >&2
  exit 1
fi

mapfile -t DATASET_FOLDERS < <(printf '%s\n' "${!SEEN_DIRS[@]}" | sort)

echo

echo "========== detected datasets =========="
for folder in "${DATASET_FOLDERS[@]}"; do
  count="$(find "$folder" -maxdepth 1 -type f \( -iname '*.mbo' -o -iname '*.mbo.json' \) | wc -l)"
  size="$(du -sh "$folder" | awk '{print $1}')"
  echo "dataset=$(basename "$folder") files=$count size=$size folder=$folder"
done

if [[ "$SKIP_BUILD" != "1" ]]; then
  echo
  echo "========== building RelWithDebInfo =========="
  rm -rf "$BUILD_DIR"
  cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer"
  cmake --build "$BUILD_DIR" -j
fi

INGEST="$BUILD_DIR/ingest"
if [[ ! -x "$INGEST" ]]; then
  echo "Error: ingest executable not found: $INGEST" >&2
  exit 1
fi

safe_name() {
  local raw="$1"
  printf '%s' "$raw" | tr -c 'A-Za-z0-9._-' '_'
}

run_one() {
  local dataset="$1"
  local mode="$2"
  local input_path="$3"
  local freq="$4"
  local safe_dataset safe_input_prefix output_file stat_file data_file record_log report_file

  safe_dataset="$(safe_name "$dataset")"
  safe_input_prefix="$OUT_DIR/${safe_dataset}_${mode}"
  output_file="${safe_input_prefix}_output.txt"
  stat_file="${safe_input_prefix}_perf_stat.txt"
  data_file="${safe_input_prefix}_perf.data"
  record_log="${safe_input_prefix}_record_output.txt"
  report_file="${safe_input_prefix}_perf_report.txt"

  echo
  echo "========== $dataset / $mode =========="
  echo "input=$input_path"

  if [[ "$DO_STAT" == "1" ]]; then
    "$PERF" stat \
      -e task-clock,context-switches,cpu-migrations,page-faults \
      -o "$stat_file" \
      -- "$INGEST" --mode "$mode" --input "$input_path" --print-events "$PRINT_EVENTS" \
      > "$output_file"

    echo "----- run summary -----"
    grep -E "strategy=|total_messages_processed=|chronological_violations=|first_timestamp=|last_timestamp=|wall_clock_seconds=|throughput_messages_per_second=" "$output_file" || true

    echo "----- perf stat -----"
    cat "$stat_file"
  fi

  if [[ "$DO_RECORD" == "1" ]]; then
    "$PERF" record \
      -e cpu-clock \
      -F "$freq" \
      -g \
      --call-graph fp \
      -o "$data_file" \
      -- "$INGEST" --mode "$mode" --input "$input_path" --print-events "$PRINT_EVENTS" \
      > "$record_log"

    "$PERF" report \
      -i "$data_file" \
      --stdio \
      > "$report_file"

    echo "----- top report -----"
    head -80 "$report_file"
  fi
}

select_standard_file() {
  local folder="$1"
  local files_file
  files_file="$(mktemp)"

  find "$folder" -maxdepth 1 -type f \( -iname '*.mbo' -o -iname '*.mbo.json' \) | sort > "$files_file"

  if [[ ! -s "$files_file" ]]; then
    rm -f "$files_file"
    echo "Error: no .mbo or .mbo.json files found in $folder" >&2
    exit 1
  fi

  if [[ "$STANDARD_FILE_SELECTION" == "first" ]]; then
    head -n 1 "$files_file"
    rm -f "$files_file"
    return
  fi

  if [[ "$STANDARD_FILE_SELECTION" != "random" ]]; then
    rm -f "$files_file"
    echo "Error: unsupported STANDARD_FILE_SELECTION=$STANDARD_FILE_SELECTION. Use random or first." >&2
    exit 1
  fi

  if [[ -n "$STANDARD_RANDOM_SEED" ]]; then
    # Reproducible pseudo-random selection. Same dataset + seed -> same file.
    awk -v seed="$STANDARD_RANDOM_SEED" -v folder="$folder" '
      BEGIN { srand(length(folder) + seed + 0) }
      { lines[++n] = $0 }
      END {
        if (n == 0) exit 1
        idx = int(rand() * n) + 1
        print lines[idx]
      }
    ' "$files_file"
  else
    shuf -n 1 "$files_file"
  fi

  rm -f "$files_file"
}

for folder in "${DATASET_FOLDERS[@]}"; do
  dataset="$(basename "$folder")"
  standard_file="$(select_standard_file "$folder")"

  if [[ "$RUN_STANDARD" == "1" ]]; then
    echo "selected_standard_file[$dataset]=$standard_file"
    run_one "$dataset" "standard" "$standard_file" "$STANDARD_FREQ"
  fi

  if [[ "$RUN_FLAT" == "1" ]]; then
    run_one "$dataset" "flat" "$folder" "$HARD_FREQ"
  fi

  if [[ "$RUN_HIERARCHY" == "1" ]]; then
    run_one "$dataset" "hierarchy" "$folder" "$HARD_FREQ"
  fi
done
# Produce compact CSV-like summary from run outputs.
python3 - "$OUT_DIR" <<'PY'
from pathlib import Path
import re
import sys

out_dir = Path(sys.argv[1])
rows = []

for path in sorted(out_dir.glob("*_output.txt")):
    text = path.read_text(errors="replace")
    name = path.name.removesuffix("_output.txt")
    parts = name.rsplit("_", 1)
    if len(parts) != 2:
        continue
    dataset, mode = parts

    def get(key):
        m = re.search(rf"^{re.escape(key)}=([^\n]+)", text, re.MULTILINE)
        return m.group(1).strip() if m else ""

    rows.append({
        "dataset": dataset,
        "mode": mode,
        "messages": get("total_messages_processed"),
        "violations": get("chronological_violations"),
        "first_ts": get("first_timestamp"),
        "last_ts": get("last_timestamp"),
        "seconds": get("wall_clock_seconds"),
        "throughput": get("throughput_messages_per_second"),
    })

summary = out_dir / "summary.csv"
with summary.open("w") as f:
    f.write("dataset,mode,messages,chronological_violations,first_timestamp,last_timestamp,wall_clock_seconds,throughput_messages_per_second\n")
    for r in rows:
        f.write(
            f"{r['dataset']},{r['mode']},{r['messages']},{r['violations']},{r['first_ts']},{r['last_ts']},{r['seconds']},{r['throughput']}\n"
        )

print("\n========== compact summary ==========")
print(summary.read_text())
PY

# Extract common bottleneck lines into one file.
BOTTLENECKS="$OUT_DIR/bottlenecks.txt"
: > "$BOTTLENECKS"
for report in "$OUT_DIR"/*_perf_report.txt; do
  [[ -f "$report" ]] || continue
  {
    echo
    echo "==================== $report ===================="
    grep -E "parseMarketDataEventLine|parseObjectForFields|parseJsonStringView|parseTimestampText|parseUInt64Text|parseFixedPoint|getline|BlockingQueue|mergeInputQueues|dispatchMergedQueue|ProcessingSummary|eventComesBefore|mutex|condition_variable|pthread_mutex" "$report" | head -120 || true
  } >> "$BOTTLENECKS"
done

echo

echo "========== outputs =========="
echo "out_dir=$OUT_DIR"
echo "summary=$OUT_DIR/summary.csv"
echo "bottlenecks=$BOTTLENECKS"
