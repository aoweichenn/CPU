# 第 7 章：cache、TLB、load/store 和 roofline

## 本章目标

本章建立内存层级性能模型。

完成本章后，你应该能：

- 解释 L1/L2/L3/DRAM 的延迟和带宽差异。
- 区分 temporal locality 和 spatial locality。
- 解释 cache line、set associativity、conflict miss、capacity miss、compulsory miss。
- 解释 TLB、page、huge page 和 pointer chasing 的关系。
- 分析 stride、AoS/SoA、alignment、prefetch 对性能的影响。
- 计算 arithmetic intensity，并用 roofline 判断 kernel 更像 compute-bound 还是 memory-bound。
- 设计 blocking/tiling 的第一性原理，为 GEMM 和 AI 算子优化做准备。

如果说第 5 章回答“CPU 后端一拍能做多少计算”，本章回答“数据能不能及时送到计算单元”。

## 为什么内存层级是性能核心

现代 CPU 的浮点计算能力很强，但内存系统的延迟和带宽跟不上计算单元的增长。

一个 AVX2 FMA 指令可以一次处理 8 个 `float`。如果核心每 cycle 能发多条 FMA，理论 FLOP 很高。  
但如果每个元素只用一次，从 DRAM 读进来，算完马上丢掉，那么性能通常被内存带宽限制。

很多 AI/HPC kernel 的优化，本质是同一件事：

```text
让每个从内存取来的字节被尽可能多地复用。
```

GEMM 的 blocking、convolution 的 im2col 或 direct tiling、attention 的 KV cache layout、layernorm 的两遍 reduction 或 fusion，背后都是内存层级设计。

## 内存层级

简化模型：

```text
register
  -> L1 data cache
  -> L2 cache
  -> L3 cache
  -> DRAM
  -> storage / mmap backing
```

越靠近核心：

- 延迟越低。
- 带宽越高。
- 容量越小。
- 竞争越激烈。

越远离核心：

- 容量越大。
- 延迟越高。
- 带宽相对低。
- 更容易受多核共享影响。

不要死记具体周期，因为不同 CPU 不同。本章训练的是结构性判断：

```text
working set fits L1?
fits L2?
fits L3?
streaming from DRAM?
random access causing latency stalls?
```

## cache line

CPU cache 以 cache line 为单位搬运数据。常见 cache line 大小是 64 字节。

对 `float`：

```text
64 bytes / 4 bytes = 16 floats
```

如果你读取 `x[i]`，硬件通常会把包含它的整条 cache line 取入 cache。  
如果后续访问 `x[i+1]`、`x[i+2]`，它们可能已经在同一条 cache line 里。

这就是 spatial locality。

## temporal locality 和 spatial locality

Temporal locality：同一数据在短时间内被重复使用。

例子：

```cpp
for (int k = 0; k < K; ++k) {
    acc += a[k] * b[k];
}
```

如果 `a[k]`、`b[k]` 很快被再次使用，cache 有价值。

Spatial locality：相邻数据被连续访问。

例子：

```cpp
for (std::size_t i = 0; i < n; ++i) {
    sum += x[i];
}
```

连续数组访问对硬件预取器友好。

## 三类 cache miss

### Compulsory miss

第一次访问某条 cache line，之前从未加载过。

### Capacity miss

working set 太大，cache 放不下。

### Conflict miss

cache 总容量够，但多个地址映射到同一组，互相驱逐。

初学阶段最常见的是 compulsory 和 capacity。  
做矩阵转置、stride 访问、固定步长扫描时，要开始关注 conflict。

## load/store subsystem

内存操作不是一句“读内存”这么简单。

一个 load 需要：

- 计算地址。
- 查 TLB。
- 查 L1。
- miss 后访问下级 cache。
- 把数据送到执行单元。

一个 store 需要：

- 计算地址。
- store data。
- store buffer。
- 最终写回 cache/memory。

现代 CPU 可以有多个 outstanding misses，通过 memory-level parallelism 隐藏部分延迟。  
但 pointer chasing 这类地址依赖访问很难并行。

