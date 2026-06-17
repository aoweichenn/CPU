# Lab 00 Report: 建立可信性能实验室

## 1. 实验环境

- 日期：
- 机器：
- OS/kernel：
- 是否 WSL2/虚拟机：
- CPU：
- GCC：
- Clang：
- CMake：
- 构建类型：
- 编译参数：
- `perf_event_paranoid`：

## 2. 正确性验证

运行命令：

```bash
ctest --test-dir build/lab00-release -R lab00_tests --output-on-failure
```

结果：

```text
在这里粘贴测试结果摘要。
```

## 3. Benchmark 设置

- warmup 次数：
- measured iterations：
- 输入规模：
- 输出 CSV：

运行命令：

```bash
bash books/cpu-volume-1/labs/lab00_benchmark_foundation/scripts/run_lab00.sh
```

## 4. 原始结果摘要

请从 `books/cpu-volume-1/results/lab00/foundation.csv` 统计每个 benchmark 的 median/min/max。

| benchmark | input size | min ns | median ns | max ns | 观察 |
|---|---:|---:|---:|---:|---|
| sum_i32 | | | | | |
| sum_f32 | | | | | |
| dot_f32 | | | | | |
| fill_u8 | | | | | |
| copy_u8 | | | | | |

## 5. 错误 benchmark 博物馆

查看：

```bash
cat books/cpu-volume-1/results/lab00/bad_benchmarks.txt
```

回答：

- 哪个错误例子最明显？
- 哪个错误例子不容易一眼看出来？
- 为什么 dead-code elimination 会让结果完全失真？
- 为什么把 allocation/RNG/formatting 放进热路径会污染测量？

## 6. 我的结论

写 5 条你现在相信的 benchmark 规则：

1.
2.
3.
4.
5.

## 7. 我还不懂的问题

至少写 3 个：

1.
2.
3.

## 8. 下一步改进

- 是否需要裸机 Linux？
- 是否需要 CPU pinning？
- 是否需要画图脚本？
- 是否需要把汇编保存到结果目录？
