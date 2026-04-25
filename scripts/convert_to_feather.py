#!/usr/bin/env python3
"""
Convert NDJSON Databento-style market-data files to Apache Arrow Feather
v2 (IPC) format and benchmark JSON-vs-Feather scan throughput.

Why Feather:
    * NDJSON is a textual format. Parsing it spends ~10x more CPU than
      reading the equivalent binary representation. For a back-tester
      that re-reads the same day many times during strategy iteration,
      converting once and reading the binary on every subsequent run is
      a strict win.
    * Feather v2 is a thin envelope around Arrow's columnar in-memory
      format. Reading is essentially memcpy, columns can be loaded
      individually, and pyarrow.ipc supports zero-copy mmap.

Usage:
    convert_to_feather.py convert  <ndjson_in> <feather_out>
    convert_to_feather.py bench    <ndjson_in> <feather_path>
    convert_to_feather.py convert-and-bench <ndjson_in> <feather_out>

Requires: pyarrow >= 14.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from typing import Any

import pyarrow as pa
import pyarrow.feather as feather


def _parse_ts(s: str | None) -> int:
    """Parse '2025-04-01T09:00:00.000000001Z' to int64 nanoseconds.
    Returns 0 on missing input (we keep the column non-nullable for speed).
    """
    if not s:
        return 0
    # Trim trailing 'Z'.
    s = s[:-1] if s.endswith("Z") else s
    date, _, t = s.partition("T")
    y, m, d = (int(x) for x in date.split("-"))
    hh, mm, rest = t.split(":", 2)
    sec, _, frac = rest.partition(".")
    if frac:
        frac = (frac + "0" * 9)[:9]
        nanos_frac = int(frac)
    else:
        nanos_frac = 0
    # Inline civil_to_days (Howard Hinnant) to avoid datetime overhead.
    yy = y - (1 if m <= 2 else 0)
    era = (yy if yy >= 0 else yy - 399) // 400
    yoe = yy - era * 400
    mp = (m - 3) if m > 2 else (m + 9)
    doy = (153 * mp + 2) // 5 + d - 1
    doe = yoe * 365 + yoe // 4 - yoe // 100 + doy
    days = era * 146097 + doe - 719468
    secs = days * 86400 + int(hh) * 3600 + int(mm) * 60 + int(sec)
    return secs * 1_000_000_000 + nanos_frac


def _flatten(rec: dict[str, Any]) -> dict[str, Any]:
    """Hoist nested 'hd' fields up to the top level."""
    if "hd" in rec and isinstance(rec["hd"], dict):
        for k, v in rec["hd"].items():
            rec.setdefault(k, v)
        del rec["hd"]
    return rec


def convert(ndjson_path: Path, feather_path: Path) -> int:
    """Stream-parse NDJSON, write a single Feather file. Returns row count."""
    cols: dict[str, list[Any]] = {
        "ts_event":      [],
        "ts_recv":       [],
        "instrument_id": [],
        "publisher_id":  [],
        "channel_id":    [],
        "rtype":         [],
        "flags":         [],
        "sequence":      [],
        "ts_in_delta":   [],
        "order_id":      [],
        "side":          [],
        "action":        [],
        "price":         [],
        "size":          [],
        "symbol":        [],
    }
    with ndjson_path.open("r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rec = _flatten(json.loads(line))
            cols["ts_event"].append(_parse_ts(rec.get("ts_event")))
            cols["ts_recv"].append(_parse_ts(rec.get("ts_recv")))
            cols["instrument_id"].append(int(rec.get("instrument_id") or 0))
            cols["publisher_id"].append(int(rec.get("publisher_id") or 0))
            cols["channel_id"].append(int(rec.get("channel_id") or 0))
            cols["rtype"].append(int(rec.get("rtype") or 0))
            cols["flags"].append(int(rec.get("flags") or 0))
            cols["sequence"].append(int(rec.get("sequence") or 0))
            cols["ts_in_delta"].append(int(rec.get("ts_in_delta") or 0))
            cols["order_id"].append(int(rec.get("order_id") or 0))
            cols["side"].append(rec.get("side") or "N")
            cols["action"].append(rec.get("action") or "N")
            px = rec.get("price")
            cols["price"].append(float(px) if px is not None else float("nan"))
            cols["size"].append(int(rec.get("size") or 0))
            cols["symbol"].append(rec.get("symbol") or "")

    schema = pa.schema([
        ("ts_event",      pa.int64()),
        ("ts_recv",       pa.int64()),
        ("instrument_id", pa.uint32()),
        ("publisher_id",  pa.uint16()),
        ("channel_id",    pa.uint16()),
        ("rtype",         pa.uint8()),
        ("flags",         pa.uint8()),
        ("sequence",      pa.uint32()),
        ("ts_in_delta",   pa.int32()),
        ("order_id",      pa.uint64()),
        ("side",          pa.string()),
        ("action",        pa.string()),
        ("price",         pa.float64()),
        ("size",          pa.int64()),
        ("symbol",        pa.string()),
    ])
    table = pa.Table.from_pydict(cols, schema=schema)
    feather.write_feather(table, feather_path, compression="zstd")
    return table.num_rows


def bench(ndjson_path: Path, feather_path: Path) -> None:
    """Run a simple read-and-touch-every-cell benchmark against both formats."""
    # JSON: count rows + sum sizes via stream-parse.
    t0 = time.perf_counter()
    n = 0
    s = 0
    with ndjson_path.open("r") as f:
        for line in f:
            if not line.strip():
                continue
            rec = json.loads(line)
            n += 1
            s += int(rec.get("size") or 0)
    t_json = time.perf_counter() - t0

    # Feather: load full table (already binary), then take a column sum.
    t0 = time.perf_counter()
    table = feather.read_table(feather_path)
    s_f = pa.compute.sum(table["size"]).as_py()
    n_f = table.num_rows
    t_feather = time.perf_counter() - t0

    sz_json    = ndjson_path.stat().st_size
    sz_feather = feather_path.stat().st_size

    print(f"NDJSON:  rows={n} sum(size)={s} time={t_json:7.3f}s "
          f"size={sz_json/1e6:8.2f} MB")
    print(f"Feather: rows={n_f} sum(size)={s_f} time={t_feather:7.3f}s "
          f"size={sz_feather/1e6:8.2f} MB")
    if t_feather > 0:
        print(f"Speedup (NDJSON / Feather): {t_json / t_feather:.1f}x")


def main(argv: list[str]) -> int:
    if len(argv) < 4:
        print(__doc__)
        return 2
    cmd, src, dst = argv[1], Path(argv[2]), Path(argv[3])
    if cmd == "convert":
        n = convert(src, dst)
        print(f"Wrote {dst} ({n} rows)")
    elif cmd == "bench":
        bench(src, dst)
    elif cmd == "convert-and-bench":
        n = convert(src, dst)
        print(f"Wrote {dst} ({n} rows)")
        bench(src, dst)
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
