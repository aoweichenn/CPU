#!/usr/bin/env python3
"""Export one book with title-safe output filenames."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


SOURCE_PDF = "main.pdf"
SOURCE_EPUB = "main.epub"
ALLOWED_EXPORT_ROOTS = (
    Path("book-exports"),
    Path("/mnt/sdcard/STU/BOOKS"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--dest", required=True, type=Path)
    parser.add_argument("--pdf-name", required=True)
    parser.add_argument("--epub-name")
    parser.add_argument(
        "--pdf-only",
        action="store_true",
        help="export only the title-named PDF and remove stale files in the destination",
    )
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


def export_book(
    source: Path,
    dest: Path,
    pdf_name: str,
    epub_name: str | None,
    *,
    pdf_only: bool,
) -> None:
    validate_export_name(pdf_name, "PDF")
    source_files = [SOURCE_PDF]
    output_files = [f"{pdf_name}.pdf"]
    if not pdf_only:
        if epub_name is None:
            raise SystemExit("--epub-name is required unless --pdf-only is used")
        validate_export_name(epub_name, "EPUB")
        source_files.append(SOURCE_EPUB)
        output_files.append(f"{epub_name}.epub")

    missing = [source_file for source_file in source_files if not (source / source_file).is_file()]
    if missing:
        raise SystemExit(f"missing build artifacts in {source}: {', '.join(missing)}")

    clean_dest(dest)
    copy_pairs = zip(source_files, output_files, strict=True)
    for source_file, output_file in copy_pairs:
        shutil.copy2(source / source_file, dest / output_file)

    exported = sorted(path.name for path in dest.iterdir() if path.is_file())
    expected = sorted(output_files)
    if exported != expected:
        raise SystemExit(f"unexpected export contents in {dest}: {exported}")

    for output_file in output_files:
        print(dest / output_file)


def main() -> int:
    args = parse_args()
    export_book(
        args.source,
        args.dest,
        args.pdf_name,
        args.epub_name,
        pdf_only=args.pdf_only,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
