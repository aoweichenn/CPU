from __future__ import annotations

from pathlib import Path

from expand_volume2_600k import CHAPTERS, ChapterPlan
from rewrite_volume2_task_flow import CONCRETE, MODEL_CHAPTER, paragraph, topic


INSERT_ANCHOR = "\\topic{本章质量标准}"
MARKER = "\\topic{学习复盘：把本章能力带到下一章}"


def render_final_margin(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    first = chapter.mechanisms[0].name
    second = chapter.mechanisms[1].name
    case_names = "、".join(case.name for case in chapter.cases)
    return "\n\n".join(
        (
            topic(
                "学习复盘：把本章能力带到下一章",
                paragraph(
                    f"离开本章前，读者应能把{chapter.running_problem}讲成一条完整因果链，而不是讲成一组概念。"
                    f"起点是事故：{info['incident']}；中间是朴素方案、失败路径、状态字段和实验矩阵；终点是能进入贯穿项目的代码边界。"
                    "如果复述时只能说出术语，却说不清第一个分歧出现在输入、执行、同步、I/O、提交还是恢复，就应该回到本章案例重新走一遍。"
                ),
                paragraph(
                    f"本章最少要带走两个能力。第一个是围绕{first}建立语义边界：哪些输入合法，哪些状态可见，哪些结果可以比较，哪些错误必须外显。"
                    f"第二个是围绕{second}建立成本边界：哪些字节被搬动，哪些线程在等待，哪些队列在积压，哪些重试或失败放大了工作量。"
                    "语义边界让结果可信，成本边界让优化有方向，两者缺一不可。"
                ),
                paragraph(
                    f"本章案例包括{case_names}。复盘时不要只看最终改进版本，还要回看朴素版本为什么会被写出来。"
                    "很多工程事故的根源不是作者不知道高级机制，而是小规模条件成立后，没有在规模、并发、故障和恢复条件变化时重新审查边界。"
                    "教材反复保留朴素版本，就是为了训练这种迁移审查能力。"
                ),
                paragraph(
                    "把本章带到下一章时，只迁移方法，不照搬数字。reference、反例、单变量实验、状态表、指标和结论边界可以迁移；具体吞吐、延迟、线程数、批大小、队列容量、分片数量都必须重新测。"
                    "这条原则能避免读者把某台机器上的经验写成普遍规律。"
                ),
                paragraph(
                    "最后给自己留一个三句话笔记：本章保护的核心不变量是什么；本章新增的成本或复杂度是什么；本章哪个实验最值得在后续章节复用。"
                    "这三句话比背诵术语更重要，因为它能把章节串成一套工程判断。"
                ),
            )
        )
    ) + "\n\n"


def insert_final_margin(root: Path, chapter: ChapterPlan) -> bool:
    if chapter.path == MODEL_CHAPTER:
        return False
    path = root / chapter.path
    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        print(f"already final-margin {chapter.path}")
        return False
    anchor_index = text.find(INSERT_ANCHOR)
    if anchor_index == -1:
        raise RuntimeError(f"quality anchor not found in {path}")
    text = text[:anchor_index] + render_final_margin(chapter) + text[anchor_index:]
    path.write_text(text, encoding="utf-8")
    print(f"final-margin {chapter.path}")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for chapter in CHAPTERS:
        if insert_final_margin(root, chapter):
            changed += 1
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
