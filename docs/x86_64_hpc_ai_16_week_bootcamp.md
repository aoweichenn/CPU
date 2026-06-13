# x86-64, CPU Performance, C++ to Assembly, HPC and AI Operators: 16-Week Expert Sprint

这份文档是当前唯一执行计划。时间：3 到 4 个月。目标：不是轻量入门，而是在有限时间内高密度打通专家主干。

基础内容不再单独拖一周。Linux、CMake、测试、benchmark 会作为每天任务的一部分穿插完成。第 1 周直接进入 C++ 到汇编，第 2 周进入 ABI 和手写汇编，第 3 周进入编译器 IR 和优化，第 4 周进入微架构模型。

## 强度要求

最低强度：

- 每天 3 小时。
- 周末每天 5 小时。
- 每周至少 25 小时。

专家冲刺强度：

- 每天 4 到 5 小时。
- 每周 30 到 35 小时。
- 每周必须完成代码、数据、汇编证据、报告。

如果当天时间不够，优先级如下：

1. 代码和测试。
2. 汇编/IR/`llvm-mca`/perf 证据。
3. benchmark 数据。
4. 报告。
5. 扩展阅读。

## 最终目标

16 周结束时，你至少应达到：

- 能把 C++ 热点函数一路追到 LLVM IR、汇编、uops、运行时间。
- 能判断一个 kernel 是 branch-bound、latency-bound、throughput-bound、front-end-bound、memory-bound、TLB-bound 还是 synchronization-bound。
- 能写 System V AMD64 ABI 下的手写汇编函数。
- 能写 AVX2/FMA kernel 和 AVX-VNNI int8 dot/GEMV。
- 能写单线程 SGEMM 学习版，包含 blocking、packing、AVX2 microkernel。
- 能实现 softmax、layernorm/RMSNorm、GELU、attention reference 和至少一个 fusion。
- 能用 perf/VTune 或替代证据分析真实热点。
- 能阅读 oneDNN/BLIS/OpenBLAS 的核心路径，并解释自己的实现和生产库差距。

## 学习资源对标

本计划把优秀教材/课程的主干压缩进 16 周：

- CS:APP：machine-level programming、linking、memory hierarchy、exceptional control flow。
- MIT 6.172：performance engineering、measurement、profiling、optimization reports。
- CMU CS:APP labs：binary、cache、systems-style reports。
- Stanford CS149：parallel thinking、SIMD、work partition。
- Intel SDM：x86-64 架构和指令权威参考。
- Intel Optimization Reference Manual：现代 Intel CPU 优化主文档。
- Intel Intrinsics Guide：SIMD intrinsic 查询。
- Agner Fog manuals：C++ 优化、汇编、微架构、指令表。
- uops.info：指令 latency/throughput/uops/port 数据。
- LLVM docs：LLVM IR、`llvm-mca`、优化报告。
- Linux perf docs：PMU 和 profiling。
- oneDNN/BLIS/OpenBLAS：生产级 CPU 算子库。

## 每周固定交付

每周必须提交：

- `docs/reports/weekXX.md`
- 本周所有源码和脚本
- 测试结果
- benchmark CSV/summary
- 至少 5 段关键汇编或 IR 证据
- 至少 1 次错误假设或失败优化记录
- 至少 1 张表：性能表、指令表、瓶颈表或误差表
- 下一周问题清单

每周报告最低结构：

```text
# Week XX Report

## 本周目标
## 必读材料摘要
## 每日完成记录
## 代码和测试
## C++ -> IR -> ASM 证据
## 性能数据
## 瓶颈判断
## 失败实验
## 作业答案
## 还不懂的问题
```

## Week 1: C++ to Assembly, ELF, Benchmark Discipline

目标：第一周就开始看汇编。建立“源码 -> IR/汇编 -> 二进制 -> benchmark”的闭环。

必读：

