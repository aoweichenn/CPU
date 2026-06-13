# x86-64, CPU, C++ Performance, HPC and AI Operator Master Plan

本文档是一套长期学习和实验计划，目标是把 x86-64 汇编、现代 CPU 微架构、Linux OS、C++ 编译优化、性能分析、HPC kernel、AI CPU 算子优化连成一条完整训练路线。

如果基础还不够稳，或者需要更细的每日/每周执行方式，先读配套详细手册：

- `docs/x86_64_hpc_ai_16_week_bootcamp.md`
- `docs/x86_64_hpc_ai_detailed_curriculum.md`

设计原则：

1. 每个主题必须落到可运行代码、汇编、性能数据和实验报告。
2. 每个 lab 都必须有 correctness baseline、performance baseline、优化版本和失败案例分析。
3. 所有性能结论必须能用至少两类证据支撑：源码/汇编/静态模型/运行时间/PMU/库对比。
4. 优先训练方法论：如何提出性能假设，如何测量，如何解释，如何迭代。
5. 不追求背诵指令表，追求能在真实工程里判断瓶颈、写出 kernel、解释差距。

## 参考课程风格

本计划借鉴公开课程的组织方式，但不复制它们的作业内容：

- MIT 6.172 Performance Engineering of Software Systems：重视从真实程序出发做性能工程、profiling、算法和底层优化。
- CMU 15-213/18-213 CS:APP Labs：重视二进制、缓存、内存分配、系统接口、实验报告和自动化验收。
- Berkeley CS61C：重视机器级表示、汇编、流水线、缓存、并行和体系结构直觉。
- Stanford CS149：重视并行编程、work/span、SIMD、GPU/多核思维和性能模型。
- 生产级库路线：oneDNN、BLIS、OpenBLAS、MKL 的 kernel 设计、ISA dispatch、blocking、packing、profiling。

## 建议周期

总周期：48 到 72 周。每周 10 到 18 小时。

- 周一：阅读资料和写预习笔记。
- 周二：完成最小正确实现。
- 周三：反汇编、编译器报告、`llvm-mca` 静态分析。
- 周四：benchmark、参数扫描、画图。
- 周五：优化迭代和失败案例。
- 周末：写实验报告，和生产库或参考实现对比。

每个 lab 的报告必须包含：

- 问题定义和正确性约束。
- 理论模型：FLOPs、bytes、arithmetic intensity、latency/throughput 预估。
- 编译命令、CPU 信息、flags、运行环境。
- 关键源码和关键汇编片段。
- 至少 3 组输入规模，至少 5 次重复测量。
- 结果图表和误差说明。
- 瓶颈判断、失败尝试、下一步优化。

## 评分标准

每个 lab 满分 100：

- 正确性 20：边界输入、随机测试、reference 对比、sanitizer。
- 测量严谨性 20：pin CPU、warmup、重复测量、防 DCE、统计口径、环境记录。
- 性能分析 25：能解释瓶颈，不只给数字。
- 优化质量 20：优化有因果链，有前后对比，有负面案例。
- 工程质量 10：代码清晰、接口稳定、脚本可复现。
- 报告表达 5：图表清楚、结论具体。

## 第一阶段：测量、二进制和 x86-64 基础

### Lab 00：建立性能实验室

教学目标：建立能长期复用的 benchmark 环境，理解为什么不可信测量比没有测量更危险。

学习内容：

- CPU 型号、指令集、频率、缓存、NUMA、虚拟化环境记录。
- Debug/Release、`-O0/-O2/-O3/-march=native` 的差异。
- 防止 dead-code elimination、constant folding、I/O 干扰。
- median、p95、min、standard deviation 的含义。

实验任务：

- 写一个最小 C++ benchmark harness。
- 支持 warmup、重复次数、输入规模扫描、checksum、CSV 输出。
- 自动记录 `lscpu`、编译器版本、编译参数、git commit。
- 在 WSL2 和裸机 Linux 上分别跑一次，记录差异。

高质量作业：

