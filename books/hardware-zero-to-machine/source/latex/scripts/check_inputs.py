#!/usr/bin/env python3
"""Validate the reachable LaTeX input graph for the hardware book."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main.tex"
INPUT_RE = re.compile(r"\\input\{([^}]+)\}")
CHAPTER_RE = re.compile(r"\\chapter\{")


def resolve_input(name: str) -> Path:
    path = ROOT / name
    if path.suffix == "":
        path = path.with_suffix(".tex")
    return path.resolve()


def main() -> int:
    if not MAIN.is_file():
        raise SystemExit(f"missing main file: {MAIN}")

    visited: set[Path] = set()
    active: list[Path] = []
    chapter_count = 0

    def visit(path: Path) -> None:
        nonlocal chapter_count
        if path in active:
            cycle = " -> ".join(item.relative_to(ROOT).as_posix() for item in (*active, path))
            raise SystemExit(f"cyclic LaTeX input: {cycle}")
        if path in visited:
            return
        if not path.is_file():
            raise SystemExit(f"missing LaTeX input: {path.relative_to(ROOT)}")
        try:
            path.relative_to(ROOT)
        except ValueError as error:
            raise SystemExit(f"input escapes LaTeX source root: {path}") from error

        active.append(path)
        text = path.read_text(encoding="utf-8")
        chapter_count += len(CHAPTER_RE.findall(text))
        for name in INPUT_RE.findall(text):
            visit(resolve_input(name))
        active.pop()
        visited.add(path)

    visit(MAIN.resolve())
    if chapter_count == 0:
        raise SystemExit("reachable manuscript contains no numbered chapters")

    print(
        "checked hardware-zero-to-machine LaTeX manuscript inputs: "
        f"{len(visited)} reachable files, {chapter_count} numbered chapters"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
