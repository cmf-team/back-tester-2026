#!/usr/bin/env python3
"""
convert_to_feather.py
---------------------
Converts Databento MBO NDJSON files to Apache Arrow Feather (IPC) format
for dramatically faster ingestion in subsequent runs.

Requirements:
    pip install pyarrow pandas

Usage:
    python3 convert_to_feather.py <input_folder> [output_folder]

The output .feather files will be placed in output_folder (default: input_folder/feather/).
"""

import sys
import os
import json
import time
import glob
from pathlib import Path

try:
    import pyarrow as pa
    import pyarrow.feather as feather
except ImportError:
    print("ERROR: pyarrow not installed. Run: pip install pyarrow")
    sys.exit(1)


# ── Schema definition ──────────────────────────────────────────────────────────

SCHEMA = pa.schema([
    ("ts_recv",       pa.uint64()),
    ("ts_event",      pa.uint64()),
    ("publisher_id",  pa.uint32()),
    ("instrument_id", pa.uint32()),
    ("order_id",      pa.uint64()),
    ("price",         pa.float64()),   # decimal form
    ("size",          pa.uint32()),
    ("action",        pa.string()),
    ("side",          pa.string()),
    ("flags",         pa.uint8()),
    ("symbol",        pa.string()),
])


# ── ISO-8601 → nanoseconds ─────────────────────────────────────────────────────

def iso_to_ns(s: str) -> int:
    """Parse '2026-03-09T07:52:41.368148840Z' → nanoseconds since epoch."""
    if not s or s == "null":
        return 0
    try:
        # Split at T
        date_part, time_part = s.rstrip("Z").split("T")
        y, mo, d = map(int, date_part.split("-"))
        h, mi, rest = time_part.split(":")
        h, mi = int(h), int(mi)
        if "." in rest:
            sec_s, frac_s = rest.split(".")
            sec = int(sec_s)
            frac = int(frac_s.ljust(9, "0")[:9])
        else:
            sec, frac = int(rest), 0

        # Days since epoch
        days_in_month = [31,28,31,30,31,30,31,31,30,31,30,31]
        days = 0
        for yr in range(1970, y):
            days += 366 if (yr%4==0 and (yr%100!=0 or yr%400==0)) else 365
        leap = (y%4==0 and (y%100!=0 or y%400==0))
        for m in range(1, mo):
            days += days_in_month[m-1] + (1 if m==2 and leap else 0)
        days += d - 1

        total_sec = days*86400 + h*3600 + mi*60 + sec
        return total_sec * 1_000_000_000 + frac
    except Exception:
        return 0


def parse_price(p) -> float:
    if p is None:
        return float("nan")
    try:
        return float(p)
    except Exception:
        return float("nan")


# ── Convert one file ───────────────────────────────────────────────────────────

def convert_file(src: Path, dst: Path, chunk_size: int = 100_000) -> dict:
    """Convert one .mbo.json file to .feather. Returns stats dict."""
    t0 = time.perf_counter()

    # Accumulators
    cols = {f.name: [] for f in SCHEMA}
    total = 0
    skipped = 0

    def flush(cols_local):
        arrays = []
        for field in SCHEMA:
            arr = pa.array(cols_local[field.name], type=field.type)
            arrays.append(arr)
        return pa.RecordBatch.from_arrays(arrays, schema=SCHEMA)

    batches = []
    chunk_cols = {f.name: [] for f in SCHEMA}
    chunk_n = 0

    with open(src, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line[0] != "{":
                skipped += 1
                continue
            try:
                rec = json.loads(line)
                hd  = rec.get("hd", {})

                chunk_cols["ts_recv"].append(iso_to_ns(rec.get("ts_recv", "")))
                chunk_cols["ts_event"].append(iso_to_ns(hd.get("ts_event", "")))
                chunk_cols["publisher_id"].append(int(hd.get("publisher_id", 0)))
                chunk_cols["instrument_id"].append(int(hd.get("instrument_id", 0)))
                chunk_cols["order_id"].append(int(rec.get("order_id", 0)))
                chunk_cols["price"].append(parse_price(rec.get("price")))
                chunk_cols["size"].append(int(rec.get("size", 0)))
                chunk_cols["action"].append(str(rec.get("action", "N")))
                chunk_cols["side"].append(str(rec.get("side", "N")))
                chunk_cols["flags"].append(int(rec.get("flags", 0)))
                chunk_cols["symbol"].append(str(rec.get("symbol", "")))

                total += 1
                chunk_n += 1

                if chunk_n >= chunk_size:
                    batches.append(flush(chunk_cols))
                    chunk_cols = {f.name: [] for f in SCHEMA}
                    chunk_n = 0

            except Exception:
                skipped += 1

    # Flush remainder
    if chunk_n > 0:
        batches.append(flush(chunk_cols))

    if batches:
        table = pa.Table.from_batches(batches, schema=SCHEMA)
        feather.write_feather(table, dst, compression="lz4")

    elapsed = time.perf_counter() - t0
    size_mb = dst.stat().st_size / 1e6 if dst.exists() else 0
    return {
        "file":    src.name,
        "rows":    total,
        "skipped": skipped,
        "time_s":  elapsed,
        "size_mb": size_mb,
        "rows_per_sec": int(total / elapsed) if elapsed > 0 else 0,
    }


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <input_folder> [output_folder]")
        sys.exit(1)

    src_folder = Path(sys.argv[1])
    dst_folder = Path(sys.argv[2]) if len(sys.argv) >= 3 else src_folder / "feather"
    dst_folder.mkdir(parents=True, exist_ok=True)

    json_files = sorted(src_folder.glob("*.mbo.json"))
    if not json_files:
        print(f"No .mbo.json files found in {src_folder}")
        sys.exit(1)

    print(f"Found {len(json_files)} files to convert → {dst_folder}\n")

    total_rows = 0
    total_time = 0.0
    total_size = 0.0

    print(f"{'File':<40} {'Rows':>10} {'Time(s)':>8} {'Size(MB)':>9} {'Rows/s':>10}")
    print("-" * 80)

    for src in json_files:
        dst = dst_folder / (src.stem + ".feather")
        stats = convert_file(src, dst)
        print(f"{stats['file']:<40} {stats['rows']:>10,} "
              f"{stats['time_s']:>8.2f} {stats['size_mb']:>9.1f} "
              f"{stats['rows_per_sec']:>10,}")
        total_rows += stats["rows"]
        total_time += stats["time_s"]
        total_size += stats["size_mb"]

    print("-" * 80)
    print(f"{'TOTAL':<40} {total_rows:>10,} {total_time:>8.2f} {total_size:>9.1f} "
          f"{int(total_rows/total_time):>10,}")
    print(f"\nFeather files written to: {dst_folder}")
    print("\nIngestion speed comparison:")
    print(f"  JSON parsing  : ~250,000 rows/s  (from hard_task benchmark)")
    print(f"  Feather write : {int(total_rows/total_time):,} rows/s")
    print(f"  Expected Feather read speed: ~5-10× faster than JSON")


if __name__ == "__main__":
    main()