- 对 `sum(vector<float>)`、`dot(float*)`、`memset` 做 3 组规模测试。
- 故意写 5 个错误 benchmark，例如循环被优化掉、测了 I/O、没有 warmup，并解释错误原因。
- 报告中必须说明本机为什么不能使用完整硬件 PMU，以及未来如何迁移到裸机测量。

验收标准：

- 任意实验可以一条命令复现。
- 输出包含 CSV 和一页 Markdown 报告。
- 能解释为什么同一个程序多次运行会有波动。

### Lab 01：C++ 到目标文件、汇编和二进制

教学目标：理解从 `.cpp` 到 ELF 可执行文件的路径，能定位一个 C++ 函数的机器码。

学习内容：

- preprocessing、compilation、assembly、linking。
- ELF section、symbol、relocation、PLT/GOT。
- `objdump -drwC -Mintel`、`readelf`、`nm`。
- inline、static、anonymous namespace 对 symbol 的影响。

实验任务：

- 写 10 个小函数：整数加法、浮点加法、循环、函数调用、模板、lambda、虚函数、异常、`std::vector`、`std::span`。
- 分别用 GCC 和 Clang 编译，并保存汇编。
- 建立源码行到汇编块的映射表。

高质量作业：

- 对比 `-O0/-O2/-O3/-fno-exceptions/-fno-rtti/-flto` 的二进制大小和关键汇编变化。
- 写一份“C++ 语法特性成本表”，每个结论必须有汇编证据。

验收标准：

- 能在反汇编中找到指定函数。
- 能解释 name mangling、inline 消失、虚调用和直接调用的区别。

### Lab 02：System V AMD64 ABI 和手写汇编

教学目标：掌握 Linux x86-64 调用约定，能让 C++ 和手写汇编互调。

学习内容：

- 整数参数寄存器、浮点参数寄存器、返回值寄存器。
- caller-saved、callee-saved、stack alignment、red zone。
- 栈帧、prologue/epilogue、leaf function。
- struct return、可变参数、异常边界。

实验任务：

- 手写 `add_i64`、`dot_i32`、`sum_f32`、`memswap`。
- C++ 调汇编，汇编调 C++ callback。
- 故意破坏 callee-saved register，观察错误。

高质量作业：

- 写一个 ABI conformance test：随机生成输入，对比 C++ reference。
- 用 GDB 单步看寄存器和栈变化，提交截图或日志。
- 分析一个 `std::function` 调用链的汇编和调用开销。

验收标准：

- 所有手写汇编通过 sanitizer 外的 correctness test。
- 报告能画出一次函数调用前后的寄存器和栈布局。

### Lab 03：数据表示、位运算和 branchless 基础

教学目标：掌握整数、浮点、补码、NaN、alignment、bit trick 的性能和正确性边界。

学习内容：

- two's complement、sign extension、zero extension。
- IEEE-754、rounding、subnormal、NaN。
- `test/cmp/setcc/cmov`。
- undefined behavior 如何影响优化。

实验任务：

- 实现 `popcount`、`abs`、`min/max`、`clamp`、`round_up_power2`。
- 写 branch 和 branchless 两版。
- 对随机数据、有序数据、极端数据分别 benchmark。

高质量作业：

- 找出 3 个因为 signed overflow 或 strict aliasing 导致的错误优化案例。
- 对比 `std::popcount`、编译器 builtin、手写算法和硬件 `popcnt`。

验收标准：

- 能说明 branchless 不一定更快的条件。
- 能解释 UB 为什么会让汇编“看起来不符合直觉”。

### Lab 04：二进制阅读和控制流逆向

教学目标：像读源码一样读简单汇编，训练从汇编恢复控制流和数据流。

学习内容：

- basic block、jump table、switch lowering。
- loop lowering、induction variable。
- recursive function、tail call。
- 简单混淆和调试技巧。

实验任务：

- 编译多个无源码小程序，只给反汇编和输入输出样例。
- 恢复伪代码，写出等价 C++。
- 设计一个小型“binary puzzle”，让别人通过反汇编解题。

高质量作业：

