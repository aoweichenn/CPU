# Technical Books Workspace

这是一个多本技术书的写作仓库。仓库根目录只保留公共入口、总构建命令和书库索引；每一本书都放在 `books/<book-id>/` 下面，正文、草稿、实验、报告、结果和工具尽量自包含，避免多本书互相污染。

当前正式书籍：

- `books/cpu-volume-1/`：CPU 底层原理教材第一册，主题是程序、C++、二进制、x86-64、汇编、Linux 工具链和可信性能测量。
- `books/cpu-volume-2/`：从 C++ 到计算系统第二册原理卷，主题是硬件原理、多核心并行、高性能计算、多线程、锁、原子、无锁数据结构、并行算法、运行时、异步 I/O 和分布式计算。
- `books/compute-systems-engine-code/`：第二册配套代码实践卷，主题是 Compute Systems Engine 的完整 C++20 实现、测试、benchmark、故障注入和报告。
- `books/cpu-volume-3/`：CPU 与 AI 计算教材第三册规划目录，主题是 AI 模型、张量、算子开发、量化和本地 CPU 推理引擎。
- `books/algorithm-interview/`：算法刷题与 C++ 面试教材，主题是数据结构、算法原理、暴力到优化、C++ 容器、力扣题单和面试表达；训练周期只作为可调节节奏，不作为内容边界。

规划中的新书可以继续放到 `books/` 下，例如 C++ 语言、编译器、操作系统、AI 系统等方向。

## 仓库结构

```text
books/
  cpu-volume-1/
    source/latex/      # 正式 LaTeX 书稿，生成 PDF/EPUB
    materials/         # 草稿、课程计划、素材、参考资料
    labs/              # 与本书绑定的代码实验
    reports/           # 报告模板和学习报告
    results/           # 实验输出，默认只跟踪 .gitkeep
    tools/             # 本书专用脚本
  cpu-volume-2/
    source/latex/      # 第二册原理卷正式 LaTeX 书稿，生成 PDF/EPUB
    materials/         # 草稿、课程计划、素材、参考资料
    labs/              # 早期实验和配套实践素材
    reports/           # 报告模板和学习报告
    results/           # 实验输出
    tools/             # 本书专用脚本
  compute-systems-engine-code/
    source/latex/      # 代码实践卷正式 LaTeX 书稿
    labs/              # Compute Systems Engine C++20 工程
    materials/
    reports/
    results/
    tools/
  cpu-volume-3/
    labs/              # 第三册 AI 推理引擎实验
    materials/
    reports/
    results/
    tools/
  algorithm-interview/
    source/latex/      # 算法面试书正式 LaTeX 主稿
    materials/         # 题单、写作标准和素材
    labs/              # C++20 算法示例和测试
    reports/           # 周报和复盘模板
    results/           # 实验输出
    tools/             # 本书检查脚本
CMakeLists.txt         # 仓库级 C++ 实验入口
Makefile               # 仓库级常用命令入口
```

## 常用命令

生成当前第一册 PDF 和 EPUB：

```bash
make cpu-pdf
make cpu-epub
```

检查第一册 LaTeX 输入和正文规模：

```bash
make cpu-check
make cpu-text-target
```

运行第一册 Lab 00：

```bash
make cpu-lab00
```

运行覆盖率检查：

```bash
make cpu-coverage
```

生成第二册 PDF 和 EPUB：

```bash
make cpu2-check
make cpu2-pdf
make cpu2-epub
make cpu2-text-count
```

统一导出第一册、第二册和第三册：

```bash
make books-export
```

该命令会重新构建三册 PDF/EPUB，并把 `book-exports/` 下每本书的导出目录清理成一份中文书名 PDF 和一份 ASCII 英文名 EPUB。EPUB 文件名固定为 `cpu-volume-1.epub`、`cpu-volume-2.epub`、`cpu-volume-3.epub`，避免微信读书导入链路误处理中文、空格或 `+`。

检查和测试算法面试书：

```bash
make algo-check
make algo-pdf
make algo-test
```

也可以进入单本书目录执行同名任务：

```bash
cd books/cpu-volume-1
make pdf
make epub
make lab00
```

算法书也可以进入单本书目录执行：

