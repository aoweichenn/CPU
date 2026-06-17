from __future__ import annotations

import textwrap
from pathlib import Path

from expand_volume2_600k import CHAPTERS, ChapterPlan, Mechanism


OLD_BLOCK_TITLE = "".join(("高密度主线", "补充：从失败推导机制"))
OLD_BLOCK_BEGIN = f"\\topic{{{OLD_BLOCK_TITLE}}}"
MODEL_CHAPTER = "chapters/part05-distributed-computing/ch16-distributed-system-model.tex"


CONCRETE = {
    "chapters/part01-hardware-foundations/ch01-computation-system-map.tex": {
        "incident": "一次日志统计作业跑到百分之八十后被杀掉，重启后结果比串行 reference 多出三百二十七条 error 事件。",
        "operator": "工程师先看总耗时，发现第二次运行更快，以为问题只在冷启动；继续查输出摘要，才发现吞吐提升和结果错误同时发生。",
        "naive": "第一版把读取、解析、聚合、写出和错误处理放在一个主循环里，解析失败时直接返回空 key，输出文件也直接覆盖。",
        "failure_a": "脏输入没有进入独立错误通道，优化版本和 reference 对同一行的解释不同，分片重跑后又把半成品当成有效输出。",
        "failure_b": "无界队列隐藏了下游写出变慢，内存峰值一路增长；进程被杀时，磁盘上既有旧输出，也有新 attempt 的临时片段。",
        "contract": "本章要把数据合同、控制面、数据面、阶段指标和提交边界全部摆清。后续章节每做一次优化，都必须能回到这条作业链路验证。",
        "diagram": (
            "input files -> split reader -> parser -> local aggregate\n"
            "           -> bounded queue -> writer -> temp output\n"
            "           -> manifest commit -> final report"
        ),
        "state": "LogJobState",
        "code": r"""
struct StageMetric {
    std::uint64_t records = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::uint64_t elapsed_nanoseconds = 0;
};

struct OutputManifestEntry {
    std::uint64_t job_id = 0;
    std::uint64_t split_id = 0;
    std::uint32_t attempt = 0;
    std::uint64_t output_bytes = 0;
    std::uint64_t output_checksum = 0;
};
""",
        "experiment": "固定一批包含合法行、缺字段行、超大数值行和重复行的输入，分别运行串行 reference、无界队列版本、有界队列版本和 manifest 提交版本。",
        "debug": "先比总记录数，再比错误分类，再比每个 key 的计数，最后检查 manifest 中每个 split 是否只有一个 committed attempt。",
    },
    "chapters/part01-hardware-foundations/ch02-core-pipeline-ooo-branch.tex": {
        "incident": "一个数组求和内核在单线程上只达到很低 IPC，而日志解析循环在随机脏数据输入下耗时翻倍。",
        "operator": "表面上两个函数都只有几行循环，profile 却显示一个被依赖链拖住，另一个在分支回滚和前端分派上浪费周期。",
        "naive": "第一版只写一个累加变量；解析器每读一个字符就进入 switch，再按合法、非法、分隔符等分支立即处理。",
        "failure_a": "单累加器形成跨迭代依赖，后一次加法必须等待前一次结果；乱序窗口里没有足够独立微操作。",
        "failure_b": "随机输入让分支预测失效，switch 目标分散，前端供给和分支恢复成本进入热路径。",
        "contract": "本章所有机制都要落到同一件事：源码循环只是语义，硬件真正执行的是依赖图、控制流、load/store 请求和微操作供给。",
        "diagram": (
            "source loop -> compiler transform -> micro ops\n"
            "            -> dependency chain / branches / loads\n"
            "            -> ports, window, predictor, front end"
        ),
        "state": "HotLoopReport",
        "code": r"""
std::uint64_t sum_four_accumulators(const std::uint64_t* values,
                                    std::uint64_t count) {
    std::uint64_t a0 = 0;
    std::uint64_t a1 = 0;
    std::uint64_t a2 = 0;
    std::uint64_t a3 = 0;
    std::uint64_t index = 0;
    for (; index + 3 < count; index += 4) {
        a0 += values[index + 0];
        a1 += values[index + 1];
        a2 += values[index + 2];
        a3 += values[index + 3];
    }
    for (; index < count; ++index) {
        a0 += values[index];
    }
    return (a0 + a1) + (a2 + a3);
}
""",
        "experiment": "同一数组分别跑单累加器、四累加器、八累加器；同一解析器分别喂全合法、全非法、交替和随机输入。",
        "debug": "先看 correctness checksum，再看 cycles、instructions、IPC、branches 和 branch misses，最后对照反汇编确认编译器是否已经自动展开。",
    },
    "chapters/part01-hardware-foundations/ch03-cache-tlb-virtual-memory.tex": {
        "incident": "两个复杂度同为 O(n) 的日志扫描版本，一个顺序扫列式数组，另一个追踪指针数组；后者在大输入下慢了数倍。",
        "operator": "起初团队只比较算法复杂度，后来用 stride 实验和 page fault 指标才看到 cache line、TLB 和 page cache 的边界。",
        "naive": "第一版把每条日志作为独立对象分配，热字段、冷 payload、错误字符串和调试信息都放在同一个结构体里。",
        "failure_a": "统计只需要 event type，却把整条记录的冷字段搬入缓存；对象分散还让 TLB 覆盖被大量页面消耗。",
        "failure_b": "mmap 第一次扫描触发缺页和 page cache 填充，第二次扫描完全不同；只报告一次运行时间会把 I/O 和内存混在一起。",
        "contract": "本章要把地址访问形状说清：数据总量、有效字节、cache line 利用率、活跃页数、冷热运行和写回边界都要进入报告。",
        "diagram": (
            "virtual address -> TLB lookup -> cache hierarchy\n"
            "                -> DRAM or page fault -> page cache\n"
            "                -> user loop observes latency"
        ),
        "state": "MemoryAccessShape",
        "code": r"""
struct LogHotColumns {
    std::vector<std::uint32_t> event_type;
    std::vector<std::uint64_t> timestamp;
    std::vector<std::uint32_t> user_bucket;
};

struct LogColdStorage {
    std::vector<std::uint64_t> payload_offset;
    std::vector<std::uint32_t> payload_size;
};
""",
        "experiment": "用同一批记录构造对象数组、指针数组、热字段列式布局和 mmap 文件扫描，分别记录冷热两轮。",
        "debug": "先确认输出一致，再画每条记录实际使用字段；随后改变 stride、对象大小和页面数量，观察性能拐点是否跟 cache 或 TLB 覆盖对应。",
    },
    "chapters/part01-hardware-foundations/ch04-numa-interconnect-coherence.tex": {
        "incident": "归约程序从一线程到四线程加速明显，继续加到十六线程后吞吐几乎不变，甚至在双 socket 机器上倒退。",
        "operator": "代码里只有一个 relaxed atomic 计数器和一个大数组，问题却不在加法，而在 cache line 所有权、互连和页面放置。",
        "naive": "第一版让所有 worker 对同一个全局计数器 \\code{fetch_add}，输入数组由主线程一次性初始化。",
        "failure_a": "全局计数器所在 cache line 在核心间来回迁移，每次写都要争夺独占所有权。",
        "failure_b": "主线程 first touch 把页面集中放在一个 NUMA 节点，其他节点 worker 做远端访问；线程迁移又让局部性更差。",
        "contract": "本章把共享写和远端内存当成一等成本。线程数、CPU 拓扑、页面初始化者、合并层级和队列位置都要可记录。",
        "diagram": (
            "worker local count -> NUMA node partial\n"
            "                   -> socket merge\n"
            "                   -> global final result"
        ),
        "state": "TopologyAwareReduce",
        "code": r"""
struct alignas(64) CounterShard {
    std::uint64_t value = 0;
};

struct NumaPartial {
    std::uint32_t numa_node = 0;
    std::vector<CounterShard> worker_counters;
};
""",
        "experiment": "比较全局 atomic、每线程 shard、每 NUMA node partial 和分层合并；再比较主线程初始化与 worker 分块初始化。",
        "debug": "先画线程到 CPU 的映射，再查每个 partial 的写入者；若机器没有 NUMA，就明确把远端内存实验标为未验证边界。",
    },
    "chapters/part02-single-node-hpc/ch05-measurement-roofline-counters.tex": {
        "incident": "一份 benchmark 报告声称优化提升百分之三十，但复跑后结果忽高忽低，换输入后提升消失。",
        "operator": "报告只给端到端耗时，没有说明是否包含分配、缺页、预热、校验、写出，也没有保留原始样本。",
        "naive": "第一版在 main 函数外层取两次时间，把初始化、热循环、输出和析构混成一个数字。",
        "failure_a": "优化改动真正影响的是初始化或 page cache，而不是核心内核；结果被冷启动和频率状态污染。",
        "failure_b": "只看耗时无法区分计算端口、内存带宽、分支、锁等待和队列等待，后续优化方向完全靠猜。",
        "contract": "本章训练可信测量：边界、样本、环境、指标、原始数据、反例和结论边界必须同时出现。",
        "diagram": (
            "hypothesis -> benchmark boundary -> raw samples\n"
            "           -> counters / trace -> report\n"
            "           -> conclusion with limits"
        ),
        "state": "BenchmarkRun",
        "code": r"""
struct BenchmarkSample {
    std::uint64_t input_bytes = 0;
    std::uint64_t records = 0;
    std::uint64_t elapsed_nanoseconds = 0;
    std::uint64_t cycles = 0;
    std::uint64_t instructions = 0;
    std::uint64_t cache_misses = 0;
};
""",
        "experiment": "同一内核分别报告冷运行、预热运行、核心循环和端到端；再给 sum、saxpy、histogram 做 Roofline 粗估。",
        "debug": "如果数字不稳定，先看样本分布和环境，再看是否计入 I/O 或分配；最后才讨论算法和代码。",
    },
    "chapters/part02-single-node-hpc/ch06-data-layout-locality-prefetch.tex": {
        "incident": "日志聚合只使用 event type，却把 timestamp、payload、error message 和调试字符串都拖进热循环。",
        "operator": "开发者看到 vector 就以为局部性已经很好，直到拆出热字段后才发现有效字节比例极低。",
        "naive": "第一版用 struct vector 表达完整业务对象，每条记录的生命周期、解析临时 token 和冷 payload 全部绑在一起。",
        "failure_a": "热循环每次搬入整条结构体，cache line 里大部分字节没有被使用。",
        "failure_b": "短生命周期 token 逐个 new/delete，分配器元数据、碎片和锁进入热路径；图邻接也因指针分散导致预取失败。",
        "contract": "本章的核心不是追求某种布局，而是让布局服从访问模式、生命周期和迁移计划。",
        "diagram": (
            "business record -> hot columns + cold storage\n"
            "                -> batch arena for temporary tokens\n"
            "                -> compact indexes for repeated traversal"
        ),
        "state": "LayoutPlan",
        "code": r"""
struct ParsedBatch {
    std::vector<std::uint32_t> event_type;
    std::vector<std::uint64_t> timestamp;
    std::vector<std::uint32_t> payload_index;
};

struct CsrGraph {
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> edges;
};
""",
        "experiment": "同一任务用 AoS、SoA、冷热分离、arena token 和 CSR 邻接分别实现，记录字节移动、分配次数和遍历时间。",
        "debug": "先问热路径到底读哪些字段，再看这些字段是否连续；若随机访问不可避免，再评估索引压缩和预取是否真能提前知道地址。",
    },
    "chapters/part02-single-node-hpc/ch07-simd-ilp-vectorization.tex": {
        "incident": "阈值过滤在标量版上正确，改成 SIMD 后某些长度漏掉尾部，浮点归约在不同机器上结果也不完全一致。",
        "operator": "问题不在指令是否足够高级，而在标量语义、元素位宽、掩码、尾部和数值边界没有先写清。",
        "naive": "第一版看到循环独立就直接尝试向量化，每个元素 if 后 push 的接口也没有改成批处理。",
        "failure_a": "长度不是向量宽度倍数时越界或漏算；输入输出可能别名时，编译器也无法证明自动向量化安全。",
        "failure_b": "branchless 和 mask 不是免费午餐，稀疏输出、随机条件和浮点水平归约都有额外成本或语义变化。",
        "contract": "本章要求每个向量化内核都有标量 reference、固定宽度类型、尾部测试、误差策略和 ISA 回退边界。",
        "diagram": (
            "scalar reference -> batch interface -> vector lanes\n"
            "                 -> mask / tail handling\n"
            "                 -> scalar fallback and tests"
        ),
        "state": "VectorKernelContract",
        "code": r"""
struct FilterResult {
    std::uint64_t written = 0;
    std::uint64_t inspected = 0;
};

FilterResult filter_greater_scalar(const std::int32_t* input,
                                   std::uint64_t count,
                                   std::int32_t threshold,
                                   std::int32_t* output) {
    FilterResult result;
    for (std::uint64_t index = 0; index < count; ++index) {
        const std::int32_t value = input[index];
        if (value > threshold) {
            output[result.written] = value;
            ++result.written;
        }
        ++result.inspected;
    }
    return result;
}
""",
        "experiment": "阈值过滤覆盖全命中、全不命中、稀疏随机和密集随机；字节分类覆盖不同批大小；浮点归约分别跑确定性和快速模式。",
        "debug": "先比标量 reference，再检查尾部长度集合；如果自动向量化失败，读 missed reason，而不是直接手写 intrinsics。",
    },
    "chapters/part02-single-node-hpc/ch08-compute-kernels-patterns.tex": {
        "incident": "日志管线里的每个内核单独测都正确，串起来后却在热点 key、filter 输出和 join 放大处出现长尾。",
        "operator": "团队起初把 histogram、top-k、join、scan、partition 当作独立算法，后来发现真正问题在组合边界和中间数据移动。",
        "naive": "第一版所有线程更新一个全局 map，filter 命中后直接 push 到共享 vector，join 每条记录查一次远端小表。",
        "failure_a": "热点桶、共享 vector 和小表随机查询把并行变成同步热点。",
        "failure_b": "每个阶段都材料化完整中间结果，cache 局部性丢失，shuffle 前的小消息数量也急剧增加。",
        "contract": "本章把典型计算内核当作管线部件来讲：每个部件都要说明输入形状、输出放大、冲突点和可合并边界。",
        "diagram": (
            "map -> local histogram / filter count\n"
            "    -> scan offsets -> partition blocks\n"
            "    -> local top-k / join -> reduce"
        ),
        "state": "KernelPipelinePlan",
        "code": r"""
struct PartitionStat {
    std::uint32_t partition_id = 0;
    std::uint64_t records = 0;
    std::uint64_t bytes = 0;
    std::uint64_t distinct_keys = 0;
};

struct LocalHistogram {
    std::vector<std::uint64_t> counts;
};
""",
        "experiment": "分别构造均匀 key、Zipf 热点 key、高命中 filter、低命中 filter、小表 join 和输出爆炸 join。",
        "debug": "先查每阶段输入输出记录数，再查共享写位置；如果 reduce 长尾，先看 partition 分布，而不是先改线程池。",
    },
    "chapters/part03-concurrency-synchronization/ch09-threads-scheduling-affinity.tex": {
        "incident": "把一千个小 split 直接变成一千个线程后，程序在手机 Termux 上比单线程还慢。",
        "operator": "CPU 没有满载在有效工作上，而是在创建线程、上下文切换、迁移和等待 I/O。",
        "naive": "第一版把任务和线程画等号，遇到一个 split 就创建一个 std::thread，I/O 等待和 CPU 计算也放在同一个池里。",
        "failure_a": "线程数超过可运行资源，run queue 变长，context switch 和 cache/TLB 局部性损失吞掉收益。",
        "failure_b": "阻塞 I/O 占住计算 worker，大小核和频率变化让扩展曲线不再单调。",
        "contract": "本章把任务、线程、worker、阻塞和亲和性分开。一个任务是业务工作单元，一个线程只是执行资源。",
        "diagram": (
            "split tasks -> bounded submit queue -> fixed workers\n"
            "            -> compute pool / io path\n"
            "            -> trace: submit, start, wait, finish"
        ),
        "state": "WorkerPoolTrace",
        "code": r"""
struct TaskTrace {
    std::uint64_t task_id = 0;
    std::uint32_t worker_id = 0;
    std::uint64_t submit_ns = 0;
    std::uint64_t start_ns = 0;
    std::uint64_t finish_ns = 0;
    std::uint32_t cpu_id = 0;
};
""",
        "experiment": "从一线程扫到硬件线程数以上，分别跑短任务、长任务、混合 I/O 任务，并记录上下文切换和任务等待。",
        "debug": "如果线程越多越慢，先看任务粒度和阻塞比例，再看调度迁移、大小核和是否把线程创建计入热路径。",
    },
    "chapters/part03-concurrency-synchronization/ch10-locks-condition-semaphores.tex": {
        "incident": "有界队列在关闭时偶发卡死，另一个计数表版本虽然加了锁，却在线程数增加后几乎不扩展。",
        "operator": "锁 API 本身没错，错在没有写清锁保护的不变量、条件变量等待的谓词和多锁组合的顺序。",
        "naive": "第一版哪个函数会访问共享变量就在哪个函数加锁；关闭时设置 closed 标志，但没有唤醒所有等待者。",
        "failure_a": "消费者在空队列上等待，生产者在满队列上等待，close 只改标志不通知，线程可能永远睡眠。",
        "failure_b": "分片转移时两个队列按调用顺序上锁，不同线程形成等待环；全局 map 锁还把所有 key 更新串行化。",
        "contract": "本章要求每把锁都能回答：保护哪个不变量，等待哪个谓词，谁负责通知，异常时状态如何恢复。",
        "diagram": (
            "mutex protects: buffer, head, tail, size, closed\n"
            "not_empty waits: size > 0 or closed\n"
            "not_full waits: size < capacity or closed"
        ),
        "state": "BoundedQueueInvariant",
        "code": r"""
template <typename T>
struct BoundedQueueState {
    std::vector<T> buffer;
    std::uint64_t head = 0;
    std::uint64_t tail = 0;
    std::uint64_t size = 0;
    bool closed = false;
};
""",
        "experiment": "容量设为一，同时启动多个生产者和消费者；在空、满、半满、等待中分别调用 close。",
        "debug": "遇到卡死先画等待图，标出每个线程持有什么锁、等待什么谓词；不要用 sleep 或超时掩盖丢唤醒。",
    },
    "chapters/part03-concurrency-synchronization/ch11-atomics-memory-order.tex": {
        "incident": "一个线程发布配置指针后，另一个线程偶尔读到半初始化字段；任务领取逻辑也偶发被两个 worker 同时执行。",
        "operator": "开发者把 atomic 当成更快的 mutex，但没有写出 release/acquire 关系，也没有把任务状态做成单一状态机。",
        "naive": "第一版用普通 bool 或 relaxed atomic 表示 ready；领取任务时先 load 状态，再普通 store 为 running。",
        "failure_a": "ready 被看见不代表配置对象的普通字段已经通过 happens-before 发布给读线程。",
        "failure_b": "两个 worker 都看到 pending，再分别写 running；复合状态更新丢失，CAS 失败路径也没有统计。",
        "contract": "本章不背内存序枚举，而是把每个 atomic 字段写成同步合同：谁写、谁读、保护哪些数据、对象何时可回收。",
        "diagram": (
            "producer writes payload\n"
            "producer release-stores pointer/state\n"
            "consumer acquire-loads pointer/state\n"
            "consumer reads payload after synchronization"
        ),
        "state": "AtomicStateMachine",
        "code": r"""
enum class TaskState : std::uint32_t {
    Pending,
    Running,
    Done,
    Failed,
};

struct AtomicTask {
    std::atomic<TaskState> state = TaskState::Pending;
    std::uint64_t task_id = 0;
};
""",
        "experiment": "构造配置发布、CAS 领取任务和热点计数三个最小程序，分别比较普通变量、relaxed、release/acquire 和 CAS。",
        "debug": "先找是否存在普通数据竞争，再写 happens-before 图；若只是计数统计，可用 relaxed，但不能把它当成发布信号。",
    },
    "chapters/part03-concurrency-synchronization/ch12-lock-free-data-structures.tex": {
        "incident": "把锁队列换成无锁队列后，压力测试偶尔丢节点，打开 ASan 又出现 use after free。",
        "operator": "吞吐数字看起来更漂亮，但线性化点、ABA、内存回收和关闭语义没有被证明。",
        "naive": "第一版把 ring buffer 的 head 和 tail 都改成 atomic，或者在链式栈 pop 成功后直接 delete 节点。",
        "failure_a": "SPSC 的单写者假设被多生产者破坏，满空和 wrap around 状态无法区分。",
        "failure_b": "线程 A 拿到旧指针还没读完，线程 B 已经 pop、delete 并复用同一地址，CAS 只比较地址就误判成功。",
        "contract": "本章把无锁当成需要证明的并发协议，不当成优化标签。先讲专用 SPSC，再说明 MPMC 和回收为什么难。",
        "diagram": (
            "producer owns tail -> writes slot -> release publish\n"
            "consumer owns head -> acquire observe -> reads slot\n"
            "capacity keeps one state distinguishable"
        ),
        "state": "SpscRingContract",
        "code": r"""
template <typename T>
struct SpscRingIndex {
    std::atomic<std::uint64_t> head = 0;
    std::atomic<std::uint64_t> tail = 0;
    std::uint64_t capacity = 0;
    std::vector<T> slots;
};
""",
        "experiment": "SPSC 覆盖空、满、wrap、生产者提前退出、消费者提前退出；无锁栈专门构造地址复用和慢读线程。",
        "debug": "先标出每个操作的线性化点，再检查节点生命周期；如果端到端瓶颈不在队列，复杂无锁不进入主路径。",
    },
    "chapters/part04-parallel-algorithms-runtime/ch13-reduce-scan-partition.tex": {
        "incident": "并行计数在小输入上正确，换成热点 key 后 reduce 长尾明显；稳定过滤多次运行输出顺序还不一致。",
        "operator": "问题不是缺少线程，而是没有先证明结合律、输出位置和分区语义。",
        "naive": "第一版让所有线程更新全局 map，filter 命中后 push 到共享 vector，shuffle 时直接按 hash 写小文件。",
        "failure_a": "全局 map 锁和热点 key 把归约串行化；浮点或顺序敏感合并还会因合并顺序变化改变结果。",
        "failure_b": "共享 vector push 无法稳定保持输入顺序，partition 倾斜让某个 reduce 等待所有人。",
        "contract": "本章先证明语义，再谈并行。reduce 要有 identity 和结合性，filter 要用 scan 分配位置，partition 要记录倾斜和版本。",
        "diagram": (
            "chunk local result -> tree reduce\n"
            "predicate count -> prefix scan -> stable write\n"
            "key hash -> partition stats -> shuffle blocks"
        ),
        "state": "ParallelAlgorithmContract",
        "code": r"""
struct ChunkSummary {
    std::uint64_t chunk_id = 0;
    std::uint64_t records = 0;
    std::uint64_t matched = 0;
    std::uint64_t output_offset = 0;
};
""",
        "experiment": "reduce 用多种切块和合并括号验证结果；filter 覆盖全命中、零命中和随机命中；partition 用均匀与热点分布对照。",
        "debug": "结果不稳定时先问合并函数是否可结合；输出错位时先看 scan 偏移；长尾时先看每个 partition 的记录和字节。",
    },
    "chapters/part04-parallel-algorithms-runtime/ch14-task-runtime-work-stealing.tex": {
        "incident": "固定线程池处理普通 map 阶段还可以，遇到热点 split 后某个 worker 拖到最后，其他 worker 长时间空闲。",
        "operator": "全局 FIFO 队列无法表达依赖、任务本地性、取消和长尾拆分；递归式任务创建还可能把栈和任务数打爆。",
        "naive": "第一版把所有任务塞进一个队列，reduce 任务靠排队顺序等待 map，失败时只给当前任务写 failed。",
        "failure_a": "固定分配遇到倾斜 split 时没有补偿机制，空闲 worker 只能等待；嵌套 future 等待还可能造成 worker 内死等。",
        "failure_b": "上游 map 失败后，下游 shuffle/reduce 已经排队甚至运行，继续消耗资源并可能提交无效结果。",
        "contract": "本章把运行时看成控制面：任务图、ready 队列、本地队列、窃取、取消和异常收束都要有状态。",
        "diagram": (
            "task graph -> dependency count -> ready queue\n"
            "           -> local deque / steal path\n"
            "           -> completion, cancel, exception summary"
        ),
        "state": "TaskGraphRuntime",
        "code": r"""
enum class RuntimeTaskState : std::uint32_t {
    Waiting,
    Ready,
    Running,
    Completed,
    Cancelled,
    Failed,
};

struct TaskNode {
    std::uint64_t task_id = 0;
    std::atomic<std::uint32_t> unmet_dependencies = 0;
    RuntimeTaskState state = RuntimeTaskState::Waiting;
};
""",
        "experiment": "构造均匀任务、热点长任务、上游失败和递归拆分四组输入，比较固定分配、全局队列和工作窃取。",
        "debug": "先看每个 worker 的忙闲时间，再看 steal 次数和任务粒度；若出现挂起，检查 worker 是否在等待需要自己池子执行的 future。",
    },
    "chapters/part04-parallel-algorithms-runtime/ch15-io-async-backpressure.tex": {
        "incident": "reduce 生成输出比磁盘写得快，CPU 看起来很空，内存却持续上涨，最终 checkpoint 还留下半写文件。",
        "operator": "计算线程、I/O 等待、完成事件和持久化提交被混在一起，系统没有把下游容量反馈给上游。",
        "naive": "第一版计算线程直接 write；慢了以后改成后台线程无界缓存；checkpoint 直接覆盖旧文件。",
        "failure_a": "无界写队列让内存承担所有下游慢速，超时后底层请求仍可能完成，buffer 生命周期不清。",
        "failure_b": "write 返回不等于可恢复提交，进程崩溃可能留下 page cache 中未持久化的数据或半截 checkpoint。",
        "contract": "本章把 I/O 视为状态机：提交、排队、完成、取消、错误分类、fsync、rename 和 manifest 都要明确。",
        "diagram": (
            "compute block -> bounded write queue\n"
            "              -> submit io -> completion event\n"
            "              -> fsync temp -> rename -> manifest"
        ),
        "state": "IoBackpressureState",
        "code": r"""
struct WriteRequest {
    std::uint64_t block_id = 0;
    std::uint64_t bytes = 0;
    std::uint64_t checksum = 0;
    std::uint64_t deadline_ns = 0;
};

struct WriteCompletion {
    std::uint64_t block_id = 0;
    std::int32_t error_code = 0;
    std::uint64_t completed_bytes = 0;
};
""",
        "experiment": "用慢写入模拟器控制写带宽，扫描不同队列容量和 batch 大小；checkpoint 在 open、write、fsync、rename、manifest 各点杀进程。",
        "debug": "内存上涨时先看写队列水位和生产者等待；恢复错误时检查是否直接读了临时文件而没有 manifest 校验。",
    },
    "chapters/part05-distributed-computing/ch17-partition-replication-consensus.tex": {
        "incident": "一个热点 event type 占了八成流量，普通 hash partition 让单个 reduce 节点拖尾；迁移 partition 后旧请求还写到了旧 owner。",
        "operator": "问题不是哈希函数不够高级，而是没有把路由版本、分片状态机、复制提交点和旧 leader fencing 写进协议。",
        "naive": "第一版 key 直接 hash 到 partition，客户端缓存路由表，复制时 leader 收到请求就返回成功。",
        "failure_a": "热点 key 必然落到同一分片，哈希无法自动拆开同一个 key；再分片期间旧路由请求仍能到达旧 owner。",
        "failure_b": "leader 崩溃或旧 leader 复活时，已经向客户端确认的写可能没有达到承诺的副本集合。",
        "contract": "本章讲的是控制面状态：route epoch、partition owner、迁移阶段、quorum 提交点和 fencing token。",
        "diagram": (
            "route epoch -> partition owner\n"
            "            -> prepare / copy / catch-up / switch / cleanup\n"
            "            -> quorum commit and follower catch-up"
        ),
        "state": "PartitionControlState",
        "code": r"""
struct RouteEntry {
    std::uint64_t epoch = 0;
    std::uint32_t partition_id = 0;
    std::uint32_t owner_worker = 0;
    std::uint32_t replica_count = 0;
};

struct ReplicaAck {
    std::uint64_t log_index = 0;
    std::uint32_t replica_id = 0;
    std::uint64_t term = 0;
};
""",
        "experiment": "构造热点 key、迁移期间旧请求、三副本中一个慢副本和旧 leader 延迟响应四类故障。",
        "debug": "先看请求携带的 epoch 是否过期，再看提交点到底是本地写、WAL、还是多数 ack；不要把所有数据都塞进共识日志。",
    },
    "chapters/part05-distributed-computing/ch18-distributed-compute-engineering.tex": {
        "incident": "端到端分布式日志引擎正常路径能跑通，但 driver 重启后重复读取旧 attempt 的 shuffle block，最终结果多计。",
        "operator": "演示代码只实现了 happy path：map 写临时文件，reduce 扫目录，driver 内存里记状态；一旦重启或 block 损坏，边界全部消失。",
        "naive": "第一版按字节切 split，map 输出文件直接放在临时目录，reduce 扫所有文件，driver 重启后相信目录现状。",
        "failure_a": "split 切在记录中间导致重复或漏读；旧 attempt 的半写 block 和新 attempt 的完整 block 同时存在。",
        "failure_b": "checkpoint 损坏或 driver 崩溃后，running 任务没有回到 pending，也没有用 manifest 校验已提交结果。",
        "contract": "本章是全书收束：split 边界、map side combine、shuffle manifest、attempt 表、checkpoint、故障注入和 runbook 必须串成一个系统。",
        "diagram": (
            "split -> map attempt -> local combine\n"
            "      -> shuffle block + checksum -> manifest commit\n"
            "      -> reduce attempt -> final manifest -> checkpoint"
        ),
        "state": "DistributedEngineState",
        "code": r"""
struct ShuffleBlockManifest {
    std::uint64_t job_id = 0;
    std::uint64_t map_task_id = 0;
    std::uint32_t attempt = 0;
    std::uint32_t partition_id = 0;
    std::uint64_t bytes = 0;
    std::uint64_t checksum = 0;
};
""",
        "experiment": "端到端跑正常输入、脏输入、热点 key、慢 reduce、driver 重启、block 截断、响应重复和 checkpoint 损坏。",
        "debug": "先从 final manifest 反推每个 reduce 输入，再查每个 shuffle block 的 map attempt；若状态来自内存而非 checkpoint，重启路径必须重建或重新验证。",
    },
}


