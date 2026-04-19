#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
import tempfile
import time
import zipfile
from dataclasses import dataclass
from pathlib import Path


def find_default_inbox(repo_root: Path) -> Path | None:
    for base in (repo_root, *repo_root.parents):
        candidate = base / ".axxeny-code" / "tasks" / "001-hw1" / "inbox"
        if candidate.exists():
            return candidate
    return None


@dataclass
class BenchRow:
    zip_path: str
    member: str
    bytes_uncompressed: int
    messages: int
    elapsed_sec: float
    msgs_per_sec: float
    skipped_rtype: int
    skipped_parse: int
    out_of_order_ts_recv: int
    exit_code: int


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    default_binary = repo_root / "build" / "bin" / "back-tester"
    default_inbox = find_default_inbox(repo_root)

    parser = argparse.ArgumentParser(
        description=(
            "Benchmark back-tester over all .mbo.json members found inside one or "
            "more task zip files. Extracts one member at a time to temp storage."
        )
    )
    parser.add_argument(
        "--binary",
        type=Path,
        default=default_binary,
        help="Path to back-tester binary.",
    )
    parser.add_argument(
        "--zip",
        dest="zip_paths",
        action="append",
        type=Path,
        default=[],
        help=(
            "Zip file to benchmark. Repeatable. Default: all zips in the "
            "auto-detected task inbox when available."
        ),
    )
    parser.add_argument(
        "--task-inbox",
        type=Path,
        default=default_inbox,
        help=(
            "Directory holding input zip files. Default: auto-detect the repo-local "
            "task inbox when available."
        ),
    )
    parser.add_argument(
        "--limit-members",
        type=int,
        default=None,
        help="Only bench first N .mbo.json members per zip.",
    )
    parser.add_argument(
        "--member-substr",
        type=str,
        default=None,
        help="Only bench members whose path contains this substring.",
    )
    parser.add_argument(
        "--csv-out",
        type=Path,
        default=None,
        help="Optional CSV output path.",
    )
    parser.add_argument(
        "--md-out",
        type=Path,
        default=None,
        help="Optional Markdown report output path.",
    )
    return parser.parse_args()


def pick_zip_paths(task_inbox: Path | None, explicit: list[Path]) -> list[Path]:
    if explicit:
        return explicit
    if task_inbox is None:
        raise FileNotFoundError(
            "No default zip location found. Pass --zip or --task-inbox."
        )
    paths = sorted(task_inbox.glob("*.zip"))
    if not paths:
        raise FileNotFoundError(f"No zip files found in {task_inbox}")
    return paths


def parse_summary(stdout: str) -> dict[str, int]:
    lines = [line.strip() for line in stdout.splitlines() if line.strip()]
    summary_line = next(
        (line for line in reversed(lines) if line.startswith("total=")),
        None,
    )
    if summary_line is None:
        raise ValueError("Could not find summary line in binary output")

    result: dict[str, int] = {}
    for token in summary_line.split():
        key, value = token.split("=", 1)
        result[key] = int(value)
    return result


def bench_member(binary: Path, zip_path: Path, info: zipfile.ZipInfo) -> BenchRow:
    with tempfile.TemporaryDirectory(prefix="back-tester-bench-") as tmp_dir_name:
        tmp_dir = Path(tmp_dir_name)
        extracted = tmp_dir / Path(info.filename).name

        with zipfile.ZipFile(zip_path) as zf:
            with zf.open(info.filename) as src, extracted.open("wb") as dst:
                dst.write(src.read())

        start = time.perf_counter()
        result = subprocess.run(
            [str(binary), str(extracted)],
            capture_output=True,
            text=True,
            check=False,
        )
        elapsed = time.perf_counter() - start

    messages = 0
    skipped_rtype = 0
    skipped_parse = 0
    out_of_order = 0
    if result.returncode == 0:
        summary = parse_summary(result.stdout)
        messages = summary["total"]
        skipped_rtype = summary["skipped_rtype"]
        skipped_parse = summary["skipped_parse"]
        out_of_order = summary["out_of_order_ts_recv"]

    msgs_per_sec = messages / elapsed if elapsed > 0 else 0.0
    return BenchRow(
        zip_path=str(zip_path),
        member=info.filename,
        bytes_uncompressed=info.file_size,
        messages=messages,
        elapsed_sec=elapsed,
        msgs_per_sec=msgs_per_sec,
        skipped_rtype=skipped_rtype,
        skipped_parse=skipped_parse,
        out_of_order_ts_recv=out_of_order,
        exit_code=result.returncode,
    )