- CS:APP machine-level programming 入门。
- CS:APP linking 入门。
- `man objdump`、`man readelf`、`man nm`。
- 本仓库 `labs/lab00_benchmark_foundation/README.md`。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | 环境、Release build、benchmark 可信性 | 跑 `run_lab00.sh`；读 `env.txt/summary.md/bad_benchmarks.txt` | 环境表、5 条 benchmark 规则 |
| D2 | 编译流水线：preprocess/compile/assemble/link | 对 8 个小函数生成 `.i/.s/.o/exe` | 每个阶段产物和命令 |
| D3 | `objdump/readelf/nm` | 反汇编 Lab 00 和小函数；找 symbol/section/relocation | symbol 表、section 表 |
| D4 | `-O0/-O2/-O3/-march=native` | 对 12 个函数比较汇编 | 优化级别差异表 |
| D5 | control flow lowering | 写 if/switch/loop/range-for；画 CFG | 5 个 CFG + 汇编标注 |
| D6 | benchmark + assembly | 新增 `scale_f32`；测试、benchmark、反汇编 | 代码、CSV、关键汇编 |
| D7 | 报告和复盘 | 完成 Week 1 report | 一份完整报告 |

作业：

- 写 12 个函数：`add_i32`、`add_f32`、`sum_i32`、`sum_f32`、`dot_f32`、`if_else`、`switch_dense`、`switch_sparse`、`template_add`、`lambda_call`、`virtual_call`、`vector_sum`。
- 每个函数提交 `-O0` 和 `-O3 -march=native` 的关键汇编片段。
- 新增 `scale_f32` 到 Lab 00：reference、test、benchmark、summary。
- 写 2 页报告：为什么 `-O0` 汇编不适合性能推理，为什么最终二进制反汇编比 `.s` 文件更可靠。

验收：

- 能从 `objdump -drwC -Mintel` 找到指定函数。
- 能解释 inline 后 symbol 为什么可能消失。
- 能解释一个 loop 的 induction variable 在汇编里是什么。

## Week 2: x86-64 Assembly and System V AMD64 ABI

目标：掌握 Linux x86-64 调用约定，能写和调试手写汇编。

必读：

- System V AMD64 ABI calling convention。
- Intel SDM Vol.1 registers/basic execution environment。
- Agner Fog calling conventions manual。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | GPR/XMM/YMM、参数寄存器、返回值 | 写 1-10 参数 C++ 函数并反汇编 | 参数位置表 |
| D2 | caller-saved/callee-saved、stack alignment | GDB 单步函数调用 | 栈和寄存器图 |
| D3 | `.S` 文件和 `extern "C"` | 手写 `add_i64/max_i64/min_i64` | 测试通过 |
| D4 | loop in assembly | 手写 `sum_i64/dot_i32` | correctness + 汇编 |
| D5 | 浮点 ABI | 手写 `add_f32/add_f64/sum_f32` | XMM 使用说明 |
| D6 | ABI bug lab | 故意破坏 `rbx`、栈对齐、返回值 | bug 复现和修复 |
| D7 | 报告 | 完成 Week 2 report | ABI 总结 |

作业：

- C++ 调汇编：`add_i64`、`max_i64`、`sum_i64`、`dot_i32`、`sum_f32`。
- 汇编调 C++ callback：`apply_i64(int64_t*, size, callback)`。
- 写 ABI conformance tests：随机输入、边界输入。
- 用 GDB 记录一次函数调用前后 `rsp/rbp/rbx/r12/xmm0`。

验收：

- 能解释前 6 个整数参数、前 8 个浮点参数、返回值和第 7 个整数参数位置。
- 能解释为什么调用前栈要 16 字节对齐。
- 能定位并修复 callee-saved 寄存器错误。

## Week 3: LLVM IR, Compiler Optimization and Auto Vectorization

目标：把 C++ 语义、LLVM IR、优化报告和最终汇编连起来。

必读：