## streaming access

连续读写：

```cpp
for (std::size_t i = 0; i < n; ++i) {
    y[i] = a * x[i] + y[i];
}
```

这是 streaming access。特点：

- spatial locality 好。
- hardware prefetcher 容易识别。
- 对 cache 容量要求不高。
- 大规模时可能被内存带宽限制。

SAXPY 每元素大致：

```text
read x[i]   4 bytes
read y[i]   4 bytes
write y[i]  4 bytes
```

还要考虑 write allocate。如果 store miss 导致先读入 cache line，再修改，实际流量可能更高。

## stride access

步长访问：

```cpp
for (std::size_t i = 0; i < n; i += stride) {
    sum += x[i];
}
```

如果 `stride = 1`，每条 cache line 中的数据都能利用。  
如果 `stride = 16` 对 `float`，每次正好跨 64 字节，可能每条 cache line 只用 1 个 `float`。

有效带宽会大幅下降：

```text
load 64 bytes cache line
use 4 bytes
waste 60 bytes
```

stride 还可能破坏 hardware prefetch 和 TLB locality。

## pointer chasing

链表：

```cpp
struct Node {
    Node* next;
    std::uint64_t value;
};

std::uint64_t sum_list(Node const* node)
{
    std::uint64_t sum = 0;
    while (node != nullptr) {
        sum += node->value;
        node = node->next;
    }
    return sum;
}
```

问题：

- 下一地址依赖当前 load。
- cache line 利用率低。
- prefetcher 难预测。
- TLB miss 风险高。
- memory-level parallelism 低。

这就是为什么高性能数值计算偏爱紧凑连续数组，而不是指针丰富的对象图。

## TLB

TLB 是 Translation Lookaside Buffer，用于缓存虚拟地址到物理地址的转换。

程序使用虚拟地址。每次访问内存都需要地址翻译。页表查询很贵，所以 CPU 用 TLB 缓存最近的翻译。

常见页面大小：

```text
4 KiB normal page
2 MiB huge page
1 GiB huge page
```

如果访问跨越大量页面，且局部性差，TLB miss 会成为瓶颈。

例子：

```cpp
for (std::size_t i = 0; i < pages; ++i) {
    sum += data[i * 4096 / sizeof(int)];
}
```

每次访问一个新页中的一个整数，cache line 利用率和 TLB locality 都很差。

## alignment

对齐影响：

- SIMD load/store 是否跨 cache line。
- 是否跨 page boundary。
- 编译器能否使用更简单的对齐假设。
- 某些老指令或特殊指令的要求。

现代 x86 对 unaligned load 支持较好，但不代表对齐不重要。

跨 cache line 的 load 可能需要访问两条 cache line。  
跨 page 的 load 可能涉及两个地址翻译。  
store 对齐不好也可能影响 store forwarding 和 write combining。

工程策略：

- 大数组分配时考虑 32 或 64 字节对齐。
- SIMD kernel 明确 load/store 是否假设对齐。
- 不要为了对齐写出复杂且更慢的 prologue，除非实测收益明确。

## AoS 和 SoA

Array of Structures：

```cpp
struct Point {
    float x;
    float y;
    float z;
};

std::vector<Point> points;
```

Structure of Arrays：

```cpp
struct Points {
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
};
```

如果只计算所有 `x`：

```cpp
sum += points[i].x;
```

AoS 会把 `y`、`z` 也带进 cache line，浪费带宽。  
SoA 连续读取 `x`，更适合 SIMD 和 cache。

如果每次都同时使用 `x/y/z`，AoS 也可能合理。

布局选择必须基于访问模式，不是基于风格偏好。

## prefetch

硬件预取器擅长识别：

- 连续访问。
- 固定 stride。
- 多条简单 stream。

它不擅长：

- 随机访问。
- pointer chasing。
- 复杂间接索引。
- 过多交错 stream。

软件 prefetch：

```cpp
__builtin_prefetch(ptr + distance);
```