- 选一个真实库中的 100 行以内热点函数，只看反汇编先猜功能，再回看源码验证。
- 报告必须列出至少 10 条“从汇编判断源码结构”的规则。

验收标准：

- 能从汇编画出控制流图。
- 能识别数组索引、结构体字段访问、虚表调用。

## 第二阶段：编译器优化和微架构模型

### Lab 05：编译器优化开关和成本模型

教学目标：理解优化级别、目标 CPU、内联、LTO、PGO 如何影响性能。

学习内容：

- `-O2/-O3/-Ofast/-march/-mtune`。
- inlining、constant propagation、DCE、LICM、unrolling。
- `-Rpass`、`-fopt-info`、优化报告。
- `perf` 不可用时如何用 disassembly 和 timing 近似分析。

实验任务：

- 建立 30 个小 loop case。
- 对每个 case 记录 GCC/Clang 是否向量化、是否展开、是否内联。
- 解释 10 个 missed optimization。

高质量作业：

- 选择 5 个 missed vectorization case，通过 `restrict`、alignment、loop rewrite、`std::span` 改写修复。
- 写一份“编译器优化不是魔法”的反例报告。

验收标准：

- 能用编译器报告解释一段循环为什么没有变快。
- 能给出源码级改写，而不是只改 flags。

### Lab 06：latency、throughput、uops 和 `llvm-mca`

教学目标：建立现代乱序 CPU 的最小性能模型。

学习内容：

- dependency chain 测 latency。
- independent instruction streams 测 throughput。
- uop、port、scheduler、ROB、register renaming。
- `llvm-mca` 的能力和局限。

实验任务：

- 对 `add`、`imul`、`vaddps`、`vfmadd231ps`、`vpermilps` 写 microbenchmark。
- 生成汇编，送入 `llvm-mca -mcpu=arrowlake-s`。
- 与 Intel Intrinsics Guide、uops.info 的数据对照。

高质量作业：

- 写一份“预测 vs 实测”表格，至少 12 条指令。
- 解释哪些误差来自前端、频率、benchmark 结构、WSL2 或调度噪声。

验收标准：

- 能区分 latency bound 和 throughput bound。
- 能用 dependency breaking 改善吞吐。

### Lab 07：分支预测、条件移动和前端瓶颈

教学目标：理解 branch predictor、mispredict penalty、code size、uop cache 对性能的影响。

学习内容：

- predictable branch、random branch、biased branch。
- `cmov`、`setcc`、masking。
- loop unroll 的收益和代码尺寸成本。
- front-end bound 的常见症状。

实验任务：

- 实现 threshold sum：branch、branchless、SIMD mask 三版。
- 控制数据分布：全真、全假、50/50、周期模式、随机模式。
- 对比不同 unroll factor。

高质量作业：

- 写一份决策表：什么时候该保留分支，什么时候改 branchless。
- 设计一个“过度 unroll 变慢”的可复现实验。

验收标准：

- 能解释为什么随机分支会严重变慢。
- 能解释为什么 branchless 可能浪费执行资源。

### Lab 08：cache、bandwidth、TLB 和数据布局

教学目标：掌握内存层级性能，能判断代码是 compute-bound 还是 memory-bound。

学习内容：

- L1/L2/L3/DRAM latency ladder。
- cache line、spatial locality、temporal locality。
- TLB、page walk、huge page。
- AoS、SoA、AoSoA。

实验任务：

- pointer chasing 测 latency。
- streaming read/write 测 bandwidth。
- stride scan 测 cache line 和 TLB 拐点。
- AoS/SoA 粒子更新实验。

高质量作业：

- 画出本机 cache/TLB 拐点曲线。
- 用 roofline 思路分析 `saxpy`、`dot`、`matrix transpose`。
- 写一个 SoA 改写，让粒子更新至少提升 1.5x，并解释原因。

验收标准：

- 能从 bytes/FLOP 判断上限。
- 能解释为什么矩阵转置比矩阵加法更难优化。

### Lab 09：store/load、alignment、prefetch 和 streaming store

教学目标：理解 load/store subsystem 对 kernel 的限制。

学习内容：

