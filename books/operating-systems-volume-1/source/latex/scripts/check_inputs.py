#!/usr/bin/env python3
"""Validate the Operating Systems LaTeX manuscript wiring."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main.tex"


INPUT_RE = re.compile(
    r"\\input(?:topic|detail|chapterbody|samplechapter)?\{([^}]+)\}"
)
CHAPTER_INPUT_RE = re.compile(r"^\\input\{chapters18/([^}]+)\}", re.MULTILINE)

EXPECTED_PARTS = 5
EXPECTED_CHAPTERS = 18
# The formal volume follows one causal route from a no-OS machine to diagnosis.
EXPECTED_CHAPTER_INPUTS = (
    "ch01-bare-machine-to-kernel",
    "ch02-task-scheduling",
    "ch03-system-call-boundary",
    "ch04-exec-image",
    "ch05-synchronization",
    "ch06-address-space",
    "ch07-file-backed-memory",
    "ch08-kernel-allocators",
    "ch09-device-requests",
    "ch10-block-vfs",
    "ch11-filesystem-formation",
    "ch12-filesystem-indexing",
    "ch13-filesystem-recovery",
    "ch14-modern-filesystems",
    "ch15-network-endpoints",
    "ch16-security-isolation",
    "ch17-persistence",
    "ch18-observability",
)


def input_paths() -> list[Path]:
    paths: list[Path] = []
    pending = [MAIN]
    visited: set[Path] = set()
    while pending:
        current = pending.pop()
        if current in visited or not current.is_file():
            continue
        visited.add(current)
        for match in INPUT_RE.finditer(current.read_text(encoding="utf-8")):
            if "#" in match.group(1):
                continue
            path = ROOT / f"{match.group(1)}.tex"
            paths.append(path)
            pending.append(path)
    return paths


def chapter_heading_count() -> int:
    text = MAIN.read_text(encoding="utf-8")
    count = len(re.findall(r"^\\chapter\{", text, flags=re.MULTILINE))
    for relative in CHAPTER_INPUT_RE.findall(text):
        path = ROOT / "chapters18" / f"{relative}.tex"
        if path.is_file():
            count += len(re.findall(r"^\\chapter\{", path.read_text(encoding="utf-8"), flags=re.MULTILINE))
    return count


def chapter_inputs() -> tuple[str, ...]:
    return tuple(CHAPTER_INPUT_RE.findall(MAIN.read_text(encoding="utf-8")))


def part_heading_count() -> int:
    text = MAIN.read_text(encoding="utf-8")
    return len(re.findall(r"^\\part\{", text, flags=re.MULTILINE))


def main() -> int:
    missing = [path for path in input_paths() if not path.is_file()]
    if missing:
        print("missing input files:")
        for path in missing:
            print(path)
        return 1

    parts = part_heading_count()
    if parts != EXPECTED_PARTS:
        print(f"expected {EXPECTED_PARTS} top-level parts in main.tex, found {parts}")
        return 1

    chapters = chapter_heading_count()
    if chapters != EXPECTED_CHAPTERS:
        print(f"expected {EXPECTED_CHAPTERS} numbered chapters in manuscript, found {chapters}")
        return 1

    actual_chapter_inputs = chapter_inputs()
    if actual_chapter_inputs != EXPECTED_CHAPTER_INPUTS:
        print("chapter input order changed:")
        print("expected:")
        for name in EXPECTED_CHAPTER_INPUTS:
            print(f"  {name}")
        print("actual:")
        for name in actual_chapter_inputs:
            print(f"  {name}")
        return 1

    required = (
        ROOT / "frontmatter" / "abbreviations.tex",
        ROOT / "backmatter" / "capability-checklist.tex",
        ROOT / "backmatter" / "supplement-reading-map.tex",
        ROOT / "chapters18" / "evolution",
        ROOT / "scripts" / "restructure_volume.py",
    )
    missing_required = [path for path in required if not path.exists()]
    if missing_required:
        print("missing required front/back matter:")
        for path in missing_required:
            print(path)
        return 1

    print("checked Operating Systems LaTeX manuscript inputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
