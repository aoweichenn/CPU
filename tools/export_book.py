#!/usr/bin/env python3
"""Export one book as one title-named PDF."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


SOURCE_FILE = "main.pdf"
ALLOWED_EXPORT_ROOTS = (
    Path("book-exports"),
    Path("/mnt/sdcard/STU/BOOKS"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--dest", required=True, type=Path)
    parser.add_argument("--pdf-name", required=True)
    parser.add_argument("--epub-name", help=argparse.SUPPRESS)
    return parser.parse_args()


HYPHEN_LIKE_CHARS = ("-", "‐", "‑", "‒", "–", "—", "―", "－", "−")


def validate_export_name(name: str, label: str) -> None:
    if not name:
        raise SystemExit(f"{label} name must not be empty")
    if any(ch in name for ch in (" ", "+", "/", "\\")):
        raise SystemExit(f"{label} name must not contain spaces, '+', or path separators: {name}")
    if any(ch in name for ch in HYPHEN_LIKE_CHARS):
        raise SystemExit(f"{label} name must not contain hyphen-like characters: {name}")


def ensure_under_allowed_root(path: Path) -> None:
    resolved = path.resolve()
    for root in ALLOWED_EXPORT_ROOTS:
        export_root = root.resolve()
        if resolved != export_root and export_root in resolved.parents:
            return
    roots = ", ".join(str(root) for root in ALLOWED_EXPORT_ROOTS)
    raise SystemExit(f"refusing to clean path outside allowed export roots ({roots}): {path}")


def clean_dest(dest: Path) -> None:
    ensure_under_allowed_root(dest)
    dest.mkdir(parents=True, exist_ok=True)
    for child in dest.iterdir():
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()


def export_book(source: Path, dest: Path, pdf_name: str) -> None:
    validate_export_name(pdf_name, "PDF")
    if not (source / SOURCE_FILE).is_file():
        raise SystemExit(f"missing build artifact in {source}: {SOURCE_FILE}")

    clean_dest(dest)
    output_file = f"{pdf_name}.pdf"
    shutil.copy2(source / SOURCE_FILE, dest / output_file)

    exported = sorted(path.name for path in dest.iterdir() if path.is_file())
    expected = [output_file]
    if exported != expected:
        raise SystemExit(f"unexpected export contents in {dest}: {exported}")

    print(dest / output_file)


def main() -> int:
    args = parse_args()
    export_book(args.source, args.dest, args.pdf_name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