- store forwarding。
- alignment 和 split load/store。
- write allocate。
- hardware prefetcher 和 software prefetch。
- non-temporal store。

实验任务：

- 写 aligned/unaligned load benchmark。
- 写 store-forwarding penalty benchmark。
- 写 copy/scale/triad benchmark，比较普通 store 和 streaming store。

高质量作业：

- 设计一个 prefetch 有收益和一个 prefetch 负收益的例子。
- 对大数组写入解释 write allocate 的隐藏读流量。

验收标准：

- 能说明为什么 non-temporal store 只在某些规模和访问模式下有效。
- 能识别 load/store unit 相关瓶颈。

## 第三阶段：SIMD、AVX2/FMA/AVX-VNNI 和 kernel

### Lab 10：自动向量化训练营

教学目标：让编译器完成 SIMD，并知道它失败时如何帮助它。

学习内容：

- vectorization legality 和 profitability。
- alias analysis、alignment、reduction recognition。
- tail handling、mask、remainder loop。
- `restrict`、`assume_aligned`、loop canonical form。

实验任务：

- 编写 20 个循环：map、zip、reduction、stencil、histogram、gather-like。
- 查看 GCC/Clang 向量化报告。
- 对每个失败 case 写修复版或解释为什么不应向量化。

高质量作业：

- 将 scalar image kernel 改成 auto-vectorization friendly 版本。
- 报告必须包含源码 diff、编译报告和关键汇编。

验收标准：

- 能让至少 12 个 case 自动向量化。
- 能解释不能向量化的真实依赖。

### Lab 11：AVX2/FMA intrinsics 基础

教学目标：熟练使用 AVX2 和 FMA 写 float kernel。

学习内容：

- `__m256`、load/store、broadcast、shuffle、permute。
- FMA accumulator、多累加器展开。
- horizontal reduction。
- tail handling。

实验任务：

- 实现 `saxpy`、`dot`、`l2_norm`、`sum`、`relu`。
- 每个函数写 scalar、auto-vectorized、intrinsics 三版。
- 用 `objdump` 验证是否生成 `vfmadd`。

高质量作业：

- `dot` 至少写 1、2、4、8 accumulator 版本，分析寄存器压力和吞吐。
- 和 `std::transform_reduce` 或编译器自动向量化对比。

验收标准：

- 能解释水平规约为什么常是 SIMD kernel 的尾部瓶颈。
- 能解释 accumulator 数量和 latency hiding 的关系。

### Lab 12：SIMD shuffle、gather、transpose 和 mask

教学目标：掌握 SIMD 中最容易变慢的数据重排。

学习内容：

- shuffle/permute/blend。
- gather 的代价。
- 4x4、8x8 transpose。
- mask load/store 的边界处理。

实验任务：

- 写 4x4 和 8x8 float transpose。
- 写 AoS to SoA 转换。
- 写 gather-based lookup，并和数据重排后的连续访问对比。

高质量作业：

- 设计一个“减少一次 shuffle 比多算几次乘法更快”的案例。
- 报告说明每个 shuffle 指令的吞吐和 port 压力。

验收标准：

- 能判断一个 SIMD kernel 是算术瓶颈还是数据重排瓶颈。
- 能用 layout 改写减少 gather。

### Lab 13：AVX-VNNI、int8 dot 和量化基础

教学目标：理解 AI 推理中 int8 matmul 的核心路径。

学习内容：

- int8、uint8、int32 accumulation。
- zero point、scale、per-tensor、per-channel。
- AVX-VNNI 的 dot-product 思路。
- saturation、requantization、rounding。

实验任务：

- 实现 int8 dot product。
- 实现小型 int8 GEMV。
- 对比 scalar、AVX2 传统乘加、AVX-VNNI intrinsic。

高质量作业：

- 写一个 quantize -> int8 GEMV -> dequantize pipeline。
- 对比 fp32 reference 的最大误差、平均误差和性能。
- 报告说明误差来自量化而不是实现 bug。

验收标准：

- 能解释 int8 算子为什么通常 memory traffic 更低。
- 能解释 accumulator 溢出风险。

