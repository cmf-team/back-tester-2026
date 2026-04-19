#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


def find_default_inbox(repo_root: Path) -> Path | None:
    for base in (repo_root, *repo_root.parents):
        candidate = base / ".axxeny-code" / "tasks" / "001-hw1" / "inbox"
        if candidate.exists():
            return candidate
    return None


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    default_binary = repo_root / "build" / "bin" / "back-tester"
    default_inbox = find_default_inbox(repo_root)

    parser = argparse.ArgumentParser(
        description=(
            "Run a bounded smoke test of the ingest binary against one real "
            "Databento JSON member stored inside a task zip."
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
        dest="zip_path",
        type=Path,
        default=None,
        help="Specific zip file to use. Default: smallest zip in task inbox.",
    )
    parser.add_argument(
        "--member",
        type=str,
        default=None,
        help=(
            "Specific .mbo.json member inside zip. Default: smallest .mbo.json member."
        ),
    )
    parser.add_argument(
        "--keep-json",
        action="store_true",
        help="Keep extracted JSON file instead of deleting temp dir.",
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
    return parser.parse_args()


def pick_zip(task_inbox: Path | None) -> Path:
    if task_inbox is None:
        raise FileNotFoundError(
            "No default zip location found. Pass --zip or --task-inbox."
        )
    candidates = sorted(task_inbox.glob("*.zip"))
    if not candidates:
        raise FileNotFoundError(f"No zip files found in {task_inbox}")
    return min(candidates, key=lambda path: path.stat().st_size)


def pick_member(zf: zipfile.ZipFile) -> zipfile.ZipInfo:
    members = [
        info
        for info in zf.infolist()
        if info.filename.endswith(".mbo.json") and not info.is_dir()
    ]
    if not members:
        raise FileNotFoundError("No .mbo.json members found in zip")
    return min(members, key=lambda info: info.file_size)


def run_smoke(binary: Path, zip_path: Path, member_name: str, keep_json: bool) -> int:
    if not binary.exists():
        raise FileNotFoundError(
            f"Binary not found: {binary}. Build first with `cmake -S . -B build && cmake --build build`."
        )

    with tempfile.TemporaryDirectory(prefix="back-tester-smoke-") as tmp_dir_name:
        tmp_dir = Path(tmp_dir_name)
        extracted = tmp_dir / Path(member_name).name

        with zipfile.ZipFile(zip_path) as zf:
            with zf.open(member_name) as src, extracted.open("wb") as dst:
                dst.write(src.read())

        command = [str(binary), str(extracted)]
        result = subprocess.run(command, capture_output=True, text=True, check=False)

        print(f"zip={zip_path}")
        print(f"member={member_name}")
        print(f"json={extracted}")
        print(f"exit_code={result.returncode}")
        if result.stdout:
            print("\n--- stdout (head 20 lines) ---")
            for line in result.stdout.splitlines()[:20]:
                print(line)
        if result.stderr:
            print("\n--- stderr ---")
            print(result.stderr.rstrip())

        if keep_json:
            kept = extracted.with_suffix(extracted.suffix + ".kept")
            extracted.rename(kept)
            print(f"\nkept_json={kept}")

        return result.returncode


def main() -> int:
    args = parse_args()
    zip_path = args.zip_path or pick_zip(args.task_inbox)

    with zipfile.ZipFile(zip_path) as zf:
        member = args.member or pick_member(zf).filename

    return run_smoke(args.binary, zip_path, member, args.keep_json)


if __name__ == "__main__":
    sys.exit(main())