def clean(text: str) -> str:
    return textwrap.dedent(text).strip()


def paragraph(text: str) -> str:
    return clean(text)


def listing(body: str, *, language: str | None = None, caption: str | None = None) -> str:
    options: list[str] = []
    if language:
        options.append(f"language={language}")
    else:
        options.append("numbers=none")
    if caption:
        options.append(f"caption={{{caption}}}")
    return "\\begin{lstlisting}[" + ",".join(options) + "]\n" + clean(body) + "\n\\end{lstlisting}"


def topic(title: str, *parts: str) -> str:
    body = "\n\n".join(part for part in parts if part.strip())
    return f"\\topic{{{title}}}\n\n{body}"


def render_problem_flow(chapter: ChapterPlan, info: dict[str, str]) -> str:
    return topic(
        "从一次具体事故开始",
        paragraph(
            f"{info['incident']}这不是抽象练习，而是后续所有机制的入口。"
            f"{info['operator']}如果这一层只写成术语列表，读者会知道很多名词，却不知道面对这类现象时先固定什么、再观察什么、最后改什么。"
        ),
        paragraph(
            f"本章把运行问题固定为{chapter.running_problem}。这件事的价值是让所有推导都落在同一条数据流上：输入如何进入系统，状态在哪里改变，输出何时对外可见，失败发生后哪些东西还能相信。"
            f"主失败可以压缩成一句话：{chapter.main_failure}。后面的每个机制都要回到这个失败，而不是各讲各的。"
        ),
        listing(info["diagram"]),
        paragraph(
            "阅读本章时要先画这条链路，而不是先背概念。链路中每个箭头都表示一次边界跨越：从文件到内存，从线程到队列，从局部状态到共享状态，从临时结果到提交结果。边界越多，隐含假设越多；隐含假设越多，优化时越容易把正确性、性能和恢复搅在一起。"
        ),
        paragraph(
            f"本章的目标不是给出一个万能框架，而是训练一种判断顺序：先写 reference，再暴露朴素版本，再沿着失败路径引入机制，最后用实验和审查表确定结论边界。"
            f"如果一个优化不能说明它改变了哪条边界，就先不要把它合进贯穿项目。"
        ),
    )