### Lab 14：GEMM baseline、roofline 和循环顺序

教学目标：从最朴素矩阵乘法开始建立 GEMM 优化路线。

学习内容：

- `ijk/ikj/jik` loop order。
- row-major 和 column-major。
- arithmetic intensity。
- naive GEMM 的 cache miss 问题。

实验任务：

- 写 6 种循环顺序的 SGEMM。
- 扫描 M/N/K 和矩阵布局。
- 对比 OpenBLAS/BLIS/oneDNN 的单线程结果。

高质量作业：

- 用 roofline 解释 naive GEMM 为什么远低于峰值。
- 找出一个维度组合使某种 loop order 特别差，并解释访问模式。

验收标准：

- 能通过访问序列解释性能差异。
- 能给出 GEMM 的理论 FLOPs 和 bytes 估计。

### Lab 15：GEMM blocking、packing 和 AVX2 microkernel

教学目标：写出接近生产库思想的单线程 GEMM。

学习内容：

- register blocking、cache blocking。
- packing A/B panel。
- microkernel `mr x nr`。
- K blocking 和 prefetch。

实验任务：

- 实现 `4x8` 或 `6x8` AVX2 SGEMM microkernel。
- 实现 B packing 和主循环。
- 对比 naive、blocked、packed、OpenBLAS/BLIS。

高质量作业：

- 至少达到参考 BLAS 单线程 50% 性能作为合格线，70% 以上为优秀线。
- 报告必须解释 microkernel 寄存器分配、load/FMA 比例、cache block 大小选择。

验收标准：

- 能说清楚为什么 packing 能提升性能。
- 能解释 mr/nr 选择和寄存器数量的关系。

### Lab 16：卷积、im2col、direct convolution 和小算子融合

教学目标：理解 CNN/深度学习算子如何落到 GEMM 或 direct kernel。

学习内容：

- NCHW/NHWC。
- im2col + GEMM。
- direct convolution。
- bias、relu、BN folding、operator fusion。

实验任务：

- 实现 2D convolution naive。
- 实现 im2col + GEMM。
- 实现一个小型 fused conv+bias+relu。

高质量作业：

- 对 1x1、3x3、depthwise convolution 分别分析哪种实现更适合。
- 和 oneDNN 对比至少 3 个 shape。

验收标准：

- 能解释 im2col 的内存膨胀。
- 能解释小 batch 和大 batch 的优化重点不同。

### Lab 17：softmax、layernorm、GELU 和 transformer 常用算子

教学目标：掌握 AI 模型中非 GEMM 算子的优化。

学习内容：

- numerically stable softmax。
- reduction、exp approximation、two-pass vs fused pass。
- layernorm/RMSNorm。
- GELU/tanh approximation。

实验任务：

- 写 softmax scalar、auto-vectorized、AVX2 版本。
- 写 layernorm 和 RMSNorm。
- 对 batch、seq_len、hidden size 扫描。

高质量作业：

- 对 `seq_len=128/1024/4096` 分析 softmax 的瓶颈变化。
- 在保证误差阈值下尝试近似 `exp`，写出精度和性能 tradeoff。

验收标准：

- 能解释 reduction 型算子为什么不容易达到峰值 FLOPs。
- 能解释数值稳定和性能优化的冲突。

### Lab 18：attention block CPU mini-kernel

教学目标：把 GEMM、softmax、layout、cache blocking 合到一个小型 transformer block。

学习内容：

- QK^T、scale、mask、softmax、PV。
- KV cache。
- batch/head/seq/head_dim layout。
- attention 中 memory-bound 和 compute-bound 的切换。

实验任务：

- 实现单头 attention reference。
- 实现 blocked attention。
- 对 causal mask 和 non-causal mask 分别测试。

高质量作业：

- 简化实现一个 FlashAttention 风格的 streaming softmax 思路，不要求达到 GPU 论文性能，但要证明减少中间矩阵写回。
- 和 PyTorch/oneDNN 或自写 reference 对比正确性和内存占用。

验收标准：

