#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# dependencies = ["pyarrow>=15"]
# ///
"""
convert_to_feather.py

Transcodes Databento NDJSON MBO files to Apache Arrow Feather (IPC) format
with a fixed schema that exactly mirrors the C++ MarketDataEvent struct
(src/market_data/MarketDataEvent.hpp). Round-trip JSON -> Feather -> C++
preserves every field, including `sequence` which is part of the strict
ordering key (ts_recv, sequence, instrument_id) used by the merger.

Usage:
    convert_to_feather.py <src_dir_or_file> <dst_dir>
    convert_to_feather.py <src_dir_or_file> <dst_dir> --benchmark
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Iterable

try:
    import pyarrow as pa
    import pyarrow.feather as feather
except ImportError:  # pragma: no cover
    sys.exit("pyarrow is not installed; run: pip install pyarrow")

# Match MarketDataEvent::kPriceScale = 1e9.
PRICE_SCALE = 1_000_000_000
UNDEF_PRICE = (1 << 63) - 1  # int64 max — matches kUndefPrice in C++

# Action and side maps. The C++ reader keeps single-byte chars, so we store
# the raw character. dictionary<int8> is much more compact than string for
# this many rows but chars on the wire keep the schema introspectable.
ACTION_CHARS = {"A", "C", "M", "R", "T", "F", "N"}
SIDE_CHARS = {"B", "A", "N"}

SCHEMA = pa.schema(
    [
        pa.field("ts_recv", pa.int64()),  # ns since epoch
        pa.field("ts_event", pa.int64()),
        pa.field("ts_in_delta", pa.int32()),
        pa.field("sequence", pa.uint32()),
        pa.field("instrument_id", pa.uint64()),
        pa.field("publisher_id", pa.uint16()),
        pa.field("flags", pa.uint16()),
        pa.field("rtype", pa.uint8()),
        pa.field("channel_id", pa.uint8()),
        pa.field("action", pa.uint8()),  # ASCII byte of MdAction
        pa.field("side", pa.int8()),  # 1=Buy, -1=Sell, 0=None
        pa.field("order_id", pa.uint64()),
        pa.field("price", pa.int64()),  # scaled 1e-9 (matches kPriceScale)
        pa.field("size", pa.uint32()),
        pa.field("symbol", pa.string()),
    ]
)


def parse_iso8601_to_ns(s: str) -> int:
    """Parses '2026-04-07T07:52:41.368148840Z' -> nanoseconds since epoch."""
    if not s:
        return 0
    # Manual parse — strptime cannot handle 9 fractional digits.
    # Format: YYYY-MM-DDTHH:MM:SS.fffffffffZ
    year = int(s[0:4])
    month = int(s[5:7])
    day = int(s[8:10])
    hour = int(s[11:13])
    minute = int(s[14:16])
    second = int(s[17:19])
    nanos = 0
    if len(s) > 20 and s[19] == ".":
        # Read up to 9 frac digits, pad with zeros to 9 if shorter.
        end = 20
        while end < len(s) and s[end].isdigit():
            end += 1
        frac = s[20:end]
        frac = frac[:9].ljust(9, "0")
        nanos = int(frac)
    # Howard Hinnant's days_from_civil — works for any Gregorian date.
    y = year - (1 if month <= 2 else 0)
    era = (y if y >= 0 else y - 399) // 400
    yoe = y - era * 400
    doy = (153 * (month + (9 if month <= 2 else -3)) + 2) // 5 + day - 1
    doe = yoe * 365 + yoe // 4 - yoe // 100 + doy
    days = era * 146097 + doe - 719468
    epoch_sec = days * 86400 + hour * 3600 + minute * 60 + second
    return epoch_sec * 1_000_000_000 + nanos


def parse_price(v) -> int:
    if v is None:
        return UNDEF_PRICE
    if isinstance(v, int):
        return v
    if isinstance(v, float):
        return int(round(v * PRICE_SCALE))
    s = str(v).strip()
    if not s or s == "null":
        return UNDEF_PRICE
    neg = False
    if s.startswith("-"):
        neg = True
        s = s[1:]
    if "." in s:
        whole, frac = s.split(".", 1)
        frac = frac[:9].ljust(9, "0")
        v = int(whole or "0") * PRICE_SCALE + int(frac)
    else:
        v = int(s) * PRICE_SCALE
    return -v if neg else v


def side_to_int(c: str) -> int:
    if c == "B":
        return 1
    if c == "A":
        return -1
    return 0


def iter_events(path: Path):
    """Yields one dict per line, ignoring malformed lines."""
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line[0] != "{":
                continue
            try:
                r = json.loads(line)
            except json.JSONDecodeError:
                continue
            yield r


def event_dict_to_columns(records: Iterable[dict]):
    cols = {f.name: [] for f in SCHEMA}
    for r in records:
        hd = r.get("hd") or {}
        cols["ts_recv"].append(parse_iso8601_to_ns(r.get("ts_recv", "")))
        cols["ts_event"].append(parse_iso8601_to_ns(hd.get("ts_event", "")))
        cols["ts_in_delta"].append(int(r.get("ts_in_delta") or 0))
        cols["sequence"].append(int(r.get("sequence") or 0))
        cols["instrument_id"].append(int(hd.get("instrument_id") or 0))
        cols["publisher_id"].append(int(hd.get("publisher_id") or 0))
        cols["flags"].append(int(r.get("flags") or 0))
        cols["rtype"].append(int(hd.get("rtype") or 0))
        cols["channel_id"].append(int(r.get("channel_id") or 0))
        a = (r.get("action") or "N")[:1]
        cols["action"].append(ord(a) if a in ACTION_CHARS else ord("N"))
        s = (r.get("side") or "N")[:1]
        cols["side"].append(side_to_int(s))
        oid = r.get("order_id")
        cols["order_id"].append(int(oid) if oid not in (None, "") else 0)
        cols["price"].append(parse_price(r.get("price")))
        cols["size"].append(int(r.get("size") or 0))
        cols["symbol"].append(r.get("symbol") or "")
    return cols


def convert_file(src: Path, dst_dir: Path) -> tuple[Path, int, float, float]:
    """Returns (output_path, row_count, json_secs, feather_write_secs)."""
    dst_dir.mkdir(parents=True, exist_ok=True)
    out = dst_dir / (src.stem.split(".")[0] + ".mbo.feather")
    t0 = time.perf_counter()
    cols = event_dict_to_columns(iter_events(src))
    t1 = time.perf_counter()
    table = pa.table(cols, schema=SCHEMA)
    feather.write_feather(table, out, compression="lz4")
    t2 = time.perf_counter()
    return out, table.num_rows, t1 - t0, t2 - t1


def benchmark_read(path: Path) -> tuple[float, int]:
    t0 = time.perf_counter()
    table = feather.read_table(path)
    n = table.num_rows
    return time.perf_counter() - t0, n


def benchmark_json_read(path: Path) -> tuple[float, int]:
    t0 = time.perf_counter()
    n = sum(1 for _ in iter_events(path))
    return time.perf_counter() - t0, n


def main() -> int:
    ap = argparse.ArgumentParser(description="JSON -> Feather transcoder")
    ap.add_argument("src", type=Path, help="source file or directory")
    ap.add_argument("dst", type=Path, help="output directory")
    ap.add_argument(
        "--benchmark",
        action="store_true",
        help="also report JSON read vs Feather read times",
    )
    args = ap.parse_args()

    if args.src.is_file():
        files = [args.src]
    else:
        files = sorted(args.src.glob("*.mbo.json"))
    if not files:
        print(f"No *.mbo.json files in {args.src}", file=sys.stderr)
        return 1

    print(f"Converting {len(files)} file(s) to {args.dst} ...")
    total_rows = 0
    json_total = 0.0
    feather_write_total = 0.0
    feather_read_total = 0.0
    json_read_total = 0.0

    for f in files:
        out, n, t_json, t_write = convert_file(f, args.dst)
        total_rows += n
        json_total += t_json
        feather_write_total += t_write
        print(
            f"  {f.name} -> {out.name}: {n} rows  parse={t_json:.3f}s  "
            f"write={t_write:.3f}s"
        )
        if args.benchmark:
            tr, nr = benchmark_read(out)
            tj, nj = benchmark_json_read(f)
            assert nr == n and nj == n, "row count mismatch"
            feather_read_total += tr
            json_read_total += tj
            print(
                f"    benchmark  json_read={tj:.3f}s  feather_read={tr:.3f}s "
                f"(speedup={tj / tr if tr > 0 else 0:.1f}x)"
            )

    print(f"\nTotal rows: {total_rows}")
    print(f"  parse JSON:    {json_total:.3f}s")
    print(f"  write feather: {feather_write_total:.3f}s")
    if args.benchmark:
        print(f"  json read:     {json_read_total:.3f}s")
        print(
            f"  feather read:  {feather_read_total:.3f}s "
            f"(speedup={json_read_total / feather_read_total if feather_read_total > 0 else 0:.1f}x)"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
