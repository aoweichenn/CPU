from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from expand_volume2_600k import CHAPTERS  # noqa: E402


MARKER_BEGIN = "% BEGIN VOLUME2_600K_DEEPENING"
MARKER_END = "% END VOLUME2_600K_DEEPENING"


def clean(text: str) -> str:
    return text.strip()


def render_deep_mechanism(chapter_index: int, mechanism_index: int, chapter, mechanism) -> str:
    scale_words = (
        "单线程小输入",
        "单机多线程",
        "NUMA 或异构核心",
        "本机分布式模拟",
        "真实线上服务",
    )
    scale = scale_words[(chapter_index + mechanism_index) % len(scale_words)]
    return "\n\n".join(
        clean(item)
        for item in (
            f"围绕“{mechanism.name}”，先把它放回一个具体问题：{mechanism.question}。"
            f"如果这个问题只在{scale}里出现，读者很容易把它当成局部技巧；但在{chapter.running_problem}中，它会沿着输入、执行、同步、I/O 和提交一路传播。"
            f"所以讲解时不能只写定义，而要让读者看到它怎样从一个小失败变成系统性约束。",
            f"朴素写法通常是“{mechanism.naive}”。这句话看似简单，却包含几个默认前提：状态只有一个拥有者，执行顺序可预测，资源足够近，失败不会穿过边界，观察结果能够代表真实成本。"
            f"这些前提在教学小程序中可能成立，在{chapter.running_problem}里却会被输入规模、线程交错、硬件拓扑或故障注入逐个打破。",
            f"失败信号是“{mechanism.failure}”。注意，这个失败不一定表现为崩溃。它也可能表现为吞吐不再增长、p99 抖动、重启后结果多了一份、某个分区长尾、CPU 使用率很高但有效工作很少。"
            f"教材要把这些信号和机制绑定起来，否则读者只会背术语，遇到真实现象时仍然不知道从哪里下手。",
            f"更可靠的推导顺序是先写出不变量。对{mechanism.name}来说，不变量不是一句口号，而是本章要保护的事实：哪些数据只能写一次，哪些状态可以重试，哪些结果可以被缓存，哪些成本必须摊销，哪些错误必须外显。"
            f"只要不变量没有写清，后面的代码、实验和优化都没有稳定坐标。",
            f"然后才引入模型：{mechanism.model}。这个模型应同时解释正确性和成本。"
            f"正确性部分回答“为什么结果可信”，成本部分回答“为什么这个实现会快或慢”。"
            f"如果一个模型只能解释速度，却解释不了失败恢复，它不适合系统教材；如果只能解释语义，却完全不关心 cache、调度、I/O 或网络成本，也不适合第二册。",
            f"实验应围绕“只改变一个变量”设计。针对{mechanism.name}，最小实验可以保留朴素版本，再实现一个改进版本，输入规模从小到大，线程数或并发度从一到目标机器上限，错误注入从无到有。"
            f"每一组实验都应记录 reference 结果、核心指标、环境和命令。这样读者看到差异时，可以判断差异来自机制本身，而不是来自初始化、缓存冷热、调度迁移或文件系统状态。",
            f"观察手段是：{mechanism.observe}。观察不是为了堆工具名，而是为了回答一个具体假设。"
            f"若假设是共享写导致扩展性下降，就应看线程数曲线、写入布局和 cache 相关指标；若假设是提交边界错误，就应做崩溃点矩阵；若假设是队列背压，就应看水位、等待和上游速率。"
            f"工具输出必须回到假设，不要把截图或命令输出当成结论。",
            f"最后要讲边界：{mechanism.boundary}。高质量教材必须把反例放在正文里。"
            f"读者需要知道什么时候该用这个机制，什么时候它会制造新问题，什么时候应该退回更简单方案。"
            f"比如一个单线程工具没有并发共享，强行引入复杂运行时只会增加维护成本；一个没有恢复要求的临时分析脚本，也不必承担完整 manifest 协议。",
            f"把这一点落实到代码审查，可以要求每个涉及{mechanism.name}的改动都回答五个问题：朴素版本是什么，新增机制保护了哪个不变量，新增成本在哪里，如何回退，哪些测试证明边界。"
            f"这五个问题比“用了某某技术”更能判断质量，也能防止团队在没有证据时追逐复杂实现。",
        )
    )


def render_case_deepening(case_index: int, case) -> str:
    return "\n\n".join(
        clean(item)
        for item in (
            f"案例深化“{case.name}”要按三次迭代写。第一次只写 reference：{case.setup}。reference 的代码可以慢，但必须把输入、输出、错误和边界写清。"
            f"第二次写朴素工程版本：{case.first_try}。这一步不能省，因为读者需要看到直觉方案为什么诱人，也需要有一个失败对象。",
            f"第三次才写改进版本：{case.improve}。改进说明要避免“因为更高级所以更快”这种表达，而要讲清它改变了哪条路径：减少了数据移动、减少了共享写、减少了系统调用、减少了重试放大，还是把不可恢复状态变成可恢复状态。",
            f"验收方式是：{case.verify}。验收报告至少包含一张对照表，一列是朴素版本，一列是改进版本，一列是反例。反例很重要，因为它能告诉读者改进不是什么时候都正确。"
            f"如果一个案例没有反例，往往说明分析还停在宣传层面。"
        )
    )


