from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


MARKER_BEGIN = "% BEGIN VOLUME2_600K_EXPANSION"
MARKER_END = "% END VOLUME2_600K_EXPANSION"


@dataclass(frozen=True)
class Mechanism:
    name: str
    question: str
    naive: str
    failure: str
    model: str
    observe: str
    boundary: str


@dataclass(frozen=True)
class CaseStudy:
    name: str
    setup: str
    first_try: str
    improve: str
    verify: str


@dataclass(frozen=True)
class ChapterPlan:
    path: str
    anchor: str
    bridge: str
    running_problem: str
    main_failure: str
    mechanisms: tuple[Mechanism, ...]
    cases: tuple[CaseStudy, ...]
    linux_paths: tuple[str, ...]
    project_steps: tuple[str, ...]


def paragraph(text: str) -> str:
    return text.strip()


def render_mechanism(chapter: ChapterPlan, mechanism: Mechanism, index: int) -> str:
    variants = [
        (
            f"先把问题收窄到一个可推导的场景：{mechanism.question}。"
            f"在{chapter.running_problem}里，这个问题不会以术语形式出现，而是以结果不稳定、吞吐不增长、延迟抖动或恢复困难的形式出现。"
            f"如果一上来只背{mechanism.name}的定义，读者很容易把它当成孤立知识点；更好的顺序是先看朴素方案为什么合理，再看它在真实约束下怎样失败。"
        ),
        (
            f"朴素方案通常是：{mechanism.naive}。这个方案的吸引力在于它贴近单线程直觉，代码短，状态少，测试也容易写。"
            f"但是它隐含了几个没有说出口的假设：输入总是干净，资源近似无限，执行不会交错，失败不会发生，硬件成本和源码结构大体一致。"
            f"一旦这些假设被打破，{mechanism.failure}。这时继续在原方案上加 if 或加锁，往往只会把真正的问题埋得更深。"
        ),
        (
            f"{mechanism.name}就是从这个失败里长出来的概念。它的核心模型可以这样理解：{mechanism.model}。"
            f"这个模型必须同时放在三个层次看。源码层要说明谁读谁写、谁拥有对象、哪个状态允许变化；运行时层要说明线程、队列、任务和 I/O 怎样排队；硬件或系统层要说明缓存、页、调度、文件系统或网络怎样参与成本。"
            f"三层缺一层，解释都会变形：只有源码层会看不见隐藏成本，只有硬件层会丢失业务语义，只有运行时层又容易把正确性和性能混在一起。"
        ),
        (
            f"观察方法不能停在“跑一下变快了”。这一点在本章尤其重要：{mechanism.observe}。"
            f"实验要有 reference，要固定输入规模，要记录环境，要把冷启动、预热、核心循环、提交和清理分开。"
            f"如果工具权限不足，也要写清楚不能观察什么，而不是把猜测写成结论。系统教材的严谨性来自证据链：现象、假设、对照、指标、结论边界。"
        ),
        (
            f"{mechanism.name}也有边界。{mechanism.boundary}。"
            f"工程判断不是把一个技术推到所有地方，而是知道它在哪些约束下成立。"
            f"读者在本章应形成一种习惯：每学到一个机制，都要问它解决了什么失败、引入了什么新成本、需要什么测试、在什么输入或部署环境下会失效。"
        ),
    ]
    return "\n\n".join(paragraph(item) for item in variants)


def render_case(case: CaseStudy) -> str:
    return "\n\n".join(
        paragraph(item)
        for item in (
            f"案例“{case.name}”用于把抽象机制落到一段可复现实验。场景是：{case.setup}。这个场景要足够小，小到读者可以手工画出数据流、状态变化和成本路径；又要足够真实，能暴露教材正文讨论的失败模式。",
            f"第一版不要急着写最终答案，而应保留一个朴素版本：{case.first_try}。朴素版本的价值不是性能，而是语义清楚。它告诉我们结果应该是什么，也告诉我们优化前系统在哪些地方偷懒。",
            f"第二版再引入改进：{case.improve}。改进必须说明成本迁移到哪里，而不是只说“更快”。例如减少共享写可能增加最终合并成本，批处理可能增加尾延迟，分片可能引入负载倾斜，异步可能让生命周期更难验证。",
            f"验证时重点看：{case.verify}。报告里至少写出输入规模、硬件或系统环境、编译选项、运行命令、关键指标和反例。若结果与预期不同，优先检查实验变量，而不是马上改代码。"
        )
    )


def render_linux(chapter: ChapterPlan) -> str:
    if not chapter.linux_paths:
        return ""
    paths = "、".join(f"\\filepath{{{path}}}" for path in chapter.linux_paths)
    return "\n\n".join(
        paragraph(item)
        for item in (
            f"Linux 源码阅读要从用户态现象倒推入口。本章可围绕 {paths} 做窄范围阅读。不要试图一次读完整子系统，先选择一个问题，例如一次阻塞为什么睡眠、一次缺页怎样建立映射、一次唤醒怎样进入 run queue、一次写回怎样变成持久化边界。",
            "阅读报告应固定内核版本、阅读路径和实验命令。第一段写用户态最小程序，第二段写观察到的现象，第三段写源码中的关键结构和状态变化，第四段写结论边界。若当前设备不是对应内核，报告必须说明源码只是机制参考，而不是当前运行系统的直接证据。",
            "源码阅读还要看数据结构，而不只是函数名。队列、树、链表、位图、引用计数、状态枚举、等待队列、页表、inode、bio、task 结构这些细节，决定了机制怎样落地。读者要练习把一个用户态概念翻译成内核对象：线程对应 task，等待对应 wait queue 或 futex 路径，文件缓存对应 page cache，内存策略对应 VMA 和页面分配。"
        )
    )


def render_project(chapter: ChapterPlan) -> str:
    steps = "\n".join(f"  {i + 1}. {step}" for i, step in enumerate(chapter.project_steps))
    diagram = (
        "\\begin{lstlisting}[numbers=none]\n"
        "chapter project checkpoints:\n"
        f"{steps}\n"
        "\\end{lstlisting}"
    )
    return "\n\n".join(
        paragraph(item)
        for item in (
            "把本章落到贯穿项目时，不要只加一个接口或一个 benchmark。每次新增能力都应有语义目标、最小实现、对照实验、指标和失败注入。项目不是堆功能，而是把本章的机制变成可运行、可检查、可复盘的工程对象。",
            diagram,
            "项目报告要回答四个问题。第一，朴素版本是什么，为什么不足。第二，改进版本改变了哪个边界，是数据边界、执行边界、同步边界、I/O 边界还是提交边界。第三，证据是什么，哪些指标支持结论。第四，剩余风险是什么，在哪些输入、机器或失败条件下结论可能不成立。只有这样，项目才不会变成代码展示，而会变成系统能力训练。"
        )
    )


def render_expansion(chapter: ChapterPlan) -> str:
    parts: list[str] = [
        MARKER_BEGIN,
        "",
        "\\topic{高密度主线补充：从失败推导机制}",
        "",
        paragraph(
            f"{chapter.bridge}这一轮补充专门解决两个问题：第一，避免把本章写成概念枚举；第二，让每个概念都能从{chapter.running_problem}的失败中自然推出。"
            f"本章的核心失败不是“代码不会写”，而是{chapter.main_failure}。"
            "所以正文的顺序必须始终保持为：先给出具体任务，再写朴素方案，再观察失败信号，再引入机制，最后给出实验和边界。"
        ),
        paragraph(
            "这样的写法比直接给定义慢一些，但它更接近工程学习的真实路径。真实系统很少把问题包装成选择题；它只会表现为吞吐上不去、CPU 利用率怪、结果偶尔错、队列越积越多、重启后状态对不上、线上延迟突然变长。"
            "读者要学会从这些现象反推机制，而不是看到术语才想起术语。"
        ),
    ]
    for index, mechanism in enumerate(chapter.mechanisms):
        parts.extend(["", f"\\topic{{{mechanism.name}}}", "", render_mechanism(chapter, mechanism, index)])
    parts.extend(["", "\\topic{案例与实验串联}", ""])
    parts.extend(render_case(case) for case in chapter.cases)
    linux = render_linux(chapter)
    if linux:
        parts.extend(["", "\\topic{Linux 源码阅读与系统观察}", "", linux])
    parts.extend(["", "\\topic{贯穿项目检查点}", "", render_project(chapter), "", MARKER_END, ""])
    return "\n".join(parts)


