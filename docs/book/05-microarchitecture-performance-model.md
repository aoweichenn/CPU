# 第 5 章：latency、throughput、uops、ports 和 llvm-mca

## 本章目标

本章开始进入真正的 CPU 性能模型。

完成本章后，你应该能：

- 区分 ISA 指令和 CPU 内部执行的 micro-op。
- 区分 latency、reciprocal throughput、总耗时、critical path。
- 解释 dependency chain、instruction-level parallelism、out-of-order execution。
- 使用 `llvm-mca` 对一段汇编做静态吞吐分析。
- 解释为什么“指令条数少”不一定快。
- 解释为什么“单条指令 latency 低”不一定让整个 loop 快。
- 把 C++ benchmark、汇编、`llvm-mca` 输出和实际运行结果放在同一张证据表里。

这一章是后续所有高性能 kernel 的语言基础。GEMM、softmax、layernorm、attention、int8 dot product，最后都要落到：

```text
hot loop
  -> instructions
  -> uops
  -> dependencies
  -> ports
  -> load/store
  -> front-end
  -> retire
  -> measured cycles
```

## 为什么只看汇编还不够

汇编是 CPU 执行路径的重要入口，但不是终点。

你写：

```asm
vfmadd231ps ymm0, ymm1, ymm2
```

在 ISA 层面，它是一条 FMA 指令。  
在 CPU 内部，它可能被解码成一个或多个 micro-op，然后送到不同执行端口。这个指令的实际成本取决于：

- 当前 CPU 的实现。
- 输入寄存器依赖是否形成长链。
- 执行端口是否拥塞。
- load/store 是否和计算重叠。
- front-end 能不能持续供给足够 uops。
- loop body 是否能放进 uop cache 或 LSD 类结构。
- 是否发生 branch mispredict、cache miss、TLB miss。

所以性能工程的基本态度是：

```text
源码只是语义。
汇编只是边界。
微架构模型解释为什么。
benchmark 负责裁决。
```

## ISA 指令和 micro-op

ISA 是软件可见的指令集，例如：

```asm
add eax, edx
vmulps ymm0, ymm1, ymm2
vmovups ymm0, ymmword ptr [rdi]
```

CPU 内部通常不会直接以“汇编文本”的形态执行这些指令，而是经过 front-end 解码成内部 micro-op，简称 uop。

一个 ISA 指令可能对应：

- 0 个 uop：例如某些被消除的寄存器 move，在特定 CPU 上可能由 rename 阶段处理。
- 1 个 uop：很多简单整数 ALU 指令。
- 多个 uop：复杂地址计算、内存操作、某些 shuffle、div、函数调用相关序列。
- microcoded 序列：非常复杂或罕见的指令。

读汇编时你不能只数指令条数，还要问：

- 这条指令会产生几个 uops？
- uops 去哪些执行端口？
- 是否需要 load port、store data port、store address port？
- 是否会占用 branch unit？
- 是否有很长 latency？
- 是否限制 throughput？

## CPU pipeline 的简化模型

现代 x86-64 CPU 非常复杂。本章先用一个工程上够用的模型：

```text
instruction bytes
  -> fetch
  -> decode / uop cache
  -> rename / allocate
  -> scheduler / reservation stations
  -> execution ports
  -> load/store subsystem
  -> retire
```

每个阶段都有可能成为瓶颈。

### Fetch

CPU 从 instruction cache 取指令字节。如果代码很大、分支混乱、I-cache miss 很多，后端再强也吃不饱。

### Decode 和 uop cache

x86 指令长度可变，decode 成本不低。现代 CPU 会缓存解码后的 uops。小而稳定的 hot loop 往往能从 uop cache 受益。

### Rename

寄存器重命名把架构寄存器映射到物理寄存器，消除很多假依赖。例如连续写 `eax` 并不一定真的依赖前一次 `eax`。

### Scheduler

已经准备好的 uops 会被调度到执行端口。只要依赖满足，CPU 可以乱序执行。

### Execution ports

不同端口连接不同执行单元，例如整数 ALU、浮点乘法、FMA、load、store、branch。一个循环如果所有 uops 都挤到同一类端口，就会端口拥塞。

### Retire

