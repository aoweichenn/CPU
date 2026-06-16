from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from expand_volume2_600k import CHAPTERS  # noqa: E402


MARKER_BEGIN = "% BEGIN VOLUME2_600K_OVER_TARGET"
MARKER_END = "% END VOLUME2_600K_OVER_TARGET"


def render(chapter) -> str:
    mechanisms = "、".join(item.name for item in chapter.mechanisms[:4])
    cases = "、".join(item.name for item in chapter.cases)
    return "\n\n".join(
        item.strip()
        for item in (
            MARKER_BEGIN,
            "\\topic{过线补充：常见误区、验收题和迁移能力}",
            f"本章最容易出现的误区，是把{mechanisms}当成互相独立的知识点。"
            f"在{chapter.running_problem}中，它们其实围绕同一个失败展开：{chapter.main_failure}。"
            "如果读者只能分别解释这些概念，却不能说明它们在同一条数据流里怎样互相影响，就还没有真正掌握本章。"
            "例如一个优化减少了计算时间，却增加了队列等待；一个同步方案修复了正确性，却制造了共享写热点；一个提交协议提高了可靠性，却增加了持久化延迟。"
            "这些都不是矛盾，而是系统设计中的成本迁移。",
            f"本章的验收题可以这样设计：先给出一个最小版本的{chapter.running_problem}，要求读者写出 reference；再引入一个压力条件，要求读者指出朴素方案会在哪里失败；最后要求读者选择本章机制中的一项或两项做改进。"
            "答案不能只给最终代码，还要给不变量、实验矩阵和反例。"
            "如果某个答案没有说明失败后如何恢复、如何观察、如何判断优化是否值得，就不能算完整答案。",
            f"围绕案例{cases}，报告还应补一个迁移问题：同样方法迁移到更大输入、更多线程、不同硬件或本机分布式模拟时，哪些结论保持，哪些结论要重新测。"
            "保持的通常是语义边界和不变量；需要重测的通常是吞吐、延迟、批大小、缓存效果、锁竞争和 I/O 行为。"
            "这能训练读者区分“原理可以迁移”和“数字不能照搬”。",
            "最后再强调一次本书的写法要求：先从问题出发，再推导机制，再落到实验。"
            "不要把实验放成装饰，也不要把 Linux 源码阅读放成资料链接。"
            "实验必须检验一个假设，源码阅读必须解释一个用户态现象，项目代码必须保护一个明确边界。"
            "读者能按这个顺序复述本章，才说明本章内容真正连贯起来。",
            MARKER_END,
            "",
        )
    )


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for chapter in CHAPTERS:
        path = root / chapter.path
        text = path.read_text(encoding="utf-8")
        if MARKER_BEGIN in text:
            print(f"already over-target expanded {chapter.path}")
            continue
        if chapter.anchor not in text:
            raise RuntimeError(f"anchor not found in {path}: {chapter.anchor}")
        text = text.replace(chapter.anchor, f"{render(chapter)}\n{chapter.anchor}", 1)
        path.write_text(text, encoding="utf-8")
        changed += 1
        print(f"over-target expanded {chapter.path}")
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
