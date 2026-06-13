# CPU Performance Study Workspace

这是一个用于系统学习 x86-64、C++ 到汇编、CPU 微架构、性能测量、HPC 和 AI CPU 算子优化的高强度学习仓库。当前主线是 16 周专家冲刺版，不按长期慢速路线执行。

先读：

1. `book-latex/README.md`
2. `book-latex/outline/master-outline.md`
3. `docs/x86_64_hpc_ai_16_week_bootcamp.md`
4. `docs/book/README.md`
5. `labs/lab00_benchmark_foundation/README.md`
6. `docs/x86_64_hpc_ai_detailed_curriculum.md`
7. `docs/x86_64_hpc_ai_master_plan.md`

## 正式教材工程

正式书稿已经切换到 LaTeX：

```text
book-latex/
```

它按百万字级教材规划，当前包含：

- 12 个 Part。
- 64 章主线。
- 5 个附录。
- LaTeX 主文件、preamble、章节模板、实验模板、习题模板。
- 全书扩写蓝图：`book-latex/outline/master-outline.md`。

`docs/book/` 保留为 Markdown 草稿和实验素材库，不再作为最终成书格式。

## 第一天要做什么

运行 Lab 00：

```bash
bash labs/lab00_benchmark_foundation/scripts/run_lab00.sh
```

查看环境：

```bash
cat results/lab00/env.txt
```

查看原始 benchmark 数据：

```bash
head results/lab00/foundation.csv
```

查看汇总结果：

```bash
cat results/lab00/summary.md
```

查看错误 benchmark 示例：

```bash
cat results/lab00/bad_benchmarks.txt
```

可选：跑一次覆盖率检查：

```bash
bash tools/run_coverage.sh
cat results/coverage/summary.txt
```

然后填写报告模板：

```text
docs/reports/lab00.md
```

## 手动构建

```bash
cmake -S . -B build/lab00-release -DCMAKE_BUILD_TYPE=Release -DCPU_BUILD_LABS=ON
cmake --build build/lab00-release --target lab00_bench lab00_bad_benchmarks lab00_tests
ctest --test-dir build/lab00-release -R lab00_tests --output-on-failure
```

## 当前已搭建内容

- Lab 00 benchmark harness。
- 基础 benchmark：`sum_i32`、`sum_f32`、`dot_f32`、`fill_u8`、`copy_u8`。
- 错误 benchmark 博物馆。
- 环境记录脚本。
- CSV 汇总脚本。
- GCC/gcov 覆盖率脚本。
- Lab 00 报告模板。

## 重要规则

- 先正确，再测量，再优化。
- Debug build 不能作为性能结论。
- 任何性能数字都必须带环境信息。
- 任何 benchmark 都必须说明如何防止 dead-code elimination。
- 当前 WSL2 环境不适合做完整硬件 PMU 结论，后续严肃 profiling 需要裸机 Linux。
