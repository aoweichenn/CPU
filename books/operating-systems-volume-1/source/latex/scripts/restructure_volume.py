#!/usr/bin/env python3
"""Generate the formal-volume chapter assets produced by the 2026 restructure."""

from __future__ import annotations

from pathlib import Path


LATEX_ROOT = Path(__file__).resolve().parents[1]
BOOKS_ROOT = LATEX_ROOT.parents[2]
SAMPLE_CHAPTERS = (
    BOOKS_ROOT
    / "operating-systems-evolution-sample"
    / "source"
    / "latex"
    / "chapters"
)
EVOLUTION_DEST = LATEX_ROOT / "chapters18" / "evolution"
FILESYSTEM_SOURCE = LATEX_ROOT / "chapters" / "ch17b-filesystem-from-scratch.tex"
FILESYSTEM_OUTPUTS = (
    "ch11-filesystem-formation.tex",
    "ch12-filesystem-indexing.tex",
    "ch13-filesystem-recovery.tex",
    "ch14-modern-filesystems.tex",
)


def import_evolution_sample() -> None:
    """Copy the approved sample prose so the formal PDF builds independently."""
    if not SAMPLE_CHAPTERS.is_dir():
        raise FileNotFoundError(SAMPLE_CHAPTERS)

    for source in sorted(SAMPLE_CHAPTERS.rglob("*.tex")):
        relative = source.relative_to(SAMPLE_CHAPTERS)
        destination = EVOLUTION_DEST / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        text = source.read_text(encoding="utf-8")
        text = text.replace(
            r"\input{chapters/ch01/",
            r"\input{chapters18/evolution/ch01/",
        )
        destination.write_text(text, encoding="utf-8")


def split_filesystem_chapters() -> None:
    """Split the legacy four-chapter filesystem source without changing its text."""
    text = FILESYSTEM_SOURCE.read_text(encoding="utf-8")
    text = text.removeprefix("\\begin{filesystemlayout}\n\n")
    text = text.removesuffix("\n\\end{filesystemlayout}\n")
    markers = [index for index in range(len(text)) if text.startswith("\\chapter{", index)]
    if len(markers) != len(FILESYSTEM_OUTPUTS):
        raise RuntimeError(
            f"expected {len(FILESYSTEM_OUTPUTS)} filesystem chapters, found {len(markers)}"
        )

    markers.append(len(text))
    for output_name, start, end in zip(
        FILESYSTEM_OUTPUTS, markers[:-1], markers[1:], strict=True
    ):
        body = text[start:end].strip()
        output = LATEX_ROOT / "chapters18" / output_name
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            "\\begin{filesystemlayout}\n\n"
            + body
            + "\n\n\\end{filesystemlayout}\n",
            encoding="utf-8",
        )


def main() -> int:
    import_evolution_sample()
    split_filesystem_chapters()
    print("generated evolution assets and four filesystem chapter files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
