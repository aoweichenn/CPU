#!/usr/bin/env python3
"""Validate the hardware book LaTeX input structure."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main.tex"

EXPECTED_INPUTS = [
    "frontmatter/title",
    "frontmatter/preface",
    "chapters/ch01-electricity-diode-switch",
    "chapters/ch02-feedback-latches-flipflops",
    "chapters/ch03-number-arithmetic-circuits",
    "chapters/ch04-minimal-cpu",
    "chapters/ch05-memory-bus-io-dma",
    "chapters/ch05b-storage-cell-structures",
    "chapters/ch06-classic-chips-platforms",
    "chapters/ch07-breadboard-computer",
    "chapters/ch08-power-clock-signal-integrity",
    "chapters/ch09-rtl-verilog-reading",
    "chapters/ch10-performance-analysis",
    "chapters/ch11-peripherals-queues",
    "chapters/ch12-gpu-display",
    "chapters/ch13-linking-loading",
    "backmatter/checklist",
]


def main() -> int:
    text = MAIN.read_text(encoding="utf-8")
    inputs = re.findall(r"\\input\{([^}]+)\}", text)
    missing = [item for item in EXPECTED_INPUTS if item not in inputs]
    if missing:
        raise SystemExit(f"missing expected inputs: {missing}")

    chapter_files = sorted((ROOT / "chapters").glob("*.tex"))
    if len(chapter_files) != 14:
        raise SystemExit(f"expected 14 chapter files, found {len(chapter_files)}")

    for chapter in chapter_files:
        chapter_text = chapter.read_text(encoding="utf-8")
        sections = re.findall(r"\\section\{", chapter_text)
        if len(sections) > 8:
            raise SystemExit(f"{chapter.name} has too many sections: {len(sections)}")

    print("checked hardware-zero-to-machine LaTeX manuscript inputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
