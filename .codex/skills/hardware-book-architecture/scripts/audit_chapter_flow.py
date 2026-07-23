#!/usr/bin/env python3
"""Audit generated chapter order and textbook-style chapter-end material."""

from __future__ import annotations

import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
CHAPTER_DIR = REPO_ROOT / "books/hardware-zero-to-machine/source/latex/chapters26"
NUMBERED_SECTION_RE = re.compile(r"(?m)^\\section\{")
EXERCISE_HEADING_RE = re.compile(r"(?m)^\\section\*\{章末练习\}")
ANSWER_HEADING_RE = re.compile(r"(?m)^\\section\*\{参考解答与要点\}")
EXPECTED_EXERCISE_UNITS = 356
MAX_NUMBERED_EXERCISES_PER_CHAPTER = 10


def main() -> None:
    failures: list[str] = []
    total_exercises = 0
    total_exercise_units = 0
    chapters = sorted(CHAPTER_DIR.glob("ch[0-9][0-9]-*.tex"))
    if len(chapters) != 26:
        failures.append(f"expected 26 generated chapters, found {len(chapters)}")

    for path in chapters:
        text = path.read_text(encoding="utf-8")
        label = path.name
        exercise_headings = list(EXERCISE_HEADING_RE.finditer(text))
        answer_headings = list(ANSWER_HEADING_RE.finditer(text))
        if len(exercise_headings) != 1 or len(answer_headings) != 1:
            failures.append(
                f"{label}: expected one exercise and one answer heading, "
                f"found {len(exercise_headings)}/{len(answer_headings)}"
            )
            continue

        exercise_start = exercise_headings[0].start()
        answer_start = answer_headings[0].start()
        if exercise_start >= answer_start:
            failures.append(f"{label}: answers do not follow exercises")
        if NUMBERED_SECTION_RE.search(text, exercise_start):
            failures.append(f"{label}: numbered reader content appears after chapter exercises")

        chapter_end = text[exercise_start:]
        if r"\begin{longtable}" in chapter_end:
            failures.append(f"{label}: chapter-end exercises or answers still use a table")

        exercise_count = text.count(r"\begin{chapterexercise}")
        answer_count = text.count(r"\begin{chapteranswer}")
        composite_count = len(
            re.findall(r"\\begin\{chapterexercise\}\{综合练习：", text)
        )
        exercise_subproblem_count = text.count(r"\begin{exercisesubproblem}")
        answer_subproblem_count = text.count(r"\begin{answersubproblem}")
        if exercise_count == 0 or exercise_count != answer_count:
            failures.append(
                f"{label}: exercise/answer count is {exercise_count}/{answer_count}"
            )
        if exercise_count > MAX_NUMBERED_EXERCISES_PER_CHAPTER:
            failures.append(
                f"{label}: {exercise_count} numbered exercises exceeds "
                f"the limit {MAX_NUMBERED_EXERCISES_PER_CHAPTER}"
            )
        if exercise_subproblem_count != answer_subproblem_count:
            failures.append(
                f"{label}: exercise/answer subproblem count is "
                f"{exercise_subproblem_count}/{answer_subproblem_count}"
            )
        total_exercises += exercise_count
        total_exercise_units += (
            exercise_count - composite_count + exercise_subproblem_count
        )

    if total_exercise_units != EXPECTED_EXERCISE_UNITS:
        failures.append(
            f"expected {EXPECTED_EXERCISE_UNITS} preserved exercise units, "
            f"found {total_exercise_units}"
        )

    if failures:
        raise SystemExit("chapter flow audit failed:\n- " + "\n- ".join(failures))
    print(
        f"chapter flow audit OK: {len(chapters)} chapters, {total_exercises} numbered "
        f"exercises containing {total_exercise_units} preserved exercise/answer units; "
        "all chapter-end material follows reader content"
    )


if __name__ == "__main__":
    main()
