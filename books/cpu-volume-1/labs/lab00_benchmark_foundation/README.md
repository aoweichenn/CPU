# Lab 00: 建立可信性能实验室

这个 lab 是后续所有性能优化实验的地基。目标不是马上写最快代码，而是建立一个可复用的实验流程：

1. 先保证 correctness。
2. 再控制测量环境。
3. 再保存原始数据。
4. 最后才做性能解释。

## 目录说明

```text
labs/lab00_benchmark_foundation/
  CMakeLists.txt
  include/lab00/
    benchmark.hpp      # benchmark harness
    kernels.hpp        # 被测函数和 benchmark 注册
  src/
    benchmark.cpp      # 计时、CSV、CLI 解析辅助
    kernels.cpp        # sum/dot/fill/copy baseline
    main.cpp           # lab00_bench 命令行入口
    bad_benchmarks.cpp # 错误 benchmark 博物馆
  tests/
    lab00_tests.cpp    # 无第三方依赖的基础测试
  scripts/
    run_lab00.sh       # 一键构建、测试、记录环境、运行实验
```

## 一键运行

```bash
bash labs/lab00_benchmark_foundation/scripts/run_lab00.sh
```

运行后查看：

```bash
cat results/lab00/env.txt
head results/lab00/foundation.csv
cat results/lab00/summary.md
cat results/lab00/bad_benchmarks.txt
```

## 手动构建

```bash
cmake -S . -B build/lab00-release -DCMAKE_BUILD_TYPE=Release -DCPU_VOLUME_1_BUILD_LABS=ON
cmake --build build/lab00-release --target lab00_bench lab00_bad_benchmarks lab00_tests
ctest --test-dir build/lab00-release -R lab00_tests --output-on-failure
```

覆盖率检查：

```bash
bash tools/run_coverage.sh
cat results/coverage/summary.txt
```

列出 benchmark：

```bash
build/lab00-release/bin/cpu-volume-1/lab00/lab00_bench --list
```

运行单个 benchmark：

```bash
build/lab00-release/bin/cpu-volume-1/lab00/lab00_bench \
  --only dot_f32 \
  --sizes 1024,16384,1048576 \
  --warmup 3 \
  --iterations 10
```

## 你需要重点观察

- `foundation.csv` 每一行代表一次测量，不是最终结论。
- 同一个 `name + input_size` 会有多次 `iteration`，你要观察波动。
- `summary.md` 是按 `name + input_size` 汇总出的 min/median/max/mean，适合写报告。
- `checksum` 用来防止结果完全不被观察，但它不是 correctness test 的替代品。
- `bad_benchmarks.txt` 里的结果是故意有问题的，用来学习哪些测量不能信。

## 本 lab 的作业

基础作业：

- 跑通脚本。
- 阅读 `env.txt`，写出 CPU 型号、编译器版本、是否 WSL2、PMU 是否可用。
- 用 `--only sum_i32` 单独运行一次。

进阶作业：

- 修改 `--sizes`，加入更小和更大的输入规模。
- 把 CSV 导入表格或 Python，计算 median/min/max。
- 对 `sum_i32`、`dot_f32`、`copy_u8` 分别写 3 条观察。

挑战作业：

- 新增一个 `scale_f32` benchmark。
- 给它写测试。
- 在报告里解释它更可能是 compute-bound 还是 memory-bound。

## 必须写进报告的问题

- 为什么不能只测一次？
- 为什么 Debug build 不能拿来做性能结论？
- 为什么不能把随机数生成放在被测循环里？
- 为什么 benchmark 需要 checksum 或 `do_not_optimize`？
- 当前机器上 `perf stat -e cycles,instructions` 是否能用？如果不能，原因是什么？

## 下一步

完成这个 lab 后，再进入 Lab 01：C++ 到目标文件、汇编和二进制。Lab 01 会要求你保存 `.s` 文件、使用 `objdump`，并建立源码到汇编的映射。