def render_naive(chapter: ChapterPlan, info: dict[str, str]) -> str:
    return topic(
        "朴素方案为什么会被写出来",
        paragraph(
            f"{info['naive']}这个写法并不是一开始就荒谬。它符合小程序直觉：对象少、状态近、调用栈清楚、失败罕见，所有问题都能在一个调试会话里看见。"
            "经典教材写到这里不能直接嘲笑朴素方案，因为读者需要先理解它在哪些条件下成立。"
        ),
        paragraph(
            "朴素方案的真正问题，是它把多个边界藏了起来。控制状态和业务数据混在一起，临时结果和提交结果混在一起，测量边界和语义边界混在一起，线程资源和任务数量混在一起。代码短只是表面现象；代价是没有地方记录不变量，也没有地方插入观测点。"
        ),
        paragraph(
            f"把这个方案放大到{chapter.running_problem}，第一个变化是输入规模让偶发路径变成常态，第二个变化是并发让顺序假设失效，第三个变化是系统资源让等待和数据移动变成主成本。"
            "所以改进不是在原函数里继续加分支，而是把边界显式化。"
        ),
        paragraph(
            "教材里的朴素版本必须保留。它一方面作为 correctness oracle 的对照，另一方面作为解释机制的反面样本。没有朴素版本，最终设计就像凭空出现；没有反例，最终设计又会被误解成永远正确。"
        ),
    )