def render_experiment_matrix(chapter) -> str:
    items = []
    for mechanism in chapter.mechanisms:
        items.append(
            f"围绕{mechanism.name}，实验矩阵至少包含正常输入、边界输入、压力输入和错误输入四类。"
            f"正常输入验证功能，边界输入验证不变量，压力输入验证成本模型，错误输入验证恢复和报告。"
            f"其中压力输入不只是“大”，还要能触发本章关心的形状：随机访问、热点 key、短任务、慢 I/O、重复消息或跨节点访问。"
        )
    return "\n\n".join(items)


def render_linux_deepening(chapter) -> str:
    if not chapter.linux_paths:
        return ""
    paths = "、".join(f"\\filepath{{{path}}}" for path in chapter.linux_paths)
    return "\n\n".join(
        clean(item)
        for item in (
            f"本章的 Linux 阅读入口仍然保持窄范围：{paths}。阅读时不要追求覆盖率，而要追求因果链。"
            f"从一个用户态现象出发，找到内核中的状态对象，再看状态怎样被修改、等待怎样被挂起、唤醒怎样发生、错误怎样返回。",
            "源码阅读可以按四栏笔记整理。第一栏写用户态操作，例如线程等待、缺页、写文件、发送消息或计时。第二栏写内核对象，例如 task、VMA、page、inode、wait queue、bio、socket buffer。第三栏写关键状态变化，例如 runnable 到 sleeping、clean page 到 dirty page、pending task 到 committed task。第四栏写可观察指标或实验，例如 context switch、page fault、writeback、queue depth、retry count。",
            "不要把 Linux 源码当成术语来源。源码阅读的目标是训练映射能力：把书中的抽象边界映射到真实系统里的结构和状态。读者不需要一次读懂整个内核，但要能解释一个最小现象为什么发生、哪些路径参与、哪些结论受当前内核版本和硬件限制。"
        )
    )


def render_project_acceptance(chapter) -> str:
    steps = "\n".join(f"{index + 1}. {step}" for index, step in enumerate(chapter.project_steps))
    return "\n\n".join(
        clean(item)
        for item in (
            "贯穿项目的验收不能只说“功能跑通”。本章至少要留下一个可重复实验、一个失败注入、一个指标输出和一个设计说明。"
            "实验说明机制是否真的被触发；失败注入说明边界是否真的被处理；指标输出说明结论是否有证据；设计说明让后续章节能复用这个模块。",
            f"本章项目检查点可以具体化为：{steps}。这些检查点要进入仓库，而不是停留在阅读笔记。"
            "如果某个检查点因为当前设备权限、硬件或系统 API 不支持而无法完成，也要写清降级方案和未验证风险。",
            "项目验收还应要求读者写一段复盘：本章新增机制让系统多了什么能力，也让系统多了什么复杂度。"
            "比如加入背压后内存更稳定，但生产者会等待；加入 route epoch 后旧请求能被拒绝，但所有消息都必须携带版本；加入 SIMD 后热内核更快，但尾部和数值语义要额外测试。"
            "这种复盘能把知识从“会用”推进到“会判断”。"
        )
    )


def render_chapter(chapter_index: int, chapter) -> str:
    parts: list[str] = [
        MARKER_BEGIN,
        "",
        "\\topic{第二轮深水区：把机制讲成可验证能力}",
        "",
        clean(
            f"本章继续扩写时，重点不再是补充更多名词，而是把已经出现的机制变成可验证能力。"
            f"围绕{chapter.running_problem}，读者最终应能做三件事：解释为什么朴素方案失败，设计能隔离变量的实验，把实验结论落到代码边界。"
            f"这三件事缺一不可。只会解释会变成空谈，只会跑实验会变成工具使用，只会改代码又会在复杂系统里丢掉语义。"
        ),
        clean(
            f"本章的主失败是：{chapter.main_failure}。这个失败会在不同尺度反复出现。"
            "小输入里它可能只是一次结果不稳定；多线程里它可能变成锁竞争、乱序或重复写；I/O 和分布式里它可能变成提交不清、恢复不可靠或重试放大。"
            "所以本章的每个机制都要写出尺度变化后的表现。"
        ),
    ]
    for mechanism_index, mechanism in enumerate(chapter.mechanisms):
        parts.extend(
            [
                "",
                f"\\topic{{深挖：{mechanism.name}}}",
                "",
                render_deep_mechanism(chapter_index, mechanism_index, chapter, mechanism),
            ]
        )
    parts.extend(["", "\\topic{案例深化：从朴素版到工程版}", ""])
    for case_index, case in enumerate(chapter.cases):
        parts.append(render_case_deepening(case_index, case))
    parts.extend(["", "\\topic{实验矩阵：让结论可复现}", "", render_experiment_matrix(chapter)])
    linux = render_linux_deepening(chapter)
    if linux:
        parts.extend(["", "\\topic{Linux 源码阅读深化}", "", linux])
    parts.extend(["", "\\topic{项目验收：把本章能力写进仓库}", "", render_project_acceptance(chapter)])
    parts.extend(["", MARKER_END, ""])
    return "\n".join(parts)


def insert(root: Path, chapter_index: int, chapter) -> bool:
    path = root / chapter.path
    text = path.read_text(encoding="utf-8")
    if MARKER_BEGIN in text:
        return False
    anchor = chapter.anchor
    if anchor not in text:
        raise RuntimeError(f"anchor not found in {path}: {anchor}")
    text = text.replace(anchor, f"{render_chapter(chapter_index, chapter)}\n{anchor}", 1)
    path.write_text(text, encoding="utf-8")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for index, chapter in enumerate(CHAPTERS):
        if insert(root, index, chapter):
            changed += 1
            print(f"deepened {chapter.path}")
        else:
            print(f"already deepened {chapter.path}")
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