- LLVM LangRef 入门：function、basic block、SSA、phi、load/store。
- Clang `-Rpass` 文档。
- GCC `-fopt-info-vec` 文档。
- Agner Fog C++ optimization manual：compiler optimization。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | LLVM IR/SSA | 生成 12 个函数的 LLVM IR | C++/IR/ASM 三列表 |
| D2 | inline/DCE/constant propagation/LICM | 写 10 个优化 case | 优化前后 IR/ASM |
| D3 | alias analysis | 写 alias 阻碍向量化 case | missed report |
| D4 | auto vectorization | 写 20 个 loop case | 成功/失败表 |
| D5 | 修复向量化 | 用 `restrict`、alignment、loop rewrite 修 8 个 | diff + 数据 |
| D6 | fast-math 和浮点语义 | 对 sum/dot 开关 `-ffast-math` | 误差/性能表 |
| D7 | 报告 | 完成 Week 3 report | 编译器优化案例库 |

作业：

- 20 个 loop case：map、zip、sum、dot、min/max、conditional map、stencil、histogram、gather、scatter、function-call-in-loop。
- 每个 case 记录：GCC 是否向量化、Clang 是否向量化、失败原因、修复方案。
- 写 3 个因为 UB 或 fast-math 导致结果变化的例子。

验收：

- 能解释 `restrict` 为什么帮助向量化。
- 能解释 reduction 识别和 tail loop。
- 能解释 `-Ofast` 和 `-ffast-math` 的风险。

## Week 4: Microarchitecture Model, llvm-mca, uops, Branch Prediction

目标：建立 latency/throughput/uops/ports/front-end/branch 的性能模型。

必读：

- Intel Optimization Reference Manual：front end、out-of-order、branch、execution ports。
- Agner Fog microarchitecture manual。
- LLVM MCA docs。
- uops.info。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | latency vs throughput | 写 dependency chain 和 independent chain | 数据 |
| D2 | uops/ports | 对 12 条指令查 uops.info 和 `llvm-mca` | 指令表 |
| D3 | accumulator 和 latency hiding | `dot` 1/2/4/8 accumulator | 性能表 |
| D4 | front-end/code size | unroll factor 1/2/4/8/16 | 过度 unroll 反例 |
| D5 | branch prediction | threshold sum 五种数据分布 | branch/branchless 决策表 |
| D6 | `llvm-mca` 深入 | 分析 Week 3 的 5 个 loops | mca 报告 |
| D7 | 报告 | 完成 Week 4 report | 预测 vs 实测 |

作业：

- 12 条指令表：`add`、`imul`、`lea`、`vaddps`、`vmulps`、`vfmadd231ps`、`vpermps`、`vshufps`、`vgatherdps`、`vpaddd`、`vpmulld`、`vpdpbusd`。
- 对每条记录 latency、throughput、uops、ports、`llvm-mca` 预测。
- 写“什么时候 branchless 更快，什么时候更慢”的报告。

验收：

- 能区分 latency-bound 和 throughput-bound。
- 能解释为什么 `llvm-mca` 不能预测 cache miss、分支预测、频率和 OS 噪声。

## Week 5: Memory Hierarchy, Cache, TLB, Load/Store, Roofline

目标：系统掌握 memory-bound 优化。

必读：

- CS:APP memory hierarchy。
- Intel Optimization Reference Manual：memory access、prefetch、store forwarding。
- Agner Fog optimization manual：memory。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | cache line/locality | sequential read/write/copy/triad | bandwidth 表 |
| D2 | latency ladder | pointer chasing 工作集扫描 | L1/L2/L3/DRAM 曲线 |
| D3 | stride/TLB | stride scan、page stride | TLB 拐点 |
| D4 | AoS/SoA/AoSoA | 粒子更新三版 | layout 性能表 |
| D5 | load/store/alignment | aligned/unaligned、store forwarding | 数据 |
| D6 | prefetch/streaming store | 正例和反例 | 适用条件 |
| D7 | roofline | `sum/dot/saxpy/transpose` roofline | Week 5 report |