def render_failure_paths(info: dict[str, str]) -> str:
    return "\n\n".join(
        (
            topic(
                "失败路径一：结果错误不是性能问题",
                paragraph(
                    f"{info['failure_a']}这类问题通常不会稳定复现。小输入没有触发，单线程没有触发，甚至同一批输入换一个调度顺序才触发。"
                    "因此第一反应不能是调线程数，也不能是继续优化热循环，而是先把语义锚点拿出来。"
                ),
                paragraph(
                    "排查时按四步走。第一步固定输入和随机种子；第二步运行串行 reference，保存可比较摘要；第三步让优化版本输出同样的摘要；第四步定位第一个不同的边界。摘要不能只写总数，还要包含错误分类、分片身份、attempt 身份、输出 checksum 和阶段状态。"
                ),
                listing(
                    "t0 run reference and save checksum\n"
                    "t1 run naive implementation on the same input\n"
                    "t2 compare record count, error count, key count, output checksum\n"
                    "t3 stop at the first boundary that diverges"
                ),
                paragraph(
                    "这里的关键是“第一个不同的边界”。如果 reference 与优化版本在解析阶段已经不同，就不要继续讨论 cache、SIMD、线程池或分布式提交。系统分析要避免把下游症状当成根因。"
                ),
            ),
            topic(
                "失败路径二：吞吐提升可能掩盖资源失控",
                paragraph(
                    f"{info['failure_b']}这类失败常常伴随一个迷惑性信号：平均吞吐看起来变好，尾延迟、内存峰值、恢复时间或输出可信度却变坏。"
                    "高质量教材必须把这类反例写进正文，否则读者会误以为所有优化只要更快就是更好。"
                ),
                paragraph(
                    "资源失控要按队列、内存、文件、线程和网络五类检查。队列看水位和等待，内存看峰值和分配次数，文件看临时产物和持久化点，线程看 runnable 与 blocked，网络或消息看 in-flight 与重试。单个总耗时无法解释这些状态。"
                ),
                listing(
                    "observe:\n"
                    "  queue_depth, producer_wait_ns, consumer_wait_ns\n"
                    "  resident_bytes, allocations, temporary_files\n"
                    "  runnable_threads, blocked_threads, retry_count\n"
                    "  committed_outputs, stale_outputs, checksum_errors"
                ),
                paragraph(
                    "如果某个指标没有采集，就在报告里明确写成未验证风险。不要把没有观测到的路径写成不存在，也不要用当前机器的一次稳定运行去替代边界证明。"
                ),
            ),
        )
    )


