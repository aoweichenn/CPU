#!/usr/bin/env python3
"""Export the formal OS book and its evolution sample without deleting either."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
from pathlib import Path


PHONE_ROOT = Path("/mnt/sdcard/STU/BOOKS")
FORMAL_NAME = "操作系统第一册.pdf"
SAMPLE_NAME = "操作系统演进样本.pdf"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--formal", required=True, type=Path)
    parser.add_argument("--sample", required=True, type=Path)
    parser.add_argument("--dest", required=True, type=Path)
    return parser.parse_args()


def ensure_phone_destination(dest: Path) -> Path:
    resolved = dest.resolve()
    root = PHONE_ROOT.resolve()
    if resolved == root or root not in resolved.parents:
        raise SystemExit(f"destination must be below {PHONE_ROOT}: {dest}")
    resolved.mkdir(parents=True, exist_ok=True)
    return resolved


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def copy_atomically(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise SystemExit(f"missing PDF: {source}")

    temporary = destination.with_name(f".{destination.name}.tmp")
    try:
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            temporary.unlink()

    if digest(source) != digest(destination):
        raise SystemExit(f"hash mismatch after export: {destination}")


def main() -> int:
    args = parse_args()
    destination = ensure_phone_destination(args.dest)

    outputs = (
        (args.formal.resolve(), destination / FORMAL_NAME),
        (args.sample.resolve(), destination / SAMPLE_NAME),
    )
    for source, output in outputs:
        copy_atomically(source, output)
        print(f"{output}  sha256={digest(output)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
