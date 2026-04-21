#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "pandas",
#   "pyarrow",
#   "tqdm",
# ]
# ///

import os
import pandas as pd
import argparse
from tqdm import tqdm
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert *.mbo.json files in a directory to Feather format."
    )
    parser.add_argument(
        "source_dir", type=Path, help="Directory containing *.mbo.json files"
    )
    return parser.parse_args()


def iter_input_files(source_dir: Path):
    return sorted(source_dir.glob("*.mbo.json"))


def convert_file(src):
    dst = src.with_suffix(src.suffix + ".feather")

    df = pd.read_json(src, lines=True)
    df = df.drop(columns=["hd"]).join(pd.json_normalize(df["hd"]))

    df["ts_recv"] = pd.to_datetime(df["ts_recv"], utc=True).astype("int64")
    df["ts_event"] = pd.to_datetime(df["ts_event"], utc=True).astype("int64")

    df.to_feather(dst)


def validate_source_dir(source_dir):
    if not os.path.exists(source_dir):
        raise ValueError(source_dir + " - not exists")
    if not os.path.isdir(source_dir):
        raise ValueError(source_dir + " - not directory")


def main(args):
    source_dir = args.source_dir

    validate_source_dir(source_dir)

    for filename in tqdm(iter_input_files(source_dir)):
        convert_file(filename)


if __name__ == "__main__":
    main(parse_args())