def render_contract(info: dict[str, str]) -> str:
    return topic(
        "把机制落成状态和接口",
        paragraph(
            f"{info['contract']}状态和接口是机制进入工程的地方。概念如果不能落成字段、状态、函数边界、错误码或实验指标，就很难在代码审查里被检查。"
        ),
        listing(info["code"], language="C++", caption=f"{info['state']} 的最小教学结构"),
        paragraph(
            "这段结构不是要替代真实项目代码，而是把本章最容易被忽略的边界写出来。字段名应回答一个具体问题：身份是什么，状态属于谁，哪次 attempt 有效，哪些字节已经被处理，哪个 checksum 能证明输出没有被误读。"
        ),
        paragraph(
            "接口设计还有一个原则：控制面字段不要被热路径随手修改，数据面批量结构不要夹带恢复协议。前者让状态机可审查，后者让硬件看到规则的数据流。若两者混在一起，代码会同时难以优化和难以恢复。"
        ),
    )


def render_mechanism(chapter: ChapterPlan, mechanism: Mechanism, index: int) -> str:
    lead = (
        f"先看一个具体问题：{mechanism.question}。"
        f"在{chapter.running_problem}中，这个问题会沿着输入、执行、同步、I/O 或提交边界继续传播，不会停留在一个局部函数里。"
    )
    naive = (
        f"朴素写法通常是：{mechanism.naive}。它吸引人的地方是短、直观、容易在小样本上通过测试；危险也在这里，"
        "因为小样本会替它隐藏资源、调度、数据分布和失败路径。"
    )
    failure = (
        f"放大以后会出现的信号是：{mechanism.failure}。这个信号未必是崩溃，也可能是吞吐平台期、p99 抖动、结果偶尔变化、"
        "队列持续变长、重启后状态不一致，或者某个分片长期拖尾。"
    )
    model = (
        f"此时引入{mechanism.name}，不是为了增加术语，而是为了建立一个可检查模型：{mechanism.model}。"
        "模型必须同时解释正确性和成本。只解释为什么快，不解释为什么可信，不能进入贯穿项目；只解释为什么正确，却完全无视缓存、调度、I/O 或网络，也不符合第二册目标。"
    )
    invariant = (
        f"把模型写成不变量时，要问四个问题：哪些状态只能由一个拥有者修改，哪些结果允许重试，哪些数据可以批量搬运，哪些错误必须外显。"
        f"围绕{mechanism.name}的代码审查，就从这四个问题开始，而不是从实现看起来是否高级开始。"
    )
    observe = (
        f"观察方式应围绕假设设计：{mechanism.observe}。实验要固定输入和环境，保留 reference，一次只改变一个变量。"
        "如果某项工具在当前手机或 Linux 环境没有权限，就写出预期命令、缺失事件和结论限制。"
    )
    boundary = (
        f"最后要写边界：{mechanism.boundary}。边界不是附录里的保守声明，而是正文的一部分。"
        "读者应该知道什么时候使用它，什么时候退回简单方案，什么时候需要换一个尺度重新测。"
    )
    review = (
        "本节验收可以用一句话判断：能否从朴素版本推导到改进版本，并能说清新增字段、新增状态或新增实验到底保护了什么。"
        "如果只能给最终代码，却解释不了这条推导链，说明机制还没有真正学会。"
    )
    title = f"机制推导：{mechanism.name}" if index < 4 else f"工程边界：{mechanism.name}"
    return topic(title, lead, naive, failure, model, invariant, observe, boundary, review)


