#!/usr/bin/env python3
"""Convert Databento MBO NDJSON files to Feather (Arrow IPC) format.

Schema mirrors C++ MarketDataEvent layout. Timestamps are stored as int64
nanoseconds since epoch so the C++ side can read columns directly.

Usage:
    mbo_json_to_feather.py <in.mbo.json> <out.feather>
"""
import sys
from pathlib import Path

import orjson
import pyarrow as pa
import pyarrow.feather as feather


SCHEMA = pa.schema([
    pa.field("ts_recv",       pa.int64()),
    pa.field("ts_event",      pa.int64()),
    pa.field("order_id",      pa.uint64()),
    pa.field("price",         pa.float64()),
    pa.field("instrument_id", pa.uint32()),
    pa.field("publisher_id",  pa.uint32()),
    pa.field("sequence",      pa.uint32()),
    pa.field("size",          pa.uint32()),
    pa.field("ts_in_delta",   pa.int32()),
    pa.field("channel_id",    pa.uint16()),
    pa.field("rtype",         pa.uint8()),
    pa.field("flags",         pa.uint8()),
    pa.field("action",        pa.uint8()),
    pa.field("side",          pa.uint8()),
    pa.field("symbol",        pa.string()),
])


def parse_iso_ns(s: str) -> int:
    # "2026-03-09T07:52:41.368148840Z" -> int64 nanos
    date_part, time_part = s.split("T", 1)
    y, mo, d = (int(x) for x in date_part.split("-"))
    hms, frac = time_part.rstrip("Z").split(".", 1) if "." in time_part else (time_part.rstrip("Z"), "0")
    h, mi, se = (int(x) for x in hms.split(":"))
    frac_ns = int((frac + "000000000")[:9])
    import datetime
    epoch = datetime.datetime(y, mo, d, h, mi, se, tzinfo=datetime.timezone.utc).timestamp()
    return int(epoch) * 1_000_000_000 + frac_ns


def convert(in_path: Path, out_path: Path) -> tuple[int, float]:
    import time
    t0 = time.perf_counter()
    cols: dict[str, list] = {f.name: [] for f in SCHEMA}
    n = 0
    with in_path.open("rb") as fh:
        for line in fh:
            if not line.strip():
                continue
            r = orjson.loads(line)
            cols["ts_recv"].append(parse_iso_ns(r["ts_recv"]))
            hd = r["hd"]
            cols["ts_event"].append(parse_iso_ns(hd["ts_event"]))
            cols["order_id"].append(int(r["order_id"]))
            px = r["price"]
            if px is None: px = float("nan")
            elif isinstance(px, str): px = float(px)
            cols["price"].append(px)
            cols["instrument_id"].append(hd["instrument_id"])
            cols["publisher_id"].append(hd["publisher_id"])
            cols["sequence"].append(r.get("sequence", 0))
            cols["size"].append(r["size"])
            cols["ts_in_delta"].append(r.get("ts_in_delta", 0))
            cols["channel_id"].append(r.get("channel_id", 0))
            cols["rtype"].append(hd["rtype"])
            cols["flags"].append(r.get("flags", 0))
            cols["action"].append(ord(r["action"]) if r["action"] else 0)
            cols["side"].append(ord(r["side"]) if r["side"] else 0)
            cols["symbol"].append(r.get("symbol", ""))
            n += 1
    table = pa.table(cols, schema=SCHEMA)
    feather.write_feather(table, out_path, compression="lz4")
    return n, time.perf_counter() - t0


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 1
    in_path  = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    n, dt = convert(in_path, out_path)
    in_size  = in_path.stat().st_size
    out_size = out_path.stat().st_size
    print(f"events={n} time={dt:.2f}s "
          f"json_size={in_size/1e6:.1f}MB feather_size={out_size/1e6:.1f}MB "
          f"ratio={out_size/in_size:.3f}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
