# 百万字级总目录与扩写蓝图

本文件定义正式 LaTeX 教材的长期扩写边界。目标不是一次写完，而是确保每一章都知道最终要扩到什么深度。

## 总体规模规划

当前主书结构：

- 12 个 Part。
- 64 章。
- 5 个附录。
- 预计最终正文 120 万到 250 万中文字符，随实验和代码讲解扩展。

章节规模分层：

- A 级核心章：3 万到 6 万字，必须完整展开底层机制、实验和作业。
- B 级重要章：1.5 万到 3 万字，必须有完整讲解和实验。
- C 级连接章：8000 到 1.5 万字，负责建立上下文和工程规范。

## Part 0：地基：从 C/C++ 程序到机器执行

目标读者：刚学完 C/C++。

写作要求：不能假设读者懂操作系统、汇编、ELF、CPU pipeline。所有后续术语都要在这里铺底。

### 第 0 章：学习地图：从 C/C++ 到机器、系统和性能

等级：B。

最终要讲：

- 为什么性能优化需要跨层。
- C++、编译器、汇编、CPU、OS、benchmark 的关系。
- 本书证据链。
- 学习方法和报告制度。

### 第 1 章：程序如何从源码变成正在运行的进程

等级：A。

最终要讲：

- 源码、头文件、翻译单元。
- 预处理、编译、汇编、链接。
- ELF、加载器、动态链接器。
- `_start`、runtime、`main`。
- 进程和虚拟地址空间。
- 实验：完整拆解一个程序。

### 第 2 章：CPU 如何执行指令

等级：A。

最终要讲：

- 寄存器、内存、指令、PC/RIP、flags。
- 架构状态和微架构。
- fetch-decode-execute。
- load/store、分支、call/ret。
- pipeline、cache、branch prediction、out-of-order 的动机。
- 实验：GDB 单步、objdump 机器码。

### 第 3 章：数据表示、内存、指针和对象布局

等级：A。

最终要讲：

- bit、byte、hex。
- endian。
- unsigned、signed、two's complement、overflow。
- float bit layout 初步。
- pointer arithmetic。
- array indexing。
- struct padding/alignment。
- aliasing 前置。

### 第 4 章：Linux、工具链、调试器和学习工作流

等级：B。

最终要讲：

- shell 基础。
- GCC/Clang 基础。
- objdump/readelf/nm。
- gdb 单步、寄存器、内存查看。
- 实验记录规范。

## Part 1：系统基础：二进制、链接、进程和操作系统

目标：把操作系统和二进制基础讲到足够支撑 ABI、性能测量和 profiling。

### 第 5 章：构建流程、编译参数和工具链心智模型

等级：B。

覆盖：

- compile driver。
- CMake。
- compile commands。
- build type。
- target ISA flags。
- LTO。

### 第 6 章：ELF、符号、重定位、链接和加载

等级：A。

覆盖：

- ELF header、section、segment。
- symbol table。
- relocation。
- static/shared library。
- PLT/GOT。
- lazy binding。
- loader。

### 第 7 章：进程、虚拟内存和地址空间

等级：A。

覆盖：

- process。
- virtual address。
- page table。
- mmap。
- page fault。
- copy-on-write。
- `/proc/self/maps`。

### 第 8 章：栈、堆、运行时和对象生命周期

等级：A。

覆盖：

- stack frame。
- heap allocator。
- global initialization。
- RAII runtime effect。
- exception unwinding 初步。

### 第 9 章：操作系统噪声、时间源和性能实验边界

等级：B。

覆盖：

- scheduler。
- interrupt。
- context switch。
- TSC。
- chrono。
- WSL2/VM limitations。

## Part 2：x86-64 汇编、ABI 与二进制接口

目标：读者能独立读普通 x86-64 汇编并写简单汇编函数。

### 第 10 章：x86-64 汇编语法总览

等级：A。

覆盖：

- Intel vs AT&T。
- operands。
- registers。
- addressing mode。
- flags。
- basic instructions。

### 第 11 章：整数指令、位运算和地址计算

等级：A。

覆盖：

- add/sub/imul。
- shifts。
- and/or/xor/test。
- lea。
- sign/zero extension。
- branchless masks。

### 第 12 章：控制流：条件跳转、循环、call/ret 和 switch

等级：A。

覆盖：

- cmp/test。
- signed/unsigned jumps。
- loops。
- jump table。
- indirect call。
- function pointer。

### 第 13 章：System V AMD64 ABI

等级：A。

覆盖：

- argument registers。
- return values。
- stack alignment。
- caller/callee saved。
- struct passing。
- vector registers。

### 第 14 章：C++ 与汇编互操作

等级：B。

覆盖：

- `.S`。
- `extern "C"`。
- name mangling。
- inline asm risk。
- testing assembly。

## Part 3：可信性能测量

目标：读者能设计可复现实验，不再相信错误 benchmark。

章节：

- 第 15 章：可信性能测量基础，A。
- 第 16 章：Benchmark 设计，A。
- 第 17 章：性能统计、噪声分析和实验复现，B。
- 第 18 章：Lab 00 深入解剖，B。

必须覆盖：

- DCE。
- warmup。
- input sizes。
- median/min/max。
- environment report。
- correctness reference。
- CSV/report pipeline。

