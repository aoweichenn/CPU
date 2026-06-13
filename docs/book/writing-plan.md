# 写作推进计划

本文档定义本书后续扩展顺序。它不是学习计划的替代品；学习节奏仍以 `docs/x86_64_hpc_ai_16_week_bootcamp.md` 为准。本文档用于保证书稿持续按高密度、可实验、可验收的方式推进。

## 写作原则

每一章都必须交付：

- 一个清晰的性能问题。
- 一个从 C++ 到 IR/汇编/机器模型的解释链路。
- 至少 3 个可运行实验。
- 至少 3 个高质量作业。
- 至少一个报告模板或报告结构。
- 常见误区。
- 验收标准。

每一章都避免只写概念。所有核心知识必须落到：

```text
source code
compiler output
assembly
benchmark
model
report
```

## 当前完成状态

已完成初稿：

- 第 0 章：序言和学习契约。
- 第 1 章：可信性能测量。
- 第 2 章：C++ 到目标文件、汇编和 ELF。
- 第 3 章：x86-64 寄存器、调用约定和 ABI。
- 第 4 章：LLVM IR、编译器优化和自动向量化。
- 第 5 章：latency、throughput、uops、ports 和 `llvm-mca`。
- 第 6 章：分支预测、front-end 和代码尺寸。
- 第 7 章：cache、TLB、load/store 和 roofline。

## 下一阶段：SIMD 和低精度计算

### 第 8 章：自动向量化和 AVX2/FMA

核心问题：

- SIMD 为什么快。
- 自动向量化需要哪些前提。
- AVX2 vector width、lane、alignment、tail 如何处理。
- intrinsics 如何映射到汇编。
- FMA 如何影响吞吐、精度和编译器选择。

必须包含实验：

- scalar add vs auto-vectorized add vs AVX2 intrinsic add。
- dot product scalar vs multi-accumulator vs AVX2 FMA。
- ReLU scalar branch vs `vmaxps`。
- tail 策略对小 `n` 的影响。

高质量作业：

- 写一个 AVX2 vector add 家族，覆盖 unaligned、aligned、tail、small-size path。
- 写一个 AVX2 dot product，并解释数值误差。
- 用 `llvm-mca` 分析 AVX2 loop 的端口压力。

### 第 9 章：shuffle、gather、layout 和 SIMD 数据移动

核心问题：

- SIMD 不只计算，数据移动经常更贵。
- lane 内 shuffle、lane 间 permute、broadcast、blend、unpack 的成本。
- gather 为什么通常昂贵。
- layout 如何决定 SIMD kernel 复杂度。

必须包含实验：

- AoS 到 SoA 转换。
- 4x4/8x8 transpose microkernel。
- gather vs contiguous load。
- deinterleave/interleave。

高质量作业：

- 优化 RGB/RGBA 简化图像处理。
- 写小型 transpose kernel 并用 `llvm-mca` 分析 shuffle 压力。
- 对一个糟糕 layout 提出数据布局重构方案。

### 第 10 章：AVX-VNNI、int8、量化和 AI 推理路径

核心问题：

- int8 推理为什么需要量化。
- scale、zero-point、saturation、rounding。
- dot product accumulation 到 int32。
- AVX-VNNI 指令路径和非 VNNI fallback。
- 精度误差和性能收益如何共同评估。

必须包含实验：

- int8 dot product 标量版。
- int8 dot product AVX2 fallback。
- AVX-VNNI 版本。
- quantize/dequantize microbenchmark。

高质量作业：

- 写一个 int8 GEMV microkernel。
- 对比 fp32 GEMV 与 int8 GEMV。
- 写量化误差报告，包括 max error、mean error、相对误差。

## 下一阶段：HPC kernel

### 第 11 章：GEMM 从 naive 到 blocking

核心问题：

- 为什么 naive GEMM 浪费 cache。
- loop order 如何改变 locality。
- blocking 如何提高 arithmetic intensity。
- tile size 如何与 cache 容量相关。

必须包含实验：

- `ijk`、`ikj`、`jik` loop order。
- L1/L2 block size 扫描。
- transpose B vs 不 transpose。
- roofline 解释 GEMM 结果。

高质量作业：

- 实现 blocked GEMM。
- 写 tile size 搜索报告。
- 对比 OpenBLAS 或系统 BLAS，解释差距来自哪里。

### 第 12 章：packing、AVX2 microkernel 和 MiniBLAS

核心问题：

- 为什么 production GEMM 需要 packing。
- register blocking。
- microkernel 的 MR/NR 设计。
- FMA latency hiding。
- packed panel layout。

必须包含实验：

- 4x8 或 6x8 AVX2 fp32 microkernel。
- A/B packing。
- edge kernel。
- 与 naive/blocked/OpenBLAS 对比。

高质量作业：

- 构建 MiniBLAS `sgemm`。
- 写 microkernel 汇编审计报告。
- 写性能差距分析：compute、load/store、packing overhead、edge overhead。

### 第 13 章：transpose、stencil、reduction 和 memory-bound kernel

核心问题：

- memory-bound kernel 的优化逻辑不同于 GEMM。
- transpose 的读写局部性。
- stencil 的 cache reuse 和边界处理。
- reduction 的依赖链和并行归约。

必须包含实验：

- blocked transpose。
- 1D/2D stencil。
- reduction scalar/SIMD/parallel。
- non-temporal store 初步实验。

