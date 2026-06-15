# 第 0 章：序言和学习契约

## 本章目标

本章建立学习契约：你要学的不是零散技巧，而是一套能支撑高性能计算和 AI CPU 算子优化的完整工程能力。

完成本章后，你应该能回答：

- 为什么高性能优化必须同时理解 C++、编译器、汇编、CPU、OS 和算法？
- 为什么 benchmark 是实验系统，不是简单计时？
- 为什么“能跑得快”不等于“懂性能”？
- 16 周专家冲刺中每周需要交付什么？

## 这本书解决什么问题

很多人学习性能优化时会陷入四种断裂。

第一种断裂：只看 C++，不看编译器生成了什么。  
这种人会说“我写了一个循环”，但不知道编译器有没有向量化、不知道函数有没有 inline、不知道循环里是否还保留了分支、不知道结果是否被优化掉。

第二种断裂：只看汇编，不懂微架构。  
这种人能认出 `vaddps`、`vfmadd231ps`，但不知道 latency、throughput、uops、ports，不知道为什么多累加器能隐藏延迟，不知道为什么 shuffle 比乘法更可能成为瓶颈。

第三种断裂：只看运行时间，不懂测量。  
这种人会说“版本 B 比版本 A 快”，但没有说明输入规模、编译参数、CPU 频率、线程绑定、warmup、统计口径，也没有证明 benchmark 没有被编译器优化掉。

第四种断裂：只会小函数，不会真实 kernel。  
这种人能写 `dot`，但写不出 GEMM blocking、packing、microkernel，也不知道 oneDNN/BLIS/OpenBLAS 生产库为什么复杂。

本书的主线就是修复这些断裂。

## 本书的核心链路

每个性能结论都要尽量沿着下面的链路走：

```text
源码语义
  -> 编译器优化
  -> IR
  -> 汇编
  -> 指令成本
  -> 微架构资源
  -> cache/TLB/branch/OS
  -> benchmark/PMU
  -> 优化设计
```

举例：你写了一个 `dot_f32`。

错误学习方式：

- 跑一下时间。
- 改成 AVX2。
- 如果变快，就结束。

本书要求的学习方式：

- 写 scalar reference。
- 写测试，覆盖随机输入、空输入、非 8 倍数长度。
- 编译 scalar、auto-vectorized、AVX2 三版。
- 看汇编是否有 `vmulps`、`vaddps`、`vfmadd`。
- 用 `llvm-mca` 看吞吐和资源压力。
- 用 benchmark 扫描 L1/L2/L3/DRAM 不同工作集。
- 解释何时 compute-bound，何时 memory-bound。
- 说明浮点累加顺序导致的误差。
- 和编译器自动向量化对比。

这才叫学习一个 kernel。

## 16 周不是终点

16 周专家冲刺的目标是打通主干，而不是穷尽所有细节。

你会在 16 周里接触：

- x86-64 汇编和 ABI。
- LLVM IR 和编译器优化。
- cache、TLB、branch、uops、ports。
- AVX2/FMA、AVX-VNNI、int8。
- GEMM、softmax、layernorm、attention。
- perf/VTune、线程、NUMA、生产库。

但真正达到大师级，需要长期反复做三件事：

- 深挖真实 workload。
- 阅读生产库。
- 用数据修正自己的错误判断。

## 学习契约

你必须遵守以下规则。

### 规则 1：先 correctness，再 performance

所有优化版都必须有 reference。没有 reference，就没有正确性标准。

比如 `softmax`，reference 可以慢，但必须数值稳定：

```text
m = max(x)
y_i = exp(x_i - m) / sum_j exp(x_j - m)
```

优化版必须和 reference 对比误差。

### 规则 2：每个性能数字都必须带环境

性能数字必须至少包含：

- CPU 型号。
- OS/kernel。
- 编译器和版本。
- 编译 flags。
- 输入规模。
- 重复次数。
- 统计口径。
- 是否 WSL2/虚拟机/裸机。

### 规则 3：看汇编

性能优化不能停留在源码。每周至少提交 5 段关键汇编或 IR 证据。

### 规则 4：记录失败实验

高手和初学者最大的区别之一是：高手知道自己哪些优化没用，以及为什么没用。

每周必须记录至少一个失败实验。

### 规则 5：不要神化单个工具

`llvm-mca` 不能预测 cache miss。  
`perf` 可能受权限、采样、事件 multiplexing 影响。  
benchmark 可能被优化掉。  
汇编不告诉你运行时数据分布。  

工具之间要互相校验。

## 本章作业

1. 阅读 `books/cpu-volume-1/materials/x86_64_hpc_ai_16_week_bootcamp.md`。
2. 运行：

```bash
bash books/cpu-volume-1/labs/lab00_benchmark_foundation/scripts/run_lab00.sh
```

3. 创建 `books/cpu-volume-1/reports/week01.md`。
4. 写下你当前最想掌握的 5 个能力。
5. 写下你当前最薄弱的 5 个基础点。
6. 写下你承诺每周提交的内容。

## 验收标准

- 你知道当前唯一执行计划是 16 周专家冲刺版。
- 你能解释为什么性能优化必须看汇编。
- 你能解释为什么 benchmark 需要 correctness reference。

