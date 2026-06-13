# 第 6 章：分支预测、front-end 和代码尺寸

## 本章目标

本章研究 CPU 前端和控制流。

完成本章后，你应该能：

- 解释 branch prediction 为什么决定很多代码的稳定性。
- 区分可预测分支、难预测分支和数据相关分支。
- 理解 branch mispredict 的代价为什么不是一个固定常数。
- 比较 branch、branchless、`cmov`、mask/SIMD 的取舍。
- 解释 front-end、I-cache、decode、uop cache 对 hot loop 的影响。
- 识别过度 inline、过度 unroll、模板膨胀带来的代码尺寸问题。
- 为 AI/HPC kernel 设计合理的主循环、尾处理和边界处理策略。

上一章关注后端执行资源，本章关注 CPU 能不能稳定地把正确的指令喂给后端。

## 为什么 front-end 很重要

很多人学习性能时先盯着 ALU、FMA、cache，但忽略 front-end。

CPU 后端再强，如果前端供不上 uops，执行单元就会空转。典型原因包括：

- 分支预测失败。
- instruction cache miss。
- decode 带宽不足。
- uop cache 容量或命中问题。
- 代码尺寸过大。
- indirect call 目标不稳定。
- 过度 inline 导致 hot path 和 cold path 混在一起。

对 HPC 和 AI 算子来说，front-end 问题常见于：

- 过度泛化的模板库。
- ISA dispatch 写得太细碎。
- fused operator 代码巨大。
- tail 和边界条件散落在主循环中。
- 小 batch、小 shape 下循环短，分支和调度开销占比高。
- 稀疏、mask、过滤类算子数据相关分支很多。

## 分支预测的基本模型

CPU 遇到条件跳转时，不会等条件真正算完才继续取指。它会预测分支方向和目标地址。

如果预测正确：

```text
pipeline continues
```

如果预测错误：

```text
speculative work discarded
front-end redirects
pipeline refills
lost cycles
```

预测失败的代价取决于：

- CPU pipeline 深度。
- 错误分支发现得早还是晚。
- 后续 speculative work 做了多少。
- front-end 重新取指是否命中 I-cache/uop cache。
- 乱序窗口里是否还有其他可完成工作。

所以不要死记“mispredict 等于 N cycles”。工程上要测。

## 可预测分支

看：

```cpp
int count_positive_sorted(int const* x, int n)
{
    int count = 0;
    for (int i = 0; i < n; ++i) {
        if (x[i] > 0) {
            ++count;
        }
    }
    return count;
}
```

如果数组先全负后全正，分支模式很规律。预测器能学到趋势，代价可能很低。

又如：

```cpp
for (int i = 0; i < n; ++i) {
    if (i + 8 <= n) {
        ...
    }
}
```

这种边界分支通常高度可预测，但如果它在主循环中每次都出现，仍然会增加指令和前端压力。

## 难预测分支

如果条件接近随机：

```cpp
if ((x[i] & 1) != 0) {
    sum += x[i];
}
```

且 `x[i]` 的低位随机，那么分支方向接近 50/50。分支预测器很难稳定预测。

这时 branchless 写法可能更好：

```cpp
sum += ((x[i] & 1) != 0) ? x[i] : 0;
```

编译器可能生成：

- `cmov`
- `setcc`
- mask 运算
- SIMD compare + blend

但 branchless 不是无条件更快。它会把“有时执行”的工作变成“总是执行”的工作。

## branch 和 branchless 的核心取舍

比较：

```cpp
if (cond) {
    y = expensive(x);
}
```

和：

```cpp
auto value = expensive(x);
y = cond ? value : y;
```

如果 `cond` 大部分为 false，且 `expensive(x)` 很贵，那么 branchless 可能做了大量无用计算。

如果 `cond` 随机，且两边计算很便宜，那么 branchless 可能更稳定。

你要判断：

- 分支是否可预测？
- true/false 比例是多少？
- 两边计算成本是否对称？
- 分支体是否有内存访问？
- branchless 是否增加寄存器压力？
- branchless 是否帮助 vectorization？

## `cmov` 的位置

`cmov` 是条件移动。它避免控制流跳转，但引入数据依赖。

例子：

```cpp
extern "C" int max_i32(int a, int b)
{
    return a > b ? a : b;
}
```

编译器常生成：

