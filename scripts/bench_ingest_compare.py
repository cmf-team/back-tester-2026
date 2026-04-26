#!/usr/bin/env python3
"""Compare ingest speed: NDJSON parse (orjson) vs Feather read (pyarrow).

This is a Python-side proxy comparison (no C++ Arrow producer available).
The point: show the ceiling on how much format change can move the needle.

Usage:
    bench_ingest_compare.py <in.mbo.json> <in.feather>
"""
import sys
import time
from pathlib import Path

import orjson
import pyarrow.feather as feather


def bench_json(p: Path) -> tuple[int, float]:
    n = 0
    t0 = time.perf_counter()
    with p.open("rb") as fh:
        for line in fh:
            if not line.strip():
                continue
            orjson.loads(line)
            n += 1
    return n, time.perf_counter() - t0


def bench_feather(p: Path) -> tuple[int, float]:
    t0 = time.perf_counter()
    table = feather.read_table(p)
    n = table.num_rows
    return n, time.perf_counter() - t0


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 1
    json_p    = Path(sys.argv[1])
    feather_p = Path(sys.argv[2])

    n_j, t_j = bench_json(json_p)
    n_f, t_f = bench_feather(feather_p)

    print(f"NDJSON  : {n_j:>10d} rows  in {t_j:6.2f}s  -> {n_j/t_j/1e6:6.2f} M rows/s", file=sys.stderr)
    print(f"Feather : {n_f:>10d} rows  in {t_f:6.2f}s  -> {n_f/t_f/1e6:6.2f} M rows/s", file=sys.stderr)
    print(f"speedup : x{(t_j/t_f):.1f}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
