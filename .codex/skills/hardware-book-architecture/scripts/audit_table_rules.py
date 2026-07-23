#!/usr/bin/env python3
"""Audit reachable hardware-book tables for visible row and column separation."""

from __future__ import annotations

import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
LATEX_ROOT = REPO_ROOT / "books/hardware-zero-to-machine/source/latex"
MAIN = LATEX_ROOT / "main.tex"
INPUT_RE = re.compile(r"\\(?:input|include)\{([^}]+)\}")
TABLE_RE = re.compile(
    r"\\begin\{longtable\}\{([^\n]+)\}(.*?)\\end\{longtable\}", re.DOTALL
)
ROW_END_RE = re.compile(r"\\\\(?:\[[^]]*\])?(?:\\tablerowrule)?\s*$")


def resolve_input(name: str) -> Path:
    candidate = LATEX_ROOT / name
    if candidate.suffix != ".tex":
        candidate = candidate.with_suffix(".tex")
    return candidate.resolve()


def reachable_files() -> list[Path]:
    pending = [MAIN.resolve()]
    seen: set[Path] = set()
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        if not path.is_file():
            raise SystemExit(f"missing reachable LaTeX input: {path}")
        seen.add(path)
        text = path.read_text(encoding="utf-8")
        pending.extend(resolve_input(match.group(1)) for match in INPUT_RE.finditer(text))
    return sorted(seen)


def main() -> None:
    failures: list[str] = []
    table_count = 0
    for path in reachable_files():
        text = path.read_text(encoding="utf-8")
        for index, match in enumerate(TABLE_RE.finditer(text), start=1):
            table_count += 1
            spec, body = match.groups()
            label = f"{path.relative_to(REPO_ROOT)} table {index}"

            column_count = spec.count("L{") + spec.count("p{")
            separators = spec.count("|") + spec.count("L{")
            if column_count >= 2 and separators < column_count - 1:
                failures.append(f"{label}: no visible separator between every adjacent column")

            lines = body.splitlines()
            for line_number, line in enumerate(lines):
                if not ROW_END_RE.search(line):
                    continue
                next_content = ""
                for following in lines[line_number + 1 :]:
                    if following.strip():
                        next_content = following.strip()
                        break
                if next_content not in (r"\midrule", r"\bottomrule") and r"\tablerowrule" not in line:
                    failures.append(f"{label}: a body row has no horizontal separator")

    macros = (LATEX_ROOT / "preamble/macros.tex").read_text(encoding="utf-8")
    layout = (LATEX_ROOT / "preamble/layout.tex").read_text(encoding="utf-8")
    if r"\setlength{\arrayrulewidth}{0.65pt}" not in macros:
        failures.append("preamble/macros.tex: table grid width is below the approved 0.65pt")
    if r"\definecolor{BookTableRule}{HTML}{66798A}" not in layout:
        failures.append("preamble/layout.tex: approved high-contrast table rule color is missing")

    if failures:
        raise SystemExit("table rule audit failed:\n- " + "\n- ".join(failures))
    print(
        f"table rule audit OK: {table_count} reachable longtables; "
        "visible row/column separators and strong rule styling present"
    )


if __name__ == "__main__":
    main()