- 能解释为什么 attention 不只是两个 GEMM。
- 能解释中间张量写回如何影响 cache 和 bandwidth。

## 第四阶段：OS、并行、PMU 和生产级优化

### Lab 19：Linux perf、VTune 和 PMU 读数

教学目标：在裸机 Linux 上把性能数字和硬件事件连起来。

学习内容：

- `perf stat`、`perf record`、`perf report`、`perf annotate`。
- cycles、instructions、IPC、branches、branch-misses、cache-misses。
- Top-Down 思路：front-end、bad speculation、back-end、retiring。
- VTune hotspot 和 microarchitecture exploration。

实验任务：

- 在裸机或云服务器重跑 Lab 07、08、15。
- 用 `perf annotate` 标注热点汇编。
- 写 PMU 数据解释报告。

高质量作业：

- 对一个 kernel 给出“没有 PMU 时的判断”和“有 PMU 后的修正”。
- 找出一个原先误判瓶颈的案例。

验收标准：

- 能解释 IPC 高低不等于性能好坏。
- 能把热点汇编和 PMU 事件对上。

### Lab 20：线程、亲和性、false sharing 和内存模型

教学目标：理解多核扩展性问题。

学习内容：

- `std::thread`、OpenMP、thread pool。
- CPU affinity、scheduler migration。
- false sharing、cache coherence。
- C++ atomic memory order。

实验任务：

- 写 parallel reduction。
- 写 false sharing benchmark。
- 写 mutex、spinlock、atomic counter 对比。

高质量作业：

- 用 padding 消除 false sharing，并证明性能提升来自 cache line 隔离。
- 对 `relaxed/acquire/release/seq_cst` 写可解释实验。

验收标准：

- 能解释为什么线程数增加性能可能下降。
- 能判断 atomic 开销来自同步、缓存一致性还是竞争。

### Lab 21：NUMA、huge page、page fault 和内存分配

教学目标：掌握服务器性能里常见的 OS/内存问题。

学习内容：

- first-touch NUMA policy。
- `numactl`、CPU/memory binding。
- transparent huge page、explicit huge page。
- page fault、`mmap`、`mlock`。

实验任务：

- 在双路或多 NUMA 节点机器上测试本地/远程内存。
- 对 GEMM packing buffer 和大数组 scan 测 huge page。
- 对 page fault 做 cold-start 分析。

高质量作业：

- 写一个 NUMA 误用导致性能腰斩的复现实验。
- 给出一份生产部署 checklist。

验收标准：

- 能解释 first-touch 为什么重要。
- 能解释 huge page 对 TLB 和 page walk 的影响。

### Lab 22：ISA dispatch、CPUID、模板 kernel 和 JIT 思路

教学目标：理解生产库如何根据机器选择不同 kernel。

学习内容：

- CPUID feature detection。
- function multiversioning。
- ifunc、dispatch table。
- template specialization 和 runtime JIT 的取舍。

实验任务：

- 为 `dot` 和 `gemm` 写 scalar、AVX2、AVX-VNNI 三个版本。
- 运行时选择最佳实现。
- 输出 verbose log，说明选择了哪个 ISA。

高质量作业：

- 设计一个 mini oneDNN 风格 primitive cache。
- 写报告比较静态模板、多版本函数和 JIT 的工程成本。

验收标准：

- 能解释为什么生产库不能只编译 `-march=native`。
- 能解释 feature detection 和 ABI 兼容性。

### Lab 23：AVX-512、AMX 和服务器前沿实验

教学目标：在支持的服务器上理解更现代的 CPU AI 指令。

学习内容：

- AVX-512 mask、宽向量、downclock 风险。
- BF16、FP16、INT8。
- Intel AMX tile、tile config、tile dot product。
- AMX 和 AVX2/AVX-512 kernel 组织差异。

实验任务：

- 在支持 AMX 的服务器上实现小型 BF16/INT8 matmul。
- 对比 AVX2/AVX-512/AMX 的吞吐、频率和功耗。
- 记录 OS 开启 AMX state 的要求。

高质量作业：