可能有用，但经常无效甚至变慢。

使用前必须回答：

- 未来地址能否提前知道？
- 预取距离是多少？
- 预取是否污染 cache？
- 预取指令是否增加 front-end 和端口压力？
- 硬件预取器是否已经做得足够好？

初学阶段不要迷信 prefetch。先把数据布局、blocking、连续访问做好。

## write allocate 和 non-temporal store

普通 store 到一个不在 cache 中的地址时，CPU 可能先把整条 cache line 读入 cache，再修改。这叫 write allocate 或 read for ownership。

如果你只是写一个很大的输出数组，并且短期不再读取它，普通 store 可能浪费 cache 容量和带宽。

non-temporal store 可以提示 CPU：数据不需要进入普通 cache 层级。

但它有约束：

- 适合大规模 streaming store。
- 通常需要对齐和整 cache line 写入更好。
- 小规模可能更慢。
- 读后马上用的数据不适合。

后面做大数组 copy、GEMM 写回、量化输出时会再次讨论。

## arithmetic intensity

Arithmetic intensity，算术强度：

```text
AI = FLOPs / bytes moved
```

它描述每搬运一个字节能做多少计算。

例子：vector add

```cpp
y[i] = a[i] + b[i];
```

每元素：

```text
read a: 4 bytes
read b: 4 bytes
write y: 4 bytes
FLOPs: 1 add
```

忽略 write allocate，AI：

```text
1 / 12 FLOP/byte
```

很低，通常 memory-bound。

例子：GEMM

```text
C[M,N] += A[M,K] * B[K,N]
```

如果 blocking 做得好，A 和 B 的元素会被多次复用，AI 可以很高，接近 compute-bound。

## roofline 模型

roofline 给一个简单上界：

```text
attainable FLOP/s = min(peak compute, memory bandwidth * arithmetic intensity)
```

如果：

```text
memory bandwidth * AI < peak compute
```

kernel 更像 memory-bound。

如果：

```text
memory bandwidth * AI >= peak compute
```

kernel 有机会 compute-bound。

roofline 不是精确预测器，而是第一性原理检查：

```text
这个 kernel 的数据复用足够支撑高 FLOP 吗？
```

## roofline 示例：SAXPY

SAXPY：

```cpp
y[i] = a * x[i] + y[i];
```

每元素：

```text
FLOPs: 2
bytes: read x 4 + read y 4 + write y 4 = 12
AI: 2 / 12 = 0.167 FLOP/byte
```

如果单核可持续内存带宽是 30 GB/s，则上界：

```text
30 * 0.167 = 5 GFLOP/s
```

即使 CPU 峰值 FMA 能力远高于这个数，SAXPY 也可能被内存限制。

注意 write allocate 可能让实际 bytes 更高，AI 更低。

## roofline 示例：dot product

```cpp
sum += a[i] * b[i];
```

每元素：

```text
read a: 4
read b: 4
FLOPs: 2
AI: 2 / 8 = 0.25 FLOP/byte
```

仍然偏低。  
但 reduction 有累加依赖，可能同时受 latency、vectorization 和 memory bandwidth 影响。

## roofline 示例：GEMM

朴素估算：

```text
C[M,N] = A[M,K] * B[K,N]
FLOPs = 2*M*N*K
```

如果理想情况下 A/B/C 数据被很好复用，bytes 近似：

```text
4*(M*K + K*N + M*N)
```

当 M/N/K 大时，AI 很高。  
这就是 GEMM 能接近峰值算力的原因。

但 naive GEMM 不会自动达到这个 AI，因为访问顺序可能导致 cache 复用失败。  
blocking/packing 的目标就是让理论复用在真实 cache 层级里发生。

## blocking 的第一性原理

假设你要做矩阵乘：

```cpp
for i
  for j
    for k
      C[i][j] += A[i][k] * B[k][j]
```

如果 `B[k][j]` 的访问跨行，cache locality 可能很差。  
blocking 把矩阵切成小块，让当前工作集 fit in cache：