CHAPTERS: tuple[ChapterPlan, ...] = (
    ChapterPlan(
        path="chapters/part01-hardware-foundations/ch01-computation-system-map.tex",
        anchor="\\section{本册贯穿实验}",
        bridge="全景章最容易写散，因为它会碰到硬件、操作系统、运行时、I/O 和分布式。",
        running_problem="日志统计作业",
        main_failure="边界不清导致优化、并发、恢复和提交互相打架",
        mechanisms=(
            Mechanism("数据合同", "一行日志缺字段、字段越界或编码损坏时，统计程序应该跳过、报错还是写入错误计数", "解析函数直接返回默认值，主循环继续累加", "错误输入被悄悄吞掉，reference 和优化版本可能给出不同结果", "数据合同定义合法输入、错误类别、错误传播和结果语义，它把脏数据从偶然情况变成显式状态", "用错误样本集、边界值样本和随机损坏样本比较 reference 与优化实现", "合同太宽会掩盖数据质量问题，合同太窄会让系统无法处理真实输入"),
            Mechanism("Reference 实现", "优化后的并行版本怎样证明没有漏算、重复计算或错误合并", "直接相信并行实现输出，因为小样本看起来正确", "输入稍大、线程交错或失败重试后，错误可能只在少数分片出现", "reference 是慢但清楚的语义锚点，所有优化都必须回到它比较", "保存固定输入的结果摘要，比较总数、分 key 计数、错误行统计和输出顺序要求", "reference 不是性能目标，也不能依赖未定义行为或不稳定遍历顺序"),
            Mechanism("控制面和数据面", "为什么不能把任务调度、错误记录、解析和累加都写进同一个热循环", "主循环既读文件又解析又更新状态，还顺便决定重试和输出", "热路径背负太多控制逻辑，难以测试，也难以在失败后恢复", "控制面负责身份、状态、调度和提交，数据面负责批量处理字节和记录", "用 profile 比较拆分前后热函数，并用状态机测试覆盖控制面", "拆分不是分层口号，若接口传递过多可变全局状态，仍然没有真正解耦"),
            Mechanism("有界资源", "读取速度超过解析速度时，队列应该无限增长还是让上游等待", "用无界 vector 或队列缓存所有待处理记录", "内存峰值不可控，延迟被隐藏，最终可能在最坏时刻崩溃", "有界资源把容量写进协议，让背压成为正常状态", "记录队列长度、生产者等待时间、丢弃或失败次数、内存峰值", "容量不是越大越好，太大会放大尾延迟和恢复成本"),
            Mechanism("可观测性", "用户说系统慢时，如何判断慢在读取、解析、聚合、写出还是等待", "只记录端到端耗时和成功失败", "缺少阶段指标时，优化只能靠猜，故障后也无法复盘", "可观测性把系统拆成可测阶段：输入、排队、执行、同步、I/O、提交", "输出 CSV 或 trace，记录每个阶段的记录数、字节数、耗时和错误", "指标有成本，热路径指标要采样、聚合并控制维度"),
            Mechanism("提交边界", "结果写到临时文件、输出文件和 manifest 的哪一刻才算作业完成", "计算完直接覆盖输出文件", "崩溃可能留下半成品，重跑可能重复或丢失结果", "提交边界定义外部可见状态，临时产物和已提交产物必须分开", "通过杀进程实验检查恢复后是否重复计数、是否能识别半成品", "教学项目可以简化持久化，但不能把写入缓存误认为可恢复提交"),
            Mechanism("阶段化学习", "为什么第二册先讲单机多核和计算系统，不直接进入 AI 推理引擎", "把所有主题一次放进一个大项目", "读者同时面对硬件、线程、I/O、模型和量化，无法定位失败来源", "阶段化学习把问题拆成可验证台阶，每一阶都有独立 reference 和指标", "每章产出一份小报告，说明本章机制在贯穿项目中改变了哪个边界", "阶段化不是降低目标，而是防止复杂系统在没有证据时堆叠"),
            Mechanism("工程审查", "一个模块合并前应该检查哪些语义和性能风险", "只看测试是否通过和接口是否能调用", "并发关闭、背压、幂等、错误传播和恢复路径常被遗漏", "审查表把经验变成可重复动作：数据、执行、同步、I/O、提交、观测逐项检查", "为每个模块写风险清单，并让测试或实验覆盖高风险项", "审查表不能替代思考，条目必须随着项目经验更新"),
        ),
        cases=(
            CaseStudy("脏日志输入", "同一批日志中混入缺字段、非法 key、超大数值和重复行", "解析失败就返回零或空 key", "把错误行计入独立错误统计，并让 reference 与优化版本共享解析合同", "错误行数量、合法记录总数、每个 key 计数必须完全一致"),
            CaseStudy("队列积压", "读取线程比解析线程快，解析线程又被写出阶段阻塞", "无界队列一直 push", "改成有界队列并记录生产者等待时间和队列水位", "内存峰值受控，端到端吞吐和尾延迟都要报告"),
            CaseStudy("提交中断", "作业完成一半时进程被杀，磁盘上留下临时输出", "重启后直接覆盖输出", "引入临时文件、校验摘要和 manifest 提交", "重复执行不会重复计数，损坏临时文件不会被当成结果"),
        ),
        linux_paths=("kernel/sched/core.c", "mm/memory.c", "fs/read_write.c"),
        project_steps=("写串行 reference 和固定样本", "定义输入合同和错误统计", "加入有界队列与阶段指标", "加入临时输出和 manifest 提交", "写失败注入报告"),
    ),
    ChapterPlan(
        path="chapters/part01-hardware-foundations/ch02-core-pipeline-ooo-branch.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="微架构章不能从一串硬件名词开始，而要从同一段循环为什么在不同写法下差异巨大开始。",
        running_problem="大数组求和和日志解析热循环",
        main_failure="源码看起来只有一个循环，硬件却可能被依赖链、前端供给、分支或内存请求卡住",
        mechanisms=(
            Mechanism("循环携带依赖", "一个 sum 变量为什么会限制多条加法同时执行", "写一个累加变量，从头加到尾", "后一次加法必须等前一次结果，硬件没有足够独立工作可调度", "依赖链是跨迭代的数据边，拆链才能释放指令级并行", "比较单累加器和多累加器版本的 cycles、instructions 和 IPC", "多累加器会改变溢出和浮点舍入语义，不能无条件替换"),
            Mechanism("乱序窗口", "为什么无关指令可以互相越过，有关指令却不能", "认为 CPU 按源码顺序一条条做完", "无法解释 load miss 时后续独立工作为什么仍可能执行", "乱序执行在保持提交语义的前提下，从窗口中寻找可执行微操作", "观察独立累加器、指针追踪和混合计算循环的吞吐差异", "窗口不是无限的，长延迟和大量未完成 load 会把窗口填满"),
            Mechanism("前端供给", "循环体很小还慢时，是否可能慢在取指和译码", "只数算术指令数量", "复杂分派、间接调用和大 switch 可能让前端跟不上后端", "前端负责取指、预测、译码和把微操作送入后端", "用编译器反汇编和 profile 找到热分派点，比较内联和批量化", "代码体积膨胀会伤害 I-cache，内联不是永远更快"),
            Mechanism("分支预测", "日志字段是否合法的 if 为什么在某些输入上几乎免费，在另一些输入上很贵", "把每个判断都看成固定成本", "随机分布让预测失败，流水线回滚成本进入热路径", "分支预测利用历史和局部模式猜测控制流，猜错后要丢弃错误路径工作", "构造全合法、全非法、交替和随机输入比较分支 miss", "branchless 也会做无用工作，可预测分支常常更好"),
            Mechanism("Load/Store 队列", "为什么指针追踪比数组扫描更难并行", "认为每次 load 成本相同", "下一地址依赖当前节点，硬件不能提前发出足够多请求", "load/store 队列跟踪内存操作，内存级并行依赖地址能否提前知道", "比较数组扫描、随机索引和链表追踪的带宽", "软件预取只有在地址能提前算出时才可靠"),
            Mechanism("编译器优化报告", "为什么看到源代码展开不等于机器码真的按预期执行", "凭源码推测指令", "编译器可能向量化、消除、内联或保留依赖", "优化报告和反汇编把源代码映射到机器执行形态", "查看优化报告、objdump 和 perf annoted 热点", "报告不是性能证明，它只是解释编译器做了什么"),
            Mechanism("调用边界", "热路径里的虚调用和函数指针为什么可能限制优化", "用抽象接口封装每条记录处理", "编译器难以内联，间接分支目标也更难预测", "控制面可以抽象，数据面热内核要尽量让编译器看清类型和循环", "比较虚接口、函数对象、枚举分派和批量接口", "去抽象会增加维护成本，必须由 profile 证明收益"),
            Mechanism("瓶颈迁移", "多累加器优化后为什么下一轮可能慢在内存而不是加法", "把一次优化当成最终答案", "旧瓶颈消失后，新瓶颈显露，继续用旧解释会误导", "性能优化是瓶颈迁移过程，每次改动后都要重新测", "记录优化前后 IPC、带宽、分支和 cache 指标变化", "优化报告必须写出新瓶颈，不只写提升百分比"),
        ),
        cases=(
            CaseStudy("四累加器求和", "同一数组用一个 sum、四个 sum 和八个 sum 求和", "只写一个累加变量", "拆成多个局部累加器，最后合并", "结果一致，吞吐提升后是否开始受内存带宽限制"),
            CaseStudy("随机分支解析", "日志字段有合法和非法两类，输入分布可控", "每条记录直接 if 判断", "按类型分桶或批量处理同类记录", "比较不同分布下分支 miss 和总耗时"),
            CaseStudy("解释器循环", "用 opcode switch 处理一批简单操作", "每条记录随机分派", "按 opcode 分组或合并常见操作", "观察 I-cache、分支和函数调用开销"),
        ),
        linux_paths=("tools/perf", "kernel/events/core.c", "arch/x86/events/core.c"),
        project_steps=("为求和内核保留单累加器 reference", "加入多累加器版本并比较反汇编", "为解析器构造不同分支分布输入", "记录 cycles、instructions 和 branch 指标", "写瓶颈迁移报告"),
    ),
    ChapterPlan(
        path="chapters/part01-hardware-foundations/ch03-cache-tlb-virtual-memory.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="内存章要从同样复杂度的程序为什么速度相差数倍讲起。",
        running_problem="同样处理 N 条记录的数据扫描",
        main_failure="算法复杂度相同，但地址访问形状让 cache、TLB、page fault 和写回成本完全不同",
        mechanisms=(
            Mechanism("Cache line", "为什么读取一个字段会把附近很多字节一起搬进缓存", "按对象字段逐个理解内存成本", "源码字段边界和硬件搬运边界不一致", "cache line 是数据搬运和一致性维护的基本单位，常见大小为 64 字节", "比较连续数组、随机索引和链表访问的带宽", "padding 能减少共享写，也会浪费缓存容量"),
            Mechanism("结构局部性", "为什么两个都使用 vector 的程序仍可能局部性差很多", "只要容器连续就认为访问连续", "随机索引会破坏空间局部性，冷热字段混放会浪费带宽", "结构局部性由数据布局和访问顺序共同决定", "记录每条记录读取的字节、实际使用字段和 cache miss", "过度拆分结构会增加接口复杂度和合并成本"),
            Mechanism("相联度冲突", "工作集没超过 L1 为什么仍然频繁 miss", "只按缓存总容量估算能否放下", "多个热点地址可能映射到同一 set，互相驱逐", "组相联缓存把地址映射到 set，每个 set 只有有限 way", "改变步长、起始偏移和 padding 做对照", "不要把所有性能拐点都归因于容量层级"),
            Mechanism("写分配", "只写输出数组为什么也会产生读流量", "认为 store 只把新值写出去", "普通写 miss 可能先把整条 cache line 读入再修改", "write allocate 和 write back 决定写路径的实际流量", "比较大块输出、后续读取和不再读取三类场景", "non temporal store 需要硬件和工作负载支持，不能盲用"),
            Mechanism("TLB 覆盖", "对象总字节不算大却跨很多页时为什么会慢", "只按数据总大小估算缓存压力", "活跃页数超过 TLB 覆盖后，地址翻译本身成为成本", "TLB 缓存虚拟页到物理页的翻译，page walk 需要访问页表", "构造每页只访问少量字节的 stride 实验", "大页会带来碎片、权限、部署和 NUMA 放置问题"),
            Mechanism("首次触页", "为什么主线程初始化会影响多线程计算性能", "malloc 后认为物理内存已经准备好", "页面常在第一次写入时分配，错误初始化会造成远端访问或大量 minor fault", "first touch 把页面分配、缺页处理和线程位置联系起来", "比较主线程初始化和 worker 分块初始化", "单 socket 或移动设备上可能看不到 NUMA 差异，但仍要记录环境"),
            Mechanism("mmap 和 page cache", "文件映射后为什么访问像内存，却仍可能被 I/O 拖慢", "把 mmap 当成无成本内存", "第一次访问未驻留页会 fault，写映射还涉及脏页和持久化", "page cache 把文件数据缓存成页，mmap 把文件页映射进地址空间", "分冷热运行、记录 page faults 和 I/O 指标", "mmap 简洁但错误处理和延迟分布更隐蔽"),
            Mechanism("分配器布局", "频繁 new 小对象为什么会同时影响 cache、TLB 和锁", "把分配看成常数时间黑盒", "对象分散、元数据和线程本地缓存都会改变局部性", "分配器把大页和 arena 切成小对象，释放未必马上归还内核", "比较 vector、对象池和分散分配的遍历", "对象池改善局部性，也会增加生命周期和峰值内存风险"),
        ),
        cases=(
            CaseStudy("对象数组和指针数组", "一百万条记录既可以连续存储，也可以每条记录单独分配后保存指针", "用指针数组保持接口灵活", "把热字段压成连续数组或对象池", "比较遍历时间、cache miss、TLB miss 和内存峰值"),
            CaseStudy("步长扫描", "按不同 stride 访问同一大数组", "只用 stride 为一的顺序扫描测带宽", "增加 stride、偏移和容量矩阵", "观察 cache line 利用率、TLB 覆盖和性能拐点"),
            CaseStudy("mmap 冷热读", "同一文件第一次扫描和第二次扫描", "只报告一次运行时间", "分开冷 page cache、热 page cache 和显式 read 对照", "记录 major fault、minor fault 和端到端耗时"),
        ),
        linux_paths=("mm/memory.c", "mm/filemap.c", "mm/mmap.c", "mm/mempolicy.c"),
        project_steps=("给 partial 数组加入对齐对照", "加入 stride 和随机访问 microbenchmark", "记录 page faults 与热冷运行", "实现主线程初始化和 worker first touch 两种模式", "写内存边界图"),
    ),
    ChapterPlan(
        path="chapters/part01-hardware-foundations/ch04-numa-interconnect-coherence.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="NUMA 和一致性章要从一个扩展性曲线突然变平的问题讲起。",
        running_problem="多线程计数和大数组归约",
        main_failure="线程数量增加后，瓶颈从计算转移到 cache line 所有权、互连带宽和远端内存",
        mechanisms=(
            Mechanism("一致性所有权", "多个核心写同一个计数器时，为什么每次加法都不是普通加法", "所有线程对同一个 atomic 做 fetch add", "cache line 在核心间迁移，吞吐随线程数上升很快停止增长", "写共享需要取得 cache line 独占所有权，读共享和写共享成本完全不同", "比较全局 atomic、分片计数器和节点内合并", "atomic 保证语言语义，不保证扩展性"),
            Mechanism("MESI 直觉", "只读表和热点计数器为什么都是共享却表现不同", "把共享数据一概视为危险", "读共享可复制，高频写共享要反复失效或迁移", "Modified、Exclusive、Shared、Invalid 可以帮助理解状态迁移", "用文本状态图解释读多写少和写热点", "真实协议更复杂，教学模型只用于解释软件现象"),
            Mechanism("Store buffer", "写 data 再写 flag 为什么仍需要同步语义", "相信源码顺序就是其他线程看到的顺序", "写入可能先在缓冲中，对其他核心的可见顺序必须由同步建立", "store buffer 和内存模型共同决定发布可见性", "用 release acquire 和错误普通变量版本做对照", "不要用硬件一致性为 C++ 数据竞争辩护"),
            Mechanism("NUMA first touch", "为什么同一数组在不同初始化方式下多线程带宽不同", "主线程分配并初始化所有页面", "页面可能集中在一个节点，其他节点 worker 远端访问", "NUMA 把内存位置引入成本模型，first touch 影响页面放置", "比较绑定与未绑定、single touch 与 parallel touch", "没有 NUMA 机器时不能声称验证了远端内存"),
            Mechanism("互连饱和", "每个线程单独跑都快，一起跑为什么都慢", "继续增加线程寻找吞吐", "内存控制器、LLC 或 socket 互连达到上限", "核心、缓存、内存控制器和 socket 之间共享有限互连资源", "画线程数到吞吐曲线，并记录带宽和 LLC 指标", "带宽瓶颈下更多线程可能只增加争用"),
            Mechanism("分层归约", "为什么先本地合并再跨节点合并比所有线程直接写全局结果更稳", "最后一个全局变量收集所有 partial", "跨节点高频写或集中合并会制造热点", "分层归约把高频路径留在近处，把远端通信压缩到少量结果", "比较 worker、NUMA node、global 三层耗时", "层级太多会增加阶段开销，小数据未必值得"),
            Mechanism("拓扑感知队列", "全局任务队列为什么在多 socket 上成为热点", "所有 worker 共享一个 MPMC 队列", "head、tail、锁和槽位状态在节点间迁移", "本地队列和工作窃取把高频访问局部化，低频偷取承担远端成本", "记录本地 pop、同节点 steal、跨节点 steal 次数", "队列分片会增加负载均衡和关闭语义复杂度"),
            Mechanism("拓扑记录", "为什么只写 worker count 不足以复现实验", "报告只记录线程数", "SMT、socket、NUMA node、频率和容器限制都会改变结果", "拓扑记录把 CPU id、core、node、cache 共享和绑定策略写进报告", "保存 lscpu、numactl、cpuset 和程序绑定配置", "生产系统是否绑定要看部署，不是所有服务都适合强绑定"),
        ),
        cases=(
            CaseStudy("全局 atomic 计数器", "多个线程每条记录都更新同一个统计值", "使用 relaxed fetch add", "改为 per worker 或 per NUMA shard", "比较扩展曲线和每次操作平均成本"),
            CaseStudy("双 socket 数组归约", "数组大小超过 LLC，worker 分布在两个 socket", "主线程初始化全部页面", "按 worker 绑定和 first touch 分块初始化", "观察带宽、远端访问迹象和节点内合并成本"),
            CaseStudy("全局队列与本地队列", "大量短任务由所有 worker 抢占执行", "一个全局队列存所有任务", "每节点队列加窃取", "比较短任务和长任务两种粒度下的队列成本"),
        ),
        linux_paths=("kernel/sched/topology.c", "mm/mempolicy.c", "kernel/sched/fair.c", "kernel/locking/qspinlock.c"),
        project_steps=("为计数器实现全局和分片两种路径", "记录 CPU 拓扑和线程绑定", "加入分层归约实验", "统计本地队列和跨节点窃取次数", "写 NUMA 可验证与不可验证边界"),
    ),
    ChapterPlan(
        path="chapters/part02-single-node-hpc/ch05-measurement-roofline-counters.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="测量章要先破除一个直觉：一次计时结果不是证据。",
        running_problem="同一个内核在不同输入、不同温度和不同系统负载下的性能报告",
        main_failure="没有清晰测量边界和统计方法，优化结论无法复现",
        mechanisms=(
            Mechanism("测量边界", "计时应该包含初始化、预热、核心循环、写出和校验中的哪些部分", "在 main 开头和结尾各取一次时间", "把 page fault、分配、I/O 和核心计算混在一起", "测量边界定义本次实验回答的问题，端到端和热循环要分开", "为每个阶段单独记录耗时和输入规模", "边界切得太细会改变系统行为，报告要说明原因"),
            Mechanism("预热", "为什么第一次运行常常不能代表稳定状态", "只运行一次", "缓存、页表、分配器、动态链接和频率状态都会影响第一次", "预热把冷启动成本和稳定状态成本分开", "记录冷运行、预热运行和正式样本", "若目标是冷启动，就不能把冷成本排除"),
            Mechanism("统计显著性", "两个版本差 3 个百分点时能否说优化成功", "只比较一次运行的平均值", "噪声可能大于提升，结论不可重复", "用多次样本、分位数、方差和置信区间描述稳定性", "保存原始样本，不只保存均值", "统计不能修正错误实验设计"),
            Mechanism("Roofline", "内核到底受计算峰值限制还是受内存带宽限制", "只看总耗时", "不知道继续优化算术还是减少数据移动", "Roofline 用算术强度和硬件峰值估计上界", "计算 FLOPs、读写字节和达到的带宽", "模型是上界，不是精确预测，字节数要按实际移动估算"),
            Mechanism("性能计数器", "源码看不出的 cache miss、branch miss 和 cycles 怎样观察", "凭经验判断瓶颈", "优化方向可能完全错", "计数器提供硬件事件样本，是连接源码和执行的证据", "使用 perf stat 对比 cycles、instructions、branches、cache 事件", "不同 CPU 事件名和含义不同，不能跨机器硬比"),
            Mechanism("Profile", "为什么整体慢不等于当前函数慢", "优化自己刚写的代码", "真正热点可能在解析、分配、锁或系统调用", "profile 按执行时间或采样把热点定位到函数、行或指令", "用 perf record 和火焰图观察热路径", "采样会丢失短事件，阻塞等待需要 trace 补充"),
            Mechanism("Trace", "队列等待和任务调度为什么 profile 看不清", "只看 CPU 样本", "线程睡眠、等待、I/O 和背压时间不消耗 CPU，却影响延迟", "trace 记录事件时间线，适合解释排队和状态机", "给任务提交、开始、完成、等待和提交点打点", "trace 维度过高会产生大量数据，需要采样和聚合"),
            Mechanism("报告格式", "怎样让别人复现实验结论", "只写一段优化心得", "缺少环境、命令和原始数据导致结论无法审查", "报告应包含问题、假设、版本、环境、输入、命令、指标、图表和结论边界", "把 benchmark 输出为 CSV 并提交脚本", "报告不是宣传材料，负结果和反例同样重要"),
        ),
        cases=(
            CaseStudy("冷启动和热循环", "同一个数组求和包含分配初始化和不包含分配初始化两种测量", "只测完整 main", "拆分 allocate、initialize、warmup、compute、validate", "说明每个数字回答的问题不同"),
            CaseStudy("Roofline 估算", "比较 saxpy、sum 和 histogram 三种内核", "只报告耗时", "估算字节移动和算术强度", "判断瓶颈在内存带宽、计算端口还是冲突写"),
            CaseStudy("队列等待 trace", "生产者消费者吞吐不稳定", "只看 CPU profile", "给 push wait、pop wait、execute、commit 打事件", "找出等待发生在上游、下游还是锁竞争"),
        ),
        linux_paths=("tools/perf", "kernel/events/core.c", "kernel/sched/core.c"),
        project_steps=("统一 benchmark 输出格式", "为每个实验写冷运行和热运行", "保存原始样本 CSV", "加入 perf stat 可选命令", "为队列和任务加入 trace 事件"),
    ),
    ChapterPlan(
        path="chapters/part02-single-node-hpc/ch06-data-layout-locality-prefetch.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="数据布局章的主线是：同样的业务记录，换一种摆放方式就会改变硬件看到的数据流。",
        running_problem="日志记录、计数表和图邻接数据",
        main_failure="对象模型符合直觉，但热字段分散、冷字段混入、分配碎片和随机访问让硬件搬运大量无用数据",
        mechanisms=(
            Mechanism("AoS 与 SoA", "记录有多个字段时，为什么按对象存储不总是最适合扫描", "使用 struct vector 保存完整记录", "只需要一个字段却每次搬入整条记录", "AoS 适合整对象操作，SoA 适合同一字段批量扫描", "比较按字段扫描、整记录处理和混合访问", "SoA 会增加接口复杂度和对象重建成本"),
            Mechanism("冷热分离", "错误信息、原始字符串和热计数字段是否应该放在一起", "所有字段放进一个大结构体", "热循环加载大量很少使用的冷字段", "冷热分离让常用字段紧凑，冷数据通过索引延迟访问", "记录 cache line 中真正被使用的字节", "过度分离会让代码难读，且随机冷访问可能变慢"),
            Mechanism("对齐和填充", "结构体变大为什么有时更快，有时更慢", "认为结构体越小越好", "并发写槽位共享 cache line 或 SIMD load 不对齐", "对齐是为了满足硬件搬运、SIMD 和一致性边界", "比较紧凑、自然对齐和 cache line 对齐", "padding 会降低缓存密度，不应全局滥用"),
            Mechanism("容器形状", "vector、deque、list 和 unordered map 的内存形状有什么差异", "只按接口复杂度选择容器", "指针结构和桶节点会带来 cache 与 TLB 成本", "容器选择同时选择了局部性、迭代顺序、失效规则和分配模式", "用同一访问任务比较多种容器", "有些业务需要稳定引用或插入删除，不能只按扫描速度选择"),
            Mechanism("Arena 分配", "大量短生命周期对象为什么适合成批释放", "每条记录单独 new 和 delete", "分配器锁、元数据和碎片进入热路径", "arena 把同生命周期对象放在连续区域，生命周期结束时整体释放", "比较分配次数、RSS、遍历速度和释放成本", "arena 不适合交错生命周期，错误持有指针会更危险"),
            Mechanism("索引压缩", "图或稀疏表为什么常用整数 id 替代指针", "节点之间直接存指针", "指针宽、不可压缩、位置分散，序列化也困难", "整数索引把关系变成数组下标，便于压缩、搬运和持久化", "比较 pointer graph 和 CSR 的遍历", "索引需要稳定映射和越界检查，调试不如指针直观"),
            Mechanism("预取友好", "硬件预取器为什么喜欢顺序，不喜欢链表", "指望 CPU 自动处理所有访问", "下一地址晚到，预取器无法提前请求", "预取友好意味着未来地址可预测，最好能提前计算", "比较顺序、固定步长、随机和指针追踪", "手写预取容易污染缓存，必须由实验支持"),
            Mechanism("布局迁移", "已有系统从 AoS 改到 SoA 为什么不能一次硬切", "直接修改核心结构体", "调用者、序列化、测试和调试工具同时受影响", "布局迁移应通过适配层、双写验证和逐步替换完成", "在项目中保留旧布局 reference 和新布局输出比较", "迁移期间重复存储会增加内存，需要清楚退出条件"),
        ),
        cases=(
            CaseStudy("热字段扫描", "记录包含 timestamp、type、payload、error message，但统计只需要 type", "扫描完整结构体数组", "拆出 type 数组并保留 id 映射", "比较每条记录读取字节和总吞吐"),
            CaseStudy("CSR 图遍历", "边列表需要多次按顶点访问邻接边", "每个顶点保存 vector 指针", "构造 offsets 和 edges 两个连续数组", "检查遍历顺序、内存占用和 TLB 行为"),
            CaseStudy("Arena 生命周期", "解析一批记录后立即聚合并丢弃临时 token", "为每个 token 单独分配", "用 batch arena 管理临时对象", "比较分配次数、峰值内存和释放时间"),
        ),
        linux_paths=("include/linux/list.h", "include/linux/rbtree.h", "lib/maple_tree.c", "mm/slab_common.c"),
        project_steps=("为日志记录实现 AoS reference", "拆出热字段列式布局", "加入 arena 临时分配实验", "实现 CSR 风格邻接小实验", "写布局迁移和回退方案"),
    ),
    ChapterPlan(
        path="chapters/part02-single-node-hpc/ch07-simd-ilp-vectorization.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="SIMD 章不能只展示指令名，而要说明为什么标量语义能被成批执行。",
        running_problem="数组变换、阈值过滤和日志字段分类",
        main_failure="循环看似简单，但依赖、分支、尾部、对齐和数值语义会决定能否安全向量化",
        mechanisms=(
            Mechanism("标量语义", "一个标量循环在什么条件下可以按多元素一组执行", "把每次迭代都视为独立", "隐藏依赖、别名或异常语义会让向量化不合法", "向量化要求多次迭代之间没有破坏语义的依赖，并且内存访问形状可描述", "查看编译器向量化报告中 accepted 和 missed 原因", "能向量化不代表值得向量化，小输入和内存瓶颈收益有限"),
            Mechanism("固定宽度类型", "为什么教材代码应使用 std::int32_t 而不是随手 int 或 long long", "用平台默认整数类型", "元素宽度不清会影响 lane 数、溢出边界和文件格式", "SIMD lane 数由向量寄存器宽度和元素位宽共同决定", "在报告中写明数据类型、溢出策略和对齐", "固定宽度类型也不能替代范围检查"),
            Mechanism("尾部处理", "数组长度不是向量宽度倍数时怎么办", "假设输入长度总能整除", "最后几个元素漏算或越界读取", "尾部可以用标量收尾、掩码 load 或补齐输入处理", "用长度覆盖零、一、小于向量宽度、正好整除和多一", "补齐会改变内存访问和边界，必须保证语义"),
            Mechanism("掩码", "有分支的过滤为什么也能部分向量化", "每个元素 if 判断后 push", "控制流分散导致分支预测和输出位置都困难", "掩码把条件结果变成 lane 上的布尔选择，再配合压缩或 scan 分配输出位置", "比较随机条件、全真、全假和稀疏命中", "mask 计算不是免费，稀疏输出还会受写入形状限制"),
            Mechanism("归约向量化", "sum、min、max 为什么不是简单逐 lane 独立", "把所有 lane 直接写回一个标量", "跨 lane 合并需要水平归约，浮点顺序还会改变", "向量归约先在向量寄存器内累积，最后水平合并", "比较整数和浮点归约的结果稳定性", "浮点优化必须说明可接受误差和确定性要求"),
            Mechanism("自动向量化", "为什么编译器有时拒绝看似明显的循环", "认为 -O3 会自动做好一切", "别名、复杂控制流、不可证明对齐或函数调用阻止转换", "自动向量化依赖编译器能证明安全和有益", "使用 restrict 等价设计、span 边界和报告定位 missed reason", "为了向量化扭曲接口不一定值得，先优化热内核"),
            Mechanism("手写 SIMD", "什么时候才需要 intrinsics", "一遇到性能问题就手写指令", "代码不可移植、难测且容易遗漏尾部和对齐", "手写 SIMD 适合稳定热点、明确数据类型和有 reference 的内核", "保留标量 reference、自动向量化版本和手写版本三者对照", "不同 ISA 需要分发和回退，维护成本必须写进设计"),
            Mechanism("批处理接口", "为什么单条记录函数不适合 SIMD", "每条记录调用一次 classify", "函数调用和控制流让编译器看不见跨记录并行", "批处理接口把多条同类数据交给一个内核，暴露连续访问和独立迭代", "比较 per record API 和 batch API", "批太大会增加延迟和缓存压力，批大小需要实验"),
        ),
        cases=(
            CaseStudy("阈值过滤", "从整数数组中选出大于阈值的元素", "每个元素 if 后 push back", "先生成 mask，再用 scan 或分块写出", "覆盖稠密、稀疏和随机命中率"),
            CaseStudy("字节分类", "解析日志时判断字符是否为数字、分隔符或空白", "逐字符 switch", "按块分类并用查表或向量比较", "比较分支分布和批大小影响"),
            CaseStudy("浮点归约", "对浮点数组求和", "直接改为向量累加", "保留确定性模式和快速模式两个路径", "报告误差、速度和输入顺序敏感性"),
        ),
        linux_paths=("arch/x86/lib", "tools/perf", "Documentation/admin-guide/perf-security.rst"),
        project_steps=("为数组变换保留标量 reference", "打开编译器向量化报告", "加入尾部长度测试矩阵", "实现 batch classify 接口", "写自动向量化和手写 SIMD 的取舍报告"),
    ),
    ChapterPlan(
        path="chapters/part02-single-node-hpc/ch08-compute-kernels-patterns.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="典型内核章要把算法模式和系统成本连起来，而不是只列 histogram、top-k、join 这些名字。",
        running_problem="日志聚合管线中的计数、过滤、排序、连接和分区",
        main_failure="每个内核单独正确，但组合后可能出现冲突写、材料化膨胀、缓存失效和长尾分区",
        mechanisms=(
            Mechanism("Histogram", "多个线程更新桶时为什么会从数组问题变成同步问题", "所有线程共享一个桶数组", "热点桶产生写共享和原子争用", "histogram 的核心是 key 到桶的映射、桶更新方式和合并策略", "比较全局桶、线程本地桶和分片桶", "桶太多会增加合并成本和缓存压力"),
            Mechanism("Top K", "只需要前 K 个元素时为什么不一定要全排序", "把所有元素排序后取前 K", "复杂度和内存移动过高，且可能材料化大量无用数据", "Top K 可用堆、选择算法或分块局部 top 再合并", "比较 K 很小、K 较大和输入已近有序三类场景", "流式 top K 的稳定性和相等元素顺序要定义"),
            Mechanism("Join", "两路数据按 key 关联时，排序和哈希该怎么选", "把一边放进 unordered map，逐条查另一边", "哈希表可能随机访问、内存膨胀或受热点影响", "Join 选择取决于大小、key 分布、是否已排序和输出规模", "比较 hash join、sort merge join 和小表广播", "输出可能远大于输入，必须提前估算爆炸风险"),
            Mechanism("Scan", "为什么并行过滤需要前缀和", "每个线程命中后直接 push 到同一 vector", "输出位置争用，顺序和确定性难保证", "scan 把局部计数转换为唯一输出偏移", "验证每个元素只写一次且位置连续", "scan 有额外阶段，小数据可能不值得"),
            Mechanism("Partition", "shuffle 前为什么要按 partition 组织输出", "每条记录直接发给目标 reduce", "小消息和随机写放大 I/O 与网络成本", "partition 把记录按目标分组，形成批量顺序写", "统计每个 partition 的记录数、字节数和倾斜", "分区数过多会产生元数据和小文件问题"),
            Mechanism("Materialization", "中间结果什么时候应该落地，什么时候应该流水线传递", "每个阶段都写一个完整中间数组", "内存和 I/O 成本膨胀，缓存局部性丢失", "材料化换来可恢复和复用，流水线换来低延迟和少搬运", "比较端到端吞吐、峰值内存和失败恢复", "不能为了性能牺牲必要提交点"),
            Mechanism("冲突消解", "多个记录映射到同一 key 时，局部合并应该在哪里做", "把所有原始记录都送到最终 reduce", "网络或队列流量被无谓放大，热点 key 形成长尾", "map side combine 先在近处消解重复 key", "记录 combine 前后字节数和 key 基数变化", "combine 函数必须满足结合律和语义一致"),
            Mechanism("图遍历模式", "BFS 或传播算法为什么容易从计算问题变成访存问题", "按指针邻接表随机访问", "frontier 分布不稳定，邻接访问和状态访问都可能随机", "图内核要关注 frontier、邻接布局、去重和方向切换", "比较 CSR、排序 frontier 和 bitmap visited", "图算法很依赖数据分布，不能只用一个样例"),
        ),
        cases=(
            CaseStudy("热点 histogram", "key 分布高度倾斜，少数 key 占大多数记录", "所有线程更新一个全局 unordered map", "线程本地 map side combine 后再合并", "比较锁等待、输出大小和热点桶合并时间"),
            CaseStudy("并行 filter", "保留满足条件的记录并保持相对顺序", "多个线程 push 到共享 vector", "局部计数、scan 偏移、按偏移写出", "验证顺序、无丢失和不同命中率性能"),
            CaseStudy("小表 join", "一张小维表和一批大日志关联", "每条日志从磁盘或远端查维表", "把小表加载为只读紧凑索引并批量查询", "比较随机访问、缓存命中和错误 key 处理"),
        ),
        linux_paths=("lib/sort.c", "lib/rhashtable.c", "include/linux/hashtable.h"),
        project_steps=("实现线程本地 histogram", "加入 map side combine 指标", "实现 scan filter 输出分配", "统计 partition 倾斜", "写内核组合管线报告"),
    ),
    ChapterPlan(
        path="chapters/part03-concurrency-synchronization/ch09-threads-scheduling-affinity.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="线程章必须从任务为什么不能直接等同于线程讲起。",
        running_problem="把日志作业拆成多个可执行任务",
        main_failure="线程数量、任务粒度、阻塞和调度位置没有边界，导致过度并行、上下文切换和不可复现实验",
        mechanisms=(
            Mechanism("任务和线程", "一个输入分片为什么不应该必然对应一个系统线程", "每个分片创建一个 thread", "线程创建和调度开销随分片数膨胀，错误传播和关闭也难控制", "任务是工作单元，线程是执行资源，运行时负责映射", "比较每任务一线程和固定线程池", "任务太大负载不均，太小调度成本高"),
            Mechanism("过度订阅", "线程数超过可用执行资源会发生什么", "按输入分片数量创建线程", "上下文切换、缓存失效和调度延迟增加", "过度订阅让 runnable 线程争夺有限 CPU", "记录 runnable 数、context switch 和吞吐曲线", "I/O 阻塞任务可能需要不同策略，不能只按核心数固定"),
            Mechanism("阻塞", "一个 worker 阻塞在 I/O 或条件变量上是否还算占用计算资源", "把阻塞任务和计算任务放同一个池", "计算线程被等待占住，吞吐下降且排队难解释", "阻塞会改变线程池容量模型，需要隔离或异步化", "记录线程状态、队列长度和等待原因", "过度拆池也会带来调度和资源配置复杂度"),
            Mechanism("上下文切换", "为什么切换线程不只是保存寄存器", "认为切换成本很小", "缓存、TLB、分支历史和运行队列都会受到影响", "上下文切换是执行上下文和局部性的迁移", "用 perf stat 记录 context switches，配合 trace 看等待", "少量切换正常，问题是无意义高频切换"),
            Mechanism("亲和性", "绑定线程为什么可能更稳定，也可能更慢", "让操作系统自由调度或固定绑定都当成绝对答案", "迁移会破坏缓存局部性，错误绑定会压住同一核心", "亲和性把 worker、CPU、cache 和 NUMA 位置联系起来", "比较绑定、未绑定、SMT sibling 和跨节点", "生产环境还要考虑容器配额和其他进程"),
            Mechanism("大小核", "手机或移动设备上线程数增加为什么可能变慢", "把所有核心视为同构", "小核、大核、温度和频率限制让扩展曲线不规则", "异构核心需要记录核心类型、频率和调度策略", "在 Termux 环境记录 CPU 列表和频率变化", "大小核不是 NUMA，不能混用解释"),
            Mechanism("线程池关闭", "作业取消时，正在等待队列的 worker 如何退出", "设置一个 bool 后等待线程自然醒来", "等待线程可能永远睡眠，任务可能半完成", "关闭语义要唤醒等待者、拒绝新任务、处理未完成任务并传播异常", "用关闭期间 push、pop、cancel 的测试矩阵验证", "粗暴 detach 线程会丢失生命周期管理"),
            Mechanism("调度 trace", "为什么平均吞吐看不出某个分片拖尾", "只记录作业总耗时", "长尾任务、迁移和等待被平均值掩盖", "调度 trace 记录任务从提交到完成的每个阶段", "输出 task id、worker id、start、end、wait 和状态", "trace 不能无限细，热路径要控制开销"),
        ),
        cases=(
            CaseStudy("每任务一线程", "把一千个小分片分别创建线程处理", "直接 thread 构造并 join", "固定线程池处理任务队列", "比较线程创建时间、切换次数和吞吐"),
            CaseStudy("阻塞混入计算池", "部分任务需要等待文件 I/O，部分任务纯 CPU", "全部放同一个 worker 池", "隔离 I/O 或使用异步完成通知", "观察计算任务是否被阻塞任务拖慢"),
            CaseStudy("移动设备扩展曲线", "在 Termux 上从一线程逐步增加到硬件线程数", "只看最大线程数结果", "记录每个线程数、频率和温度趋势", "区分大小核、降频和同步瓶颈"),
        ),
        linux_paths=("kernel/sched/core.c", "kernel/sched/fair.c", "kernel/futex/core.c"),
        project_steps=("实现固定大小线程池", "记录 worker id 和 task id", "加入关闭和取消测试", "比较每任务一线程和线程池", "写亲和性与环境记录"),
    ),
    ChapterPlan(
        path="chapters/part03-concurrency-synchronization/ch10-locks-condition-semaphores.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="锁章节要从共享状态的不变量讲起，而不是从 mutex API 讲起。",
        running_problem="有界队列、共享计数表和线程池状态",
        main_failure="只保护代码片段而不保护不变量，会造成丢唤醒、死锁、关闭悬挂和扩展性下降",
        mechanisms=(
            Mechanism("不变量", "锁到底保护变量还是保护关系", "哪个变量会被多线程访问就给哪个变量加锁", "多个变量之间的关系可能在锁外被破坏", "锁保护的是不变量，例如 head、tail、size 和 buffer 内容的一致关系", "为每个临界区写前置条件和后置条件", "锁范围太大降低并发，太小破坏语义"),
            Mechanism("临界区粒度", "一个大锁简单，为什么有时不够好", "用一个 mutex 保护整个对象", "低竞争时清楚，高竞争时所有操作串行", "粒度选择是在正确性、复杂度和争用之间取舍", "比较全局锁、分片锁和读写锁", "分锁必须定义锁顺序，否则死锁风险上升"),
            Mechanism("条件变量谓词", "为什么 wait 必须放在 while 谓词里", "收到 notify 就认为条件成立", "虚假唤醒、竞争唤醒和关闭状态会让线程醒来后条件仍不成立", "条件变量等待的是状态谓词，不是通知本身", "构造多消费者和 close 场景验证", "notify 不是消息队列，不能携带业务事件"),
            Mechanism("丢唤醒", "先 notify 后 wait 会不会丢事件", "把通知当成存储的信号", "若状态没有记录，等待者可能永远睡眠", "正确模式是先修改受锁保护状态，再 notify，让等待者检查状态", "用压力测试和超时测试暴露悬挂", "靠 sleep 修复丢唤醒是错误做法"),
            Mechanism("信号量", "资源槽位为什么不一定需要完整队列锁", "用 mutex 加计数器手写等待", "容易遗漏释放路径和异常安全", "信号量表示可用资源数量，适合连接数、缓冲槽和并发限流", "测试 acquire、release、超时和异常路径", "信号量不表达队列内容所有权，仍需数据结构同步"),
            Mechanism("死锁", "两个锁都正确为什么组合后会卡死", "每个函数按自己方便的顺序加锁", "线程之间形成等待环", "死锁来自资源获取顺序和不可抢占等待", "画等待图，定义全局锁顺序", "try lock 和超时只能缓解，不能替代设计"),
            Mechanism("异常安全", "临界区里抛异常会不会留下半更新状态", "手动 lock 后在多个分支 unlock", "异常或早返回导致锁不释放或状态半更新", "RAII 和先构造后提交能保持异常路径清楚", "写抛异常的单元测试验证状态不变量", "异常安全不是只靠 lock guard，状态更新顺序也要设计"),
            Mechanism("锁性能路径", "锁慢是因为内核调用还是 cache line 争用", "看到 mutex 就认为一定进内核", "低竞争 fast path 可能很快，高竞争才进入等待和唤醒", "锁性能由竞争、临界区长度、唤醒策略和 cache line 所有权共同决定", "记录等待次数、持锁时间和上下文切换", "不要为了避免 mutex 直接写错误 atomic 协议"),
        ),
        cases=(
            CaseStudy("有界队列 close", "生产者和消费者都可能在关闭时等待", "只设置 closed 标志", "在锁内更新状态并 notify all，让 push 和 pop 都检查谓词", "验证等待中关闭、空队列关闭、满队列关闭"),
            CaseStudy("分片锁 map", "多个 key 更新同一张计数表", "整张表一个 mutex", "按 shard 加锁并定义合并阶段", "比较热点 key 和均匀 key 下的争用"),
            CaseStudy("双锁转移", "把任务从一个队列移动到另一个队列", "先锁源再锁目标或按调用者顺序锁", "按地址或 id 定义全局锁顺序", "压力测试不应悬挂"),
        ),
        linux_paths=("kernel/futex/core.c", "kernel/locking/mutex.c", "kernel/locking/semaphore.c"),
        project_steps=("为 bounded queue 写不变量注释", "实现 close 语义和 notify all", "加入锁等待指标", "为分片计数表比较全局锁和分片锁", "写死锁等待图练习"),
    ),
    ChapterPlan(
        path="chapters/part03-concurrency-synchronization/ch11-atomics-memory-order.tex",
        anchor="\\section{工程审查与项目落地}",
        bridge="原子章要从数据竞争为什么让程序无定义开始，而不是从 memory order 枚举开始。",
        running_problem="计数器、发布配置、无锁状态位和任务完成标志",
        main_failure="把 atomic 当成更快的锁，忽略发布关系、状态机和生命周期",
        mechanisms=(
            Mechanism("数据竞争", "两个线程普通读写同一变量为什么不是硬件最终一致就可以", "用 bool 标志和普通变量通信", "C++ 层面出现数据竞争，编译器优化可让行为无定义", "语言内存模型先定义程序是否合法，硬件一致性只是执行基础", "用线程 sanitizer 或错误示例解释未同步读写", "不要用某次运行正确证明无数据竞争"),
            Mechanism("Relaxed 计数", "统计次数为什么可以使用 relaxed", "所有 atomic 都用 seq cst", "过强内存序增加约束，且让读者误以为计数器发布了其他数据", "relaxed 保证单个原子对象操作原子性，不建立跨对象同步", "比较计数器只用于最终统计和用于发布数据两种场景", "relaxed 不能用来保护非原子对象"),
            Mechanism("Release Acquire", "写数据后设置 ready，读线程怎样看到完整数据", "data 普通写，ready 普通写或 relaxed 写", "读线程可能看到 ready 却没有可靠同步到 data", "release 发布之前写入，acquire 获取并建立 happens before", "写最小发布对象示例并用错误版本对照", "发布的是关系，不是把所有未来访问都变安全"),
            Mechanism("Seq Cst", "最强内存序为什么仍不是默认答案", "所有原子都使用 seq cst 以求安全", "程序可能正确但成本和全局顺序约束更高，也掩盖真实同步意图", "seq cst 提供所有 seq cst 操作的单一全序直觉", "在状态机中标注哪些操作需要全序，哪些只需发布", "不要为追求性能随意降级，先写清同步关系"),
            Mechanism("CAS 循环", "多个线程更新复合状态时为什么需要 compare exchange", "load 后计算再 store", "中间可能被其他线程修改，更新丢失", "CAS 把检查旧值和写入新值变成一个原子条件转换", "统计 CAS 成功率、失败次数和退避", "CAS 循环内不能做不可重复副作用"),
            Mechanism("ABA", "指针值从 A 到 B 又回到 A 为什么仍可能出错", "CAS 只比较地址相等", "地址相同不代表对象生命周期相同", "ABA 是值相等掩盖历史变化，可用版本、标记指针或回收协议缓解", "构造节点弹出释放再复用的场景", "版本位有限会回绕，内存回收仍要严谨"),
            Mechanism("Atomic 状态机", "一个任务从 pending 到 running 到 done 是否只需要几个 bool", "为每个状态放一个 atomic bool", "状态组合可能出现 impossible state", "把状态编码成单一枚举，用 CAS 做合法转移", "测试所有合法和非法转移", "状态机越复杂，越应考虑锁而不是原子拼图"),
            Mechanism("审查同步关系", "代码评审时怎样判断 memory order 是否正确", "看起来没有崩就接受", "同步关系遗漏常只在弱时序或高并发下出现", "审查要写出谁 release、谁 acquire、保护哪些数据、生命周期到哪里", "为每个 atomic 字段写注释和测试", "注释不能替代验证，但没有注释的复杂内存序不可维护"),
        ),
        cases=(
            CaseStudy("发布配置快照", "一个线程构造配置，另一个线程读取并使用", "写指针后读线程直接使用", "构造完成后 release store 指针，读线程 acquire load", "验证读线程不会看到半初始化对象"),
            CaseStudy("任务状态 CAS", "多个 worker 竞争领取 pending 任务", "先读状态再写 running", "compare exchange pending 到 running", "统计任务不会被领取两次"),
            CaseStudy("热点 atomic 计数", "所有请求都更新同一个原子统计", "用 relaxed fetch add", "按 worker 分片并周期合并", "比较正确性、读取实时性和 cache line 迁移风险"),
        ),
        linux_paths=("include/linux/atomic", "kernel/locking/lockref.c", "include/linux/refcount.h"),
        project_steps=("为所有 atomic 字段写语义注释", "实现 relaxed 统计和发布状态两个对照", "用 CAS 领取任务", "记录 CAS 失败率", "写 memory order 审查表"),
    ),
    ChapterPlan(
        path="chapters/part03-concurrency-synchronization/ch12-lock-free-data-structures.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="无锁结构章要从锁阻塞带来的问题推导，而不是把无锁当作高级标签。",
        running_problem="高频队列、任务提交和跨线程节点传递",
        main_failure="移除锁后，线性化点、内存回收、ABA 和测试难度同时出现",
        mechanisms=(
            Mechanism("进展保证", "无锁到底保证谁能继续前进", "把不用 mutex 的代码都叫无锁", "自旋、活锁或单线程饿死仍可能存在", "obstruction free、lock free、wait free 描述不同进展级别", "在报告中写清算法保证和不保证什么", "进展保证不等于低延迟，也不等于公平"),
            Mechanism("SPSC Ring", "单生产者单消费者为什么能比通用队列简单", "直接使用 MPMC 队列处理所有场景", "通用协议更复杂，热点原子更多", "SPSC 利用单 writer 属性分离 head 和 tail 更新", "测试满、空、 wrap around 和关闭", "一旦变成多生产者或多消费者，假设立即失效"),
            Mechanism("线性化点", "并发操作到底在哪一瞬间生效", "只看函数返回值", "两个操作交错时无法定义历史顺序", "线性化点把并发历史映射到某个合法串行历史", "为 push、pop、close 标注线性化点", "找不到线性化点的结构很难证明正确"),
            Mechanism("内存回收", "节点从队列摘下后为什么不能马上 delete", "pop 成功后直接释放节点", "其他线程可能仍持有旧指针或正在读取字段", "无锁回收需要证明没有线程再访问节点", "比较 hazard pointer、epoch 和引用计数思路", "回收协议常比队列算法本身更难"),
            Mechanism("Hazard Pointer", "怎样让线程声明自己正在访问某个节点", "依赖短时间窗口足够安全", "释放线程无法知道读线程是否还拿着指针", "hazard pointer 发布受保护指针，释放前扫描危险指针集合", "构造慢读线程和快释放线程压力测试", "扫描成本和线程退出清理需要设计"),
            Mechanism("Epoch", "批量延迟释放为什么常比逐节点判断更便宜", "每个节点都即时判断是否可删", "判断成本高且难与线程状态关联", "epoch 把线程活动划入时期，跨过安全时期后批量回收", "测试长时间停留线程导致回收延迟", "epoch 适合短临界区，不适合线程可能无限挂起的场景"),
            Mechanism("MPMC 队列", "多生产者多消费者为什么比 SPSC 难很多", "给 ring 的 head tail 都换成 atomic", "槽位状态、序号、ABA 和内存序都要处理", "MPMC 通常需要每槽序号或链式节点来区分轮次", "压力测试不同生产者消费者数量和容量", "若锁队列足够快，不必为了标签使用复杂无锁"),
            Mechanism("并发测试", "为什么普通单元测试很难证明无锁结构正确", "跑几次 push pop 看结果", "错误只在特殊交错、回收和关闭时出现", "并发测试要结合模型、压力、随机调度和不变量检查", "记录操作历史并离线检查线性化可能性", "测试不能完全证明，但能暴露大量工程错误"),
        ),
        cases=(
            CaseStudy("SPSC ring buffer", "一个解析线程把 batch 交给一个计算线程", "用 mutex queue", "用固定容量 ring 和分离 head tail", "覆盖满空、 wrap、关闭和批量 push"),
            CaseStudy("节点释放 ABA", "无锁栈中节点被 pop、delete、再分配到同一地址", "CAS 只比较指针", "加入版本或回收协议", "构造复用地址压力测试"),
            CaseStudy("无锁是否值得", "队列操作占端到端耗时很小", "直接替换为复杂无锁队列", "先测锁队列等待和持锁时间", "若瓶颈不在队列，无锁改造不应进入主体"),
        ),
        linux_paths=("kernel/locking/lockref.c", "include/linux/rcupdate.h", "kernel/rcu/tree.c"),
        project_steps=("实现 SPSC ring 作为专用通道", "为每个操作标注线性化点", "加入关闭和容量测试", "写内存回收风险说明", "比较锁队列和专用队列"),
    ),
    ChapterPlan(
        path="chapters/part04-parallel-algorithms-runtime/ch13-reduce-scan-partition.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="并行算法章要从串行算法的语义哪些可以拆、哪些不能拆讲起。",
        running_problem="大规模日志计数、过滤输出和 shuffle 分区",
        main_failure="没有先证明结合性、输出位置和分区语义，就直接并行会产生不确定结果或重复写",
        mechanisms=(
            Mechanism("结合律", "为什么 sum 容易并行，而按顺序拼接日志不一定容易", "把任意循环切块并合并", "合并顺序改变会改变结果或错误处理顺序", "并行 reduce 需要可结合的合并函数和明确 identity", "用随机切块比较结果一致性", "浮点、字符串和错误列表要额外定义顺序语义"),
            Mechanism("树形归约", "为什么所有线程最后写一个变量不是好归约", "全局锁或全局 atomic 累加", "共享写热点限制扩展", "树形归约先局部合并，再分层合并", "记录每层 partial 数量和合并耗时", "层级过深增加同步阶段"),
            Mechanism("确定性", "并行结果为什么可能每次顺序不同", "接受 unordered map 的任意遍历输出", "测试和恢复难以比较，用户看到输出不稳定", "确定性需要固定 partition、合并顺序和输出排序或摘要", "多次运行比较字节级输出或稳定 checksum", "确定性可能牺牲性能，要说明取舍"),
            Mechanism("Prefix Scan", "过滤时如何给每个保留元素唯一位置", "多个线程 push 共享 vector", "输出争用且顺序不稳定", "scan 把每块命中数转换为全局偏移", "测试空块、全命中、稀疏命中和边界长度", "scan 需要额外 pass，小输入可能用串行更简单"),
            Mechanism("Partition", "分布式 shuffle 前如何把记录归到目标分区", "每条记录直接发送", "小消息、随机写和重复元数据放大成本", "partition 先按目标计数、分配空间、批量写出", "记录 partition 大小分布和热点 key", "分区函数一旦改变，需要版本和兼容策略"),
            Mechanism("负载倾斜", "所有 worker 记录数相同为什么耗时仍不同", "按记录条数平均切分", "key 热点、记录长度和解析难度不同导致长尾", "负载要按成本估计，而不只按数量", "记录每块字节、key 基数、命中率和耗时", "动态调度改善长尾，但可能降低局部性"),
            Mechanism("容错切块", "任务失败后从哪里重做", "失败就重跑整个作业", "大输入下恢复成本过高", "按 chunk 切分并记录 partial 提交点，可重试小块", "故障注入 worker 崩溃和重复 attempt", "切块太小会增加调度和 manifest 元数据"),
            Mechanism("组合律验证", "自定义聚合函数怎样证明能并行", "直接把业务 merge 放入 reduce", "不同合并顺序暴露隐藏状态", "用性质测试检查 identity、associativity 和可交换要求", "随机生成 partial 并用多种括号化顺序合并", "业务上需要顺序时不要伪装成无序 reduce"),
        ),
        cases=(
            CaseStudy("并行计数", "按 key 统计日志事件数量", "全局 map 加锁更新", "每块本地 map，最后树形合并", "比较正确性、锁等待和热点 key"),
            CaseStudy("稳定过滤", "保留错误行并保持输入顺序", "线程共享 vector push", "局部计数、scan 偏移、按块写出", "多次运行输出完全一致"),
            CaseStudy("分区倾斜", "少数 key 占据大多数记录", "普通 hash partition", "map side combine 加热点拆分", "比较 reduce 长尾和总 shuffle 字节"),
        ),
        linux_paths=("lib/sort.c", "lib/btree.c", "include/linux/bitmap.h"),
        project_steps=("为聚合函数写性质测试", "实现树形 reduce", "实现 scan filter", "记录 partition 分布", "加入失败后 chunk 重试"),
    ),
    ChapterPlan(
        path="chapters/part04-parallel-algorithms-runtime/ch14-task-runtime-work-stealing.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="任务运行时章要从固定线程池为什么不够表达依赖和动态负载讲起。",
        running_problem="解析、map、shuffle、reduce 多阶段任务图",
        main_failure="把所有任务塞进一个 FIFO 队列，无法表达依赖、本地性、取消和长尾处理",
        mechanisms=(
            Mechanism("任务图", "reduce 为什么必须等对应 map 输出完成", "用队列顺序隐式保证阶段", "动态重试和部分完成后顺序假设被打破", "任务图用节点和边显式表达依赖", "输出每个任务的依赖计数和状态转换", "图太细会带来调度开销"),
            Mechanism("Future", "异步任务的结果、异常和取消怎样传播", "任务完成后写共享变量", "读者不知道结果何时可用，异常可能丢失", "future 表示未来完成的值或错误，是控制面对象", "测试正常完成、异常完成和等待取消", "future 不是无限缓存，生命周期和等待策略要清楚"),
            Mechanism("本地队列", "worker 为什么优先运行自己产生的任务", "所有任务进入全局队列", "全局锁和 cache 迁移成为热点", "本地队列保留数据和执行局部性", "记录本地执行比例和全局入队次数", "本地性可能导致负载不均，需要窃取补偿"),
            Mechanism("工作窃取", "某个 worker 空闲时怎样帮助其他 worker", "空闲线程阻塞等待全局任务", "长尾任务让其他核心闲置", "work stealing 让空闲 worker 从其他队列偷取任务", "记录 steal 次数、成功率和被偷任务成本", "窃取会破坏本地性，偷取粒度要控制"),
            Mechanism("任务粒度", "任务越小负载越均衡，为什么不能无限小", "每条记录一个任务", "调度成本超过计算成本，队列和 trace 膨胀", "粒度是在负载均衡、局部性和调度开销之间取舍", "扫描任务大小到吞吐曲线", "最佳粒度随机器和输入变化"),
            Mechanism("NUMA 本地性", "窃取应该先偷近处还是全局随机偷", "所有 worker 等价", "跨节点偷取可能带来远端内存访问", "NUMA 感知窃取先同节点，再跨节点", "记录 steal 距离和任务数据位置", "策略复杂度必须由目标机器收益证明"),
            Mechanism("取消传播", "上游失败后，下游已经排队的任务怎么办", "只让失败任务返回错误", "下游继续运行浪费资源甚至提交错误结果", "取消是任务图中的状态传播，需要定义可取消点和补偿", "故障注入 map 失败，观察 reduce 是否停止", "强行杀线程不安全，取消通常是协作式"),
            Mechanism("异常收束", "多个任务同时失败时，driver 应该报告哪个错误", "谁最后写错误变量就报告谁", "错误丢失或顺序不确定", "运行时应收集首错、所有错误摘要和受影响任务", "测试多点失败和重试后成功", "错误报告不能淹没主路径指标"),
        ),
        cases=(
            CaseStudy("递归式任务拆分改迭代", "大输入分块后继续拆成小块", "递归调用创建子任务", "使用显式 worklist 迭代生成任务", "验证栈深度稳定且任务数量受控"),
            CaseStudy("工作窃取长尾", "部分分片包含大量热点 key，处理时间更长", "固定分配给 worker", "长任务拆分并允许空闲 worker 窃取", "比较尾部完成时间和 steal 成本"),
            CaseStudy("取消下游", "某个 map 发现输入损坏无法继续", "只标记 map failed", "传播取消到依赖的 reduce 和 shuffle", "验证不会提交依赖失败输入的结果"),
        ),
        linux_paths=("kernel/sched/fair.c", "kernel/workqueue.c", "kernel/softirq.c"),
        project_steps=("把线程池扩展为任务状态机", "实现依赖计数和 ready 队列", "记录本地执行和 steal 指标", "加入协作式取消", "写任务图 trace 报告"),
    ),
    ChapterPlan(
        path="chapters/part04-parallel-algorithms-runtime/ch15-io-async-backpressure.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="I/O 章要从 CPU 很空但系统仍然慢的现象讲起。",
        running_problem="读取输入文件、写 shuffle block、提交 checkpoint",
        main_failure="执行和完成分离后，生命周期、背压、超时和持久化边界变得比函数调用复杂",
        mechanisms=(
            Mechanism("阻塞 I/O", "read 阻塞时 worker 到底在等什么", "在计算线程中直接读写文件", "线程被 I/O 等待占住，计算资源和等待资源混在一起", "阻塞 I/O 简单但会把等待带入执行线程", "记录 blocked time、CPU 利用率和队列长度", "对小工具阻塞 I/O 可能足够，不必过度异步"),
            Mechanism("异步完成", "提交请求后，结果何时回来，谁拥有 buffer", "把异步调用当成立刻完成", "buffer 生命周期、错误返回和取消都不清楚", "异步 I/O 把提交和完成拆开，需要 completion 事件和所有权规则", "测试完成前释放 buffer 和取消场景", "异步增加状态机复杂度，必须有清晰封装"),
            Mechanism("背压", "写出速度慢于计算速度时，上游应该怎么办", "继续产生输出并排队", "内存增长、延迟增长，最终 OOM 或超时", "背压把下游容量反馈到上游，让生产速度受限制", "记录队列水位、生产者等待和丢弃策略", "背压策略是业务语义，不能只有技术默认"),
            Mechanism("批处理", "为什么每条记录一次 write 会慢", "记录一来就写出", "系统调用、锁和设备提交成本被放大", "批处理让一次昂贵操作服务多条记录", "比较批大小、延迟分位数和吞吐", "批太大增加尾延迟和崩溃后重做成本"),
            Mechanism("超时", "一次 I/O 请求慢到什么程度算失败", "无限等待完成", "上游资源被长期占用，调用链预算失控", "timeout 是资源预算的一部分，需要和重试、取消、幂等配合", "注入慢 I/O，观察超时后状态清理", "底层操作可能超时后仍完成，不能假设自动撤销"),
            Mechanism("可靠写入", "write 返回成功是否等于 checkpoint 可恢复", "写文件后立即认为提交", "数据可能只在 page cache，目录项也可能未持久化", "可靠写入需要临时文件、fsync、rename 和必要的目录同步", "用崩溃点矩阵分析提交前后", "不同文件系统和设备语义有差异，教学项目要写边界"),
            Mechanism("完成队列", "多个 I/O 完成事件如何交给计算线程", "完成回调直接做大量计算", "回调线程被阻塞，完成队列堆积", "完成队列把 I/O 事件转换为运行时任务", "记录 completion 到任务开始的延迟", "回调中应保持短小，复杂工作交给任务池"),
            Mechanism("错误分类", "I/O 错误都重试是否正确", "失败就立即重试", "权限、空间不足和数据损坏重试无效，忙碌和临时错误可以退避", "错误分类决定重试、降级、失败和告警", "构造 busy、not found、permission、corrupt 四类错误", "重试必须有预算和抖动，否则会放大故障"),
        ),
        cases=(
            CaseStudy("慢写出背压", "reduce 产生输出比磁盘写入快", "无界缓存输出 block", "有界写队列加生产者等待", "观察内存峰值和尾延迟"),
            CaseStudy("批大小扫描", "同样总字节按不同 batch 写出", "每条记录单独写", "按 4KiB、64KiB、1MiB 等批大小测试", "报告吞吐和 p99 延迟"),
            CaseStudy("checkpoint 崩溃点", "写 checkpoint 时在不同步骤 kill 进程", "直接覆盖 checkpoint", "临时文件加 manifest 提交", "恢复只接受完整且校验通过的版本"),
        ),
        linux_paths=("fs/read_write.c", "mm/filemap.c", "fs/sync.c", "io_uring/io_uring.c"),
        project_steps=("给写出队列设置容量", "记录 I/O 提交和完成事件", "实现临时文件加 manifest", "加入慢 I/O 和失败注入", "写背压传播图"),
    ),
    ChapterPlan(
        path="chapters/part05-distributed-computing/ch16-distributed-system-model.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="分布式模型章要从一次函数调用变成一次消息后丢失了什么保证讲起。",
        running_problem="driver 向 worker 发送任务并等待结果",
        main_failure="远端执行的完成、失败和重复都无法靠本地调用栈表达",
        mechanisms=(
            Mechanism("消息身份", "响应回来时如何知道它属于哪个请求和哪个 attempt", "只按连接或 worker 匹配响应", "重试、乱序和重复响应会污染状态", "request id、attempt 和 epoch 让消息具有可验证身份", "在模拟网络中打乱响应顺序", "身份字段必须进入提交边界，不只是日志字段"),
            Mechanism("Deadline", "超时应该从每一层重新计算还是继承调用预算", "每层都设置固定超时", "多层调用叠加后总时间失控", "deadline 表示绝对预算，沿调用链传递", "记录剩余时间和超时原因", "过短 deadline 会造成无意义重试，过长会占住资源"),
            Mechanism("重试", "请求超时后是否可以直接再发一次", "立即重试直到成功", "服务端可能已经执行，重复请求可能重复提交", "重试需要预算、退避、抖动和幂等语义", "注入响应丢失和服务端慢处理", "不可幂等操作不能盲目重试"),
            Mechanism("幂等", "重复执行同一个请求为什么不应改变最终结果", "服务端每收到一次就执行一次", "响应丢失后重试会重复计数或重复写输出", "幂等表记录 request id、状态和结果，使重复请求返回同一结果", "测试执行成功但响应丢失", "幂等记录本身也要和业务提交同边界"),
            Mechanism("部分失败", "客户端活着、服务端活着但网络断开时系统处于什么状态", "把失败视为进程崩溃或成功两种", "真实系统存在消息丢失、延迟、分区和旧 leader", "部分失败意味着一方无法可靠知道另一方状态", "用消息丢弃、重复、延迟和乱序模拟", "超时只是未知，不是远端已停止"),
            Mechanism("流控", "worker 返回 busy 时 driver 应该怎么做", "继续提交任务直到队列满", "重试和排队放大故障", "流控把接收方容量反馈给发送方", "记录 in flight、busy 响应和退避时间", "流控不是错误，它是保护系统的协议"),
            Mechanism("Trace Id", "一次作业跨多个 worker 后如何定位慢在哪里", "每个进程各写自己的日志", "无法串联同一请求的路径", "trace id 把跨组件事件连成一条时间线", "记录 driver send、worker receive、execute、reply", "trace 也要采样和限流，不能让观测拖垮系统"),
            Mechanism("本机模拟", "没有集群时怎样学习分布式失败", "只写正常路径函数调用", "无法暴露乱序、重复和超时", "本机 message bus 可以注入延迟、丢失、重复和乱序", "用固定随机种子复现实验", "模拟不能证明真实网络性能，但能训练协议语义"),
        ),
        cases=(
            CaseStudy("响应丢失", "worker 已经完成任务并提交结果，但回复在路上丢失", "driver 超时后派发新 attempt", "worker 或 reduce 端用 request id 去重", "结果只提交一次，重复响应返回同一摘要"),
            CaseStudy("重试风暴", "服务端变慢，客户端同时超时并重试", "所有客户端立即重试", "指数退避、抖动和全局 in flight 限制", "请求量不会因重试超过预算"),
            CaseStudy("乱序响应", "attempt 2 的响应先到，attempt 1 的响应后到", "谁后到就覆盖状态", "用 attempt 和 epoch 做状态检查", "旧 attempt 不会覆盖已提交结果"),
        ),
        linux_paths=("net/core/dev.c", "net/ipv4/tcp.c", "kernel/time/timer.c"),
        project_steps=("为每条消息加入 request id 和 attempt", "实现可注入故障的 message bus", "加入 deadline 和重试预算", "实现幂等结果表", "输出跨组件 trace"),
    ),
    ChapterPlan(
        path="chapters/part05-distributed-computing/ch17-partition-replication-consensus.tex",
        anchor="\\section{本章落到贯穿项目}",
        bridge="分片复制章要从单机数组切块自然推进到集群数据布局。",
        running_problem="把 key 空间分给多个 worker 并在故障后保持结果可信",
        main_failure="没有版本、提交点和副本语义，重分片、旧请求和故障切换会破坏结果",
        mechanisms=(
            Mechanism("分片函数", "一个 key 应该去哪个 partition", "直接对 key 做 hash", "热点、范围查询和再分片成本可能不符合业务", "分片函数定义数据位置和负载分布", "统计每个 partition 的记录数、字节数和热点", "分片函数改变需要兼容旧数据和路由版本"),
            Mechanism("热点 key", "少数 key 占大多数请求时 hash 是否足够", "相信哈希会平均", "同一个热点 key 必然落到同一分区", "热点处理需要拆分 key、局部聚合或二级 reduce", "构造 Zipf 分布输入", "拆热点会增加合并语义和状态管理"),
            Mechanism("路由版本", "迁移期间旧客户端还按旧路由发请求怎么办", "客户端缓存 partition 到 worker 映射", "旧请求可能写到旧 owner", "epoch 或 route version 让服务端拒绝过期请求", "模拟迁移中旧请求延迟到达", "版本检查不能代替数据迁移校验"),
            Mechanism("再分片状态机", "把一个 partition 从 A 移到 B 需要哪些阶段", "复制完数据就切换", "增量写、校验、旧路由和清理都可能出错", "再分片应有 prepare、copy、catch up、switch、cleanup 状态", "在每个状态注入失败并恢复", "状态机太粗会隐藏危险，太细会增加操作成本"),
            Mechanism("复制提交点", "写到 leader 内存、WAL、本地磁盘或多数副本，哪个算成功", "leader 收到请求就返回成功", "leader 崩溃可能丢失已确认写", "提交点定义成功响应后系统承诺什么", "比较单副本、异步复制和 quorum 写", "强提交点增加延迟，弱提交点要写清风险"),
            Mechanism("Quorum", "为什么多数派能避免两个 leader 同时提交冲突结果", "任意一个副本成功就算成功", "网络分区后两个集合都可能认为自己可写", "多数派交叠保证两个成功决策至少共享一个副本", "画三副本和五副本故障矩阵", "quorum 不自动解决慢副本、成员变更和读语义"),
            Mechanism("Leader fencing", "旧 leader 恢复后为什么不能继续接受写", "恢复后继续按旧身份工作", "旧 leader 可能覆盖新 leader 的决定", "fencing 用任期、租约或版本阻止旧权威写入", "模拟旧响应延迟和旧 leader 复活", "时间租约依赖时钟假设，版本检查更明确"),
            Mechanism("共识边界", "哪些状态需要共识，哪些数据不应放进共识", "所有数据都走强一致日志", "吞吐被最慢副本限制，数据面成本过高", "共识适合少量控制面决策，数据面走批量传输和校验", "把 manifest 指针放控制面，把 block 内容放数据面", "控制面太弱会不安全，太强会拖垮系统"),
        ),
        cases=(
            CaseStudy("热点拆分", "一个 event type 占百分之八十记录", "普通 hash partition", "把热点 key 拆成多个 subkey 后二级合并", "比较 reduce 长尾和最终结果一致性"),
            CaseStudy("迁移旧请求", "partition 从 worker A 迁到 B 后，旧请求延迟到达 A", "A 继续接受并写入", "请求携带 route epoch，旧 owner 拒绝", "旧请求不会产生新提交"),
            CaseStudy("多数写", "三副本中一个副本慢或丢失响应", "等待所有副本", "多数确认后返回，并让落后副本追赶", "写清读路径是否可能读到旧副本"),
        ),
        linux_paths=("Documentation/filesystems", "kernel/locking/lockdep.c", "lib/idr.c"),
        project_steps=("实现 key 到 partition 的路由表", "为路由加入 epoch", "实现热点 key 拆分实验", "为 shuffle block 写 checksum 和 manifest", "写再分片状态机和故障矩阵"),
    ),
    ChapterPlan(
        path="chapters/part05-distributed-computing/ch18-distributed-compute-engineering.tex",
        anchor="\\section{全书收束与扩展}",
        bridge="工程实践章要把前面所有机制收束成一个能失败、能恢复、能测量的计算系统。",
        running_problem="本机模拟的分布式日志统计引擎",
        main_failure="只有正常路径的 MapReduce 演示无法处理重复 attempt、shuffle 校验、checkpoint 损坏和 driver 重启",
        mechanisms=(
            Mechanism("Map 任务", "输入 split 怎样保证每条记录被覆盖一次", "按字节平均切分文件", "切在记录中间会重复或漏读", "split 要定义 begin、end、记录边界和错误处理", "用边界样本检查 split 覆盖", "压缩文件和多字节编码会改变切分策略"),
            Mechanism("Map side combine", "为什么不把每条原始记录都发给 reduce", "map 直接输出 key value 记录", "shuffle 字节数和网络队列被放大", "combine 在近处压缩重复 key，减少远端移动", "记录 combine 前后 key 数和字节数", "combine 函数必须与最终 reduce 语义一致"),
            Mechanism("Shuffle manifest", "reduce 怎样知道哪些 block 完整可读", "扫描目录里现有文件", "半写文件、旧 attempt 和损坏文件可能被读入", "manifest 记录 block 身份、attempt、partition、大小和 checksum", "损坏 block 和丢 manifest 的恢复实验", "manifest 是提交边界，不是普通日志"),
            Mechanism("Task attempt", "同一个任务重试多次时，哪些结果有效", "后完成的 attempt 覆盖前一个", "旧 attempt 可能在新 attempt 后提交", "attempt id 和提交表决定唯一有效结果", "模拟旧 attempt 延迟完成", "取消旧 attempt 不代表它已经停止，提交时仍要检查"),
            Mechanism("Checkpoint", "driver 重启后怎样知道哪些任务完成、哪些要重跑", "内存里维护状态，崩溃后从头开始", "大作业恢复成本高，且可能重复提交", "checkpoint 记录可恢复状态，恢复时 running 任务回到 pending 或重新验证", "在不同状态 kill driver 后恢复", "checkpoint 频率影响开销和恢复时间"),
            Mechanism("Driver 状态机", "作业状态为什么不能只用 running 和 done", "用几个 bool 表示阶段", "状态组合不一致，错误路径难处理", "driver 应有明确状态：planning、running、draining、committing、completed、failed", "测试每个状态收到重复消息和失败消息", "状态机越核心，越需要可视化和审查"),
            Mechanism("故障注入", "为什么正常测试通过还不能说明系统可靠", "只跑 happy path", "真实故障发生在提交、重试、慢节点和损坏数据上", "故障注入把失败变成可重复输入", "覆盖丢消息、重复消息、慢 worker、损坏 block、提交失败", "故障注入也要有预算，避免测试不稳定"),
            Mechanism("Runbook", "系统出问题时，操作者应该按什么顺序检查", "现场凭经验排查", "容易先改错模块或破坏证据", "runbook 写明指标、命令、前置条件、回滚和验证", "为慢 reduce、checkpoint 损坏和热点 key 写手册", "runbook 要随项目演进更新，否则会过期"),
        ),
        cases=(
            CaseStudy("完整 shuffle", "map 输出多个 partition block，reduce 读取对应 block", "reduce 扫描所有临时文件", "map 提交 manifest，reduce 只读 manifest 中校验通过的 block", "删除、截断或篡改 block 后恢复行为明确"),
            CaseStudy("driver 重启", "driver 在部分任务 running 时崩溃", "重启后继续相信内存状态", "从 checkpoint 恢复，running 任务转回 pending 并验证已提交结果", "最终输出不重复且不丢失"),
            CaseStudy("慢 reduce 背压", "一个 reduce partition 因热点 key 变慢", "map 继续发送所有 block", "reduce 队列返回 busy，map 端退避或本地 combine", "队列水位下降且作业不会 OOM"),
        ),
        linux_paths=("fs/sync.c", "kernel/workqueue.c", "net/core/dev.c", "mm/filemap.c"),
        project_steps=("实现 split 边界检查", "实现 shuffle manifest", "实现 task attempt 提交表", "实现 checkpoint 恢复", "写端到端故障注入矩阵"),
    ),
)


def insert_expansion(root: Path, chapter: ChapterPlan) -> bool:
    path = root / chapter.path
    text = path.read_text(encoding="utf-8")
    if MARKER_BEGIN in text:
        return False
    if chapter.anchor not in text:
        raise RuntimeError(f"anchor not found in {path}: {chapter.anchor}")
    expansion = render_expansion(chapter)
    text = text.replace(chapter.anchor, f"{expansion}\n{chapter.anchor}", 1)
    path.write_text(text, encoding="utf-8")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for chapter in CHAPTERS:
        if insert_expansion(root, chapter):
            changed += 1
            print(f"expanded {chapter.path}")
        else:
            print(f"already expanded {chapter.path}")
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