- 写一个 AMX matmul 学习版，重点是 tile blocking 和数据 layout。
- 对同一 shape 比较 oneDNN AMX kernel，并解释差距。

验收标准：

- 能解释 AMX 为什么适合矩阵块计算。
- 能解释宽向量和 tile ISA 带来的上下文切换和调度成本。

### Lab 24：生产库阅读和性能复现

教学目标：学习真实高性能库的工程结构，而不是停留在玩具 kernel。

学习内容：

- oneDNN primitive、ISA dispatch、verbose log。
- BLIS microkernel、packing、control tree。
- OpenBLAS kernel 目录和架构分发。
- benchmark 与 correctness suite。

实验任务：

- 选择 oneDNN、BLIS、OpenBLAS 中一个库，阅读一个 GEMM 或 matmul 路径。
- 用自己的输入 shape 复现实验。
- 画出从 API 到 microkernel 的调用路径。

高质量作业：

- 写一份 8 到 12 页代码阅读报告。
- 找到一个可以小幅修改或添加日志的位置，证明你理解了路径。

验收标准：

- 能解释生产库比教学 kernel 多出的工程复杂度。
- 能把自己的 Lab 15 和生产库设计进行结构性对比。

## 大作业

### 大作业 A：可信性能实验平台

目标：做一个长期使用的 CPU benchmark 工具箱。

必须功能：

- 自动记录环境。
- 支持 pin CPU、warmup、重复测量、CSV/JSON 输出。
- 支持 correctness reference 和 checksum。
- 支持参数扫描和图表生成。

优秀标准：

- 能跑完本计划至少 10 个 lab 的 benchmark。
- 报告模板自动生成。
- 能在 WSL2、裸机、服务器上保留环境差异记录。

### 大作业 B：MiniBLAS 单线程库

目标：实现 `saxpy/dot/sgemv/sgemm`。

必须功能：

- scalar baseline。
- auto-vectorized version。
- AVX2/FMA intrinsics version。
- 单线程 SGEMM blocked + packed + microkernel。

优秀标准：

- SGEMM 达到参考 BLAS 单线程 50% 以上，70% 以上为优秀。
- 对至少 10 个 shape 给出 roofline 和瓶颈分析。
- 代码支持 ISA dispatch。

### 大作业 C：MiniDNN CPU Operator Pack

目标：实现一组 transformer/CNN 常用 CPU 算子。

必须功能：

- matmul 或 batched matmul。
- softmax。
- layernorm 或 RMSNorm。
- GELU 或 SiLU。
- int8 dot/GEMV 或 quantized matmul。

优秀标准：

- 每个算子都有 fp32 reference、误差测试、性能测试。
- 至少一个算子包含 operator fusion。
- 和 oneDNN 或 PyTorch CPU 做对比。

### 大作业 D：性能事故分析报告

目标：模拟真实工作中的性能 debug。

要求：

- 选择一个你自己写的或真实开源项目中的热点。
- 先给出错误假设，再用数据推翻。
- 最终给出至少 2 个有效优化和 2 个无效优化。
- 报告必须包含源码、汇编、运行数据、PMU 或替代证据。

优秀标准：

- 结论可以指导工程决策，而不是只展示数字。
- 对性能差距有定量解释。

### 大作业 E：前沿简化实验

从下面任选一个：

- AMX BF16/INT8 matmul 学习版。
- AVX-512 masked softmax。
- FlashAttention 风格 CPU streaming attention。
- JIT generated microkernel。
- CPU sparse matmul 或 block sparse attention。
- LLM inference CPU path：KV cache、int8 weight-only、batching。

优秀标准：

- 不要求达到生产库性能，但必须说明前沿技术解决的核心瓶颈。
- 必须有一个简化实现和一个严谨对照实验。

## 阶段性考试

### Midterm 1：汇编和 ABI

给定 5 个 C++ 函数和 5 段汇编：

- 匹配源码和汇编。
- 标注参数、返回值、栈布局。
- 找出一个 ABI bug。
- 解释一个优化级别导致的结构变化。

### Midterm 2：微架构性能模型

给定 3 个 loop：

