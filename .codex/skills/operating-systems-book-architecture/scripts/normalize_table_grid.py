#!/usr/bin/env python3
"""Normalize every reachable OS-book longtable to a visible, full grid."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
LATEX = REPO / "books" / "operating-systems-volume-1" / "source" / "latex"
INPUT_RE = re.compile(r"\\input(?:topic|detail)?\{([^}]+)\}")
BEGIN_RE = re.compile(r"\\begin\{longtable\}\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}")
COLUMN_RE = re.compile(r"([pLF])\{([^{}]+)\}")
ROW_END_RE = re.compile(r"\\\\(?![A-Za-z])(?=\s*(?:%.*)?$)")
ROW_RULE_RE = re.compile(r"\\(?:tablerowrule|hline)\b")
STRUCTURAL_RULE_RE = re.compile(
    r"^\\(?:toprule|midrule|bottomrule|hline|tablerowrule|"
    r"endfirsthead|endhead|endfoot|endlastfoot)\b"
)


def reachable_files(latex: Path) -> list[Path]:
    pending = [latex / "main.tex"]
    seen: set[Path] = set()
    while pending:
        path = pending.pop()
        path = path.resolve()
        if path in seen:
            continue
        if not path.is_file():
            raise FileNotFoundError(path)
        seen.add(path)
        text = path.read_text(encoding="utf-8")
        for match in INPUT_RE.finditer(text):
            if "#" in match.group(1):
                continue
            child = (latex / f"{match.group(1)}.tex").resolve()
            if child not in seen:
                pending.append(child)
    return sorted(seen)


def grid_spec(spec: str) -> str:
    cleaned = spec.replace("@{}", "").replace("|", "")
    columns = COLUMN_RE.findall(cleaned)
    raw = "".join(f"{kind}{{{width}}}" for kind, width in columns)
    rebuilt = "".join(f"{kind}{{{width}}}|" for kind, width in columns)
    if not columns or raw != cleaned:
        raise ValueError(f"unsupported longtable column specification: {spec}")
    return f"|{rebuilt}"


def normalize_text(text: str) -> tuple[str, int, int]:
    lines = text.splitlines(keepends=True)
    in_table = False
    changed_specs = 0
    changed_rows = 0

    for index, line in enumerate(lines):
        begin = BEGIN_RE.search(line)
        if begin:
            spec = begin.group(1)
            normalized = grid_spec(spec)
            if spec != normalized:
                lines[index] = line[: begin.start(1)] + normalized + line[begin.end(1) :]
                changed_specs += 1
            in_table = True
            continue

        if not in_table:
            continue
        if "\\end{longtable}" in line:
            in_table = False
            continue
        if not ROW_END_RE.search(line) or ROW_RULE_RE.search(line):
            continue

        next_index = index + 1
        while next_index < len(lines):
            stripped = lines[next_index].strip()
            if stripped and not stripped.startswith("%"):
                break
            next_index += 1
        following = lines[next_index].strip() if next_index < len(lines) else ""
        if STRUCTURAL_RULE_RE.match(following):
            continue

        newline = "\n" if line.endswith("\n") else ""
        body = line[:-1] if newline else line
        lines[index] = f"{body}\\tablerowrule{newline}"
        changed_rows += 1

    return "".join(lines), changed_specs, changed_rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        action="store_true",
        help="report required changes without writing files",
    )
    parser.add_argument(
        "--latex-dir",
        type=Path,
        default=LATEX,
        help="包含 main.tex 的 LaTeX 根目录",
    )
    args = parser.parse_args()
    latex = args.latex_dir.resolve()

    dirty: list[tuple[Path, int, int]] = []
    for path in reachable_files(latex):
        original = path.read_text(encoding="utf-8")
        normalized, specs, rows = normalize_text(original)
        if original == normalized:
            continue
        dirty.append((path, specs, rows))
        if not args.check:
            path.write_text(normalized, encoding="utf-8")

    if dirty:
        action = "need normalization" if args.check else "normalized"
        for path, specs, rows in dirty:
            relative = path.relative_to(REPO)
            print(f"{relative}: {action}: {specs} specs, {rows} rows")
        return 1 if args.check else 0

    print("all reachable longtables use explicit grid columns and row separators")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
