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
    "chapters/ch02-switches-to-gates",
    "chapters/ch03-combinational-logic",
    "chapters/ch04-feedback-latches-flipflops",
    "chapters/ch05-clock-reset-sync",
    "chapters/ch06-registers-counters-fsm",
    "chapters/ch07-number-arithmetic-circuits",
    "chapters/ch08-alu-flags",
    "chapters/ch09-datapath",
    "chapters/ch10-controller",
    "chapters/ch11-minimal-cpu",
    "chapters/ch12-memory-bus-io-machine",
    "backmatter/checklist",
]


def main() -> int:
    text = MAIN.read_text(encoding="utf-8")
    inputs = re.findall(r"\\input\{([^}]+)\}", text)
    missing = [item for item in EXPECTED_INPUTS if item not in inputs]
    if missing:
        raise SystemExit(f"missing expected inputs: {missing}")

    chapter_files = sorted((ROOT / "chapters").glob("*.tex"))
    if len(chapter_files) != 12:
        raise SystemExit(f"expected 12 chapter files, found {len(chapter_files)}")

    for chapter in chapter_files:
        chapter_text = chapter.read_text(encoding="utf-8")
        sections = re.findall(r"\\section\{", chapter_text)
        if len(sections) > 5:
            raise SystemExit(f"{chapter.name} has too many sections: {len(sections)}")

    print("checked hardware-zero-to-machine LaTeX manuscript inputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
