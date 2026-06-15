# 第 1 章：可信性能测量

## 本章目标

本章解决一个根本问题：你测到的性能数字到底能不能信。

完成本章后，你应该能：

- 写出最小可信 benchmark。
- 解释 dead-code elimination、constant folding、warmup、输入规模、统计口径。
- 区分 Debug、Release、sanitizer、coverage build 的用途。
- 读懂本仓库 Lab 00 的 benchmark harness。
- 设计错误 benchmark，并解释为什么它们不可信。

## 为什么测量是第一课

性能优化最危险的地方不是写错 SIMD，而是相信错误数据。

一个错误 benchmark 可能告诉你：

- 一个空循环用了 0 ns。
- 一个带 I/O 的函数很慢。
- 一个把随机数生成放进热路径的算法很慢。
- 一个 Debug build 的版本比另一个 Release build 慢。
- 一个只测一次的结果代表真实表现。

这些结论都不能指导工程优化。

## benchmark 不是计时器

很多初学者把 benchmark 理解成：

```cpp
auto begin = now();
foo();
auto end = now();
```

这只是计时，不是性能实验。

可信 benchmark 至少需要：

- 明确被测对象。
- 明确输入规模。
- 明确构建类型。
- 防止结果被优化掉。
- 排除初始化、分配、I/O 等非热路径成本。
- warmup。
- 重复测量。
- 统计结果。
- 保存环境。
- 有 correctness test。

## 被编译器删除的循环

看这个函数：

```cpp
int sum(int n)
{
    int x = 0;
    for (int i = 0; i < n; ++i) {
        x += i;
    }
    return x;
}
```

如果调用方没有使用返回值，编译器可能认为整个计算没有可观察副作用，于是删除它。

错误 benchmark：

```cpp
auto begin = std::chrono::steady_clock::now();
sum(1000000);
auto end = std::chrono::steady_clock::now();
```

你以为测了求和，实际可能测了一个空块。

Lab 00 里使用 `do_not_optimize`：

```cpp
template <typename T>
inline void do_not_optimize(T const& value) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(value) : "memory");
#else
    auto const volatile* volatile_value = &value;
    static_cast<void>(volatile_value);
#endif
}
```

这段代码的目的不是让程序“更快”，而是告诉编译器：这个值被外部观察到了，不要随便删除相关计算。

注意：`do_not_optimize` 不是魔法。它只是 benchmark 工具的一部分。你仍然需要看汇编确认关键循环存在。

## warmup 和重复测量

为什么要 warmup？

- 首次运行可能触发 page fault。
- 分支预测器、cache、TLB 状态可能还没稳定。
- 动态链接、懒加载、CPU 频率状态可能影响第一次测量。

为什么要重复？

- OS 调度会打断程序。
- 虚拟化环境会增加噪声。
- CPU 频率和温度会变化。
- 后台任务会干扰。

为什么常看 median？

- min 可能代表理想情况，但不代表稳定性。
- max 可能包含 OS 干扰。
- mean 容易被极端值拉偏。
- median 对偶发干扰更稳。

## 输入规模决定瓶颈

同一个函数，不同输入规模可能完全不同。

比如 `sum_i32`：

- 小数组在 L1 cache，可能受 loop overhead 或吞吐限制。
- 中等数组在 L2/L3，cache bandwidth 成为关键。
- 大数组进入 DRAM，memory bandwidth 成为关键。

所以每个 benchmark 至少要测 3 组规模：

- 小规模：L1 friendly。
- 中规模：L2/L3。
- 大规模：DRAM。

Lab 00 默认：

```cpp
inline constexpr std::size_t LAB00_SMALL_INPUT_SIZE = 1'024;
inline constexpr std::size_t LAB00_MEDIUM_INPUT_SIZE = 16'384;
inline constexpr std::size_t LAB00_LARGE_INPUT_SIZE = 1'048'576;
```

这些不是最终标准，只是入门刻度。后续你需要按 CPU cache 大小设计更精细的扫描。

## 构建类型

常见构建类型：

- Debug：用于调试，不用于性能结论。
- Release：用于性能测量。
- sanitizer build：用于查内存和 UB 错误，不用于性能结论。
- coverage build：用于测试覆盖率，不用于性能结论。

Debug build 通常包含：

- 没有优化或优化很少。
- 更多栈帧和临时变量。
- 更少 inline。
- 更差寄存器分配。

所以 Debug 性能没有代表性。

## 读 Lab 00 的设计

Lab 00 的核心类型：

```cpp
struct BenchmarkConfig {
    std::size_t warmup_iterations;
    std::size_t measured_iterations;
    std::vector<std::size_t> input_sizes;
    std::string only_name;
};
```

它明确了 benchmark 的四个维度：

- warmup 次数。
- 正式测量次数。
- 输入规模。
- 被测 case。

一个 benchmark case 被建模成：

