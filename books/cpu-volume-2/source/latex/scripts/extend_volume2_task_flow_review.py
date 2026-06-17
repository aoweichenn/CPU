from __future__ import annotations

from pathlib import Path

from expand_volume2_600k import CHAPTERS, ChapterPlan
from rewrite_volume2_task_flow import CONCRETE, MODEL_CHAPTER, listing, paragraph, topic


INSERT_ANCHOR = "\\topic{本章质量标准}"
MARKER = "\\topic{章末审查：验收题、反例输入和提交标准}"


def render_review(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    case_names = "、".join(case.name for case in chapter.cases)
    mechanism_names = "、".join(mechanism.name for mechanism in chapter.mechanisms[:5])
    return "\n\n".join(
        (
            topic(
                "章末审查：验收题、反例输入和提交标准",
                paragraph(
                    f"本章最后再做一次审查。审查不是重复正文，而是把{chapter.running_problem}压成可检查的交付物。"
                    f"最小验收题应覆盖这些案例：{case_names}。如果读者只能描述概念，却不能为这些案例写出 reference、朴素版本、反例和改进版本，就还没有达到本章要求。"
                ),
                paragraph(
                    f"第一类验收题检查语义。给定一组固定输入，要求写出串行 reference 和结果摘要。摘要要能暴露{info['debug']}。"
                    "答案必须说明哪些字段参与比较，哪些错误要计数，哪些状态可以忽略，哪些状态必须持久化或记录。"
                ),
                paragraph(
                    f"第二类验收题检查机制推导。要求从{mechanism_names}中选择一个机制，先写朴素方案为什么自然，再写它在本章主线中如何失败，最后写一个只改变单一边界的改进。"
                    "答案不能直接给最终方案。没有朴素版本，就没有推导；没有失败证据，就没有必要性；没有反例，就没有边界。"
                ),
                paragraph(
                    "第三类验收题检查实验设计。要求给出正常输入、边界输入、压力输入和错误输入四组样本。每组样本都要说明目的：验证功能、打到边界、暴露成本、检查恢复。"
                    "如果一个样本只能让程序跑完，却不能支持或否定某个假设，就不应进入实验矩阵。"
                ),
                listing(
                    "chapter review checklist:\n"
                    "  reference digest is stable\n"
                    "  naive version is preserved\n"
                    "  counterexample input is committed\n"
                    "  improved version changes one boundary\n"
                    "  metrics explain the chosen hypothesis\n"
                    "  report states unverified limits"
                ),
            ),
            topic(
                "反例输入怎么设计",
                paragraph(
                    "反例输入不要追求稀奇，而要追求直击本章边界。边界输入通常比大输入更有价值，因为它能把 off-by-one、空队列、重复请求、旧 attempt、半写文件、错误路由、尾部元素、分片倾斜这些问题直接打出来。"
                ),
                paragraph(
                    f"围绕{chapter.running_problem}，一个好的反例输入应满足三个条件。第一，人工可以解释预期结果；第二，朴素版本会稳定暴露问题；第三，改进版本通过后仍能说明机制边界。"
                    "如果反例太随机，失败了也难解释；如果反例太简单，可能无法触发真实风险。"
                ),
                paragraph(
                    "反例还要保留原始材料。输入文件、随机种子、故障注入配置、运行命令、环境摘要和输出 digest 都应进入仓库或报告。"
                    "不要只在正文里描述“构造一个错误输入”。没有材料，下一次重构时无法回归。"
                ),
            ),
            topic(
                "项目提交前的自检",
                paragraph(
                    "提交本章项目代码前，先问六个问题。第一，reference 是否还在。第二，朴素版本是否还能跑。第三，改进版本是否只改变一个主要机制。第四，错误路径是否有测试。第五，指标是否能解释结论。第六，报告是否写明没有验证的硬件或系统边界。"
                ),
                paragraph(
                    "如果这些问题有任何一个回答不清，就先不要把章节标为完成。系统书的质量来自可复查，而不是来自一次性写出大量文字。读者能复查，作者能复查，后续章节也能复用，才说明本章真正稳定。"
                ),
                paragraph(
                    f"最后回到本章事故：{info['incident']}。章末自检的意义，就是让读者面对同类事故时能不慌乱。"
                    "先固定事实，再找第一个分歧，再选机制，再写实验。这个顺序会在整本第二册反复出现。"
                ),
            ),
        )
    ) + "\n\n"


def insert_review(root: Path, chapter: ChapterPlan) -> bool:
    if chapter.path == MODEL_CHAPTER:
        return False
    path = root / chapter.path
    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        print(f"already review-extended {chapter.path}")
        return False
    anchor_index = text.find(INSERT_ANCHOR)
    if anchor_index == -1:
        raise RuntimeError(f"quality anchor not found in {path}")
    text = text[:anchor_index] + render_review(chapter) + text[anchor_index:]
    path.write_text(text, encoding="utf-8")
    print(f"review-extended {chapter.path}")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for chapter in CHAPTERS:
        if insert_review(root, chapter):
            changed += 1
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
