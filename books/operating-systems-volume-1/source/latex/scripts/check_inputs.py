#!/usr/bin/env python3
"""Validate the Operating Systems LaTeX manuscript wiring."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main.tex"


INPUT_RE = re.compile(r"\\input(?:topic|detail)?\{([^}]+)\}")
CHAPTER_INPUT_RE = re.compile(r"^\\input\{chapters/([^}]+)\}", re.MULTILINE)

EXPECTED_CHAPTERS = 13
# Only these files may be included as numbered chapters. Extra hardware,
# source-reading, and lab material must be wired through \inputdetail so the
# printed book stays at 13 chapters instead of drifting back to a 20+ chapter
# outline.
EXPECTED_CHAPTER_INPUTS = (
    "ch01-os-map-and-contracts",
    "ch00-hardware-foundations",
    "ch00-machine-contracts",
    "ch05-process-thread-scheduling",
    "ch09-system-calls-permissions",
    "ch10-exec-elf-loader",
    "ch11-virtual-memory",
    "ch12-mmap-page-cache",
    "ch14-device-drivers-dma",
    "ch16-files-devices-io",
    "ch17b-filesystem-from-scratch",
    "ch18-socket-network-entry",
    "ch20-security-isolation",
)


def input_paths() -> list[Path]:
    paths: list[Path] = []
    for line in MAIN.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        match = INPUT_RE.search(stripped)
        if match is None:
            continue
        paths.append(ROOT / f"{match.group(1)}.tex")
    return paths


def chapter_heading_count() -> int:
    text = MAIN.read_text(encoding="utf-8")
    count = len(re.findall(r"^\\chapter\{", text, flags=re.MULTILINE))
    for relative in CHAPTER_INPUT_RE.findall(text):
        path = ROOT / "chapters" / f"{relative}.tex"
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
    if parts != 5:
        print(f"expected 5 top-level parts in main.tex, found {parts}")
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
        ROOT / "backmatter" / "source-reading-index.tex",
        ROOT / "supplements" / "hardware",
        ROOT / "supplements" / "pre-os",
        ROOT / "supplements" / "models",
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