```text
A block + B block + C block <= target cache capacity
```

目标：

- A block 被多个 C 元素复用。
- B block 被多个 C 元素复用。
- C block 留在寄存器或 cache 中累加。
- 减少从 DRAM 重复读取同一数据。

真正的 GEMM 优化还要继续分层：

```text
register block
L1 block
L2 block
L3 / packing block
NUMA / thread block
```

第 11-12 章会展开。

## 实验 A：缓存容量扫描

写一个连续数组求和 benchmark：

```cpp
extern "C" std::uint64_t sum_u64(std::uint64_t const* x, std::size_t n)
{
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += x[i];
    }
    return sum;
}
```

输入规模按 2 倍增长：

```text
4 KiB
8 KiB
16 KiB
32 KiB
64 KiB
128 KiB
256 KiB
512 KiB
1 MiB
2 MiB
4 MiB
8 MiB
16 MiB
32 MiB
64 MiB
128 MiB
```

记录：

- ns/element。
- GB/s。
- median。
- min/max。

报告：

- 哪些区间可能对应 L1/L2/L3/DRAM？
- 是否能看到明显台阶？
- WSL2 或系统噪声是否影响结果？
- 和 `lscpu` 看到的 cache 信息是否一致？

命令：

```bash
lscpu
getconf LEVEL1_DCACHE_SIZE
getconf LEVEL2_CACHE_SIZE
getconf LEVEL3_CACHE_SIZE
```

不是所有系统都支持这些 `getconf` 键，失败时记录即可。

## 实验 B：stride 扫描

实现：

```cpp
std::uint64_t sum_stride(
    std::uint64_t const* x,
    std::size_t n,
    std::size_t stride)
{
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < n; i += stride) {
        sum += x[i];
    }
    return sum;
}
```

stride：

```text
1, 2, 4, 8, 16, 32, 64, 128, 512
```

注意 `std::uint64_t` 每个元素 8 字节。stride 8 等于 64 字节。

报告：

- 每个访问实际使用多少 cache line 数据？
- stride 到多少后 GB/s 明显下降？
- stride 很大时是否更像 TLB 限制？
- 和连续访问相比慢多少？

## 实验 C：AoS vs SoA

定义：

```cpp
struct ParticleAoS {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
};

struct ParticleSoA {
    float* x;
    float* y;
    float* z;
    float* vx;
    float* vy;
    float* vz;
};
```

任务 1：只更新 x

```cpp
x[i] += vx[i] * dt;
```

任务 2：更新 x/y/z

```cpp
x[i] += vx[i] * dt;
y[i] += vy[i] * dt;
z[i] += vz[i] * dt;
```

比较 AoS 和 SoA。

报告：

- 哪个布局更快？
- 为什么任务 1 和任务 2 可能结论不同？
- 编译器是否自动向量化？
- cache line 利用率如何估算？

## 实验 D：pointer chasing

构造两种链表：

- 节点连续分配，next 指向下一个。
- 节点随机打乱，next 指向随机排列中的下一个。

benchmark 遍历求和。

报告：

- 连续链表和随机链表差多少？
- 为什么随机链表比数组慢很多？
- memory-level parallelism 为什么帮不上太多？
- TLB 是否可能参与？

高质量扩展：

- 同时遍历 4 条独立链表，观察是否比 1 条链表快。
- 解释这是如何增加 memory-level parallelism 的。

## 实验 E：简化 roofline

测两个基础上界：

1. 计算峰值近似：写一个 FMA-heavy kernel，让数据 fit in L1 或寄存器。
2. 内存带宽近似：写 stream copy 或 triad。

然后测：

- vector add。
- SAXPY。
- dot product。
- naive GEMM。

对每个 kernel 计算：

```text
FLOPs
bytes
arithmetic intensity
measured GFLOP/s
measured GB/s
roofline predicted bound
```

报告必须说明：

- 哪些 kernel 明显 memory-bound？
- 哪些 kernel 有机会 compute-bound？
- naive GEMM 为什么理论 AI 高但实测可能差？
- blocking 能改变什么？

