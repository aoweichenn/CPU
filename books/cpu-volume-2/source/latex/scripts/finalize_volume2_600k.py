from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from expand_volume2_600k import CHAPTERS  # noqa: E402


MARKER_BEGIN = "% BEGIN VOLUME2_600K_FINAL_PASS"
MARKER_END = "% END VOLUME2_600K_FINAL_PASS"


def render_final(chapter) -> str:
    first = chapter.mechanisms[0]
    last = chapter.mechanisms[-1]
    case = chapter.cases[0]
    steps = "、".join(chapter.project_steps)
    return "\n\n".join(
        item.strip()
        for item in (
            MARKER_BEGIN,
            "\\topic{最终收束：本章应形成的判断力}",
            f"读完本章，读者不应只记住{first.name}、{last.name}这些词，而应能围绕{chapter.running_problem}做一次完整判断。"
            f"完整判断从问题开始：现象是什么，朴素方案是什么，失败信号是什么，保护的不变量是什么，成本从哪里移动到哪里。"
            f"若无法回答这些问题，就说明还停留在概念层，没有真正进入系统层。",
            f"本章最重要的反例是：{chapter.main_failure}。"
            "遇到类似反例时，第一步不是换技术，而是缩小问题。先固定输入，固定环境，保留 reference，再一次只改变一个变量。"
            "如果改动同时改变布局、线程数、批大小、同步方式和提交策略，即使结果变快，也无法知道原因。高质量工程实验必须克制变量。",
            f"以案例“{case.name}”为例，最终报告应包含三段。第一段写{case.setup}，说明读者要解决的不是抽象题，而是一个有输入、有状态、有失败方式的任务。"
            f"第二段写朴素版本：{case.first_try}，并诚实说明它为什么吸引人。第三段写改进版本：{case.improve}，再用{case.verify}验收。"
            "报告中若没有朴素版本，读者会误以为最终设计是凭空出现；若没有反例，读者会误以为最终设计永远正确。",
            "本章还应留下一个审查表。正确性方面，检查输入合同、状态转移、所有权、重复执行、关闭和恢复。性能方面，检查数据移动、共享写、排队、系统调用、批大小、缓存冷热和拓扑。"
            "可观测性方面，检查是否有阶段耗时、队列水位、错误分类、任务身份和原始样本。每一项都要能落到代码、测试或报告，不要只停在口头承诺。",
            f"贯穿项目里，本章至少要完成这些检查点：{steps}。"
            "完成后不要急着进入下一章，要先写一页复盘：本章新增能力解决了什么失败，新增了哪些复杂度，哪些结论只在当前机器上验证，哪些结论可以迁移到更大系统。"
            "这种复盘会让整本书的知识保持连续，不会变成每章讲完就散掉的材料。",
            "最后，用一句话收束本章：系统能力来自清楚的边界、可验证的不变量和可复现的证据。"
            "无论本章讨论的是硬件执行、内存层级、同步、运行时、I/O 还是分布式协议，只要缺少边界、不变量和证据，复杂实现就会变成风险来源。"
            "反过来，只要这三件事稳住，读者就能把一个朴素程序逐步推进成可靠的计算系统。",
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
            print(f"already finalized {chapter.path}")
            continue
        if chapter.anchor not in text:
            raise RuntimeError(f"anchor not found in {path}: {chapter.anchor}")
        text = text.replace(chapter.anchor, f"{render_final(chapter)}\n{chapter.anchor}", 1)
        path.write_text(text, encoding="utf-8")
        changed += 1
        print(f"finalized {chapter.path}")
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
