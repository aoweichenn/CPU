# 从 C++ 到 x86-64、现代 CPU 与 AI 算子优化

副标题：一条面向高性能计算和 AI CPU 算子的系统路径。

这个目录现在作为 Markdown 草稿和实验素材库保留。正式教材书稿已经迁移到：

- `books/cpu-volume-1/source/latex/`
- `books/cpu-volume-1/source/latex/main.tex`

配套执行计划是：

- `books/cpu-volume-1/materials/x86_64_hpc_ai_16_week_bootcamp.md`

本书的目标不是让你“看懂一些汇编”，而是训练你形成完整链路：

```text
C++ 源码语义
  -> 编译器 IR
  -> 优化报告
  -> x86-64 汇编
  -> uops / ports / latency / throughput
  -> cache / TLB / branch / OS noise
  -> benchmark / PMU / perf evidence
  -> kernel design / SIMD / GEMM / AI operators
```

## 读法

每章都必须按顺序完成：

1. 读正文。
2. 跑命令。
3. 写代码。
4. 看汇编或 IR。
5. 运行 benchmark。
6. 写报告。
7. 记录误区。

只读正文不算完成。

## 书籍结构

### Part I: 地基和观察方法

- [第 0 章：序言和学习契约](00-preface.md)
- [第 1 章：可信性能测量](01-measurement.md)
- [第 2 章：C++ 到目标文件、汇编和 ELF](02-cpp-to-binary.md)
- [第 3 章：x86-64 寄存器、调用约定和 ABI](03-x86-64-abi.md)
- [第 4 章：LLVM IR、编译器优化和自动向量化](04-compiler-ir-optimization.md)

### Part II: 现代 CPU 性能模型

- [第 5 章：latency、throughput、uops、ports 和 llvm-mca](05-microarchitecture-performance-model.md)
- [第 6 章：分支预测、front-end 和代码尺寸](06-branch-frontend-code-size.md)
- [第 7 章：cache、TLB、load/store 和 roofline](07-cache-tlb-load-store-roofline.md)

### Part III: SIMD 和低精度计算

- 第 8 章：自动向量化和 AVX2/FMA
- 第 9 章：shuffle、gather、layout 和 SIMD 数据移动
- 第 10 章：AVX-VNNI、int8、量化和 AI 推理路径

### Part IV: HPC kernel

- 第 11 章：GEMM 从 naive 到 blocking
- 第 12 章：packing、AVX2 microkernel 和 MiniBLAS
- 第 13 章：transpose、stencil、reduction 和 memory-bound kernel

### Part V: AI CPU 算子

- 第 14 章：softmax、layernorm、RMSNorm、GELU
- 第 15 章：attention、KV cache 和 streaming softmax
- 第 16 章：operator fusion 和 MiniDNN

### Part VI: 系统、工具和生产库

- 第 17 章：perf、VTune、PMU 和 Top-Down
- 第 18 章：threads、atomics、false sharing、NUMA
- 第 19 章：ISA dispatch、CPUID、primitive cache
- 第 20 章：oneDNN、BLIS、OpenBLAS 生产库阅读
- 第 21 章：最终 Capstone 和性能事故复盘

## 章节固定结构

每章都采用同一结构：

- 本章目标
- 为什么重要
- 核心概念
- 从源码到机器的链路
- 必会命令
- 实验
- 作业
- 常见误区
- 验收标准

## 配套实验

当前已经搭好：

- `books/cpu-volume-1/labs/lab00_benchmark_foundation`

运行：

```bash
bash books/cpu-volume-1/labs/lab00_benchmark_foundation/scripts/run_lab00.sh
```

## 写作推进

书稿按 16 周冲刺计划扩展，但章节组织按系统知识链路展开。详细写作顺序见：

- [写作推进计划](writing-plan.md)
- [资源地图：课程、手册、工具和生产库](resources.md)

当前已完成 Part I 和 Part II 的初稿。先学到第 7 章，再进入 SIMD、GEMM 和 AI 算子章节。

## 质量要求

任何章节作业都必须满足：

- 有 correctness reference。
- 有测试。
- 有运行环境记录。
- 有至少 3 组输入规模。
- 有汇编、IR、`llvm-mca` 或 PMU 证据。
- 有失败实验记录。
- 有清晰报告。