def render_cases(chapter: ChapterPlan) -> str:
    parts = [
        "\\topic{案例推导：先有问题，再有答案}",
        "",
        paragraph(
            "本章案例不直接给最终实现。每个案例都按同一条教学链路展开：先描述输入和失败，再写朴素版本为什么自然，接着指出第一处证据，最后才引入改进。这样读者能学会推导，而不是只记住答案。"
        ),
    ]
    for case in chapter.cases:
        parts.extend(
            [
                "",
                f"\\topic{{案例：{case.name}}}",
                "",
                paragraph(
                    f"场景是：{case.setup}。先把它缩到可以手工检查的规模，再扩到能触发真实性能或恢复问题的规模。"
                    "小规模负责证明语义，大规模负责暴露成本，两者缺一不可。"
                ),
                paragraph(
                    f"朴素版本是：{case.first_try}。它必须保留在仓库里，名字可以直接标成 baseline 或 naive。"
                    "保留它不是为了上线，而是为了让反例可复现。"
                ),
                paragraph(
                    f"改进版本是：{case.improve}。改进说明要写清楚它改变了哪条路径：少搬了哪些字节，少争了哪条 cache line，"
                    "少等了哪个队列，减少了哪些重复请求，或者把哪个半提交状态变成了可恢复状态。"
                ),
                paragraph(
                    f"验收重点是：{case.verify}。报告里至少有一组反例。没有反例的案例很容易变成宣传文字，读者也无法判断机制的边界。"
                ),
            ]
        )
    return "\n".join(parts)


