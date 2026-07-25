#!/usr/bin/env python3
"""Validate the Operating Systems LaTeX manuscript wiring."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main.tex"
BOOKS_ROOT = ROOT.parents[2]
SAMPLE_CHAPTERS = (
    BOOKS_ROOT
    / "operating-systems-evolution-sample"
    / "source"
    / "latex"
    / "chapters"
)
EVOLUTION_DEST = ROOT / "chapters18" / "evolution"
FILESYSTEM_SOURCE = ROOT / "chapters" / "ch17b-filesystem-from-scratch.tex"
FILESYSTEM_OUTPUTS = (
    "ch11-filesystem-formation.tex",
    "ch12-filesystem-indexing.tex",
    "ch13-filesystem-recovery.tex",
    "ch14-modern-filesystems.tex",
)


INPUT_RE = re.compile(
    r"\\input(?:topic|detail|chapterbody|samplechapter)?\{([^}]+)\}"
)
CHAPTER_INPUT_RE = re.compile(r"^\\input\{chapters18/([^}]+)\}", re.MULTILINE)

EXPECTED_PARTS = 5
EXPECTED_CHAPTERS = 18
# The formal volume follows the companion project's milestones first, then
# deepens each milestone into the corresponding operating-system mechanism.
EXPECTED_CHAPTER_INPUTS = (
    "ch01-bare-machine-to-kernel",
    "ch06-address-space",
    "ch04-exec-image",
    "ch03-system-call-boundary",
    "ch08-kernel-allocators",
    "ch09-device-requests",
    "ch02-task-scheduling",
    "ch05-synchronization",
    "ch10-block-vfs",
    "ch11-filesystem-formation",
    "ch12-filesystem-indexing",
    "ch07-file-backed-memory",
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


def generated_drift() -> list[str]:
    """Return generated chapter assets that no longer match their sources."""
    drift: list[str] = []
    for source in sorted(SAMPLE_CHAPTERS.rglob("*.tex")):
        relative = source.relative_to(SAMPLE_CHAPTERS)
        destination = EVOLUTION_DEST / relative
        expected = source.read_text(encoding="utf-8").replace(
            r"\input{chapters/ch01/",
            r"\input{chapters18/evolution/ch01/",
        )
        if not destination.is_file() or destination.read_text(encoding="utf-8") != expected:
            drift.append(str(destination.relative_to(ROOT)))

    filesystem = FILESYSTEM_SOURCE.read_text(encoding="utf-8")
    filesystem = filesystem.removeprefix("\\begin{filesystemlayout}\n\n")
    filesystem = filesystem.removesuffix("\n\\end{filesystemlayout}\n")
    markers = [
        index
        for index in range(len(filesystem))
        if filesystem.startswith("\\chapter{", index)
    ]
    if len(markers) != len(FILESYSTEM_OUTPUTS):
        drift.append("chapters/ch17b-filesystem-from-scratch.tex: chapter markers")
        return drift

    markers.append(len(filesystem))
    for output_name, start, end in zip(
        FILESYSTEM_OUTPUTS, markers[:-1], markers[1:], strict=True
    ):
        expected = (
            "\\begin{filesystemlayout}\n\n"
            + filesystem[start:end].strip()
            + "\n\n\\end{filesystemlayout}\n"
        )
        output = ROOT / "chapters18" / output_name
        if not output.is_file() or output.read_text(encoding="utf-8") != expected:
            drift.append(str(output.relative_to(ROOT)))
    return drift


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
        ROOT / "frontmatter" / "project-spine.tex",
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

    drift = generated_drift()
    if drift:
        print("generated chapter assets are stale:")
        for path in drift:
            print(f"  {path}")
        print("run source/latex/scripts/restructure_volume.py and inspect the result")
        return 1

    print("checked Operating Systems LaTeX manuscript inputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