```asm
cmp edi, esi
mov eax, esi
cmovg eax, edi
ret
```

`cmov` 适合小选择，不适合替代所有分支。

如果分支体很大，`cmov` 不适用。  
如果条件结果本身很晚才 ready，`cmov` 会延长数据依赖链。  
如果分支高度可预测，普通 branch 可能更便宜。

## SIMD mask 和 blend

在 SIMD 中，branchless 更常见。

例如 ReLU：

```cpp
y[i] = x[i] > 0.0f ? x[i] : 0.0f;
```

AVX2 可以生成类似：

```asm
vmaxps ymm0, ymm0, ymm_zero
```

或者 compare + blend：

```asm
vcmpgtps ymm_mask, ymm_x, ymm_zero
vblendvps ymm_y, ymm_zero, ymm_x, ymm_mask
```

对 AI 算子来说，mask 是核心机制：

- ReLU。
- GELU 近似中的条件路径。
- attention mask。
- padding mask。
- tail processing。
- quantization saturation。

关键问题不是“有没有分支”，而是 mask 操作是否比控制流更适合当前数据布局和 ISA。

## indirect branch 和函数指针

直接调用：

```cpp
y[i] = f(x[i]);
```

如果 `f` 可 inline，编译器可能完全展开优化。

函数指针或虚函数：

```cpp
y[i] = op->apply(x[i]);
```

可能生成 indirect call。它的问题：

- 目标地址运行时才知道。
- 难 inline。
- 分支预测目标可能不稳定。
- call/ret 和 ABI 边界增加开销。
- 阻碍 vectorization。

生产级 AI/HPC 库通常会把 dispatch 放在外层：

```text
once:
  choose avx2 implementation

hot loop:
  call selected kernel with simple inner loop
```

不要在每个元素、每个小 tile 内做复杂动态派发。

## front-end 的简化模型

本章使用如下模型：

```text
L1 instruction cache
  -> fetch bytes
  -> decode x86 instructions
  -> uop cache / decoded stream
  -> deliver uops to backend
```

前端瓶颈的常见信号：

- hot loop 指令很多，但后端资源并不满。
- 过度 unroll 后性能变差。
- `-O3` 比 `-O2` 更慢。
- inline 后更慢。
- 模板版本比手写简单版本慢。
- 小数据规模性能波动很大。
- `llvm-mca` 后端模型很好，但实测差。

如果你在原生 Linux 上可以用 PMU，后续第 17 章会用 Top-Down 方法判断 front-end bound。  
在 WSL2 中硬件 PMU 可能受限，所以本阶段先用汇编、代码尺寸、benchmark 和 `llvm-mca` 建立判断。

## 代码尺寸为什么会伤性能

代码尺寸增加会带来：

- I-cache miss 增加。
- uop cache 压力增加。
- branch target buffer 压力增加。
- 编译器布局更难。
- hot path 和 cold path 混在一起。
- instruction TLB 压力增加。

常见来源：

- 大量模板实例化。
- 过度 inline。
- 过度 unroll。
- 多 ISA 多 dtype 多 shape 全部展开。
- 错误处理和 debug path 混在 hot function。
- fusion 太贪心，把不该融合的东西塞进一个巨大 kernel。

代码尺寸优化不是追求二进制小，而是让 hot path 紧凑、稳定、可预测。

## inline 的收益和代价

inline 收益：

- 消除 call/ret。
- 暴露常量。
- 暴露 alias 和范围信息。
- 帮助 vectorization。
- 帮助 LICM、DCE、CSE。

inline 代价：

- 代码尺寸变大。
- I-cache/uop cache 压力变大。
- register pressure 变大。
- 编译时间变长。
- debug 和 profile 更复杂。

工程判断：

```text
小、热、稳定、能暴露优化机会 -> 倾向 inline
大、冷、错误处理、低频路径 -> 不要 inline 到 hot loop
```

## loop unrolling 的收益和代价

unrolling 收益：

- 减少 loop branch 开销。
- 增加 ILP。
- 暴露更多调度空间。
- 降低循环控制指令占比。

unrolling 代价：

- 代码尺寸增加。
- 寄存器压力增加。
- spill 风险增加。
- tail 处理复杂。
- front-end 压力增加。

高性能 kernel 常用适度 unroll，而不是无限展开。  
GEMM microkernel 中的 unroll 通常和寄存器 blocking、FMA latency、load/store 节奏一起设计。