高质量作业：

- 优化一个 memory-bound kernel，并用 roofline 证明改进空间。
- 写一个 stencil tile 设计文档。
- 对比不同 reduction tree 的精度和性能。

## 下一阶段：AI CPU 算子

### 第 14 章：softmax、layernorm、RMSNorm、GELU

核心问题：

- reduction、数值稳定性、SIMD、内存流量。
- softmax 的 max/sum 两遍结构。
- layernorm 的 mean/variance。
- GELU 近似和向量化。

必须包含实验：

- stable softmax。
- layernorm scalar/SIMD。
- RMSNorm。
- GELU tanh approximation 或 polynomial approximation。

高质量作业：

- 写 MiniOps normalization 模块。
- 写数值稳定性测试。
- 对比 PyTorch/NumPy 输出误差。

### 第 15 章：attention、KV cache 和 streaming softmax

核心问题：

- attention 的 FLOP/byte 模型。
- QK、softmax、PV 三段瓶颈。
- KV cache layout。
- streaming softmax 避免 materialize 大矩阵。

必须包含实验：

- small attention reference。
- streaming softmax。
- KV cache stride/layout benchmark。
- prefill vs decode 对比。

高质量作业：

- 写单头 attention CPU kernel。
- 写 KV cache layout 设计文档。
- 分析 batch、seq_len、head_dim 对瓶颈的影响。

### 第 16 章：operator fusion 和 MiniDNN

核心问题：

- fusion 节省内存流量。
- fusion 增加代码尺寸和寄存器压力。
- epilogue fusion：bias、activation、scale。
- primitive cache 和 shape specialization。

必须包含实验：

- matmul + bias。
- matmul + bias + ReLU/GELU。
- layernorm + residual。
- fusion vs separate kernels。

高质量作业：

- 实现 MiniDNN 的 primitive interface。
- 写 shape-specialized kernel cache。
- 写 fusion 是否值得的决策报告。

## 下一阶段：系统、工具和生产库

### 第 17 章：perf、VTune、PMU 和 Top-Down

核心问题：

- PMU event 如何解释。
- Top-Down：front-end bound、bad speculation、back-end bound、retiring。
- WSL2 限制和裸机 Linux 的差异。
- profiling 采样与 microbenchmark 的关系。

必须包含实验：

- `perf stat`。
- `perf record/report`。
- VTune 或替代工具流程。
- flame graph 初步。

高质量作业：

- 对前面一个 kernel 做完整性能事故复盘。
- 写 PMU 证据链。
- 解释静态模型、benchmark、PMU 三者不一致的原因。

### 第 18 章：threads、atomics、false sharing、NUMA

核心问题：

- 线程并行不是自动加速。
- false sharing。
- atomics 和 memory order。
- work partition。
- NUMA locality。

必须包含实验：

- parallel reduction。
- false sharing benchmark。
- producer/consumer 简化实验。
- thread pinning 和 chunk size。

高质量作业：

- 写多线程 GEMM 或 reduction。
- 做 scaling curve。
- 解释为什么某个版本扩展性不好。

### 第 19 章：ISA dispatch、CPUID、primitive cache

核心问题：

- 运行时检测 CPU feature。
- 多版本函数。
- ABI 稳定和 dispatch overhead。
- primitive cache。

必须包含实验：

- CPUID feature dump。
- scalar/AVX2/AVX-VNNI dispatch。
- first-call overhead。
- primitive cache hit/miss。

高质量作业：

- 为 MiniOps/MiniBLAS 加 ISA dispatch。
- 写 fallback 策略。
- 写测试确保不同 ISA 输出一致。

### 第 20 章：oneDNN、BLIS、OpenBLAS 生产库阅读

核心问题：

- 真实库如何组织 kernel。
- packing、blocking、threading、dispatch。
- primitive descriptor。
- production code 和教学代码的差异。

必须包含实验：

- 阅读一个 BLIS GEMM path。
- 阅读一个 oneDNN primitive path。
- 对比自己的 MiniBLAS。

高质量作业：

- 写生产库阅读报告。
- 追踪一次 matmul 调用路径。
- 列出自己实现缺失的 10 个工程问题。

### 第 21 章：最终 Capstone 和性能事故复盘

核心问题：

- 把所有知识合成一个工程项目。

可选项目：

- MiniBLAS fp32 SGEMM。
- MiniOps normalization + attention。
- int8 GEMV/GEMM 推理 kernel。
- memory-bound kernel 优化套件。

最终交付：

- 正确性测试。
- benchmark 套件。
- 汇编审计。
- `llvm-mca` 分析。
- PMU 或替代 profiling。
- roofline。
- 与生产库对比。
- 完整性能复盘。

## 每次扩章节的工作流

后续继续写书时，按这个顺序：

1. 先确定章节核心问题。
2. 写本章目标和为什么重要。
3. 写核心机制，不跳过必要底层细节。
4. 写能独立运行的 scratch 实验。
5. 写高质量作业，至少一个必须能产出报告。
6. 写常见误区和验收标准。
7. 更新 `docs/book/README.md` 链接。
8. 检查占位词和断链。

检查命令：

```bash
find docs/book -maxdepth 1 -type f | sort
wc -l docs/book/*.md
rg -n "TODO|TBD|FIXME|待补|占位" docs/book README.md docs/x86_64_hpc_ai_16_week_bootcamp.md
```
