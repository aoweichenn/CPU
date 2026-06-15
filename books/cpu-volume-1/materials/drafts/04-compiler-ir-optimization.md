# 第 4 章：LLVM IR、编译器优化和自动向量化

## 本章目标

本章把 C++ 源码、编译器中间表示、优化报告和最终汇编连接起来。

完成本章后，你应该能：

- 生成 LLVM IR。
- 识别 basic block、SSA、phi、load/store。
- 理解常见优化：inline、DCE、constant propagation、LICM、unrolling、vectorization。
- 使用 Clang/GCC 向量化报告。
- 解释自动向量化失败的原因。
- 通过源码改写帮助编译器优化。

## 为什么要看 IR

汇编太低层，源码太高层。IR 是中间桥梁。

C++ 源码：

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

汇编里你看到的是寄存器、跳转、load、add。  
IR 里你能看到：

- loop 结构。
- phi 节点。
- load/store。
- fast-math 标记。
- alias metadata。
- vectorization 之前和之后的结构。

IR 能帮助你理解编译器为什么能优化，或者为什么不能优化。

## 生成 LLVM IR

命令：

```bash
clang++ -O0 -emit-llvm -S kernel.cpp -o kernel_O0.ll
clang++ -O3 -emit-llvm -S kernel.cpp -o kernel_O3.ll
```

对比：

- `-O0` IR 更接近源码和调试结构。
- `-O3` IR 已经经过大量优化。

还可以输出 bitcode：

```bash
clang++ -O3 -emit-llvm -c kernel.cpp -o kernel.bc
```

## SSA 和 phi

SSA 是 Static Single Assignment。每个 SSA value 只被定义一次。

循环变量在 SSA 中通常通过 `phi` 表达。

伪 IR：

```llvm
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %sum = phi float [0.0, %entry], [%sum.next, %loop]
  ...
  %i.next = add i32 %i, 1
  br i1 %cond, label %loop, label %exit
```

`phi` 的意思：

- 如果从 entry 来，值是初始值。
- 如果从 loop 回来，值是更新值。

理解 `phi` 是读懂优化后 loop 的关键。

## 常见优化

### Dead Code Elimination

如果计算结果没有可观察用途，编译器可能删除它。

这就是 benchmark 里必须防 DCE 的原因。

### Constant Propagation

如果编译器知道值是常量，就可以提前计算。

```cpp
int f()
{
    return 2 * 3 + 4;
}
```

可能直接变成：

```asm
mov eax, 10
ret
```

### Inlining

函数调用被替换成函数体。

收益：

- 消除 call/ret 开销。
- 暴露更多优化机会。

代价：

- 增加代码尺寸。
- 可能造成 front-end 压力。

### LICM

Loop Invariant Code Motion，把循环不变量移出循环。

```cpp
for (...) {
    y[i] = x[i] * (a + b);
}
```

`a + b` 可以移出循环。

### Unrolling

展开循环，减少分支开销，增加 instruction-level parallelism。

代价：

- 代码尺寸增加。
- register pressure 增加。
- front-end 可能变慢。

### Vectorization

把 scalar loop 改成 SIMD loop。

例如：

```cpp
y[i] = a[i] + b[i];
```

可能变成 AVX2：

```asm
vmovups ymm0, ymmword ptr [rdi + ...]
vaddps  ymm0, ymm0, ymmword ptr [rsi + ...]
vmovups ymmword ptr [rdx + ...], ymm0
```

## 自动向量化的两个问题

编译器要回答两个问题：

### 1. Legality

向量化是否合法？

阻碍因素：

- loop-carried dependency。
- pointer alias。
- 函数调用有副作用。
- 不可证明的内存重叠。
- 浮点语义限制。

### 2. Profitability

向量化是否值得？

阻碍因素：

- trip count 太小。
- gather/scatter 太贵。
- 分支太复杂。
- 数据重排成本过高。
- register pressure 太大。

## alias 为什么重要

看：

```cpp
void saxpy(float* y, float const* x, float a, int n)
{
    for (int i = 0; i < n; ++i) {
        y[i] += a * x[i];
    }
}
```

如果 `x` 和 `y` 可能重叠，编译器必须小心。  
它可能需要 runtime alias check，或者拒绝某些优化。

C 风格可以用 `restrict` 表示不重叠：