def render_experiments(chapter: ChapterPlan, info: dict[str, str]) -> str:
    mechanism_names = "、".join(item.name for item in chapter.mechanisms[:4])
    return topic(
        "实验矩阵：让结论可复现",
        paragraph(
            f"{info['experiment']}实验要把正常输入、边界输入、压力输入和错误输入分开。正常输入证明功能还在，边界输入证明不变量，压力输入暴露成本，错误输入验证恢复。"
        ),
        paragraph(
            f"围绕{mechanism_names}，不要把所有变量一次改完。比如改变布局时固定线程数，改变线程数时固定输入分布，改变批大小时固定写入设备状态，改变重试策略时固定故障注入脚本。变量克制是性能报告可信的前提。"
        ),
        listing(
            "experiment matrix:\n"
            "  input: normal / boundary / pressure / faulty\n"
            "  version: reference / naive / improved / counterexample\n"
            "  metrics: correctness / throughput / latency / resource / recovery\n"
            "  evidence: command / environment / raw sample / conclusion limit"
        ),
        paragraph(
            "报告中的数字要能追溯到命令。只贴一张结果表不够；还要写 CPU 或设备环境、编译选项、输入生成方式、是否预热、是否绑定线程、是否清理 page cache、是否启用故障注入。做不到的项目要写成未验证风险。"
        ),
        paragraph(
            "如果改进版本更慢，也不是失败。负结果常常比正结果更能训练判断力。它可能说明瓶颈不在这里，输入规模太小，机制成本超过收益，或者朴素方案在当前边界下已经足够。工程能力包括知道什么时候不改。"
        ),
    )