作业：

- 画本机 cache/TLB/bandwidth 曲线。
- AoS 改 SoA 至少一个 case 提升 1.5x。
- 写 `matrix transpose` naive/tiled 两版。
- 对 `sum/dot/saxpy/transpose` 写 FLOPs、bytes、arithmetic intensity、上限判断。

验收：

- 能判断一个 kernel 是 memory-bound 还是 compute-bound。
- 能解释 TLB miss、write allocate、store forwarding、non-temporal store。

## Week 6: SIMD Auto Vectorization and AVX2/FMA Intrinsics

目标：能写和分析 AVX2/FMA float kernel。

必读：

- Intel Intrinsics Guide：AVX、AVX2、FMA。
- Intel Optimization Reference Manual：SIMD。
- Agner Fog vector/instruction tables。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | SIMD lane/register/load/store | 写 `add_f32/mul_f32` AVX2 | 测试 |
| D2 | FMA | `saxpy` scalar/auto/AVX2 | benchmark + `vfmadd` 证据 |
| D3 | reduction | `sum/dot/l2_norm` AVX2 | tail + 误差 |
| D4 | multiple accumulators | dot 1/2/4/8 accumulator | latency hiding 报告 |
| D5 | mask/blend | threshold map/relu/clamp | branch vs SIMD |
| D6 | shuffle/transpose | 4x4/8x8 transpose | `llvm-mca` |
| D7 | 报告 | Week 6 report | AVX2 kernel set |

作业：

- `saxpy`、`dot`、`sum`、`l2_norm`、`relu`、`clamp`：scalar、auto-vectorized、AVX2 三版。
- 每个函数有 correctness test、benchmark、关键汇编。
- 对 `dot` accumulator 数量写完整分析。

验收：

- 能解释 horizontal reduction 成本。
- 能解释 tail handling。
- 能从汇编确认 `ymm` 和 `vfmadd`。

## Week 7: AVX2 Data Movement, Gather, Layout and AVX-VNNI/int8

目标：掌握 SIMD 的数据重排和 AI int8 基础。

必读：

- Intel Intrinsics Guide：shuffle、permute、gather、AVX-VNNI。
- oneDNN int8/matmul docs。
- 量化基础：scale、zero point、per-tensor/per-channel。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | shuffle/permute/blend | 写 AoS to SoA | layout 数据 |
| D2 | gather/scatter | gather lookup vs contiguous | gather 反例 |
| D3 | quant/dequant | fp32/int8 转换 | 误差表 |
| D4 | int8 dot scalar/AVX2 | 传统 int8 dot | correctness |
| D5 | AVX-VNNI | `vpdpbusd` dot | benchmark |
| D6 | int8 GEMV | 实现小型 GEMV pipeline | 误差 + 性能 |
| D7 | 报告 | Week 7 report | int8 pipeline |

作业：

- 完成 fp32 -> int8 -> int8 GEMV -> dequantize。
- 比较 per-tensor 和 per-channel 的误差。
- 对 scalar、AVX2、AVX-VNNI 三个 int8 dot 做性能对比。

验收：

- 能解释 int8 为什么可能快。
- 能解释 accumulator 溢出和 requantization。

## Week 8: GEMM I, Naive, Loop Order, Blocking, Roofline

目标：进入 HPC 核心，系统优化 GEMM。

必读：

- BLIS GEMM 设计介绍。
- Intel Optimization Reference Manual：cache blocking。
- Agner Fog matrix optimization examples。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | GEMM FLOPs/layout | naive `ijk` | correctness |
| D2 | loop order | 6 种顺序 | shape 性能表 |
| D3 | roofline | GEMM arithmetic intensity | roofline 分析 |
| D4 | cache blocking | `mc/nc/kc` blocking | block scan |
| D5 | register blocking | scalar microkernel | 汇编 |
| D6 | baseline compare | 对比 BLAS/oneDNN/OpenBLAS 任一 | 差距表 |
| D7 | 报告 | Week 8 report | GEMM v1 |