- 预测瓶颈。
- 估算 FLOPs、bytes、arithmetic intensity。
- 用 `llvm-mca` 预测吞吐。
- 写出优化方案，并说明风险。

### Final：真实 kernel 优化

给定一个未优化 kernel：

- 24 小时内完成 correctness、benchmark、profiling、优化、报告。
- 不能只提交最快代码，必须提交分析链。
- 评分重点是解释力、复现性和工程质量。

## 推荐资料

核心文档：

- Intel 64 and IA-32 Architectures Software Developer's Manual: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
- AMD64 Architecture Programmer's Manual: https://docs.amd.com/v/u/en-US/40332_4.09_APM_PUB
- Agner Fog optimization manuals: https://www.agner.org/optimize/
- uops.info: https://uops.info/
- LLVM MCA documentation: https://llvm.org/docs/CommandGuide/llvm-mca.html
- Linux perf wiki: https://perfwiki.github.io/main/
- perf-stat man page: https://man7.org/linux/man-pages/man1/perf-stat.1.html
- oneDNN documentation: https://uxlfoundation.github.io/oneDNN/

公开课程入口：

- MIT 6.172 Performance Engineering of Software Systems: https://ocw.mit.edu/courses/6-172-performance-engineering-of-software-systems-fall-2018/
- CMU CS:APP labs: https://csapp.cs.cmu.edu/3e/labs.html
- Berkeley CS61C: https://cs61c.org/
- Stanford CS149 Parallel Computing: https://gfxcourses.stanford.edu/cs149/

## 立即执行的第一个 14 天计划

Day 1：

- 建立 `cpu-lab` 目录结构。
- 记录 `lscpu`、编译器、OS、WSL2/裸机状态。
- 写第一个 `sum` benchmark。

Day 2：

- 修复 dead-code elimination。
- 加 warmup、重复次数、CSV。
- 对 3 个输入规模测量。

Day 3：

- 编译 `-O0/-O2/-O3/-march=native`。
- 保存汇编。
- 写第一份源码到汇编映射。

Day 4：

- 写 5 个错误 benchmark。
- 故意制造不可信结果。
- 写错误原因报告。

Day 5：

- 实现 `dot` scalar。
- 用 Clang/GCC 比较自动向量化。
- 记录 vectorization report。

Day 6：

- 用 `llvm-mca` 分析关键循环。
- 对比 `objdump` 和 `.s` 文件。

Day 7：

- 写 Lab 00 报告。
- 自评分，列出测量 checklist。

Day 8：

- 开始 Lab 01，写 10 个 C++ 小函数。
- 编译并反汇编。

Day 9：

- 分析模板、lambda、虚函数。
- 记录符号名和二进制大小。

Day 10：

- 比较 GCC 和 Clang。
- 写优化差异表。

Day 11：

- 开始 Lab 02，写第一个手写汇编函数。
- C++ 调用并测试。

Day 12：

- 实现汇编 `sum_i64` 和 `dot_i32`。
- GDB 单步观察寄存器。

Day 13：

- 故意破坏 ABI，观察错误。
- 写栈帧和寄存器图。

Day 14：

- 写 Lab 01 和 Lab 02 的短报告。
- 整理下一周问题清单。

## 长期能力验收标准

初级合格：

- 能看懂普通 C++ 函数的汇编。
- 能写可信 microbenchmark。
- 能解释 cache miss、branch miss、vectorization failed 的基本原因。

中级合格：

- 能写 AVX2/FMA kernel。
- 能用 roofline 判断 kernel 上限。
- 能对比编译器自动优化和手写 intrinsics。

高级合格：

- 能写 GEMM microkernel 和 packing。
- 能用 PMU 或替代证据解释瓶颈。
- 能处理线程、NUMA、huge page、affinity。

大师级门槛：

- 能阅读 oneDNN/BLIS/OpenBLAS 的核心路径。
- 能为一个真实 AI 算子设计数据布局、blocking、ISA dispatch 和 benchmark。
- 能解释自己的 kernel 与生产库差距，不靠猜测。
- 能把性能优化变成可复现、可维护、可迁移的工程流程。
