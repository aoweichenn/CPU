from __future__ import annotations

from pathlib import Path

from expand_volume2_600k import CHAPTERS, ChapterPlan, CaseStudy, Mechanism
from rewrite_volume2_task_flow import CONCRETE, MODEL_CHAPTER, listing, paragraph, topic


INSERT_ANCHOR = "\\topic{本章质量标准}"
MARKER = "\\topic{工程补充：实现走读与反例库}"


def render_implementation_walk(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    first_case = chapter.cases[0]
    second_case = chapter.cases[1]
    third_case = chapter.cases[2]
    return topic(
        "工程补充：实现走读与反例库",
        paragraph(
            f"把本章写成教材，最终要能落到一段能维护的工程实现。围绕{chapter.running_problem}，实现顺序建议从{first_case.name}开始，因为它最容易把本章主失败暴露出来。"
            f"先做 reference，再做朴素版本，最后做改进版本。reference 不追求速度，但它定义输入合同和输出摘要；朴素版本保留错误直觉；改进版本只改一个边界。"
        ),
        paragraph(
            f"第一天实现不要碰太多机制。只写固定输入、固定输出和最小指标。输入要能触发这个事故：{info['incident']}。"
            "输出摘要不要只用一个总数，要至少包含记录数、错误数、分片数、关键输出 checksum 和阶段状态。这样后面每次改布局、线程、队列或协议，都能回到同一个摘要比较。"
        ),
        paragraph(
            f"第二天再引入{second_case.name}。这一类案例通常负责暴露资源或调度问题。"
            "写实现时要特别注意观测点放在哪里：放在热循环里会改变成本，放在边界处又可能看不到细节。教学项目可以先用较粗粒度的阶段指标，等路径稳定后再增加更细的 trace。"
        ),
        paragraph(
            f"第三天引入{third_case.name}。这一类案例通常负责恢复、尾部、长尾或提交边界。"
            "此时不要让改进版本直接覆盖朴素版本。两个版本应能在同一输入上并排运行，并输出同一套摘要。并排运行可以让读者看到差异来自机制，而不是来自输入变化或测量变化。"
        ),
        paragraph(
            "实现走读还要保持模块边界。输入生成、reference、朴素版本、改进版本、指标采集、报告输出最好分成独立函数或独立小文件。不要为了写得快，把实验、业务逻辑和日志打印都塞进一个函数。这样的代码短期能跑，长期无法解释，也无法被后续章节复用。"
        ),
    )


def render_state_walk(chapter: ChapterPlan) -> str:
    mechanisms = list(chapter.mechanisms)
    return topic(
        "状态走读：哪些事实必须显式记录",
        paragraph(
            f"{chapter.running_problem}里最容易出错的地方，是把状态留在脑子里或调用栈里。只要出现线程、异步、重试、I/O 或分布式边界，调用栈就不再足够。"
            "教材要逼读者把事实写成字段：当前处理到哪里，谁拥有数据，哪次尝试有效，哪个输出已提交，哪个错误可重试，哪个指标能解释等待。"
        ),
        paragraph(
            f"围绕{mechanisms[0].name}，要记录的是语义事实。它回答“结果为什么可信”。如果这个事实没有字段承载，测试只能在结果出错后才发现问题；如果字段存在，状态机就能在错误路径上提前拒绝非法转换。"
        ),
        paragraph(
            f"围绕{mechanisms[1].name}，要记录的是成本事实。它回答“为什么慢或为什么不扩展”。成本事实不一定进入业务结构，但必须进入实验报告。"
            "例如队列水位、批大小、任务等待、cache miss、重试次数、partition 大小，这些数字决定了优化方向。"
        ),
        paragraph(
            f"围绕{mechanisms[2].name}，要记录的是边界事实。它回答“什么时候可以进入下一阶段”。边界事实尤其适合写成枚举状态，而不是一组布尔值。"
            "多个布尔值会产生 impossible state，枚举状态能让代码审查直接检查允许的转移。"
        ),
        paragraph(
            "状态走读的输出可以是一张简单表：状态名、拥有者、进入条件、退出条件、持久化要求、观测指标。读者不需要一开始画出完美状态机，但必须知道哪些状态不能靠注释维持。"
            "凡是会跨线程、跨文件、跨消息或跨重启的状态，都应优先显式化。"
        ),
        listing(
            "state review table:\n"
            "  state name\n"
            "  owner\n"
            "  allowed incoming events\n"
            "  allowed outgoing events\n"
            "  persisted or transient\n"
            "  metric that proves progress"
        ),
    )


def render_counterexamples(chapter: ChapterPlan) -> str:
    entries: list[str] = []
    for mechanism in chapter.mechanisms[:6]:
        entries.append(
            paragraph(
                f"反例：如果只按{mechanism.name}的直觉写代码，而不检查“{mechanism.question}”，就会落回朴素路径“{mechanism.naive}”。"
                f"触发条件可以设计成：扩大输入、改变分布、插入等待、打乱顺序、制造重复或提前关闭。预期失败信号是：{mechanism.failure}。"
                "这个反例应成为测试或实验的一部分，而不是只写在正文里。"
            )
        )
    return topic(
        "反例库：防止机制变成口号",
        paragraph(
            "反例库的作用，是防止读者把本章机制学成一句正确但空泛的话。每个反例都要小到可以复现，大到能代表真实系统的一类失败。"
            "反例不一定导致崩溃，很多时候它只表现为吞吐不上升、输出顺序不稳定、恢复后重复、队列增长或长尾放大。"
        ),
        *entries,
        paragraph(
            "反例库还要写出修复后如何证明不再发生。证明不能只靠“这次没有复现”。更好的方式是把反例输入固定下来，把状态摘要固定下来，把失败路径上的关键指标固定下来。"
            "以后重构时，这些反例就是回归测试。"
        ),
    )


def render_lab_protocol(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    return topic(
        "实验协议：同一脚本跑出四类结论",
        paragraph(
            f"本章实验脚本要围绕一句话展开：{info['experiment']}。脚本至少要支持四种模式：correctness、pressure、fault、report。"
            "correctness 模式跑小输入和边界输入，pressure 模式跑大输入或高并发，fault 模式启用故障注入，report 模式把原始样本整理成表。"
        ),
        paragraph(
            "correctness 模式不应该被性能优化绕过。它要在每个版本后运行，覆盖空输入、单元素、非整齐长度、重复 key、非法记录、关闭边界和恢复边界。"
            "如果某章没有这些具体路径，也要选择与本章机制等价的边界输入。"
        ),
        paragraph(
            "pressure 模式要把瓶颈逼出来。输入太小，很多机制看起来都没有意义；输入过大，又可能被外部环境噪声掩盖。合适的做法是用几档规模扫描，找到性能曲线的拐点，再围绕拐点解释。"
            "只报告最大规模，往往看不到瓶颈迁移过程。"
        ),
        paragraph(
            "fault 模式要把错误变成输入。错误不应依赖人工按 Ctrl-C 或碰运气，而应在脚本里用固定种子、固定时刻、固定任务或固定文件触发。"
            "例如在写 manifest 前杀进程，在某个消息上丢响应，在某个 partition 上制造热点，在某个 worker 上注入慢 I/O。"
        ),
        paragraph(
            "report 模式要保留原始样本，而不是只保留平均值。报告可以生成 CSV、纯文本摘要或 LaTeX 表格，但至少要能重新计算平均、分位数、最大值和错误计数。"
            "如果读者只能看到最终一句结论，就无法判断结论是否可靠。"
        ),
        listing(
            "lab modes:\n"
            "  correctness: small and boundary inputs\n"
            "  pressure: scale, concurrency, and distribution sweep\n"
            "  fault: deterministic failure injection\n"
            "  report: raw samples, digest, and conclusion limits"
        ),
    )


def render_source_review(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    return topic(
        "源码审查：从实现反推本章原则",
        paragraph(
            "源码审查时不要先看函数数量，也不要先看是否用了某个高级技巧。先从数据入口开始，一路跟到提交或输出。每经过一个边界，就问这个边界有没有字段、状态、测试和指标。"
            "这个方法比逐行读代码更有效，因为它直接对应系统失败路径。"
        ),
        paragraph(
            f"以本章为例，最小走查路线是：{info['debug']}。这条路线里，每个动作都应该能在源码中找到对应位置。"
            "如果找不到，说明代码把语义藏在约定里；如果能找到但没有测试，说明边界没有被固定；如果有测试但没有指标，说明性能和恢复结论仍然靠猜。"
        ),
        paragraph(
            "审查还要看错误路径。很多项目的正常路径清晰，错误路径却散在多个 return、异常、回调和清理函数里。"
            "教材里的示例必须展示错误如何返回、资源如何释放、等待者如何唤醒、临时输出如何清理、重复请求如何去重。否则读者学到的只是 happy path。"
        ),
        paragraph(
            "最后看命名。状态字段、计数器、日志事件和文件名都应体现语义身份。一个叫 data、flag、tmp、done 的字段，在小程序里能理解，在系统里会变成歧义源。"
            "更好的名字应包含对象、阶段和语义，例如 \\code{current_attempt}、\\code{committed_checksum}、\\code{producer_wait_ns}、\\code{route_epoch}。"
        ),
    )


def render_migration_notes(chapter: ChapterPlan) -> str:
    return topic(
        "迁移说明：从本章到下一章",
        paragraph(
            "本章内容不能学完就断掉。每个机制都要问它会在后续哪里复用。单核里的依赖链会迁移到 SIMD 和并行归约；cache line 所有权会迁移到锁、原子和分片计数；有界队列会迁移到 I/O 背压和分布式流控；manifest 会迁移到 shuffle 和 checkpoint。"
        ),
        paragraph(
            f"对于{chapter.running_problem}，可迁移的是语义边界和实验方法，不可直接迁移的是具体数字。吞吐、延迟、批大小、线程数、队列容量、partition 数量，都要在新输入和新机器上重新测。"
            "这也是为什么本书反复要求记录环境和原始样本。"
        ),
        paragraph(
            "迁移时还要防止过度抽象。看到两个章节都有“队列”，不代表它们可以共用同一个类；看到两个章节都有“状态机”，不代表可以写一个万能状态机框架。"
            "抽象只有在减少真实重复、保护共同不变量、并且不会隐藏关键成本时才值得引入。"
        ),
        paragraph(
            "读者可以在每章末尾写三句话。第一，本章最重要的不变量是什么。第二，本章最可能迁移到后续哪一章。第三，本章最不能照搬的性能数字是什么。"
            "这三句话能帮助整本书形成连续推理，而不是把章节当作孤立文章。"
        ),
    )


def render_extension(chapter: ChapterPlan) -> str:
    parts = [
        render_implementation_walk(chapter),
        render_state_walk(chapter),
        render_counterexamples(chapter),
        render_lab_protocol(chapter),
        render_source_review(chapter),
        render_migration_notes(chapter),
    ]
    return "\n\n".join(parts) + "\n\n"


def insert_practice(root: Path, chapter: ChapterPlan) -> bool:
    if chapter.path == MODEL_CHAPTER:
        return False
    path = root / chapter.path
    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        print(f"already practice-extended {chapter.path}")
        return False
    anchor_index = text.find(INSERT_ANCHOR)
    if anchor_index == -1:
        raise RuntimeError(f"quality anchor not found in {path}")
    text = text[:anchor_index] + render_extension(chapter) + text[anchor_index:]
    path.write_text(text, encoding="utf-8")
    print(f"practice-extended {chapter.path}")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for chapter in CHAPTERS:
        if insert_practice(root, chapter):
            changed += 1
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