作业：

- 写 `sgemm_naive`、`sgemm_loop_orders`、`sgemm_blocked`。
- 对至少 10 个 M/N/K shape 测试。
- 写访问模式解释和 roofline。

验收：

- 能解释为什么 `ijk` 通常差。
- 能解释 blocking 的 cache reuse。

## Week 9: GEMM II, Packing, AVX2 Microkernel, MiniBLAS

目标：写出可展示的单线程 SGEMM 学习版。

必读：

- BLIS microkernel/packing 思路。
- Intel Intrinsics Guide：FMA。
- oneDNN/BLIS/OpenBLAS benchmark docs。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | microkernel shape | 设计 4x8 或 6x8 | 寄存器分配图 |
| D2 | AVX2 FMA microkernel | 实现 kernel | correctness |
| D3 | pack B | 实现 B panel packing | 对比数据 |
| D4 | pack A | 实现 A panel packing | 对比数据 |
| D5 | full path | 接 blocking + packing + microkernel | SGEMM v2 |
| D6 | BLAS 对比 | 10 shapes 单线程对比 | 百分比 |
| D7 | 报告 | Week 9 report | MiniBLAS v1 |

作业：

- MiniBLAS：`saxpy/dot/sgemv/sgemm`。
- `sgemm` 至少达到参考 BLAS 单线程 30%-50%；50% 以上优秀。
- 报告必须解释 register blocking、packing、load/FMA 比例、剩余差距。

验收：

- 能解释生产库为什么还更快。
- 能解释 `mr/nr/kc/mc/nc` 的选择。

## Week 10: AI Operators I, Softmax, LayerNorm, GELU, Fusion

目标：掌握 transformer 常用非 GEMM 算子。

必读：

- oneDNN eltwise/layernorm docs。
- PyTorch softmax/layernorm 行为参考。
- 数值稳定 softmax 资料。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | stable softmax | reference + tests | 误差 |
| D2 | softmax optimize | SIMD/reduction/近似 exp 调研 | 性能 |
| D3 | layernorm | two-pass + AVX2 reduction | benchmark |
| D4 | RMSNorm | 实现并对比 layernorm | 数据 |
| D5 | GELU/SiLU | exact vs approximate | 误差/性能 |
| D6 | fusion | bias+GELU 或 residual+layernorm | 流量减少 |
| D7 | 报告 | Week 10 report | MiniDNN op pack v1 |

作业：

- softmax、layernorm、RMSNorm、GELU、SiLU。
- 至少一个 fused operator。
- 对 hidden size / seq_len 扫描。

验收：

- 能解释数值稳定性。
- 能解释 reduction 型算子为什么难达到峰值 FLOP/s。

## Week 11: AI Operators II, Attention, KV Cache, int8 Path

目标：把 matmul、softmax、layout、cache blocking 串成 attention。

必读：

- FlashAttention 思想概要。
- oneDNN matmul docs。
- LLM inference KV cache 基础资料。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | Q/K/V attention | reference single-head | correctness |
| D2 | mask/scale/softmax | causal/non-causal | 测试 |
| D3 | blocked attention | block QK^T 和 PV | benchmark |
| D4 | streaming softmax | 不写回完整 score 简化版 | 内存对比 |
| D5 | KV cache | decode path 访问模式 | 分析 |
| D6 | int8 weight-only 调研 | 简化 int8 GEMV 接入 | 误差/性能 |
| D7 | 报告 | Week 11 report | attention mini-kernel |

作业：

- 实现 attention reference 和 blocked 版本。
- 实现一个 CPU FlashAttention-style 简化版。
- 写中间矩阵写回 bytes 分析。

