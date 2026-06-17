# 资源地图：课程、手册、工具和生产库

本文档记录本书参考的公开课程、官方手册、工具文档和生产库。它的作用不是让你在 16 周内把所有资料通读完，而是给每个学习阶段提供高质量外部坐标。

已核对日期：2026-06-13。

## 使用原则

这些资料按三种方式使用：

- 作为概念校准：确认术语、模型和工具用法。
- 作为实验灵感：借鉴经典课程 lab 的训练目标和验收方式。
- 作为深入阅读：当你完成本书章节实验后，再读原始资料补深度。

不要做三件事：

- 不要直接抄公开课程作业答案。
- 不要用厂商手册里的峰值数据替代本机实测。
- 不要同时打开十几门课学习，导致主线断掉。

本书主线仍然是：

```text
本书章节 -> 本仓库实验 -> 报告 -> 外部资料补强
```

## 公开课程和 lab 灵感

### CMU 15-213 / 18-213: Intro to Computer Systems

链接：

- https://www.cs.cmu.edu/afs/cs/academic/class/18213-f25/www/labs.html

公开页面显示这门课的 lab 体系包括 C Programming Lab、Data Lab、Bomb Lab、Attack Lab、Cache Lab、Malloc Lab、Tiny Shell、Proxy Lab 等。它的强项是把 C、汇编、二进制、安全、cache、内存分配、系统调用和网络串成硬核实验链。

本书吸收方式：

- 第 1-3 章吸收其“从 C 到机器表示”的训练精神。
- 第 7 章吸收 Cache Lab 的局部性和 cache simulation 思路。
- 后续 OS/并发章节吸收 shell/proxy/malloc 一类系统实验的质量标准。

不直接复制的原因：

- 我们目标更偏现代 x86-64、C++、HPC 和 AI CPU 算子。
- 我们需要更多 SIMD、GEMM、roofline、operator optimization。

### MIT 6.172: Performance Engineering of Software Systems

链接：

- https://ocw.mit.edu/courses/6-172-performance-engineering-of-software-systems-fall-2018/
- https://ocw.mit.edu/courses/6-172-performance-engineering-of-software-systems-fall-2018/video_galleries/lecture-videos/

MIT OCW 页面把 6.172 描述为 hands-on、project-based 的高性能软件课程，主题覆盖 performance analysis、instruction-level optimization、caching optimization、parallel programming 和 scalable systems。

本书吸收方式：

- 第 1 章 benchmark discipline。
- 第 5-7 章 instruction-level、front-end、cache 证据链。
- 第 11-13 章矩阵乘、cache blocking、并行扩展。
- 第 21 章 capstone 和性能复盘。

### UC Berkeley CS61C

链接：

- https://cs61c.org/
- https://www-inst.eecs.berkeley.edu/~cs61c/su18/labs/08/

CS61C 的优势是把 C、汇编、流水线、cache、虚拟内存、SIMD 和线程并行组织成较完整的 machine structures 路径。其 SIMD lab 强调 intrinsics 和 loop unrolling，和本书第 8-9 章高度相关。

本书吸收方式：

- 第 3 章补强 calling convention 和汇编意识。
- 第 6-7 章补强 pipeline/cache 训练。
- 第 8 章吸收 SIMD intrinsics + unroll 的实验形态。

### Stanford CS149: Parallel Computing

链接：

- https://cs149.stanford.edu/
- https://github.com/stanford-cs149/asst1
- https://cs149.stanford.edu/fall23

CS149 关注现代并行系统的工程取舍。公开 Assignment 1 的目标是帮助学生理解现代多核 CPU 上的 SIMD 和多核并行两类执行形式。

本书吸收方式：

- 第 8-9 章 SIMD。
- 第 18 章多线程、false sharing、scaling。
- 第 21 章最终项目的并行扩展评价方式。

### Georgia Tech CS 6290: High Performance Computer Architecture

链接：

- https://omscs.gatech.edu/cs-6290-high-performance-computer-architecture

官方页面显示课程覆盖 branch prediction、out-of-order execution、cache optimizations、multi-level caches、memory/storage、cache coherence/consistency 和 many-core processors。

