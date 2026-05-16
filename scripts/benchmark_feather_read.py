#!/usr/bin/env python3
"""Benchmark NDJSON row parsing against Feather table reads."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Callable


JSON_SUFFIXES = (".mbo.json", ".jsonl", ".ndjson")
FEATHER_SUFFIXES = (".feather", ".ftr")


def load_pyarrow_feather():
    try:
        import pyarrow.feather as feather
    except ModuleNotFoundError as exc:
        print(
            "pyarrow is required for Feather read benchmarking. "
            "Install it with: python3 -m pip install pyarrow",
            file=sys.stderr,
        )
        raise SystemExit(2) from exc

    return feather


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare JSON NDJSON scan/parse speed with Feather read_table speed."
    )
    parser.add_argument("--json-input", required=True, type=Path, help="Input NDJSON file or folder")
    parser.add_argument("--feather-input", required=True, type=Path, help="Input Feather file or folder")
    parser.add_argument(
        "--columns",
        default="ts_recv,ts_event,instrument_id,order_id,side,action,price,size",
        help="Comma-separated Feather columns to read",
    )
    return parser.parse_args(argv)


def is_supported(path: Path, suffixes: tuple[str, ...]) -> bool:
    name = path.name.lower()
    if not name or name.startswith("."):
        return False
    return any(name.endswith(suffix) for suffix in suffixes)


def discover_files(input_path: Path, suffixes: tuple[str, ...], label: str) -> list[Path]:
    if input_path.is_file():
        if not is_supported(input_path, suffixes):
            raise ValueError(f"unsupported {label} file extension: {input_path}")
        return [input_path]

    if not input_path.is_dir():
        raise ValueError(f"{label} path does not exist or is not readable: {input_path}")

    files = [path for path in input_path.rglob("*") if path.is_file() and is_supported(path, suffixes)]
    files.sort()
    if not files:
        raise ValueError(f"{label} folder contains no supported files: {input_path}")
    return files


def benchmark(label: str, files: list[Path], reader: Callable[[Path], int]) -> tuple[int, float]:
    started_at = time.perf_counter()
    rows = 0
    for path in files:
        rows += reader(path)
    elapsed = time.perf_counter() - started_at
    if elapsed <= 0.0:
        elapsed = 1e-12
    return rows, elapsed


def read_json_rows(path: Path) -> int:
    rows = 0
    with path.open("r", encoding="utf-8") as input_stream:
        for line_number, line in enumerate(input_stream, start=1):
            if not line.strip():
                continue
            try:
                json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_number}: invalid JSON: {exc}") from exc
            rows += 1
    return rows


def print_row(label: str, file_count: int, rows: int, elapsed: float) -> None:
    rows_per_second = rows / elapsed if elapsed > 0.0 else 0.0
    print(f"{label},{file_count},{rows},{elapsed:.6f},{rows_per_second:.2f}")


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    feather = load_pyarrow_feather()
    columns = [column.strip() for column in args.columns.split(",") if column.strip()]

    try:
        json_files = discover_files(args.json_input, JSON_SUFFIXES, "JSON")
        feather_files = discover_files(args.feather_input, FEATHER_SUFFIXES, "Feather")
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    def read_feather_rows(path: Path) -> int:
        return feather.read_table(path, columns=columns).num_rows

    try:
        json_rows, json_elapsed = benchmark("json", json_files, read_json_rows)
        feather_rows, feather_elapsed = benchmark("feather", feather_files, read_feather_rows)
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print("Format,Files,Rows,WallClockSeconds,RowsPerSecond")
    print_row("json", len(json_files), json_rows, json_elapsed)
    print_row("feather", len(feather_files), feather_rows, feather_elapsed)

    if json_rows != feather_rows:
        print(
            f"Error: row count mismatch: json_rows={json_rows} feather_rows={feather_rows}",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