验收：

- 能解释 attention 不只是两个 GEMM。
- 能解释 KV cache 如何改变访存模式。

## Week 12: perf, VTune, PMU, OS Noise, Hotspot Analysis

目标：用硬件/工具证据修正性能判断。

必读：

- Linux perf wiki。
- `man perf-stat`、`man perf-record`、`man perf-annotate`。
- Intel VTune hotspot/microarchitecture docs。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | PMU/WSL2 限制 | 在当前环境验证 perf 限制 | 环境说明 |
| D2 | 裸机/服务器准备 | 选 3 个 kernel 重跑 | 原始数据 |
| D3 | `perf stat` | cycles/instructions/branches/cache | PMU 表 |
| D4 | `perf record/report` | 采样热点 | flame/hotspot |
| D5 | `perf annotate` | 标注汇编热点 | annotated asm |
| D6 | VTune 或替代 | hotspot/microarchitecture | 工具对比 |
| D7 | 报告 | Week 12 report | PMU 修正案例 |

作业：

- 对 branch、memory、GEMM 三类 kernel 各做一次分析。
- 必须写一个“我原先判断错了，后来用 PMU/汇编修正”的案例。

验收：

- 能解释 IPC 高低不等于好坏。
- 能把 PMU 事件和热点汇编对应。

## Week 13: Parallel CPU, Threads, Atomics, False Sharing, NUMA

目标：掌握多核扩展性和 OS/内存策略。

必读：

- cppreference：`std::thread`、`std::atomic`、memory order。
- Linux CPU affinity docs。
- NUMA/numactl docs。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | thread/work partition | parallel reduction | scaling |
| D2 | affinity | pinning vs no pinning | 数据 |
| D3 | false sharing | 复现 + padding 修复 | 对比 |
| D4 | atomics/mutex/spin | counter benchmark | 成本表 |
| D5 | memory order | relaxed/acq_rel/seq_cst 实验 | 语义说明 |
| D6 | NUMA/huge page | 服务器实验或部署 checklist | 报告 |
| D7 | 报告 | Week 13 report | parallel checklist |

作业：

- parallel reduction。
- false sharing 复现。
- atomic/mutex/spinlock 对比。
- NUMA first-touch checklist。

验收：

- 能解释线程数增加为什么可能变慢。
- 能解释 false sharing 和 cache coherence。

## Week 14: Production Libraries, ISA Dispatch, oneDNN, BLIS, OpenBLAS

目标：读生产库，理解真实高性能工程结构。

必读：

- oneDNN primitive/verbose/matmul docs。
- BLIS framework and microkernel docs。
- OpenBLAS build/kernel docs。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | 生产库结构 | 选 oneDNN/BLIS/OpenBLAS | 阅读计划 |
| D2 | build/benchmark | 编译并跑 benchmark | 版本和参数 |
| D3 | ISA dispatch | 找 dispatch 入口或 verbose | dispatch 记录 |
| D4 | API -> kernel path | 追踪 matmul/GEMM | 调用图 |
| D5 | packing/threading/cache | 找相关源码 | 代码阅读笔记 |
| D6 | 对比自己实现 | 同 shape 跑 MiniBLAS/MiniDNN | 差距表 |
| D7 | 报告 | Week 14 report | 8-12 页阅读报告 |

作业：

- 写生产库阅读报告：API、dispatch、packing、threading、microkernel、benchmark、测试。
- 修改一处日志或增加一个观察点，证明你真正走通路径。

验收：

- 不能只说“库更快”。必须列出至少 8 个工程差距。

## Week 15: Capstone Implementation Sprint

目标：集中完成一个可展示项目。

Capstone 三选一：