def write_csv(path: Path, rows: list[BenchRow]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as fh:
        writer = csv.writer(fh)
        writer.writerow(BenchRow.__annotations__.keys())
        for row in rows:
            writer.writerow(
                [
                    row.zip_path,
                    row.member,
                    row.bytes_uncompressed,
                    row.messages,
                    f"{row.elapsed_sec:.6f}",
                    f"{row.msgs_per_sec:.2f}",
                    row.skipped_rtype,
                    row.skipped_parse,
                    row.out_of_order_ts_recv,
                    row.exit_code,
                ]
            )


def write_markdown(path: Path, rows: list[BenchRow]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    total_messages = sum(row.messages for row in rows)
    total_elapsed = sum(row.elapsed_sec for row in rows)
    total_rate = total_messages / total_elapsed if total_elapsed > 0 else 0.0

    with path.open("w") as fh:
        fh.write("# Ingest Bench\n\n")
        fh.write(
            f"- files: {len(rows)}\n"
            f"- messages: {total_messages}\n"
            f"- elapsed_sec: {total_elapsed:.6f}\n"
            f"- msgs_per_sec: {total_rate:.2f}\n\n"
        )
        fh.write(
            "| zip | member | bytes | messages | elapsed_sec | msgs_per_sec | exit |\n"
        )
        fh.write("|---|---|---:|---:|---:|---:|---:|\n")
        for row in rows:
            fh.write(
                f"| `{Path(row.zip_path).name}` | `{Path(row.member).name}` | "
                f"{row.bytes_uncompressed} | {row.messages} | {row.elapsed_sec:.6f} | "
                f"{row.msgs_per_sec:.2f} | {row.exit_code} |\n"
            )


def main() -> int:
    args = parse_args()
    if not args.binary.exists():
        raise FileNotFoundError(
            f"Binary not found: {args.binary}. Build first with `cmake -S . -B build && cmake --build build`."
        )

    rows: list[BenchRow] = []
    for zip_path in pick_zip_paths(args.task_inbox, args.zip_paths):
        with zipfile.ZipFile(zip_path) as zf:
            members = [
                info
                for info in zf.infolist()
                if info.filename.endswith(".mbo.json") and not info.is_dir()
            ]
        if args.member_substr is not None:
            members = [info for info in members if args.member_substr in info.filename]
        if args.limit_members is not None:
            members = members[: args.limit_members]

        for info in members:
            row = bench_member(args.binary, zip_path, info)
            rows.append(row)
            print(
                f"{Path(zip_path).name},{Path(info.filename).name},"
                f"messages={row.messages},elapsed_sec={row.elapsed_sec:.6f},"
                f"msgs_per_sec={row.msgs_per_sec:.2f},exit={row.exit_code}"
            )

    if not rows:
        print("No matching .mbo.json members found.", file=sys.stderr)
        return 2

    total_messages = sum(row.messages for row in rows)
    total_elapsed = sum(row.elapsed_sec for row in rows)
    total_rate = total_messages / total_elapsed if total_elapsed > 0 else 0.0
    print(
        f"TOTAL files={len(rows)} messages={total_messages} "
        f"elapsed_sec={total_elapsed:.6f} msgs_per_sec={total_rate:.2f}"
    )

    if args.csv_out is not None:
        write_csv(args.csv_out, rows)
        print(f"csv_out={args.csv_out}")
    if args.md_out is not None:
        write_markdown(args.md_out, rows)
        print(f"md_out={args.md_out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