def render_linux(chapter: ChapterPlan) -> str:
    if not chapter.linux_paths:
        return ""
    paths = "、".join(f"\\filepath{{{path}}}" for path in chapter.linux_paths)
    return topic(
        "Linux 源码阅读：从用户态现象倒推对象",
        paragraph(
            f"本章 Linux 阅读入口保持窄范围：{paths}。阅读目的不是把内核子系统背下来，而是把一个用户态现象映射到内核对象和状态变化上。"
        ),
        paragraph(
            "阅读笔记建议固定四列：用户态操作、内核对象、状态变化、可观察指标。用户态操作可以是等待、缺页、写文件、调度、计时或消息收发；内核对象可以是 task、VMA、page、inode、wait queue、bio、socket buffer 或计数器。"
        ),
        paragraph(
            "源码里的数据结构要比函数名更重要。链表、红黑树、位图、引用计数、状态枚举、等待队列、页表和缓存结构，决定了机制如何落地。读者不需要一次读完整个内核，但要能解释一个最小现象的因果链。"
        ),
        paragraph(
            "如果当前 Termux 或设备内核无法直接对应源码路径，也要写清版本边界。源码阅读提供机制参考，运行时证据仍来自当前机器上的实验。把二者混为一谈，会让报告看起来权威，实际不可复现。"
        ),
    )


def render_debug(info: dict[str, str]) -> str:
    return topic(
        "排查演练：把事故走完一遍",
        paragraph(f"排查从具体差异开始：{info['debug']}这一步要慢，不要跳。越早跳到熟悉的工具，越容易把系统带到错误方向。"),
        paragraph(
            "排查记录应保留时间线。第一行写事故输入和版本，第二行写 reference 摘要，第三行写朴素版本摘要，第四行写第一次分歧，后面才写指标和修复。时间线能防止复盘变成事后故事。"
        ),
        listing(
            "debug timeline:\n"
            "  1. freeze input and version\n"
            "  2. run reference and save digest\n"
            "  3. run naive or optimized version\n"
            "  4. compare first divergent boundary\n"
            "  5. inspect metrics for that boundary\n"
            "  6. patch invariant and rerun matrix"
        ),
        paragraph(
            "修复时只改一个边界。若同时改数据布局、线程数、队列容量、批大小和提交协议，即使结果变好也无法解释为什么。高质量教材的案例要能被读者复做，而不是只让读者相信作者经验。"
        ),
    )


def render_project(chapter: ChapterPlan) -> str:
    steps = "\n".join(f"  {index + 1}. {step}" for index, step in enumerate(chapter.project_steps))
    return topic(
        "项目验收边界",
        paragraph(
            "贯穿项目不是把每章代码堆到一起，而是让每章新增一个可证明的能力。能力必须有 reference、朴素实现、改进实现、实验脚本、指标输出和失败注入中的至少几项。"
        ),
        listing("project checkpoints:\n" + steps),
        paragraph(
            "每个检查点都要进入仓库。只在正文里说“应该实现”不够；读者需要看到代码边界、测试边界和报告边界。若某个检查点受当前设备限制，提交降级说明，而不是把它从质量标准里删除。"
        ),
        paragraph(
            "项目报告最后写三段：本章新增能力解决了什么失败；新增能力引入了什么成本；哪些结论能迁移，哪些结论必须在新机器、新输入或新故障条件下重测。这个复盘会把整本书连成一条主线。"
        ),
    )


def render_quality(chapter: ChapterPlan) -> str:
    return topic(
        "本章质量标准",
        paragraph(
            f"读完本章，读者应该能围绕{chapter.running_problem}讲清完整推导：朴素方案为什么成立，哪条失败路径先出现，引入的机制保护了哪个不变量，实验如何证明或否定假设。"
        ),
        paragraph(
            "如果正文只列概念，没有具体输入、状态、时间线和反例，就不合格；如果只给最终代码，没有朴素版本和失败证据，也不合格；如果只给 benchmark 数字，没有语义 reference 和结论边界，同样不合格。"
        ),
        paragraph(
            "本章最终留下的是判断力：面对一个慢、错、卡死、重复提交或无法恢复的系统，先固定问题，再画边界，再写 reference，再做对照，再改机制。这个顺序比任何单个技巧更重要。"
        ),
    )


def render_replacement(chapter: ChapterPlan) -> str:
    info = CONCRETE[chapter.path]
    sections = [
        render_problem_flow(chapter, info),
        render_naive(chapter, info),
        render_failure_paths(info),
        render_contract(info),
    ]
    for index, mechanism in enumerate(chapter.mechanisms):
        sections.append(render_mechanism(chapter, mechanism, index))
    sections.extend(
        [
            render_cases(chapter),
            render_experiments(chapter, info),
            render_linux(chapter),
            render_debug(info),
            render_project(chapter),
            render_quality(chapter),
        ]
    )
    return "\n\n".join(section for section in sections if section.strip()) + "\n\n"


def find_next_section(text: str, start: int) -> int:
    next_section = text.find("\n\\section{", start + len(OLD_BLOCK_BEGIN))
    if next_section == -1:
        raise RuntimeError("could not find next section after old block")
    return next_section + 1


def rewrite_chapter(root: Path, chapter: ChapterPlan) -> bool:
    if chapter.path == MODEL_CHAPTER:
        print(f"skip model chapter {chapter.path}")
        return False
    path = root / chapter.path
    text = path.read_text(encoding="utf-8")
    start = text.find(OLD_BLOCK_BEGIN)
    if start == -1:
        print(f"no old block in {chapter.path}")
        return False
    end = find_next_section(text, start)
    replacement = render_replacement(chapter)
    path.write_text(text[:start] + replacement + text[end:], encoding="utf-8")
    print(f"rewrote {chapter.path}")
    return True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    missing = sorted(set(CONCRETE) - {chapter.path for chapter in CHAPTERS})
    if missing:
        raise RuntimeError(f"concrete data without chapter: {missing}")
    changed = 0
    for chapter in CHAPTERS:
        if chapter.path == MODEL_CHAPTER:
            continue
        if chapter.path not in CONCRETE:
            raise RuntimeError(f"missing concrete data for {chapter.path}")
        if rewrite_chapter(root, chapter):
            changed += 1
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