乱序执行完成后，指令仍然按程序顺序退休。retire 带宽有限，但很多算子瓶颈更早出现在 load/store、端口或 front-end。

## latency

latency 是一个操作从输入 ready 到输出 ready 的延迟。

例如假设某条浮点加法 latency 是 3 cycles。下面这个循环是依赖链：

```cpp
float chain(float x, int n)
{
    for (int i = 0; i < n; ++i) {
        x = x + 1.0f;
    }
    return x;
}
```

每次加法都依赖上一次加法的结果：

```text
x0 -> add -> x1 -> add -> x2 -> add -> x3
```

如果 add latency 是 3 cycles，那么理想情况下也很难做到每 cycle 完成一个有效迭代。critical path 被 latency 限制。

### latency 不是单次函数耗时

初学者容易把 latency 理解成“这条指令需要多少时间执行完”。这不够准确。

现代乱序 CPU 可以同时让很多指令在飞。只要没有依赖，多个操作可以重叠。  
latency 主要影响依赖链，而不是孤立指令的总时间。

## throughput

throughput 描述 CPU 持续执行同类操作的能力。常用 reciprocal throughput 表示：平均每发出一条同类指令需要多少 cycles。

如果某条指令 reciprocal throughput 是 0.5 cycles，意思是理想条件下每 cycle 可以发出 2 条。

看这个循环：

```cpp
void independent(float* a, float* b, float* c, float* d, int n)
{
    for (int i = 0; i < n; ++i) {
        a[i] = a[i] + 1.0f;
        b[i] = b[i] + 2.0f;
        c[i] = c[i] + 3.0f;
        d[i] = d[i] + 4.0f;
    }
}
```

四条更新彼此独立。CPU 可以把它们重叠执行。此时性能更可能受 throughput、load/store 带宽、端口压力限制，而不是单条 add latency。

## critical path

critical path 是决定最短执行时间的最长依赖路径。

例如：

```cpp
int f(int a, int b, int c, int d)
{
    int x = a + b;
    int y = c + d;
    return x * y;
}
```

依赖图：

```text
a,b -> add -> x
c,d -> add -> y
x,y -> mul -> result
```

两条 add 可以并行，mul 必须等两个 add 都完成。critical path 不是“3 条指令相加”，而是：

```text
max(add latency, add latency) + mul latency
```

优化 hot loop 时，你需要画出关键依赖链。很多高手不是凭感觉优化，而是在脑中维护这张图。

## dependency chain 的常见来源

### 标量累加

```cpp
float sum(float const* x, int n)
{
    float s = 0.0f;
    for (int i = 0; i < n; ++i) {
        s += x[i];
    }
    return s;
}
```

`s` 是 loop-carried dependency。每次迭代依赖上一轮。

改写成多累加器：

```cpp
float sum4(float const* x, int n)
{
    float s0 = 0.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;
    float s3 = 0.0f;
    int i = 0;
    for (; i + 3 < n; i += 4) {
        s0 += x[i + 0];
        s1 += x[i + 1];
        s2 += x[i + 2];
        s3 += x[i + 3];
    }
    float s = (s0 + s1) + (s2 + s3);
    for (; i < n; ++i) {
        s += x[i];
    }
    return s;
}
```

这不是为了减少加法条数，而是为了把一条长依赖链拆成四条较短链，提高 ILP。

### 地址依赖

链表遍历：

```cpp
while (node != nullptr) {
    sum += node->value;
    node = node->next;
}
```

下一次 load 的地址依赖当前 load 的结果。CPU 很难提前发起后续 load。这类 pointer chasing 常常被 memory latency 限制。

### 分支依赖

```cpp
if (x[i] > threshold) {
    sum += x[i];
}
```

如果分支难预测，会破坏流水线。即使分支可预测，控制依赖也可能影响 vectorization 和 front-end。

## instruction-level parallelism

Instruction-level parallelism，简称 ILP，指同一线程内可以并行执行的独立指令数量。

增加 ILP 的常见方法：

- 使用多个累加器。
- 循环展开。
- 把独立工作交错排列。
- 减少 loop-carried dependency。
- 避免不必要的内存依赖。
- 使用 SIMD 一条指令处理多个元素。

但 ILP 不是越多越好。代价包括：

