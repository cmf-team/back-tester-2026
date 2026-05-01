#!/usr/bin/env python3
"""
convert_to_feather.py
Convert Databento NDJSON MBO files to Apache Arrow Feather (IPC) format.

Usage:
    python3 convert_to_feather.py <dir_or_file>
    python3 convert_to_feather.py --benchmark <dir_or_file>
"""

import argparse
import json
import sys
import time
from pathlib import Path

try:
    import pyarrow as pa
    import pyarrow.feather as feather
except ImportError:
    sys.exit("pyarrow not installed. Run: pip install pyarrow")

SCHEMA = pa.schema(
    [
        pa.field("ts_recv", pa.uint64()),
        pa.field("ts_event", pa.uint64()),
        pa.field("action", pa.string()),
        pa.field("side", pa.string()),
        pa.field("order_id", pa.uint64()),
        pa.field("instrument_id", pa.uint64()),
        pa.field("price", pa.int64()),  # scaled 1e-9
        pa.field("size", pa.int64()),
        pa.field("flags", pa.uint8()),
    ]
)

UNDEF_PRICE = 9_223_372_036_854_775_807
UNDEF_TS = 18_446_744_073_709_551_615
VALID_ACTIONS = {"A", "C", "M", "T", "F", "R", "N"}


def _safe_int(v, default=0):
    try:
        if v is None:
            return default
        return int(v)
    except (ValueError, TypeError):
        return default


def _parse_price(v) -> int:
    if v is None:
        return 0

    if isinstance(v, int):
        return 0 if v == UNDEF_PRICE else v

    if isinstance(v, float):
        raw = int(round(v * 1_000_000_000))
        return 0 if raw == UNDEF_PRICE else raw

    s = str(v).strip()
    if not s or s == "null":
        return 0

    if "." in s:
        integer, frac = s.split(".", 1)
        frac = frac[:9].ljust(9, "0")
        return int(integer) * 1_000_000_000 + int(frac)

    return _safe_int(s)


def convert_file(json_path: Path, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    out_path = output_dir / (json_path.stem + ".feather")

    cols = {f.name: [] for f in SCHEMA}
    t0 = time.perf_counter()

    with open(json_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()

            if not line or line[0] != "{":
                continue

            try:
                r = json.loads(line)
            except json.JSONDecodeError:
                continue

            action = r.get("action", "")
            if action not in VALID_ACTIONS:
                continue

            ts_recv = _safe_int(r.get("ts_recv", 0))
            ts_event = _safe_int(r.get("ts_event", 0))

            if ts_recv == UNDEF_TS:
                ts_recv = 0

            if ts_event == UNDEF_TS:
                ts_event = 0

            cols["ts_recv"].append(ts_recv)
            cols["ts_event"].append(ts_event)
            cols["action"].append(action)
            cols["side"].append(r.get("side", "N"))
            cols["order_id"].append(_safe_int(r.get("order_id", 0)))
            cols["instrument_id"].append(_safe_int(r.get("instrument_id", 0)))
            cols["price"].append(_parse_price(r.get("price")))
            cols["size"].append(_safe_int(r.get("size", 0)))
            cols["flags"].append(_safe_int(r.get("flags", 0)))

    table = pa.table(cols, schema=SCHEMA)
    feather.write_feather(table, out_path, compression="lz4")

    elapsed = time.perf_counter() - t0
    json_mb = json_path.stat().st_size / 1_048_576
    feather_mb = out_path.stat().st_size / 1_048_576
    n = len(table)

    print(
        f"  {json_path.name}: {n:,} records | "
        f"JSON {json_mb:.1f} MB → Feather {feather_mb:.1f} MB "
        f"({feather_mb / json_mb * 100:.0f}%) | {elapsed:.2f}s"
    )

    return out_path


def benchmark(json_path: Path, feather_path: Path):
    print(f"\n  --- Read benchmark: {json_path.name} ---")

    t0 = time.perf_counter()
    n_json = 0

    with open(json_path, encoding="utf-8") as f:
        for line in f:
            if line.strip():
                try:
                    json.loads(line)
                    n_json += 1
                except Exception:
                    pass

    json_sec = time.perf_counter() - t0

    print(
        f"  JSON    : {n_json:>10,} records  {json_sec:6.3f}s  "
        f"{n_json / json_sec:>12,.0f} rec/s"
    )

    t0 = time.perf_counter()
    tbl = feather.read_table(feather_path)
    n_f = len(tbl)
    feat_sec = time.perf_counter() - t0

    print(
        f"  Feather : {n_f:>10,} records  {feat_sec:6.3f}s  "
        f"{n_f / feat_sec:>12,.0f} rec/s  "
        f"  ↑ {json_sec / feat_sec:.1f}x faster"
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="JSON file or directory")
    ap.add_argument("--output", "-o", default=None)
    ap.add_argument("--benchmark", "-b", action="store_true")
    args = ap.parse_args()

    inp = Path(args.input)

    if inp.is_dir():
        files = [
            f
            for f in inp.rglob("*.json")
            if f.name not in ("metadata.json", "manifest.json", "condition.json")
        ]
        files.sort()
    else:
        files = [inp]

    if not files:
        sys.exit(f"No JSON files found in {inp}")

    if args.output:
        out_dir = Path(args.output)
    else:
        if inp.is_dir():
            out_dir = inp / "feather"
        else:
            out_dir = inp.parent / "feather"

    print(f"Converting {len(files)} file(s) → {out_dir}/")

    t_all = time.perf_counter()
    feather_files = []

    for jf in files:
        ff = convert_file(jf, out_dir)
        feather_files.append((jf, ff))

    print(f"Conversion done in {time.perf_counter() - t_all:.2f}s\n")

    if args.benchmark:
        for jf, ff in feather_files:
            benchmark(jf, ff)


if __name__ == "__main__":
    main()