## Part 4：编译器、IR 与优化

目标：读者能理解编译器为什么改写代码，如何读 IR 和优化报告。

章节：

- 第 19 章：编译器流水线，A。
- 第 20 章：LLVM IR、SSA 和控制流图，A。
- 第 21 章：标量优化，A。
- 第 22 章：循环优化，A。
- 第 23 章：自动向量化，A。
- 第 24 章：Alias、UB 和 fast-math，A。

必须覆盖：

- AST/IR。
- SSA/phi。
- DCE/inline/LICM。
- loop dependencies。
- vectorization legality/profitability。
- alias/UB/fast-math。

## Part 5：现代 CPU 微架构性能模型

目标：读者能用 latency、throughput、uops、ports、branch、front-end、OOO 模型解释性能。

章节：

- 第 25 章：Pipeline、前端和后端总览，A。
- 第 26 章：Latency、Throughput、依赖链和 ILP，A。
- 第 27 章：uops、执行端口和 llvm-mca，A。
- 第 28 章：分支预测和推测执行，A。
- 第 29 章：前端带宽、I-cache、uop cache 和代码尺寸，B。
- 第 30 章：乱序执行、寄存器重命名和提交，A。

必须覆盖：

- pipeline。
- uops。
- rename。
- scheduler。
- ROB。
- ports。
- branch prediction。
- front-end bottleneck。

## Part 6：内存层级、TLB 与 Roofline

目标：读者能从数据运动角度分析 kernel。

章节：

- 第 31 章：内存层级，A。
- 第 32 章：Cache line、局部性和 miss 类型，A。
- 第 33 章：TLB、页和地址翻译性能，A。
- 第 34 章：Load/Store、store buffer、prefetch 和写入策略，A。
- 第 35 章：数据布局，A。
- 第 36 章：Roofline，A。

必须覆盖：

- SRAM/DRAM。
- cache line。
- set associativity。
- TLB。
- prefetch。
- write allocate。
- AoS/SoA/AoSoA。
- FLOP/byte。

## Part 7：SIMD、AVX2/FMA 与低精度计算

目标：读者能写、测、审计 AVX2/FMA/VNNI kernel。

章节：

- 第 37 章：SIMD 思维模型，A。
- 第 38 章：AVX2 Intrinsics，A。
- 第 39 章：FMA、归约和浮点误差，A。
- 第 40 章：Shuffle、Permute、Gather 和数据移动，A。
- 第 41 章：Tail、Mask、Alignment 和边界策略，B。
- 第 42 章：int8 量化、AVX-VNNI 和低精度推理，A。

## Part 8：HPC Kernel：从循环到 MiniBLAS

目标：读者能从 naive loop 做到 MiniBLAS。

章节：

- 第 43 章：Loop Nest 性能模型，A。
- 第 44 章：Transpose、Stencil 和 Reduction，A。
- 第 45 章：GEMM 从 naive 到 cache blocking，A。
- 第 46 章：GEMM Packing，A。
- 第 47 章：AVX2 GEMM Microkernel，A。
- 第 48 章：MiniBLAS，A。

## Part 9：AI CPU 算子优化

目标：读者能把 CPU 性能模型用于真实 AI 算子。

章节：

- 第 49 章：AI CPU 算子性能模型，B。
- 第 50 章：Softmax，A。
- 第 51 章：LayerNorm 和 RMSNorm，A。
- 第 52 章：GELU、激活函数和近似计算，B。
- 第 53 章：Attention、KV Cache 和 Streaming Softmax，A。
- 第 54 章：Operator Fusion，A。
- 第 55 章：MiniDNN，A。

## Part 10：工具、并行、NUMA 与生产库

目标：进入真实工程 profiling、多线程和生产库阅读。

章节：

- 第 56 章：perf、PMU 和 Top-Down，A。
- 第 57 章：VTune、AMD uProf，B。
- 第 58 章：线程、Atomics 和内存模型入门，A。
- 第 59 章：False Sharing、Cache Coherence 和 NUMA，A。
- 第 60 章：CPUID、ISA Dispatch 和多版本 Kernel，B。
- 第 61 章：生产库阅读，A。

## Part 11：Capstone

目标：最终项目和大师级性能工程训练。

章节：

- 第 62 章：Capstone 项目设计，B。
- 第 63 章：性能事故复盘方法，B。
- 第 64 章：最终评审，C。

最终项目必须交付：

- correctness tests。
- benchmark suite。
- environment capture。
- assembly audit。
- llvm-mca 或 PMU evidence。
- roofline。
- production baseline comparison。
- performance incident report。

## 写作推进优先级

第一阶段必须扩写：

1. Part 0 全部章节。
2. Part 2 第 10-13 章。
3. Part 3 第 15-18 章。
4. Part 5 第 25-27 章。
5. Part 6 第 31-36 章。

原因：这些是后面 SIMD、GEMM、AI 算子的地基。

第二阶段扩写：

1. Part 4 编译器。
2. Part 7 SIMD。
3. Part 8 GEMM。

第三阶段扩写：

1. Part 9 AI 算子。
2. Part 10 profiling/parallel/production libs。
3. Part 11 capstone。