```cpp
void saxpy(float* __restrict y, float const* __restrict x, float a, int n)
```

C++ 没有标准 `restrict`，但 GCC/Clang 支持扩展。

更现代的 C++ 接口可以通过约定、span、文档和内部实现控制 alias，但编译器未必能从类型上证明。

## Clang 向量化报告

命令：

```bash
clang++ -O3 -march=native \
  -Rpass=loop-vectorize \
  -Rpass-missed=loop-vectorize \
  -Rpass-analysis=loop-vectorize \
  kernel.cpp -c
```

你会看到：

- vectorized loop。
- missed loop。
- analysis reason。

不要只看“有没有 vectorized”，要看为什么。

## GCC 向量化报告

命令：

```bash
g++ -O3 -march=native \
  -fopt-info-vec-optimized \
  -fopt-info-vec-missed \
  kernel.cpp -c
```

GCC 和 Clang 的判断可能不同。这是正常的。  
你的任务是解释差异，而不是盲目选一个。

## fast-math 的危险

浮点加法不满足严格结合律。

```cpp
(a + b) + c
```

不一定等于：

```cpp
a + (b + c)
```

`-ffast-math` 允许编译器做更激进的浮点重排。  
它可能提高性能，也可能改变结果。

所以对 `sum`、`dot`、`softmax`、`layernorm`，必须写误差报告。

## 20 个 loop case

本章必须做 20 个 loop。

建议列表：

1. simple add: `y[i] = a[i] + b[i]`
2. saxpy: `y[i] += alpha * x[i]`
3. sum reduction
4. dot reduction
5. min reduction
6. max reduction
7. conditional map
8. branchless conditional map
9. prefix sum
10. 1D stencil
11. 2D stencil row
12. histogram
13. indirect read
14. indirect write
15. gather-like lookup
16. scatter-like update
17. function call in loop
18. virtual call in loop
19. aliasing pointers
20. mixed int/float conversion

每个 case 表格：

```text
case name:
GCC vectorized:
Clang vectorized:
missed reason:
source fix:
assembly evidence:
performance before:
performance after:
```

## 源码改写策略

常见有效策略：

- 把复杂表达式拆成简单 loop。
- 移除 loop 内不可 inline 函数调用。
- 明确不 alias。
- 让内存连续。
- 用 SoA 替代 AoS。
- 把分支改成 mask。
- 把 reduction 写成编译器能识别的形式。
- 使用 alignment hint。
- 把小 trip count 合并或批处理。

常见无效策略：

- 盲目加 `inline`。
- 盲目加 `-O3`。
- 盲目用 `-ffast-math`。
- 在 memory-bound kernel 上只减少一两条算术指令。
- 把 gather-heavy 代码硬改成 SIMD。

## 必会命令

生成 IR：

```bash
clang++ -O3 -march=native -emit-llvm -S kernel.cpp -o kernel.ll
```

Clang 向量化报告：

```bash
clang++ -O3 -march=native \
  -Rpass=loop-vectorize \
  -Rpass-missed=loop-vectorize \
  -Rpass-analysis=loop-vectorize \
  kernel.cpp -c
```

GCC 向量化报告：

```bash
g++ -O3 -march=native \
  -fopt-info-vec-optimized \
  -fopt-info-vec-missed \
  kernel.cpp -c
```

看最终汇编：

```bash
objdump -drwC -Mintel ./kernel | less
```

## 作业

1. 完成 20 个 loop case。
2. 每个 case 记录 GCC/Clang 向量化结果。
3. 至少修复 8 个 missed vectorization。
4. 写 3 个 fast-math 改变结果的例子。
5. 对 `sum_f32` 和 `dot_f32` 比较：
   - scalar
   - auto-vectorized
   - fast-math
   - hand-written AVX2

## 常见误区

- 只看优化报告，不看汇编。
- 只看汇编，不看正确性和误差。
- 以为 `-O3` 必然比 `-O2` 快。
- 以为自动向量化失败就是编译器差。
- 以为 `restrict` 可以随便加。
- 以为 `-ffast-math` 没有语义成本。

## 验收标准

- 能读懂简单 LLVM IR。
- 能解释 `phi`。
- 能用 GCC/Clang 报告分析 vectorization。
- 能通过源码改写修复 missed vectorization。
- 能解释 fast-math 的数值风险。