本书吸收方式：

- 第 5 章 out-of-order、uops、ports。
- 第 6 章 branch prediction 和 speculation。
- 第 7 章 multi-level cache。
- 第 18 章 coherence、consistency、NUMA。

## x86-64 和 CPU 官方手册

### Intel Software Developer Manuals

链接：

- https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

用途：

- ISA 指令语义。
- 系统编程结构。
- SIMD 指令参考。
- 异常、内存模型、CPUID 等底层主题。

使用方式：

- 查指令语义优先看 SDM。
- 查性能建议再看 Optimization Reference Manual。

### Intel Optimization Reference Manual

链接：

- https://www.intel.com/content/www/us/en/content-details/671488/intel-64-and-ia-32-architectures-optimization-reference-manual-volume-1.html
- https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html

用途：

- Intel CPU 优化指南。
- cache、prefetch、branch、SIMD、memory ordering、microarchitecture 等主题。

注意：

- 手册是重要参考，但不是你机器上的最终性能真相。
- 本书要求手册建议必须通过 benchmark 和 profiling 验证。

### AMD64 Architecture Programmer's Manual

链接：

- https://docs.amd.com/v/u/en-US/40332_4.09_APM_PUB
- https://docs.amd.com/v/u/en-US/24592_3.24
- https://docs.amd.com/v/u/en-US/24594_3.37

用途：

- AMD64 架构、寄存器、指令、系统结构。
- 和 Intel SDM 交叉确认 x86-64/AMD64 基础语义。

### AMD Zen Optimization 和工具

链接：

- https://docs.amd.com/v/u/en-US/58455_1.00
- https://www.amd.com/en/developer/browse-by-product-type/processor-resources.html
- https://www.amd.com/en/developer/uprof.html

用途：

- AMD Zen 系列优化建议。
- AMD uProf 性能分析。
- AMD 平台下的 profiling 和优化路线。

## 指令、intrinsics 和微架构数据

### Intel Intrinsics Guide

链接：

- https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
- https://www.intel.com/content/www/us/en/content-details/794831/intel-intrinsics-guide-download.html

用途：

- 查询 x86 SIMD intrinsic。
- 查看 intrinsic 对应指令、ISA requirement、伪代码和 latency/throughput 数据来源。

本书使用方式：

- 第 8-10 章写 AVX2/FMA/AVX-VNNI 时必查。
- 每个 intrinsic 都要确认 ISA flag，不允许凭名字猜。

### LLVM llvm-mca

链接：

- https://llvm.org/docs/CommandGuide/llvm-mca.html

LLVM 文档说明 `llvm-mca` 使用 LLVM 调度模型对机器码做静态性能分析，关注 throughput 和 processor resource consumption。

本书使用方式：

- 第 5 章开始引入。
- 第 8-12 章分析 SIMD 和 microkernel。
- 结论必须和 benchmark 对照，不允许把静态模型当实测。

### uops.info

链接：

- https://uops.info/
- https://uops.info/table.html
- https://uops.info/background.html

uops.info 提供大量 x86 微架构上的 latency、throughput、uops、port usage 测量数据。

本书使用方式：

- 第 5 章建立指令成本笔记。
- 第 8-12 章查 shuffle、FMA、load/store、VNNI 指令成本。
- 和 `llvm-mca`、Agner Fog、实测结果交叉检查。

### Agner Fog Optimization Resources

链接：

- https://www.agner.org/optimize/
- https://www.agner.org/optimize/microarchitecture.pdf

用途：

- x86 优化手册。
- 微架构说明。
- calling convention。
- instruction tables。

注意：

- 这是高价值研究资料，但很多数据来自作者测量，不是厂商官方承诺。
- 本书把它作为分析参考，不作为唯一事实来源。

## 编译器和工具链文档

### LLVM / Clang

链接：

- https://llvm.org/docs/
- https://clang.llvm.org/docs/
- https://llvm.org/docs/CommandGuide/llvm-mca.html

用途：

- LLVM IR。
- pass pipeline。
- optimization remarks。
- vectorization。
- `llvm-mca`。

本书使用方式：

- 第 4 章 IR 和优化。
- 第 5 章 `llvm-mca`。
- 第 8 章自动向量化。

