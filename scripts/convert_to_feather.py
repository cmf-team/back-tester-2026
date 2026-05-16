#!/usr/bin/env python3
"""Convert Databento-style NDJSON MBO/L3 files to one Feather file per input file."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


SUPPORTED_SUFFIXES = (".mbo.json", ".jsonl", ".ndjson")
FIELDS = (
    "ts_recv",
    "ts_event",
    "instrument_id",
    "order_id",
    "side",
    "action",
    "price",
    "size",
)


def load_pyarrow():
    try:
        import pyarrow as pa
        import pyarrow.feather as feather
    except ModuleNotFoundError as exc:
        print(
            "pyarrow is required for Feather conversion. "
            "Install it with: python3 -m pip install pyarrow",
            file=sys.stderr,
        )
        raise SystemExit(2) from exc

    return pa, feather


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert .mbo.json/.jsonl/.ndjson L3 NDJSON files to Feather."
    )
    parser.add_argument("--input", required=True, type=Path, help="Input file or folder")
    parser.add_argument("--output", required=True, type=Path, help="Output folder")
    parser.add_argument(
        "--batch-size",
        type=int,
        default=200_000,
        help="Rows per Arrow record batch while converting one input file",
    )
    parser.add_argument(
        "--compression",
        default="lz4",
        choices=("lz4", "zstd", "uncompressed"),
        help="Feather compression codec",
    )
    return parser.parse_args(argv)


def is_supported_file(path: Path) -> bool:
    name = path.name.lower()
    if not name or name.startswith("."):
        return False
    return any(name.endswith(suffix) for suffix in SUPPORTED_SUFFIXES)


def discover_input_files(input_path: Path) -> list[Path]:
    if input_path.is_file():
        if not is_supported_file(input_path):
            raise ValueError(f"unsupported input file extension: {input_path}")
        return [input_path]

    if not input_path.is_dir():
        raise ValueError(f"input path does not exist or is not readable: {input_path}")

    files = [path for path in input_path.rglob("*") if path.is_file() and is_supported_file(path)]
    files.sort()
    if not files:
        raise ValueError(f"input folder contains no supported NDJSON files: {input_path}")
    return files


def scalar_to_string(value: Any) -> str | None:
    if value is None:
        return None
    return str(value)


def uint_or_none(value: Any) -> int | None:
    if value is None:
        return None
    return int(value)


def extract_row(row: dict[str, Any]) -> dict[str, Any]:
    header = row.get("hd")
    if not isinstance(header, dict):
        header = {}

    return {
        "ts_recv": scalar_to_string(row.get("ts_recv")),
        "ts_event": scalar_to_string(row.get("ts_event", header.get("ts_event"))),
        "instrument_id": uint_or_none(row.get("instrument_id", header.get("instrument_id"))),
        "order_id": scalar_to_string(row.get("order_id")),
        "side": scalar_to_string(row.get("side")),
        "action": scalar_to_string(row.get("action")),
        "price": scalar_to_string(row.get("price")),
        "size": uint_or_none(row.get("size")),
    }


def make_schema(pa):
    return pa.schema(
        [
            ("ts_recv", pa.string()),
            ("ts_event", pa.string()),
            ("instrument_id", pa.uint64()),
            ("order_id", pa.string()),
            ("side", pa.string()),
            ("action", pa.string()),
            ("price", pa.string()),
            ("size", pa.uint64()),
        ]
    )


def flush_batch(pa, schema, columns: dict[str, list[Any]], batches: list[Any]) -> None:
    if not columns["ts_recv"]:
        return

    arrays = [
        pa.array(columns[field.name], type=field.type)
        for field in schema
    ]
    batches.append(pa.record_batch(arrays, schema=schema))

    for values in columns.values():
        values.clear()


def output_path_for(input_file: Path, input_root: Path, output_root: Path) -> Path:
    if input_root.is_file():
        relative = Path(input_file.name)
    else:
        relative = input_file.relative_to(input_root)
    return (output_root / relative).with_suffix(".feather")


def convert_file(
    input_file: Path,
    output_file: Path,
    pa,
    feather,
    schema,
    batch_size: int,
    compression: str,
) -> int:
    columns: dict[str, list[Any]] = {field: [] for field in FIELDS}
    batches: list[Any] = []
    row_count = 0

    with input_file.open("r", encoding="utf-8") as input_stream:
        for line_number, line in enumerate(input_stream, start=1):
            if not line.strip():
                continue

            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{input_file}:{line_number}: invalid JSON: {exc}") from exc

            extracted = extract_row(row)
            for field in FIELDS:
                columns[field].append(extracted[field])

            row_count += 1
            if row_count % batch_size == 0:
                flush_batch(pa, schema, columns, batches)

    flush_batch(pa, schema, columns, batches)
    table = pa.Table.from_batches(batches, schema=schema)

    output_file.parent.mkdir(parents=True, exist_ok=True)
    codec = None if compression == "uncompressed" else compression
    feather.write_feather(table, output_file, compression=codec)
    return row_count


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.batch_size <= 0:
        print("--batch-size must be greater than zero", file=sys.stderr)
        return 2

    pa, feather = load_pyarrow()
    schema = make_schema(pa)

    try:
        input_files = discover_input_files(args.input)
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    total_rows = 0
    converted_files = 0

    try:
        for input_file in input_files:
            output_file = output_path_for(input_file, args.input, args.output)
            rows = convert_file(
                input_file=input_file,
                output_file=output_file,
                pa=pa,
                feather=feather,
                schema=schema,
                batch_size=args.batch_size,
                compression=args.compression,
            )
            converted_files += 1
            total_rows += rows
            print(f"converted {input_file} -> {output_file} rows={rows}")
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print("Conversion Summary")
    print(f"input={args.input}")
    print(f"output={args.output}")
    print(f"converted_files={converted_files}")
    print(f"total_rows={total_rows}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
