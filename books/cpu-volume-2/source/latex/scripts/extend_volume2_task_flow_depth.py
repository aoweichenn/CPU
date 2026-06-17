from __future__ import annotations

from pathlib import Path

from expand_volume2_600k import CHAPTERS, ChapterPlan, CaseStudy, Mechanism
from rewrite_volume2_task_flow import CONCRETE, MODEL_CHAPTER, listing, paragraph, topic


INSERT_ANCHOR = "\\topic{本章质量标准}"
MARKER = "\\topic{深度补充：完整走读、反例和实验手册}"


def render_walkthrough(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    return topic(
        "深度补充：完整走读、反例和实验手册",
        paragraph(
            f"现在把本章再走深一层。前面的正文已经给出事故入口：{info['incident']}。"
            "这一节不再添加新的名词，而是把一次真实排查拆成可重复的学习动作。读者要能拿着这一节，在自己的机器上复现一个小版本，再把结论迁移到贯穿项目。"
        ),
        paragraph(
            f"第一步仍然是固定任务：{chapter.running_problem}。固定任务不是为了限制想象，而是为了让所有机制共享同一组输入、同一条状态链、同一个 reference 和同一套指标。"
            "如果每讲一个概念就换一个例子，读者很难看到概念之间的因果关系；如果一直围绕同一条链路推进，读者能看到一个失败怎样把下一个机制推出来。"
        ),
        paragraph(
            "第二步是拆出最小可复现实验。最小实验不等于玩具实验，它必须保留会触发本章核心失败的形状：有足够大的输入，有明确的边界条件，有可比较的 reference，有一个朴素版本，有一个改进版本，还有一个能证明边界的反例。缺少任意一项，实验都会变成看起来能跑、实际无法解释的演示。"
        ),
        listing(
            "minimum reproducible study:\n"
            "  1. freeze input and reference digest\n"
            "  2. run baseline with stage metrics\n"
            "  3. inject one pressure or failure condition\n"
            "  4. change one mechanism only\n"
            "  5. compare correctness, cost, and recovery\n"
            "  6. write the counterexample before the conclusion"
        ),
        paragraph(
            "第三步是把结果写成证据链，而不是写成心得。证据链的形状很固定：现象是什么，怀疑哪条边界，为什么朴素版本会在这里失败，改进版本保护了哪个不变量，哪些指标支持这个判断，哪些情况没有覆盖。"
            "这比直接写“使用某技术后性能提升”更慢，但它能防止误导，也能让后续章节复用结论。"
        ),
    )


def render_mechanism_drill(chapter: ChapterPlan, mechanism: Mechanism) -> str:
    return topic(
        f"单点分析：{mechanism.name}",
        paragraph(
            f"围绕{mechanism.name}，不要先记定义，先问它解决哪一个具体卡点：{mechanism.question}。"
            f"在本章的{chapter.running_problem}里，这个卡点通常不会独立出现，而是和输入规模、数据分布、执行顺序、资源上限或故障注入一起出现。"
        ),
        paragraph(
            f"朴素版本是：{mechanism.naive}。它在小输入下往往能通过，因为小输入没有把隐藏假设压穿。"
            "但是教材不能停在“它错了”这句话上，要说明它为什么一开始会被写出来：它减少了状态，靠调用栈维持顺序，靠当前机器的资源余量掩盖等待，靠人工观察代替指标。"
        ),
        paragraph(
            f"一旦规模或失败形状变化，就会出现：{mechanism.failure}。排查这类信号时，先不要改实现。"
            "先把它翻译成不变量问题：是不是同一个状态被多个拥有者写，是否某个输出位置没有唯一归属，是否某个对象生命周期跨过了异步边界，是否某个提交点缺少身份和校验。"
        ),
        paragraph(
            f"机制模型是：{mechanism.model}。把模型落到代码时，至少要能指出一个字段、一个状态转换、一个测试和一个指标。"
            "字段让语义进入结构体，状态转换让路径可审查，测试让反例固定，指标让性能判断不靠猜。"
        ),
        paragraph(
            f"实验观察是：{mechanism.observe}。这里要避免一个常见错误：看到工具输出后直接写结论。"
            "正确做法是先写假设，再写为什么这个指标能支持或否定假设。若指标和假设没有关系，就不要把它放进报告。"
        ),
        paragraph(
            f"边界是：{mechanism.boundary}。边界要写在正文里，因为工程设计通常不是“会不会”，而是“在什么条件下值得”。"
            "一个机制可能在大输入、高并发、失败恢复中必要，却在单线程小工具中完全多余。能说清这个取舍，才说明真正理解了机制。"
        ),
    )


def render_case_drill(case: CaseStudy) -> str:
    return topic(
        f"案例三轮推导：{case.name}",
        paragraph(
            f"第一轮只写 reference。输入场景是：{case.setup}。reference 的代码允许慢，甚至可以很笨，但语义必须清楚。"
            "它要说明哪些输入合法，哪些输入算错误，输出如何排序或比较，错误如何计数，重复运行是否得到同一摘要。"
        ),
        paragraph(
            f"第二轮写朴素工程版本：{case.first_try}。这一版的价值是暴露直觉缺陷。"
            "写作时要诚实说明它为什么诱人：代码短，状态少，不需要额外结构，小样本容易通过。然后立刻给出反例，而不是直接跳到最终方案。"
        ),
        paragraph(
            f"第三轮才写改进版本：{case.improve}。改进不是装饰，而是针对第二轮反例的最小修复。"
            "如果修复引入了新的成本，也要同时写出来。例如局部合并会增加最终 merge，scan 会增加一轮 pass，manifest 会增加持久化开销，分片会增加元数据。"
        ),
        paragraph(
            f"验收是：{case.verify}。验收报告还要额外写一条“不适用条件”。"
            "例如输入太小时，复杂机制可能不值得；数据分布变化时，热点处理可能失效；设备不支持某些计数器时，性能结论只能保留为假设。"
        ),
    )


def render_experiment_manual(chapter: ChapterPlan) -> str:
    mechanism_names = "、".join(mechanism.name for mechanism in chapter.mechanisms)
    return topic(
        "实验手册：从命令到结论",
        paragraph(
            f"本章实验覆盖的机制包括：{mechanism_names}。实验不是把这些名字逐个跑一遍，而是从同一条主线中抽取变量。"
            "先固定数据，再固定环境，再选择一个变量。变量可以是线程数、批大小、布局、分区函数、队列容量、故障注入点或重试预算，但一次只动一个。"
        ),
        paragraph(
            "实验输入建议分四类。正常输入用于确认功能没有被改坏；边界输入用于打到 off-by-one、空输入、满队列、零命中、重复消息等路径；压力输入用于暴露缓存、TLB、锁、队列、I/O 或网络成本；错误输入用于检查恢复、取消、幂等和错误分类。"
        ),
        paragraph(
            "每次实验至少记录五类结果。第一是 correctness digest，证明结果可信；第二是 throughput 或 elapsed time，说明端到端效果；第三是资源指标，例如内存峰值、队列水位、文件数量或 in-flight 数；第四是等待或失败指标，例如锁等待、超时、重试、late reply；第五是环境信息，例如 CPU、内核、编译器、输入规模和运行命令。"
        ),
        listing(
            "report skeleton:\n"
            "  problem: one concrete symptom\n"
            "  hypothesis: one boundary or cost source\n"
            "  baseline: reference and naive version\n"
            "  change: one mechanism only\n"
            "  data: raw samples and digest\n"
            "  result: what changed and what did not\n"
            "  limit: unverified environment or counterexample"
        ),
        paragraph(
            "结论必须包含“没有变化”的部分。如果吞吐提高但内存峰值上升，要写；如果平均延迟降低但 p99 变差，要写；如果正确性通过但恢复路径未验证，要写。高质量教材不是让所有结果看起来漂亮，而是让读者学会在证据边界内说话。"
        ),
    )


def render_linux_mapping(chapter: ChapterPlan) -> str:
    if not chapter.linux_paths:
        return ""
    paths = "、".join(f"\\filepath{{{path}}}" for path in chapter.linux_paths)
    return topic(
        "Linux 源码映射：对象、状态和等待",
        paragraph(
            f"本章推荐源码入口是 {paths}。阅读时不要展开成百科式笔记，而要围绕一个最小用户态现象。"
            "比如线程为什么睡眠，缺页为什么发生，写文件为什么没有立刻持久化，计数器为什么需要权限，队列为什么会唤醒。"
        ),
        paragraph(
            "源码阅读的核心是对象映射。用户态的线程，在内核里要找到 task 和调度状态；用户态的内存访问，要找到 VMA、页表项和 page；用户态的文件写入，要找到 file、inode、page cache 和 writeback；用户态的等待，要找到 wait queue、futex 或完成事件。"
        ),
        paragraph(
            "读内核代码时要特别注意状态字段如何变化。一个对象从 runnable 到 sleeping，从 clean page 到 dirty page，从 unlocked 到 contended，从 pending I/O 到 completed I/O，这些状态变化比函数名更能解释现象。"
            "把状态变化和本章实验指标对应起来，源码阅读才不是资料堆砌。"
        ),
        paragraph(
            "当前设备和源码版本可能不完全一致。报告要写明内核版本、阅读的源码版本、哪些结论来自源码机制、哪些结论来自本机实验。"
            "这能避免把源码阅读当作不可质疑的证明，也能训练读者区分机制理解和运行时证据。"
        ),
    )


def render_project_acceptance(chapter: ChapterPlan) -> str:
    steps = "\n".join(f"  {index + 1}. {step}" for index, step in enumerate(chapter.project_steps))
    return topic(
        "贯穿项目验收：把能力写进系统",
        paragraph(
            "本章进入贯穿项目时，不能只留下文字。至少要有一个目录、一个输入生成脚本或固定样本、一个 reference、一个朴素版本、一个改进版本、一个实验报告。"
            "代码可以小，但边界必须完整。"
        ),
        listing("acceptance checklist:\n" + steps),
        paragraph(
            "每个检查点都要对应一个失败路径。比如某个检查点是队列容量，就要能触发队列满；某个检查点是 manifest，就要能触发半写文件；某个检查点是 route epoch，就要能触发旧请求。"
            "没有失败注入的能力很容易只是正常路径演示。"
        ),
        paragraph(
            "验收时还要写回退策略。若改进版本复杂度太高、收益不足或设备环境不支持，项目应该能退回朴素但正确的版本。"
            "能回退说明边界清楚，不能回退往往说明机制已经和业务逻辑缠在一起。"
        ),
        paragraph(
            "本章结束前，读者应写一页复盘：本章新增了什么能力，解决了什么事故，引入了什么成本，下一章会复用哪个边界。"
            "这页复盘能让整本书保持连续，而不是每章都从零开始。"
        ),
    )


def render_appendix(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    parts = [
        render_walkthrough(chapter),
    ]
    for mechanism in chapter.mechanisms:
        parts.append(render_mechanism_drill(chapter, mechanism))
    for case in chapter.cases:
        parts.append(render_case_drill(case))
    parts.extend(
        [
            render_experiment_manual(chapter),
            render_linux_mapping(chapter),
            render_project_acceptance(chapter),
            topic(
                "本节收束：回到最初事故",
                paragraph(
                    f"最后再回到开头的事故：{info['incident']}。如果现在重新排查，读者不应只说“可能是某机制的问题”，而应能写出一条可执行路线：固定输入，运行 reference，复现朴素失败，选择一个机制，改一个边界，采集指标，写出反例和结论限制。"
                ),
                paragraph(
                    "这种路线就是本章真正要交付的能力。知识点会随着硬件、内核和框架变化而更新，但从事故出发、沿边界推导、用证据收束的习惯不会过时。"
                ),
            ),
        ]
    )
    return "\n\n".join(part for part in parts if part.strip()) + "\n\n"


def insert_depth(root: Path, chapter: ChapterPlan) -> bool:
    if chapter.path == MODEL_CHAPTER:
        return False
    path = root / chapter.path
    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        print(f"already depth-extended {chapter.path}")
        return False
    anchor_index = text.find(INSERT_ANCHOR)
    if anchor_index == -1:
        raise RuntimeError(f"quality anchor not found in {path}")
    text = text[:anchor_index] + render_appendix(chapter) + text[anchor_index:]
    path.write_text(text, encoding="utf-8")
    print(f"depth-extended {chapter.path}")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for chapter in CHAPTERS:
        if insert_depth(root, chapter):
            changed += 1
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