```cpp
using BenchmarkWorkload = std::function<std::uint64_t()>;
using BenchmarkFactory = std::function<BenchmarkWorkload(std::size_t input_size)>;
```

为什么要 factory？

因为输入数据应该在计时外构造。  
如果每次计时都分配 vector，你测到的就不是 kernel，而是 allocator + page fault + 初始化。

正确结构：

```text
make_workload(input_size):
    allocate input outside timing
    return lambda:
        run hot kernel
        return checksum
```

## 错误 benchmark 博物馆

Lab 00 里有 `lab00_bad_benchmarks`，它故意展示几类错误。

### 错误 1：结果没有被观察

```text
bad_dead_result_not_observed
controlled_dead_result_observed
```

如果结果没有被观察，编译器可能删除整个计算。

### 错误 2：allocation 放进热路径

如果每次测量都创建大 vector，你测到的是分配和初始化，不是计算。

### 错误 3：RNG 放进热路径

随机数生成可能比你要测的 kernel 更贵。

### 错误 4：格式化/I/O 放进热路径

字符串格式化、日志、打印会严重污染测量。

## 必会命令

构建并运行 Lab 00：

```bash
bash books/cpu-volume-1/labs/lab00_benchmark_foundation/scripts/run_lab00.sh
```

查看结果：

```bash
cat books/cpu-volume-1/results/lab00/env.txt
cat books/cpu-volume-1/results/lab00/summary.md
cat books/cpu-volume-1/results/lab00/bad_benchmarks.txt
```

手动构建：

```bash
cmake -S . -B build/lab00-release -DCMAKE_BUILD_TYPE=Release -DCPU_VOLUME_1_BUILD_LABS=ON
cmake --build build/lab00-release --target lab00_bench lab00_bad_benchmarks lab00_tests
ctest --test-dir build/lab00-release -R lab00_tests --output-on-failure
```

单独跑一个 benchmark：

```bash
build/lab00-release/bin/cpu-volume-1/lab00/lab00_bench \
  --only dot_f32 \
  --sizes 1024,16384,1048576 \
  --warmup 3 \
  --iterations 10
```

## 实验

### 实验 1：重跑 Lab 00 三次

运行三次：

```bash
bash books/cpu-volume-1/labs/lab00_benchmark_foundation/scripts/run_lab00.sh
cp books/cpu-volume-1/results/lab00/summary.md books/cpu-volume-1/results/lab00/summary_run1.md
bash books/cpu-volume-1/labs/lab00_benchmark_foundation/scripts/run_lab00.sh
cp books/cpu-volume-1/results/lab00/summary.md books/cpu-volume-1/results/lab00/summary_run2.md
bash books/cpu-volume-1/labs/lab00_benchmark_foundation/scripts/run_lab00.sh
cp books/cpu-volume-1/results/lab00/summary.md books/cpu-volume-1/results/lab00/summary_run3.md
```

观察：

- 哪个 benchmark 最稳定？
- 哪个 benchmark 波动最大？
- 大输入规模是否更容易受 OS 干扰？

### 实验 2：改变输入规模

运行：

```bash
build/lab00-release/bin/cpu-volume-1/lab00/lab00_bench \
  --sizes 64,256,1024,4096,16384,65536,262144,1048576 \
  --warmup 3 \
  --iterations 10 \
  --csv books/cpu-volume-1/results/lab00/size_scan.csv

books/cpu-volume-1/tools/summarize_bench_csv.py books/cpu-volume-1/results/lab00/size_scan.csv
```

观察 cache 阶段变化。

### 实验 3：新增 `scale_f32`

实现：

```cpp
void scale_f32(std::span<float> values, float scale)
{
    for (float& value : values) {
        value *= scale;
    }
}
```

要求：

- 有 reference 或可验证 checksum。
- 有测试。
- 有 benchmark。
- 有汇编。
- 判断它更可能 memory-bound 还是 compute-bound。

## 作业

1. 完成 `books/cpu-volume-1/reports/lab00.md`。
2. 新增 `scale_f32`。
3. 写 5 个错误 benchmark，并解释每个错在哪里。
4. 对 `sum_i32`、`sum_f32`、`dot_f32`、`copy_u8` 做 8 个输入规模扫描。
5. 写报告：为什么性能数字必须带环境、输入规模和统计口径。

## 常见误区

- 只测一次。
- 用 Debug build 做结论。
- 在计时区间内分配内存。
- benchmark 里打印。
- 没有 checksum。
- 没有 correctness test。
- 不看汇编确认循环存在。
- 用 WSL2 的结果推断裸机 PMU 行为。

## 验收标准

- 能解释 Lab 00 harness 的结构。
- 能新增一个 benchmark case。
- 能说明 4 类错误 benchmark。
- 能生成 CSV 和 summary。
- 能写出一份可信测量报告。

