#!/usr/bin/env python3
"""Verify that the pre-migration hardware-book sources remain intact."""

from __future__ import annotations

import hashlib
from pathlib import Path
import sys


BASELINE = {
    "chapters/ch-control-plane.tex": ("36eed8435e3128f8a766fd1be3b73eeb0389528eb9932357ef22c8cd12e8fb36", 1408, 165858),
    "chapters/ch01-electricity-diode-switch.tex": ("e6110b4d2ec70d51814b44f5a833a8fa42c625f936b719e0442548ab9dd26504", 2528, 239144),
    "chapters/ch02-feedback-latches-flipflops.tex": ("c82ad85d082fcf3c9ef1cf7b89fafe5c630fe79f18482b74e013bcc24449d800", 2154, 210656),
    "chapters/ch03-number-arithmetic-circuits.tex": ("306bd2ad03c39468a4d9432924fd17d79e7afda1a6562d060e462ad5a0b5e42e", 2036, 184969),
    "chapters/ch04-minimal-cpu.tex": ("48d1241110de69674018aaac3093d0adb89c6f87c93d7b735bbfe8e507510233", 1837, 153504),
    "chapters/ch05-memory-bus-io-dma.tex": ("717352ce0ceeae7f3c55f780dd0ecbe1e13b040c4315b1e99723c0fad16bfb59", 1862, 164154),
    "chapters/ch05b-storage-cell-structures.tex": ("0cfb99f94d48b2333a1f5b96ab407581f13ebee608197882238ff0e98ae35b5b", 4713, 342556),
    "chapters/ch06-classic-chips-platforms.tex": ("edb9e664e0ed7324667ec323197560e6d0467554be4fb567a0ccfaa21f2fc0ff", 1885, 171671),
    "chapters/ch07-breadboard-computer.tex": ("e3771c90e97a7a55c1d3b5471170e5060a5cbc6dcc91391b3f78d028b40b6b98", 2325, 203913),
    "chapters/ch08-power-clock-signal-integrity.tex": ("04b9f8672606b5ccc6e95b9e8b472f1148b3779659b744b74b780c685127415c", 2125, 154636),
    "chapters/ch09-rtl-verilog-reading.tex": ("fe9ef4b30c7d5bae30034346f0060a20f0ed41eca036b3e346767c0da1421462", 2343, 173684),
    "chapters/ch10-performance-analysis.tex": ("cb19466073d6b81ef85cd9676ca696d48417fe49f7d2a4741067b322ae720565", 2157, 164378),
    "chapters/ch11-peripherals-queues.tex": ("450c02b6a148bc5d30b40a0d9e48c5d19b26096b9ad6bb49517829506aaef84c", 2133, 143321),
    "chapters/ch12-gpu-display.tex": ("13a9d884816fd46e1dc8440b490ef2e9b5a51810794152c905760a3386030c9d", 2191, 139127),
    "chapters/ch13-linking-loading.tex": ("039b4c664224d0e07fd61d4450d3dac49447de6e7753cf2393f9c8495c4a6939", 2109, 156687),
    "frontmatter/preface.tex": ("2d53b3f9ceb0b0dbb26b6c11279105bab3f378c56d630d202d8ba0ff1551998e", 9, 1124),
    "backmatter/checklist.tex": ("17b57e5c6c44cb6b965574f623668f3ac731da1e64706a0671a99932edc5286c", 97, 7792),
}


def find_repo_root(start: Path) -> Path:
    marker = Path("books/hardware-zero-to-machine/source/latex/main.tex")
    for candidate in (start, *start.parents):
        if (candidate / marker).is_file():
            return candidate
    raise FileNotFoundError(f"cannot find repository root from {start}")


def main() -> int:
    try:
        root = find_repo_root(Path.cwd().resolve())
    except FileNotFoundError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    latex = root / "books/hardware-zero-to-machine/source/latex"
    failures: list[str] = []
    total_lines = 0
    total_bytes = 0

    for relative, (expected_hash, expected_lines, expected_bytes) in BASELINE.items():
        path = latex / relative
        if not path.is_file():
            failures.append(f"missing: {relative}")
            continue
        data = path.read_bytes()
        actual_hash = hashlib.sha256(data).hexdigest()
        actual_lines = len(data.splitlines())
        actual_bytes = len(data)
        total_lines += actual_lines
        total_bytes += actual_bytes
        if (actual_hash, actual_lines, actual_bytes) != (
            expected_hash,
            expected_lines,
            expected_bytes,
        ):
            failures.append(
                f"changed: {relative} "
                f"sha256={actual_hash} lines={actual_lines} bytes={actual_bytes}"
            )

    if failures:
        print("Legacy baseline mismatch:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("Do not refresh the baseline automatically; reconcile the migration ledger first.", file=sys.stderr)
        return 1

    print(
        "Legacy baseline OK: "
        f"{len(BASELINE)} files, {total_lines} lines, {total_bytes} bytes"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
