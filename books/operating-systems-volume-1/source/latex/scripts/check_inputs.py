#!/usr/bin/env python3
"""Validate the Operating Systems LaTeX manuscript wiring."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main.tex"


INPUT_RE = re.compile(r"\\input(?:topic)?\{([^}]+)\}")


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
    return len(re.findall(r"^\\chapter\{", text, flags=re.MULTILINE))


def main() -> int:
    missing = [path for path in input_paths() if not path.is_file()]
    if missing:
        print("missing input files:")
        for path in missing:
            print(path)
        return 1

    chapters = chapter_heading_count()
    if chapters != 7:
        print(f"expected 7 top-level chapters in main.tex, found {chapters}")
        return 1

    required = (
        ROOT / "frontmatter" / "abbreviations.tex",
        ROOT / "backmatter" / "capability-checklist.tex",
        ROOT / "backmatter" / "source-reading-index.tex",
    )
    missing_required = [path for path in required if not path.is_file()]
    if missing_required:
        print("missing required front/back matter:")
        for path in missing_required:
            print(path)
        return 1

    print("checked Operating Systems LaTeX manuscript inputs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