## tail handling

SIMD 和 block kernel 经常遇到尾部元素。

策略：

- scalar cleanup loop。
- masked vector load/store。
- overread with padding。
- specialized remainder kernels。
- size-class dispatch。

取舍：

- scalar tail 简单，但小 shape 下占比高。
- masked vector 统一，但 AVX2 mask load/store 成本和语义要小心。
- padding 可以消除分支，但改变内存布局和边界要求。
- specialized kernels 快，但代码尺寸大。
- size-class dispatch 稳定，但需要维护多个版本。

AI 算子里常见 shape 很多，尾处理不能事后再补。你设计 kernel 时一开始就要写清楚：

```text
main vector loop covers what
tail path covers what
small shape path covers what
alignment/padding assumptions are what
```

## 实验 A：可预测和不可预测分支

创建 `scratch/ch06/branch_count.cpp`：

```cpp
#include <cstddef>
#include <cstdint>

extern "C" std::uint64_t count_branch(std::uint32_t const* x, std::size_t n)
{
    std::uint64_t count = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if ((x[i] & 1U) != 0U) {
            ++count;
        }
    }
    return count;
}

extern "C" std::uint64_t count_branchless(std::uint32_t const* x, std::size_t n)
{
    std::uint64_t count = 0;
    for (std::size_t i = 0; i < n; ++i) {
        count += x[i] & 1U;
    }
    return count;
}
```

生成汇编：

```bash
clang++ -O3 -march=native -S -masm=intel \
  scratch/ch06/branch_count.cpp -o scratch/ch06/branch_count.s

rg -n "count_branch|jne|je|cmov|set|and|v" scratch/ch06/branch_count.s
```

构造三类输入：

- 全偶数。
- 全奇数。
- 随机奇偶。

benchmark 每类输入。

报告必须回答：

- 编译器是否把 `count_branch` 改写成 branchless？
- 如果改写了，如何阻止它以观察真实 branch？例如降低优化或引入不可优化路径，但要解释这样做的副作用。
- 哪类输入最快？
- branchless 版本是否更稳定？
- 是否发生自动向量化？

## 实验 B：branch vs cmov

创建 `scratch/ch06/max_select.cpp`：

```cpp
#include <cstddef>

extern "C" int max_scalar(int a, int b)
{
    return a > b ? a : b;
}

extern "C" void max_array(
    int* __restrict y,
    int const* __restrict a,
    int const* __restrict b,
    std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = a[i] > b[i] ? a[i] : b[i];
    }
}
```

命令：

```bash
clang++ -O3 -march=native -S -masm=intel \
  scratch/ch06/max_select.cpp -o scratch/ch06/max_select.s

rg -n "max_scalar|max_array|cmov|vpmax|vpcmp|blend|j" scratch/ch06/max_select.s
```

任务：

- `max_scalar` 是否使用 `cmov`？
- `max_array` 是否使用 SIMD max 指令？
- 如果使用 SIMD，它如何处理 tail？
- 对比 GCC 和 Clang。

## 实验 C：过度 unroll

写 4 个版本：

- `sum_unroll1`
- `sum_unroll4`
- `sum_unroll8`
- `sum_unroll16`

要求：

- 每个版本使用独立累加器。
- 关闭或观察编译器自动 unroll 的影响。
- 查看汇编代码尺寸。

命令参考：

```bash
clang++ -O3 -march=native -S -masm=intel \
  scratch/ch06/sum_unroll.cpp -o scratch/ch06/sum_unroll.s

objdump -drwC -Mintel build-or-object-file.o
nm -S --size-sort build-or-object-file.o
```

报告：

- 每个函数的 text size。
- 每个函数 benchmark 结果。
- 每个函数寄存器使用情况。
- 是否出现 spill。
- unroll 到多少后收益变小或变差。

如果你看到 `vmov` 到栈上，说明可能发生 spill。不要只看运行时间，要解释原因。

## 实验 D：cold path 分离

比较两种写法。

版本 1：

```cpp
extern "C" int parse_hot_bad(int x)
{
    if (x < 0) {
        return -1;
    }
    if (x == 1234567) {
        return -2;
    }
    return x * 3 + 1;
}
```

版本 2：

