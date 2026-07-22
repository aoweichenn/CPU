#!/usr/bin/env python3
"""Audit generated chapters for complete, non-duplicated pseudocode comments."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys


SCRIPT_DIR = Path(__file__).resolve().parent
BUILDER_PATH = SCRIPT_DIR / "build_26_chapters.py"


def load_builder():
    spec = importlib.util.spec_from_file_location("hardware_book_builder", BUILDER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load builder: {BUILDER_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    builder = load_builder()
    root = builder.find_root(Path.cwd().resolve())
    chapter_dir = root / "books/hardware-zero-to-machine/source/latex/chapters26"
    total_blocks = 0

    for chapter in builder.CHAPTERS:
        path = chapter_dir / f"ch{chapter.number:02d}-{chapter.slug}.tex"
        if not path.is_file():
            raise FileNotFoundError(f"missing generated chapter: {path}")
        total_blocks += builder.audit_pseudocode_annotations(
            path.read_text(encoding="utf-8"), chapter.number
        )

    expected_blocks = sum(
        item.expected
        for specs in builder.PSEUDOCODE_SPECS.values()
        for item in specs
    )
    if total_blocks != expected_blocks:
        raise ValueError(
            f"pseudocode total mismatch: expected {expected_blocks}, found {total_blocks}"
        )

    print(
        f"pseudocode comment audit OK: {total_blocks} blocks, "
        f"{total_blocks * 2} generated teaching comments"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
