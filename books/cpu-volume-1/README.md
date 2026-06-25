# CPU 底层原理教材第一册

本目录是第一册的自包含工程。它把正式书稿、草稿素材、实验、报告、结果和工具集中在一起，后续仓库再写算法、C++、编译器、操作系统或 AI 系统书时，不再和本书混在同一层目录。

## 目录结构

```text
source/latex/      # 正式 LaTeX 书稿，生成 main.pdf 和 main.epub
materials/drafts/  # Markdown 草稿和旧章节素材
materials/         # 16 周计划、长期路线和参考材料
labs/              # C++ 实验
reports/           # 报告模板
results/           # 实验输出
tools/             # 本书脚本
```

## 生成书籍

```bash
make check
make pdf
make epub
make text-target
```

生成文件位置：

- `source/latex/main.pdf`
- `source/latex/main.epub`

正式排版规则、字体、目录策略、EPUB 生成方式见 `source/latex/README.md`。

## 运行实验

运行 Lab 00：

```bash
make lab00
```

查看结果：

```bash
cat results/lab00/env.txt
head results/lab00/foundation.csv
cat results/lab00/summary.md
cat results/lab00/bad_benchmarks.txt
```

覆盖率检查：

```bash
make coverage
cat results/coverage/summary.txt
```

手动构建实验：

```bash
cmake -S . -B build/lab00-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/lab00-release --target lab00_bench lab00_bad_benchmarks lab00_tests
ctest --test-dir build/lab00-release --output-on-failure
```

Lab 可执行文件默认输出到：

```text
build/lab00-release/bin/cpu-volume-1/lab00/
```

## 写作入口

- 正式书稿：`source/latex/main.tex`
- 正式主稿：`source/latex/main.tex`
- 16 周计划：`materials/x86_64_hpc_ai_16_week_bootcamp.md`
- 扩展手册：`materials/x86_64_hpc_ai_detailed_curriculum.md`
- 长期计划：`materials/x86_64_hpc_ai_master_plan.md`
- 草稿素材：`materials/drafts/README.md`
- Lab 00：`labs/lab00_benchmark_foundation/README.md`

## 本书维护规则

- 正式成书以 `source/latex/` 为准。
- `materials/drafts/` 只作为素材库，不作为最终排版版本。
- 每章应包含背景知识、可运行实验、C++ 代码、汇编或二进制证据、常见误区和验收标准。
- 实验必须有正确性验证；性能实验必须记录环境、输入规模、重复次数和原始数据。
- 结果目录默认不提交批量运行输出，除非它是书中需要固定引用的精选样例。
