#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build-coverage}"
COVERAGE_MIN="${COVERAGE_MIN:-90}"
COVERAGE_DIR="$BUILD_DIR/coverage"

rm -rf "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON >/dev/null
cmake --build "$BUILD_DIR" -j >/dev/null
ctest --test-dir "$BUILD_DIR" --output-on-failure

rm -rf "$COVERAGE_DIR"
mkdir -p "$COVERAGE_DIR"

mapfile -t gcno_files < <(
  find "$BUILD_DIR/CMakeFiles/ingest_core.dir/src" "$BUILD_DIR/CMakeFiles/ingest.dir/src" \
    -name '*.gcno' -print 2>/dev/null | sort
)

if [[ ${#gcno_files[@]} -eq 0 ]]; then
  echo "No gcov notes found. Was the project built with -DENABLE_COVERAGE=ON?" >&2
  exit 1
fi

for gcno in "${gcno_files[@]}"; do
  (cd "$COVERAGE_DIR" && gcov -pbc -o "$(dirname "$gcno")" "$gcno" >/dev/null)
done

mapfile -t source_reports < <(find "$COVERAGE_DIR" -name '*#src#*.cpp.gcov' -print | sort)
if [[ ${#source_reports[@]} -eq 0 ]]; then
  echo "No project source gcov reports were generated." >&2
  exit 1
fi

awk -v min="$COVERAGE_MIN" '
  BEGIN { covered = 0; total = 0 }
  {
    split($0, fields, ":")
    count = fields[1]
    gsub(/[ \t]/, "", count)
    if (count != "" && count != "-") {
      ++total
      if (count != "#####" && count != "=====") {
        ++covered
      }
    }
  }
  END {
    pct = total == 0 ? 0 : covered * 100.0 / total
    printf("Line coverage: %.2f%% (%d/%d executable lines)\n", pct, covered, total)
    if (pct + 0.00001 < min) {
      printf("Coverage is below required threshold %.2f%%\n", min) > "/dev/stderr"
      exit 1
    }
  }
' "${source_reports[@]}"