- 更多寄存器压力。
- 更多代码尺寸。
- 更高 front-end 压力。
- 更复杂的尾处理。
- 可能破坏 cache locality。

性能优化是约束下的平衡，不是把所有技巧堆上去。

## port pressure

执行端口可以理解为后端执行资源的入口。不同 uops 需要不同端口。

一个简化例子：

```asm
vaddps ymm0, ymm0, ymm1
vaddps ymm2, ymm2, ymm3
vaddps ymm4, ymm4, ymm5
vaddps ymm6, ymm6, ymm7
```

如果浮点加法只能使用特定端口，这些 uops 会竞争那些端口。

再看包含内存的版本：

```asm
vaddps ymm0, ymm0, ymmword ptr [rdi]
```

它既需要执行浮点加法，也需要 load 资源。即使 FMA 单元还有空闲，load ports 也可能满了。

所以看 SIMD kernel 时必须问：

- 每迭代需要多少 load？
- 每迭代需要多少 store？
- 每迭代需要多少 FMA/add/mul？
- load/store 和 compute 的比例是多少？
- 哪类端口最忙？

## llvm-mca 是什么

`llvm-mca` 是 LLVM 提供的机器码分析工具。它从汇编出发，基于目标 CPU 的调度模型估计：

- uop 数量。
- block throughput。
- 指令 latency。
- resource pressure。
- timeline。
- bottleneck hint。

它非常适合学习和初步分析 hot loop。

但必须记住：

```text
llvm-mca 是静态模型，不是实测。
它不模拟真实 cache miss、TLB miss、OS 调度、频率变化、分支历史、数据相关异常路径。
```

正确用法是：

```text
llvm-mca 给假设和解释。
benchmark 给事实。
两者不一致时，继续找缺失因素。
```

## 生成可分析汇编

创建 scratch 文件：

```bash
mkdir -p scratch/ch05
```

写 `scratch/ch05/saxpy.cpp`：

```cpp
#include <cstddef>

extern "C" void saxpy(
    float* __restrict y,
    float const* __restrict x,
    float a,
    std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) {
        y[i] += a * x[i];
    }
}
```

生成汇编：

```bash
clang++ -O3 -march=native -S -masm=intel \
  scratch/ch05/saxpy.cpp -o scratch/ch05/saxpy.s
```

查看：

```bash
sed -n '/saxpy:/,/\\.Lfunc_end/p' scratch/ch05/saxpy.s
```

如果输出太多，先找 vector loop 标签：

```bash
rg -n "saxpy:|vfmadd|vmulps|vaddps|vmov" scratch/ch05/saxpy.s
```

## 给 llvm-mca 准备 loop body

`llvm-mca` 可以直接吃 `.s`，但学习时建议你抽取 hot loop body。创建 `scratch/ch05/saxpy_loop.s`：

```asm
.intel_syntax noprefix
.text
.globl kernel
kernel:
    vmovups ymm1, ymmword ptr [rsi + rax]
    vfmadd213ps ymm1, ymm0, ymmword ptr [rdi + rax]
    vmovups ymmword ptr [rdi + rax], ymm1
    add rax, 32
    cmp rdx, rax
    jne kernel
    ret
```

这段不一定和你的编译器输出完全一致。它是一个教学 loop body：

- load `x`。
- FMA with memory operand using `y`。
- store `y`。
- index 加 32 字节。
- loop branch。

运行：

```bash
llvm-mca -mcpu=native -iterations=100 scratch/ch05/saxpy_loop.s
```

如果 `-mcpu=native` 在你的 LLVM 版本不可用，先查可用 CPU：

```bash
llvm-mca --version
llc -mcpu=help 2>&1 | sed -n '/Available CPUs/,/Available features/p'
```

然后选择最接近本机的模型。

## 读 llvm-mca 输出

你重点看几块。

### Summary

典型字段：

```text
Iterations
Instructions
Total Cycles
Total uOps
Block RThroughput
```

`Block RThroughput` 是静态估计的 steady-state block throughput。  
它不是“这段代码实际运行几 cycles”，而是理想稳态下每次 block 的吞吐下界估计。

### Instruction Info

重点字段：

```text
Latency
RThroughput
uOps
```

对于每条指令，你要记录：

