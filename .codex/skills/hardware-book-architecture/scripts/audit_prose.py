#!/usr/bin/env python3
"""Audit the generated 26-chapter manuscript for editorial migration residue."""

from __future__ import annotations

from collections import Counter
from pathlib import Path
import re
import sys


HARD_PATTERNS = {
    "old numeric chapter reference": re.compile(r"第[一二三四五六七八九十]+章"),
    "section-symbol reference": re.compile(r"§\s*[〇一二三四五六七八九十0-9]+"),
    "numeric self-section reference": re.compile(
        r"本章第\s*[〇一二三四五六七八九十0-9]+\s*节"
    ),
    "vague relative section reference": re.compile(r"上一节|下一节"),
    "legacy chapter nickname": re.compile(
        r"主存与总线章|存储器件结构章|经典芯片与平台(?:演进)?章|外设队列章|电源时钟章"
    ),
    "mechanical chapter replacement": re.compile(
        r"数字硬件基础部分|数值与数据通路部分|处理器核心部分|存储与设备事务部分|NVMe 与存储 I/O 两章|最后的教学计算机项目"
    ),
    "mechanical duplicate": re.compile(r"(部分|项目|案例|章节|一章)\1"),
    "informal coercive or violent metaphor": re.compile(
        r"逼(?!近|真)|硬扛|挨打|吃掉|吞掉|砍掉|白白|喂给|踩坑|死磕|翻车现场|"
        r"遮羞布|打穿缓存|当宝贝|养老|盯住|吐出|狠狠干扰|掐个表|"
        r"最先摔倒|钉死|写死|揪出|代码自己在被啃|收拾其余一切|"
        r"被仪器扔掉|从另一头榨容量|有效载荷就被咬一口|"
        r"帧预算啃掉|真正干活|线程在干活|按命令干活|设备干活|替谁干活|"
        r"兜底|饿死|一眼见底|一招|无感|还债|随手|"
        r"脾气|玄学|喊停|帮倒忙|大象流|免费午餐|抢跑|开工|杀死整块盘"
    ),
}


def find_root(start: Path) -> Path:
    marker = Path("books/hardware-zero-to-machine/source/latex/main.tex")
    for candidate in (start, *start.parents):
        if (candidate / marker).is_file():
            return candidate
    raise FileNotFoundError(f"cannot find repository root from {start}")


def main() -> int:
    root = find_root(Path.cwd().resolve())
    chapter_dir = root / "books/hardware-zero-to-machine/source/latex/chapters26"
    paths = sorted(chapter_dir.glob("ch[0-9][0-9]-*.tex"))
    if len(paths) != 26:
        print(f"expected 26 generated chapters, found {len(paths)}", file=sys.stderr)
        return 1

    failures: list[str] = []
    stats: Counter[str] = Counter()
    for path in paths:
        text = path.read_text(encoding="utf-8")
        stats["lines"] += len(text.splitlines())
        stats["characters"] += len(text)
        stats["sections"] += len(re.findall(r"(?m)^\\section\{", text))
        for label, pattern in HARD_PATTERNS.items():
            for match in pattern.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                failures.append(
                    f"{path.relative_to(root)}:{line}: {label}: {match.group(0)}"
                )

    if failures:
        print("Prose audit failed:", file=sys.stderr)
        print("\n".join(failures), file=sys.stderr)
        return 1

    print(
        "Prose audit OK: "
        f"{len(paths)} chapters, {stats['sections']} sections, "
        f"{stats['lines']} lines, {stats['characters']} characters; "
        "no stale references or disallowed colloquial metaphors"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
