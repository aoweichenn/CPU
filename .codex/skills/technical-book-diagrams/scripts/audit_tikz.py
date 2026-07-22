#!/usr/bin/env python3
"""Inventory textbook TikZ figures and rank layouts that need visual review."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
import os
from pathlib import Path
import re
import subprocess


ENVIRONMENTS = ("pdfdiagram", "circuit", "waveform", "block")
BEGIN_RE = re.compile(r"\\begin\{(" + "|".join(ENVIRONMENTS) + r")\}")


@dataclass(frozen=True)
class FigureAudit:
    file: str
    line: int
    environment: str
    score: int
    reasons: tuple[str, ...]
    nodes: int
    routes: int
    arrows: int
    labels: int
    coordinates: int
    orthogonal_bends: int
    curved_routes: int
    source_lines: int
    page: int | None = None


def repository_root(start: Path) -> Path:
    marker = Path("books/hardware-zero-to-machine/source/latex/main.tex")
    for candidate in (start, *start.parents):
        if (candidate / marker).is_file():
            return candidate
    raise FileNotFoundError(f"cannot find repository root from {start}")


def figure_blocks(text: str):
    """Yield complete figure environments without trying to parse all of TeX."""
    position = 0
    while match := BEGIN_RE.search(text, position):
        environment = match.group(1)
        end_token = rf"\end{{{environment}}}"
        end = text.find(end_token, match.end())
        if end < 0:
            raise ValueError(f"unterminated {environment} environment")
        end += len(end_token)
        yield environment, match.start(), text[match.start():end]
        position = end


def count(pattern: str, body: str) -> int:
    return len(re.findall(pattern, body, flags=re.MULTILINE))


def audit_figure(path: Path, root: Path, environment: str, offset: int, body: str) -> FigureAudit:
    nodes = count(r"\\node(?:\s|\[|\s+at)", body)
    routes = count(r"\\(?:draw|path)\b|\\edge\b", body)
    arrows = count(r"(?:->|-\{|Stealth|Latex)", body)
    labels = count(r"\bnode(?:\s|\[)", body) - nodes
    coordinates = count(r"\([^()]*?[,][^()]*?\)", body)
    orthogonal_bends = body.count("|-") + body.count("-|")
    curved_routes = count(r"\bto\s*\[(?:out|bend)|\.\.", body)
    source_lines = body.count("\n") + 1

    score = 0
    reasons: list[str] = []

    def flag(points: int, reason: str) -> None:
        nonlocal score
        score += points
        reasons.append(reason)

    density = nodes + routes + labels
    if density >= 30:
        flag(5, f"high density ({density})")
    elif density >= 20:
        flag(3, f"dense ({density})")
    elif density >= 14:
        flag(1, f"moderate density ({density})")

    if coordinates >= 24:
        flag(4, f"coordinate-heavy ({coordinates})")
    elif coordinates >= 14:
        flag(2, f"many coordinates ({coordinates})")

    if orthogonal_bends >= 8:
        flag(4, f"many orthogonal bends ({orthogonal_bends})")
    elif orthogonal_bends >= 4:
        flag(2, f"orthogonal bends ({orthogonal_bends})")

    if curved_routes >= 3:
        flag(4, f"many curved routes ({curved_routes})")
    elif curved_routes:
        flag(2, f"curved routes ({curved_routes})")

    if labels >= 6:
        flag(3, f"many inline route labels ({labels})")
    elif labels >= 3:
        flag(1, f"inline route labels ({labels})")

    if arrows >= 10:
        flag(3, f"many arrows ({arrows})")
    elif arrows >= 6:
        flag(1, f"several arrows ({arrows})")

    if count(r"[xy]shift\s*=", body) >= 5:
        flag(2, "many manual shifts")
    if count(r"overlay|remember picture", body):
        flag(5, "page-overlay drawing")
    if count(r"scale\s*=\s*0\.[0-6]", body):
        flag(3, "very small local scale")

    label_texts = re.findall(r"\\node(?:\[[^]]*\])?\s*\{([^{}]+)\}", body)
    long_labels = [text for text in label_texts if len(re.sub(r"\\\w+|\$", "", text)) >= 22]
    if long_labels and "text width" not in body:
        flag(2, f"long labels without text width ({len(long_labels)})")

    if nodes >= 7 and routes >= 7 and not any(token in body for token in ("matrix", "positioning", "node distance")):
        flag(2, "dense layout lacks explicit grid/positioning")

    return FigureAudit(
        file=str(path.relative_to(root)),
        line=body_line(path.read_text(encoding="utf-8"), offset),
        environment=environment,
        score=score,
        reasons=tuple(reasons),
        nodes=nodes,
        routes=routes,
        arrows=arrows,
        labels=labels,
        coordinates=coordinates,
        orthogonal_bends=orthogonal_bends,
        curved_routes=curved_routes,
        source_lines=source_lines,
    )


def body_line(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--minimum-score", type=int, default=0)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--json", action="store_true")
    parser.add_argument(
        "--synctex-pdf",
        type=Path,
        help="annotate results with the first SyncTeX page for this PDF",
    )
    return parser.parse_args()


def synctex_page(item: FigureAudit, root: Path, pdf: Path) -> int | None:
    source = root / item.file
    relative_source = os.path.relpath(source, pdf.parent)
    result = subprocess.run(
        [
            "synctex",
            "view",
            "-i",
            f"{item.line}:1:{relative_source}",
            "-o",
            pdf.name,
        ],
        cwd=pdf.parent,
        check=False,
        capture_output=True,
        text=True,
    )
    match = re.search(r"(?m)^Page:(\d+)$", result.stdout)
    return int(match.group(1)) if match else None


def main() -> int:
    args = parse_args()
    root = repository_root(Path.cwd().resolve())
    chapter_dir = root / "books/hardware-zero-to-machine/source/latex/chapters26"
    audits: list[FigureAudit] = []
    for path in sorted(chapter_dir.glob("ch*.tex")):
        text = path.read_text(encoding="utf-8")
        for environment, offset, body in figure_blocks(text):
            audits.append(audit_figure(path, root, environment, offset, body))

    audits.sort(key=lambda item: (-item.score, item.file, item.line))
    visible = [item for item in audits if item.score >= args.minimum_score]
    if args.limit:
        visible = visible[: args.limit]
    if args.synctex_pdf:
        pdf = args.synctex_pdf.resolve()
        visible = [
            FigureAudit(**(asdict(item) | {"page": synctex_page(item, root, pdf)}))
            for item in visible
        ]

    if args.json:
        print(json.dumps([asdict(item) for item in visible], ensure_ascii=False, indent=2))
    else:
        counts = {environment: sum(item.environment == environment for item in audits) for environment in ENVIRONMENTS}
        print(f"figures={len(audits)} " + " ".join(f"{key}={value}" for key, value in counts.items()))
        print(f"flagged={sum(item.score > 0 for item in audits)} high_risk={sum(item.score >= 8 for item in audits)}")
        for item in visible:
            reasons = "; ".join(item.reasons) or "baseline"
            page = f"  page={item.page}" if item.page else ""
            print(f"{item.score:2d}  {item.file}:{item.line}  {item.environment}{page}  {reasons}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