```bash
cd books/algorithm-interview
make check
make pdf
make test
```

第二册也可以进入单本书目录执行：

```bash
cd books/cpu-volume-2
make check
make pdf
make epub
```

## CMake 构建

仓库根工程用于构建所有启用的 C++ 实验：

```bash
cmake -S . -B build/root-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/root-release
ctest --test-dir build/root-release --output-on-failure
```

如果只构建第一册，也可以直接从书目录启动：

```bash
cmake -S books/cpu-volume-1 -B books/cpu-volume-1/build/lab00-release -DCMAKE_BUILD_TYPE=Release
cmake --build books/cpu-volume-1/build/lab00-release --target lab00_bench lab00_bad_benchmarks lab00_tests
ctest --test-dir books/cpu-volume-1/build/lab00-release --output-on-failure
```

## 新增一本书的约定

新增书籍时使用统一骨架：

```text
books/<book-id>/
  README.md
  Makefile
  source/
  materials/
  labs/
  reports/
  results/
  tools/
```

建议规则：

- `source/` 放正式成书工程，优先保持一个主版本，不制造多个并行版本。
- `materials/` 放草稿、课程计划、参考资料、题单、临时素材。
- `labs/` 放能独立构建和测试的代码实验。
- `reports/` 放报告模板和学习报告。
- `results/` 放实验输出，通常不提交大批运行结果。
- `tools/` 放本书专用脚本，跨书共享脚本再考虑提升到仓库级。
- 根目录只维护书库级索引和一键命令，不直接塞单本书的章节、实验或结果。

## 当前第一册入口

- 第一册说明：`books/cpu-volume-1/README.md`
- 正式 LaTeX 工程：`books/cpu-volume-1/source/latex/README.md`
- 全书扩写蓝图：`books/cpu-volume-1/source/latex/outline/master-outline.md`
- 16 周执行计划：`books/cpu-volume-1/materials/x86_64_hpc_ai_16_week_bootcamp.md`
- Markdown 草稿素材：`books/cpu-volume-1/materials/drafts/README.md`
- Lab 00：`books/cpu-volume-1/labs/lab00_benchmark_foundation/README.md`

## 当前第二册入口

- 第二册说明：`books/cpu-volume-2/README.md`
- 原理卷正式主稿：`books/cpu-volume-2/source/latex/main.tex`
- 全书结构：`books/cpu-volume-2/source/latex/outline/book-architecture.tex`
- 硬件执行模型：`books/cpu-volume-2/source/latex/chapters/part01-hardware-foundations/`
- 单机高性能计算：`books/cpu-volume-2/source/latex/chapters/part02-single-node-hpc/`
- 多线程与同步：`books/cpu-volume-2/source/latex/chapters/part03-concurrency-synchronization/`
- 并行算法与运行时：`books/cpu-volume-2/source/latex/chapters/part04-parallel-algorithms-runtime/`
- 分布式计算：`books/cpu-volume-2/source/latex/chapters/part05-distributed-computing/`
- 早期实验素材：`books/cpu-volume-2/labs/compute_systems/`

## 当前代码实践卷入口

- 代码实践卷说明：`books/compute-systems-engine-code/README.md`
- 正式主稿：`books/compute-systems-engine-code/source/latex/main.tex`
- 第一章：`books/compute-systems-engine-code/source/latex/chapters/part01-foundation/ch01-reference-and-contract.tex`

## 当前第三册入口

- 第三册说明：`books/cpu-volume-3/README.md`
- AI 推理引擎实验：`books/cpu-volume-3/labs/linux_cpu_inference/`

## 当前算法面试书入口

- 算法书说明：`books/algorithm-interview/README.md`
- 正式主稿：`books/algorithm-interview/source/latex/main.tex`
- 学习训练路线：`books/algorithm-interview/source/latex/chapters/part00-method/ch01-study-roadmap.tex`
- 数据结构主线：`books/algorithm-interview/source/latex/chapters/part01-data-structures/`
- 核心算法主线：`books/algorithm-interview/source/latex/chapters/part02-core-algorithms/`
- 题单地图：`books/algorithm-interview/materials/problem-map.md`
- C++20 示例：`books/algorithm-interview/labs/cpp20-examples/`