```cpp
[[gnu::noinline]] int parse_error(int code)
{
    return code;
}

extern "C" int parse_hot_better(int x)
{
    if (x < 0) {
        return parse_error(-1);
    }
    if (x == 1234567) {
        return parse_error(-2);
    }
    return x * 3 + 1;
}
```

任务：

- 看两个版本的汇编。
- 判断 hot path 是否更短。
- benchmark 正常输入和异常输入。
- 解释是否值得这样写。

这个实验不是说所有错误处理都要 `noinline`，而是训练你识别 hot/cold path 混杂。

## 实验 E：AI 算子 tail 设计

实现一个简化 ReLU：

```cpp
void relu(float* y, float const* x, std::size_t n);
```

写三个版本：

- scalar branch：`if (x[i] > 0) y[i] = x[i]; else y[i] = 0;`
- scalar branchless：`std::max(x[i], 0.0f)` 或条件表达式。
- 编译器自动向量化版本。

输入：

- 全正。
- 全负。
- 正负交替。
- 随机。
- 小 `n`：1、3、7、15、31。
- 大 `n`：1024、16384、1048576。

报告：

- 小 shape 下哪个版本好？
- 大 shape 下哪个版本好？
- 编译器是否生成 `vmaxps`？
- tail 的代码形态是什么？
- 你会如何为生产库设计 size dispatch？

## 作业 6.1：分支预测实验报告

围绕实验 A 写完整报告。

最低要求：

- 3 类输入。
- 2 个编译器：GCC 和 Clang。
- 2 个优化级别：`-O2` 和 `-O3`。
- 汇编截图或摘录。
- benchmark 表格。
- 对编译器改写行为的解释。

高质量要求：

- 解释为什么编译器可能消除 branch。
- 解释 branchless 是否帮助 vectorization。
- 给出你对真实业务代码的选择准则。

## 作业 6.2：代码尺寸和性能

选择一个函数，制造 4 个版本：

- 小函数不 inline。
- 小函数 inline。
- 大函数 inline。
- 大函数拆分 hot/cold path。

记录：

```text
function text size
binary size
benchmark median
benchmark p95 or max
assembly structure
branch count by static inspection
```

报告必须包含：

- 哪个版本最快。
- 哪个版本最稳定。
- 是否存在代码尺寸变大但性能变差。
- 你如何判断 front-end 可能成为瓶颈。

## 作业 6.3：设计一个 tail policy

给定一个向量长度为 8 个 `float` 的 AVX2 kernel，输入长度 `n` 任意。

你要设计：

- 主循环。
- tail 处理。
- 小 `n` 路径。
- 是否需要 padding。
- 是否需要 specialized remainder kernel。

场景：

- ReLU。
- vector add。
- layernorm 的 reduction 阶段。

分别给出策略。  
要求写清楚你为什么不选择其他方案。

## 常见误区

### 误区 1：branchless 永远更快

错误。branchless 会增加总计算量、寄存器压力和数据依赖。

### 误区 2：分支一定很慢

错误。高度可预测分支可以很便宜。

### 误区 3：inline 永远优化

错误。inline 可能增加代码尺寸，导致 front-end 压力。

### 误区 4：unroll 越大越好

错误。过度 unroll 可能导致 I-cache/uop cache 压力和 spill。

### 误区 5：尾处理不重要

错误。AI 推理经常有小 shape、动态 shape、非整齐长度。tail 可能决定延迟。

## 验收标准

本章完成标准：

- 你能解释可预测分支和不可预测分支的性能差异。
- 你能说出 branch、branchless、`cmov`、SIMD mask 的适用条件。
- 你能通过汇编判断编译器是否消除了分支。
- 你完成实验 E，并能解释 tail 处理对小 shape 的影响。
- 你写出不少于 1000 字的作业 6.2 报告。
- 你能在看到一个巨大的模板 kernel 时提出代码尺寸和 front-end 风险，而不是只看后端吞吐。

## 本章之后你应该形成的习惯

每当你准备做以下优化时：

- inline。
- unroll。
- branchless 改写。
- operator fusion。
- 多 dtype 多 ISA 模板展开。

都要问：

```text
hot path 是否更短？
分支是否更可预测？
代码尺寸是否可控？
后端收益是否超过前端成本？
小 shape 是否变差？
```

能回答这些问题，你才算开始真正理解 CPU 性能。