- A. MiniBLAS：`saxpy/dot/sgemv/sgemm`，含 AVX2/FMA、dispatch、benchmark。
- B. MiniDNN：softmax/layernorm/GELU/int8 GEMV/attention，含误差和性能。
- C. Performance Postmortem：真实热点分析，包含误判、PMU/汇编证据和优化。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | 需求和指标 | 写 design doc | 项目范围 |
| D2 | correctness | reference + tests | test matrix |
| D3 | benchmark | shape 扫描 + CSV | baseline |
| D4 | optimization 1 | 一个有效优化 | 对比 |
| D5 | optimization 2 | 第二个有效优化 | 对比 |
| D6 | failed optimizations | 两个无效优化 | 失败解释 |
| D7 | 报告初稿 | 汇总数据 | capstone draft |

作业：

- 项目必须一条命令可构建、测试、运行 benchmark。
- 必须包含 baseline、optimized、生产库或权威 reference 对比。
- 必须有汇编/IR/PMU/`llvm-mca` 证据。

验收：

- 另一个工程师能复现实验。
- 报告能解释为什么快、为什么还不够快。

## Week 16: Final Report, Review, Oral Defense Preparation

目标：把 16 周成果整理成 portfolio，而不是一堆散乱实验。

每天任务：

| Day | Knowledge | Lab Work | Required Output |
|---:|---|---|---|
| D1 | 全量复现 | 重跑核心脚本 | reproducibility log |
| D2 | 代码审查 | 清理 API、测试、README | review notes |
| D3 | 性能审查 | 重跑 benchmark、检查噪声 | final data |
| D4 | 报告写作 | 写 12-20 页 master report | report draft |
| D5 | 答辩材料 | 准备 10 分钟讲解 | slides/outline |
| D6 | 模拟问答 | ABI/ASM/cache/SIMD/GEMM/AI/perf 题 | Q&A list |
| D7 | 最终提交 | 归档代码、数据、报告 | final portfolio |

最终作业：

- 12-20 页 master report。
- 可复现命令。
- benchmark 数据。
- 关键汇编/IR/PMU 证据。
- 与生产库或 reference 的对比。
- 下一阶段深入路线。

验收：

- 你能用 10 分钟清楚讲完一个性能优化案例。
- 你能回答：瓶颈是什么、证据是什么、优化为什么有效、为什么还有差距。

## 16 周最低完成清单

必须完成：

1. Lab 00 benchmark harness 和报告。
2. C++ 到汇编：12 个函数，`-O0/-O3` 对比。
3. ELF/symbol/relocation 基础报告。
4. ABI 手写汇编：`add_i64/sum_i64/dot_i32/sum_f32`。
5. 编译器向量化：20 个 loop case。
6. LLVM IR：10 个 C++/IR/ASM 对照。
7. `llvm-mca`：12 条指令和 5 个 loop。
8. cache/TLB/stride/roofline 实验。
9. AVX2/FMA：`saxpy/dot/sum/relu/clamp`。
10. AVX-VNNI/int8：quantize + int8 dot/GEMV。
11. GEMM：naive -> blocked -> packing -> AVX2 microkernel。
12. AI 算子：softmax/layernorm/GELU/attention。
13. perf/VTune：至少一次裸机或服务器分析。
14. 并行：parallel reduction + false sharing。
15. 生产库阅读：oneDNN/BLIS/OpenBLAS 任一。
16. Capstone 和 master report。

## 每天结束检查

每天必须写：

```text
今天读了什么权威资料：
今天写了什么代码：
今天跑了什么测试：
今天生成了什么汇编/IR/PMU/llvm-mca 证据：
今天的性能数据是什么：
今天的失败尝试是什么：
今天还不懂什么：
```

## 不允许的做法

- 不写代码，只看资料。
- 不看汇编，只看运行时间。
- 不写 reference，只写优化版。
- 不做 correctness test，就做 benchmark。
- 不记录输入规模、编译参数、机器环境。
- 在 WSL2 下得出硬件 PMU 结论。
- 浮点/量化优化不写误差。
- GEMM/AI 算子不和 reference 或生产库对比。