- 它产生几个 uops。
- latency 是否很高。
- reciprocal throughput 是否限制持续发射。

### Resource Pressure

这部分告诉你每次迭代对不同执行资源的压力。

学习阶段不用死记端口编号。你要训练的是判断：

- load 资源是否重。
- store 资源是否重。
- FP/FMA 资源是否重。
- branch/整数资源是否重。
- 是否有单个资源明显成为瓶颈。

### Timeline

Timeline 显示 uops 如何随周期流动。它适合观察依赖链：

- 如果同一类指令被迫一条接一条等待，可能是 latency chain。
- 如果很多指令 ready 但排队，可能是资源压力。
- 如果前端供给不足，会看到发射受限。

## 实验 A：latency chain 和多累加器

写 `scratch/ch05/sum_chains.cpp`：

```cpp
#include <cstddef>

extern "C" float sum1(float const* x, std::size_t n)
{
    float s = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        s += x[i];
    }
    return s;
}

extern "C" float sum4(float const* x, std::size_t n)
{
    float s0 = 0.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;
    float s3 = 0.0f;
    std::size_t i = 0;
    for (; i + 3 < n; i += 4) {
        s0 += x[i + 0];
        s1 += x[i + 1];
        s2 += x[i + 2];
        s3 += x[i + 3];
    }
    float s = (s0 + s1) + (s2 + s3);
    for (; i < n; ++i) {
        s += x[i];
    }
    return s;
}
```

命令：

```bash
clang++ -O3 -march=native -S -masm=intel \
  scratch/ch05/sum_chains.cpp -o scratch/ch05/sum_chains.s

rg -n "sum1:|sum4:|addss|vaddss|vaddps|ymm|xmm" scratch/ch05/sum_chains.s
```

任务：

1. 对比 `sum1` 和 `sum4` 的汇编。
2. 判断编译器是否自动向量化。
3. 如果没有向量化，解释为什么。
4. 如果向量化了，找出 scalar tail。
5. 估计 `sum4` 为什么可能比 `sum1` 快。

注意：浮点求和改变顺序会改变舍入结果。性能优化必须写清楚数值误差约束。

## 实验 B：吞吐受限和端口压力

写 `scratch/ch05/fma_pressure.cpp`：

```cpp
#include <cstddef>

extern "C" void fma1(
    float* __restrict y,
    float const* __restrict a,
    float const* __restrict b,
    float const* __restrict c,
    std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = a[i] * b[i] + c[i];
    }
}
```

生成汇编和报告：

```bash
clang++ -O3 -march=native -ffp-contract=fast -S -masm=intel \
  scratch/ch05/fma_pressure.cpp -o scratch/ch05/fma_pressure.s

rg -n "fma1:|vfmadd|vmulps|vaddps|vmovups" scratch/ch05/fma_pressure.s
```

抽取 vector loop，用 `llvm-mca` 分析。

报告里必须回答：

- 是否生成 FMA？
- 每个 vector 元素需要多少 load？
- 每个 vector 元素需要多少 store？
- 估计瓶颈更像 load/store 还是 FMA？
- 如果把 `c[i]` 改成常数，瓶颈预测如何变化？

## 实验 C：指令条数更少但不一定更快

比较两种写法：

```cpp
extern "C" int div_by_8(int x)
{
    return x / 8;
}

extern "C" int shift_by_3(int x)
{
    return x >> 3;
}
```

命令：

```bash
clang++ -O3 -S -masm=intel scratch/ch05/div_shift.cpp -o scratch/ch05/div_shift.s
objdump -drwC -Mintel scratch/ch05/div_shift.o
```

你会遇到一个细节：有符号除法向 0 截断，而算术右移向负无穷方向靠近。编译器不能随便把 `x / 8` 对所有 `int` 替换成简单 `sar`，除非它处理负数语义。

任务：

- 看编译器为 `div_by_8` 生成了什么。
- 写无符号版本再比较。
- 解释 C++ 语义如何限制指令选择。
- 说明为什么优化不是“用某条汇编替换另一条汇编”这么简单。

## 实验 D：和 Lab 00 benchmark 连接

你需要把本章 kernel 接入 Lab 00 的思想，而不是只跑一次。

要求：