## 作业 7.1：缓存扫描报告

完成实验 A，并写报告。

要求：

- 至少 16 个工作集大小。
- 每个大小至少 30 次测量。
- 输出 CSV。
- 绘制或手工整理表格。
- 标注你推测的 L1/L2/L3/DRAM 区间。

高质量要求：

- 对比 `int32_t`、`int64_t`、`float`。
- 说明元素大小对 GB/s 和 ns/element 的影响。
- 写出你对硬件预取器的判断。

## 作业 7.2：矩阵转置优化

实现：

- naive transpose。
- blocked transpose。
- blocked + 合理 tile size 扫描。

矩阵：

```text
512 x 512
1024 x 1024
2048 x 2048
4096 x 4096
```

tile：

```text
8, 16, 32, 64
```

报告：

- naive 为什么慢？
- blocked 为什么快？
- tile 太小有什么问题？
- tile 太大有什么问题？
- 是否存在 conflict miss 的迹象？
- 写入和读取哪个更可能是瓶颈？

这项作业非常重要，因为它是 GEMM packing 和 cache blocking 的前置训练。

## 作业 7.3：roofline 小论文

选择 4 个 kernel：

- vector add。
- SAXPY。
- dot product。
- naive GEMM 或 matrix-vector multiply。

为每个 kernel 写：

```text
1. 源码
2. FLOP 计数
3. byte traffic 估计
4. arithmetic intensity
5. benchmark 结果
6. roofline 解释
7. 优化方向
```

最低 2000 字。  
要求你不要只写“memory-bound”，必须说明哪个证据支持这个判断。

## 作业 7.4：AI 算子的内存模型

选择两个 AI 算子：

- layernorm。
- softmax。
- embedding lookup。
- attention KV cache read。
- int8 dequant + matmul 前处理。

对每个算子写内存模型：

- 输入输出 tensor shape。
- 每元素或每 token 读写字节。
- FLOP 估计。
- 是否有 reduction。
- 是否有随机访问。
- 是否容易 vectorize。
- 是否更像 bandwidth-bound、latency-bound 还是 compute-bound。
- 可以怎样提高数据复用。

这个作业不要求你实现完整算子，但要求分析像工程设计文档。

## 常见误区

### 误区 1：cache 越大程序越快

错误。关键是 working set、访问模式和复用。大 cache 不能拯救完全随机且无复用的访问。

### 误区 2：连续访问一定 compute-bound

错误。连续访问只是让带宽利用更好。vector add、SAXPY 仍然可能 memory-bound。

### 误区 3：prefetch 是万能优化

错误。预取可能污染 cache、增加指令压力，或者被硬件预取器重复。

### 误区 4：只看 FLOP 不看 bytes

错误。AI/HPC 优化最常见错误就是只算计算量，不算数据搬运。

### 误区 5：GEMM 快是因为它指令特殊

不完整。GEMM 快的核心原因是数据复用高，blocking/packing 让复用符合 cache 和寄存器层级。

## 验收标准

本章完成标准：

- 你能解释 cache line 和 stride 为什么影响带宽。
- 你能通过工作集扫描推测 cache 层级。
- 你能解释 TLB miss 为什么影响大跨度访问。
- 你能计算至少 4 个 kernel 的 arithmetic intensity。
- 你能用 roofline 判断优化方向。
- 你完成矩阵转置作业，并写出 tile size 对性能影响的解释。
- 你能把 layernorm 或 softmax 分析成 FLOP、bytes、reduction、vectorization 和 memory-bound 风险。

## 本章之后你应该形成的习惯

以后分析任何 kernel，先写这张表：

```text
kernel:
shape:
FLOPs:
bytes read:
bytes written:
reuse:
access pattern:
working set:
AI:
expected bottleneck:
measurement:
```

如果这张表写不出来，就不要急着写 AVX2。  
高性能优化不是从指令开始，而是从数据运动开始。
