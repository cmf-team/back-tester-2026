#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import shutil
import subprocess
import sys
import tempfile
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
    strategy: str
    files: int
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
            "Benchmark hard-variant folder ingest. By default extracts all "
            ".mbo.json members from task zip files into a temp folder, then "
            "runs both merge strategies."
        )
    )
    parser.add_argument("--binary", type=Path, default=default_binary)
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=None,
        help="Pre-extracted folder of .json/.ndjson files. Skips zip extraction.",
    )
    parser.add_argument(
        "--zip",
        dest="zip_paths",
        action="append",
        type=Path,
        default=[],
        help=(
            "Zip file to use. Repeatable. Default: all zips in the auto-detected "
            "task inbox when available."
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
        "--member-substr",
        type=str,
        default=None,
        help="Only extract members whose path contains this substring.",
    )
    parser.add_argument(
        "--limit-files",
        type=int,
        default=None,
        help="Only extract first N matching members after sorting.",
    )
    parser.add_argument(
        "--scratch-dir",
        type=Path,
        default=None,
        help=(
            "Directory under which temporary extracted files should live. "
            "Useful when the default system temp volume is too small."
        ),
    )
    parser.add_argument("--csv-out", type=Path, default=None)
    parser.add_argument("--md-out", type=Path, default=None)
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


def extract_members(args: argparse.Namespace, dst: Path) -> int:
    extracted = 0
    members: list[tuple[Path, zipfile.ZipInfo]] = []
    for zip_path in pick_zip_paths(args.task_inbox, args.zip_paths):
        with zipfile.ZipFile(zip_path) as zf:
            for info in zf.infolist():
                if info.is_dir() or not info.filename.endswith(".mbo.json"):
                    continue
                if args.member_substr and args.member_substr not in info.filename:
                    continue
                members.append((zip_path, info))

    members.sort(key=lambda item: (item[0].name, item[1].filename))
    if args.limit_files is not None:
        members = members[: args.limit_files]
    if not members:
        raise FileNotFoundError("No matching .mbo.json members found.")

    for index, (zip_path, info) in enumerate(members):
        dst_name = f"{index:03d}_{Path(info.filename).name}"
        out_path = dst / dst_name
        with zipfile.ZipFile(zip_path) as zf:
            with zf.open(info.filename) as src, out_path.open("wb") as out:
                shutil.copyfileobj(src, out)
        extracted += 1
    return extracted


def parse_summary(stdout: str) -> dict[str, str]:
    lines = [line.strip() for line in stdout.splitlines() if line.strip()]
    summary_line = next(
        (line for line in reversed(lines) if line.startswith("strategy=")),
        None,
    )
    if summary_line is None:
        raise ValueError("Could not find strategy summary line in binary output")

    result: dict[str, str] = {}
    for token in summary_line.split():
        key, value = token.split("=", 1)
        result[key] = value
    return result


def bench_strategy(binary: Path, input_dir: Path, strategy: str) -> BenchRow:
    result = subprocess.run(
        [
            str(binary),
            str(input_dir),
            "--strategy",
            strategy,
            "--preview-limit",
            "0",
        ],
        capture_output=True,
        text=True,
        check=False,
    )

    if result.returncode != 0:
        return BenchRow(strategy, 0, 0, 0.0, 0.0, 0, 0, 0, result.returncode)

    summary = parse_summary(result.stdout)
    return BenchRow(
        strategy=summary["strategy"],
        files=int(summary["files"]),
        messages=int(summary["total"]),
        elapsed_sec=float(summary["elapsed_sec"]),
        msgs_per_sec=float(summary["msgs_per_sec"]),
        skipped_rtype=int(summary["skipped_rtype"]),
        skipped_parse=int(summary["skipped_parse"]),
        out_of_order_ts_recv=int(summary["out_of_order_ts_recv"]),
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
                    row.strategy,
                    row.files,
                    row.messages,
                    f"{row.elapsed_sec:.6f}",
                    f"{row.msgs_per_sec:.2f}",
                    row.skipped_rtype,
                    row.skipped_parse,
                    row.out_of_order_ts_recv,
                    row.exit_code,
                ]
            )


def write_markdown(path: Path, rows: list[BenchRow], input_dir: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as fh:
        fh.write("# Hard Ingest Bench\n\n")
        fh.write(f"- input_dir: `{input_dir}`\n")
        fh.write(f"- strategies: {', '.join(row.strategy for row in rows)}\n\n")
        fh.write(
            "| strategy | files | messages | elapsed_sec | msgs_per_sec | skipped_rtype | skipped_parse | out_of_order | exit |\n"
        )
        fh.write("|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for row in rows:
            fh.write(
                f"| `{row.strategy}` | {row.files} | {row.messages} | "
                f"{row.elapsed_sec:.6f} | {row.msgs_per_sec:.2f} | "
                f"{row.skipped_rtype} | {row.skipped_parse} | "
                f"{row.out_of_order_ts_recv} | {row.exit_code} |\n"
            )


def main() -> int:
    args = parse_args()
    if not args.binary.exists():
        raise FileNotFoundError(
            f"Binary not found: {args.binary}. Build first with `cmake -S . -B build && cmake --build build`."
        )

    scratch_dir = args.scratch_dir
    if scratch_dir is not None:
        scratch_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(
        prefix="back-tester-hard-bench-",
        dir=str(scratch_dir) if scratch_dir is not None else None,
    ) as tmp_name:
        extracted_dir = Path(tmp_name)
        input_dir = args.input_dir
        if input_dir is None:
            extract_count = extract_members(args, extracted_dir)
            input_dir = extracted_dir
            print(f"extracted_files={extract_count}")

        rows = [
            bench_strategy(args.binary, input_dir, "flat"),
            bench_strategy(args.binary, input_dir, "hierarchy"),
        ]

        for row in rows:
            print(
                f"strategy={row.strategy},files={row.files},messages={row.messages},"
                f"elapsed_sec={row.elapsed_sec:.6f},msgs_per_sec={row.msgs_per_sec:.2f},"
                f"exit={row.exit_code}"
            )

        if any(row.exit_code != 0 for row in rows):
            return 1

        if args.csv_out is not None:
            write_csv(args.csv_out, rows)
            print(f"csv_out={args.csv_out}")
        if args.md_out is not None:
            write_markdown(args.md_out, rows, input_dir)
            print(f"md_out={args.md_out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
