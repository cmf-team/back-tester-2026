#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "pyarrow>=16.1.0",
# ]
# ///

from __future__ import annotations

import argparse
import json
from pathlib import Path

import pyarrow as pa
import pyarrow.feather as feather


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert Databento NDJSON MBO files into Feather tables."
    )
    parser.add_argument("inputs", nargs="+", help="Input .json/.mbo.json files")
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory for .feather files (default: beside each input)",
    )
    parser.add_argument(
        "--compression",
        default="lz4",
        choices=("lz4", "zstd", "uncompressed"),
        help="Feather compression codec",
    )
    return parser.parse_args()


def normalise_record(record: dict) -> dict:
    header = record.get("hd", {})
    return {
        "ts_recv": record.get("ts_recv"),
        "ts_event": header.get("ts_event", record.get("ts_event")),
        "rtype": header.get("rtype", record.get("rtype")),
        "publisher_id": header.get("publisher_id", record.get("publisher_id")),
        "instrument_id": header.get("instrument_id", record.get("instrument_id")),
        "order_id": record.get("order_id"),
        "action": record.get("action"),
        "side": record.get("side"),
        "price": record.get("price"),
        "size": record.get("size"),
        "channel_id": record.get("channel_id"),
        "flags": record.get("flags"),
        "ts_in_delta": record.get("ts_in_delta"),
        "sequence": record.get("sequence"),
        "symbol": record.get("symbol"),
    }


def convert_file(input_path: Path, output_path: Path, compression: str) -> int:
    rows: list[dict] = []
    with input_path.open("r", encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, start=1):
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{input_path}:{line_no}: invalid JSON: {exc}") from exc
            rows.append(normalise_record(record))

    table = pa.Table.from_pylist(rows)
    feather.write_feather(
        table,
        output_path,
        compression=None if compression == "uncompressed" else compression,
    )
    return table.num_rows


def main() -> int:
    args = parse_args()
    total_rows = 0

    for raw_input in args.inputs:
        input_path = Path(raw_input)
        if not input_path.is_file():
            raise FileNotFoundError(f"Input file not found: {input_path}")

        output_dir = args.out_dir if args.out_dir is not None else input_path.parent
        output_dir.mkdir(parents=True, exist_ok=True)
        output_path = output_dir / f"{input_path.stem}.feather"

        rows = convert_file(input_path, output_path, args.compression)
        total_rows += rows
        print(f"{input_path} -> {output_path} rows={rows}")

    print(f"total_rows={total_rows}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
