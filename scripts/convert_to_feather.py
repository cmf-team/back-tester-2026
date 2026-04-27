#!/usr/bin/env python3
"""
Convert Databento NDJSON market-data files to Apache Arrow Feather format.

Feather is a binary columnar format that is ~10-100x faster to read than JSON
because there is no parsing — bytes on disk are already in column form.

Usage:
    python convert_to_feather.py --in /path/to/json_dir --out /path/to/feather_dir

Requires:
    pip install pyarrow
"""

import argparse
import json
import os
import sys
import time
from pathlib import Path

try:
    import pyarrow as pa
    import pyarrow.feather as feather
except ImportError:
    sys.exit("pyarrow not installed.  pip install pyarrow")


def convert_file(json_path: Path, feather_path: Path) -> tuple[int, float, int, int]:
    """Returns (rows, seconds, json_size, feather_size)."""
    columns: dict[str, list] = {
        "ts_recv":       [],
        "ts_event":      [],
        "rtype":         [],
        "publisher_id":  [],
        "instrument_id": [],
        "action":        [],
        "side":          [],
        "price":         [],
        "size":          [],
        "order_id":      [],
        "flags":         [],
        "ts_in_delta":   [],
        "sequence":      [],
    }

    t0 = time.perf_counter()
    with json_path.open("r") as f:
        for line in f:
            try:
                rec = json.loads(line)
            except json.JSONDecodeError:
                continue
            hd = rec.get("hd", {})
            columns["ts_recv"].append(rec.get("ts_recv"))
            columns["ts_event"].append(hd.get("ts_event"))
            columns["rtype"].append(hd.get("rtype"))
            columns["publisher_id"].append(hd.get("publisher_id"))
            columns["instrument_id"].append(hd.get("instrument_id"))
            columns["action"].append(rec.get("action"))
            columns["side"].append(rec.get("side"))
            price_str = rec.get("price")
            columns["price"].append(float(price_str) if price_str is not None else None)
            columns["size"].append(rec.get("size"))
            order_id_str = rec.get("order_id")
            columns["order_id"].append(int(order_id_str) if order_id_str is not None else None)
            columns["flags"].append(rec.get("flags"))
            columns["ts_in_delta"].append(rec.get("ts_in_delta"))
            columns["sequence"].append(rec.get("sequence"))

    table = pa.table(columns)
    feather.write_feather(table, feather_path, compression="zstd")
    elapsed = time.perf_counter() - t0

    return (
        len(columns["ts_recv"]),
        elapsed,
        json_path.stat().st_size,
        feather_path.stat().st_size,
    )


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in",  dest="src", required=True, help="folder with .mbo.json files")
    ap.add_argument("--out", dest="dst", required=True, help="output folder for .feather files")
    args = ap.parse_args()

    src = Path(args.src)
    dst = Path(args.dst)
    dst.mkdir(parents=True, exist_ok=True)

    total_rows = 0
    total_secs = 0.0
    total_json = 0
    total_feather = 0

    for json_file in sorted(src.glob("*.mbo.json")):
        feather_file = dst / (json_file.stem.removesuffix(".mbo") + ".feather")
        rows, secs, j_sz, f_sz = convert_file(json_file, feather_file)
        total_rows += rows
        total_secs += secs
        total_json += j_sz
        total_feather += f_sz
        print(f"{json_file.name}: {rows} rows in {secs:.1f}s "
              f"({j_sz/1e6:.1f} MB → {f_sz/1e6:.1f} MB, {f_sz/j_sz:.2f}x)")

    if total_rows:
        print()
        print(f"Totals: {total_rows} rows in {total_secs:.1f}s "
              f"({total_rows/total_secs:.0f} rows/sec)")
        print(f"Disk: {total_json/1e6:.1f} MB JSON → {total_feather/1e6:.1f} MB Feather "
              f"(compression {total_feather/total_json:.2f}x)")
        print()
        print("Note: typical C++ Feather ingestion is 10-50x faster than JSON "
              "(no string parsing, columnar already in memory layout).")


if __name__ == "__main__":
    main()