### GCC

链接：

- https://gcc.gnu.org/onlinedocs/

用途：

- GCC 优化选项。
- vectorization reports。
- target attributes。
- inline asm constraints。

本书使用方式：

- 所有 benchmark 尽量同时对比 Clang 和 GCC。
- 编译器差异必须作为学习对象，而不是噪声。

## 生产库和高性能实现

### BLIS

链接：

- https://github.com/flame/blis
- https://github.com/flame/blis/blob/master/docs/KernelsHowTo.md

BLIS 文档说明 `gemm` microkernel 是 level-3 操作使用的核心小矩阵乘 kernel。

本书使用方式：

- 第 11 章理解 blocking。
- 第 12 章实现 AVX2 microkernel。
- 第 20 章阅读生产库结构。

### oneDNN

链接：

- https://uxlfoundation.github.io/oneDNN/
- https://uxlfoundation.github.io/oneDNN/dev_guide_matmul.html
- https://oneapi-spec.uxlfoundation.org/specifications/oneapi/v1.2-rev-1/elements/onednn/source/primitives/

oneDNN 是面向深度学习应用和框架开发者的高性能库。其 primitive 模型会保存 shape、layout 等参数，并可进行预计算以便后续复用。

本书使用方式：

- 第 14-16 章 AI 算子。
- 第 19 章 primitive cache。
- 第 20 章生产库阅读。

### OpenBLAS

链接：

- https://github.com/OpenMathLib/OpenBLAS

用途：

- BLAS 对比基线。
- GEMM 性能参考。
- 生产级 CPU kernel 组织参考。

本书使用方式：

- 第 11-12 章和自己的 MiniBLAS 对比。
- 第 20 章生产库阅读。

## 推荐阅读顺序

### 第 1-4 章阶段

优先：

- CMU 15-213 lab 页面。
- LLVM/Clang 文档中 IR 和 optimization remarks 相关内容。
- Intel/AMD 架构手册的寄存器、调用、指令基础。

不要过早深读：

- 完整 Intel SDM。
- 完整 Agner Fog。
- 完整生产库源码。

### 第 5-7 章阶段

优先：

- `llvm-mca` 官方文档。
- uops.info 表格。
- Intel Optimization Reference Manual 中与 pipeline、branch、cache 相关章节。
- Agner Fog microarchitecture 手册相关 CPU 章节。

学习动作：

- 每读一条规则，都在本机写 microbenchmark 验证。
- 每查一个指令数据，都用 `llvm-mca` 和实际汇编对照。

### 第 8-10 章阶段

优先：

- Intel Intrinsics Guide。
- CS61C SIMD lab。
- Stanford CS149 Assignment 1。
- uops.info shuffle/gather/VNNI 指令数据。

学习动作：

- 每写一个 intrinsic，都确认汇编。
- 每个 SIMD kernel 都写 scalar reference。

### 第 11-13 章阶段

优先：

- MIT 6.172 matrix multiplication 和 caching optimization 相关材料。
- BLIS microkernel 文档。
- OpenBLAS/BLIS 对比。

学习动作：

- 每个 tile size 都有 benchmark。
- 每个 microkernel 都有汇编审计。

### 第 14-16 章阶段

优先：

- oneDNN developer guide。
- oneDNN matmul、eltwise、softmax、layernorm 相关文档和源码。
- Intel Intrinsics Guide 中 VNNI/int8 相关内容。

学习动作：

- 每个算子先写数值 reference。
- 每个优化版本必须给误差报告。

### 第 17-21 章阶段

优先：

- Linux `perf` 文档。
- Intel VTune 文档。
- AMD uProf 文档。
- oneDNN/BLIS/OpenBLAS 源码。

学习动作：

- 用 profiling 解释 benchmark。
- 用生产库解释自己代码的缺陷。
- 用最终 capstone 汇总证据链。

## 资料使用检查表

每次引用外部资料时，报告里写清：

```text
source:
version/date:
topic used:
what it claims:
how I tested it:
whether my machine agrees:
```

这条规则很重要。成为高手不是背资料，而是能把资料、模型和实测统一起来。