- 每个 kernel 至少测 3 个规模。
- 每个规模至少 30 次正式测量。
- 输出 median、min、max。
- 保存编译器版本、CPU 信息、编译参数。
- 写 correctness reference。
- 对至少一个 kernel 给出汇编和 `llvm-mca` 分析。

你可以复用：

```bash
bash tools/env_report.sh
```

并参考：

```bash
labs/lab00_benchmark_foundation
```

## 作业 5.1：建立你的指令成本笔记

选择 12 条你后续会频繁见到的指令：

- `add`
- `imul`
- `lea`
- `cmp`
- `cmov`
- `vmovups`
- `vaddps`
- `vmulps`
- `vfmadd231ps`
- `vperm2f128`
- `vpshufd`
- `vgatherdps`

对每条指令建立表格：

```text
instruction
operand shape
uops from llvm-mca
latency from llvm-mca
RThroughput from llvm-mca
resource pressure summary
notes
```

要求：

- 至少包含 3 种 operand shape，例如寄存器到寄存器、内存到寄存器、立即数。
- 不要求背数值，但要求解释差异。
- 对比一条标量指令和一条 SIMD 指令的吞吐含义。

## 作业 5.2：多累加器求和研究

实现：

- `sum1`
- `sum2`
- `sum4`
- `sum8`
- 编译器自动向量化版本
- 手动 AVX2 版本，后续第 8 章可补全

当前阶段如果还不会 intrinsics，先写前四个标量版本。

测试：

- 输入全 1。
- 输入递增小数。
- 输入随机数。
- 输入包含非常大和非常小的数。

benchmark：

- `n = 1024`
- `n = 16384`
- `n = 1048576`
- `n = 16777216`

报告：

- 运行时间。
- GB/s。
- 每元素 ns。
- 汇编差异。
- 是否自动向量化。
- 数值误差。
- 你认为的瓶颈。

高质量答案必须解释：为什么某个版本在小规模快，但大规模不再扩大优势。

## 作业 5.3：llvm-mca 与实测冲突分析

选择一个 kernel，让 `llvm-mca` 预测它吞吐很好，但实测没有那么好。

可能选项：

- 连续数组加法。
- stride load。
- gather。
- pointer chasing。
- 带难预测分支的 filter。

报告结构：

```text
1. kernel 源码
2. 编译命令
3. hot loop 汇编
4. llvm-mca 结论
5. benchmark 结果
6. 冲突点
7. 可能缺失因素
8. 下一步验证实验
```

你必须至少提出 3 个缺失因素，例如：

- cache miss。
- TLB miss。
- 分支预测失败。
- 编译器生成了不同版本 loop。
- benchmark 测到了初始化成本。
- CPU 频率变化。
- WSL2/虚拟化限制。

## 常见误区

### 误区 1：指令越少越快

错误。复杂指令可能有更多 uops、更高 latency、更低 throughput。

### 误区 2：latency 表能直接预测函数耗时

错误。latency 主要约束依赖链。独立操作可以重叠。

### 误区 3：llvm-mca 输出就是实际性能

错误。它是静态模型。真实机器还受内存、分支、OS、频率影响。

### 误区 4：循环展开越多越好

错误。展开会增加代码尺寸和寄存器压力。超过某个点后可能变慢。

### 误区 5：看到 FMA 就说明性能好

错误。FMA 只是计算能力，kernel 可能被 load/store、cache、TLB 或 front-end 限制。

## 验收标准

本章完成标准：

- 你能解释 latency 和 throughput 的差别，并举出依赖链例子。
- 你能从汇编中找出 hot loop。
- 你能运行 `llvm-mca` 并解释 Summary、Instruction Info、Resource Pressure。
- 你能说清一个 kernel 是更像 dependency-limited、port-limited、memory-limited 还是 front-end-limited。
- 你完成作业 5.2，并写出不少于 1000 字的实验报告。
- 你的报告至少包含一段汇编、一段 `llvm-mca` 输出摘要和 benchmark 表格。

## 本章之后你应该形成的习惯

以后每看到一个性能问题，不要直接改代码。先建立四层证据：

```text
source: 源码语义和数据规模
assembly: hot loop 指令
model: uops / ports / latency / throughput
measurement: benchmark / perf / PMU
```

只有这四层能互相解释，优化结论才站得住。
