# Technical Books Workspace

这是一个多本技术书的写作仓库。仓库根目录只保留公共入口、总构建命令和书库索引；每一本书都放在 `books/<book-id>/` 下面，正文、草稿、实验、报告、结果和工具尽量自包含，避免多本书互相污染。

当前正式书籍：

- `books/cpu-volume-1/`：CPU 底层原理教材第一册，主题是程序、C++、二进制、x86-64、汇编、Linux 工具链和可信性能测量。
- `books/cpu-volume-1-practice/`：第一册配套实践卷，主题是 Executable Evidence Kit、ELF64 解析、ABI/符号观察、Linux 工具链、GTest/GMock、Google Benchmark 和研究报告。
- `books/cpu-volume-2/`：从 C++ 到计算系统第二册原理卷，主题是硬件原理、多核心并行、高性能计算、多线程、锁、原子、无锁数据结构、并行算法、运行时、异步 I/O 和分布式计算。
- `books/compute-systems-engine-code/`：第二册配套实践与代码卷，主题是 Compute Systems Engine 的完整 C++20 实现、测试、benchmark、故障注入和报告。
- `books/cpu-volume-3/`：CPU 与 AI 计算教材第三册规划目录，主题是 AI 模型、张量、算子开发、量化和本地 CPU 推理引擎。
- `books/cpu-volume-3-practice/`：第三册配套实践与代码卷，主题是 Linux 本地量化推理引擎、完整源码、KV Cache、reference trace、7B 账本、服务 SLO、CPU/GPU 后端边界和验收门禁。
- `books/cpu-volume-3-source/`：第三册源码卷历史素材目录；默认导出已合并到第三册实践与代码卷。
- `books/algorithm-interview/`：算法刷题与 C++ 面试教材，主题是数据结构、算法原理、暴力到优化、C++ 容器、力扣题单和面试表达；训练周期只作为可调节节奏，不作为内容边界。
- `books/cpp-zero-to-advanced/`：Cpp 从零到高级教材，主题是执行模型、值和类型、RAII、标准库、泛型、构建测试、性能、并发、大型代码组织和综合项目。

规划中的新书继续放到 `books/<book-id>/` 下。分类入口已经预留：

- `books/by_type/`：按原理卷、实践与代码卷分类。
- `books/by_topic/`：按 C++、算法、计算系统、AI 计算、计算机组成原理、操作系统和网络分类。

分类目录只作为入口和索引，不放正文、实验或构建产物。真实书稿、代码、测试和导出仍以 `books/<book-id>/` 为准。

## 仓库结构

```text
books/
  by_type/             # 按原理卷、实践与代码卷整理入口
  by_topic/            # 按 C++、算法、计算系统、AI、组成原理、OS、网络整理入口
  cpu-volume-1/
    source/latex/      # 正式 LaTeX 书稿，生成 PDF/EPUB
    materials/         # 草稿、课程计划、素材、参考资料
    labs/              # 与本书绑定的代码实验
    reports/           # 报告模板和学习报告
    results/           # 实验输出，默认只跟踪 .gitkeep
    tools/             # 本书专用脚本
  cpu-volume-1-practice/
    source/latex/      # 第一册实践卷正式 LaTeX 书稿，生成 PDF/EPUB
    labs/              # Executable Evidence Kit C++20 工程
    reports/           # 研究报告输出
    results/           # 实验输出
    tools/             # 本书专用脚本
  cpu-volume-2/
    source/latex/      # 第二册原理卷正式 LaTeX 书稿，生成 PDF/EPUB
    materials/         # 草稿、课程计划、素材、参考资料
    labs/              # 早期实验和配套实践素材
    reports/           # 报告模板和学习报告
    results/           # 实验输出
    tools/             # 本书专用脚本
  compute-systems-engine-code/
    source/latex/      # 实践与代码卷正式 LaTeX 书稿
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
  cpu-volume-3-practice/
    source/latex/      # 第三册实践与代码卷正式 LaTeX 书稿
    reports/
    results/
    tools/
  cpu-volume-3-source/
    source/latex/      # 第三册源码卷历史素材 LaTeX 书稿
  algorithm-interview/
    source/latex/      # 算法面试书正式 LaTeX 主稿
    materials/         # 题单、写作标准和素材
    labs/              # C++20 算法示例和测试
    reports/           # 周报和复盘模板
    results/           # 实验输出
    tools/             # 本书检查脚本
  cpp-zero-to-advanced/
    source/latex/      # Cpp 从零到高级正式 LaTeX 主稿，生成 PDF/EPUB
CMakeLists.txt         # 仓库级 C++ 实验入口
Makefile               # 仓库级常用命令入口
```

## 分类入口

按卷类型查找：

- 原理卷：`books/by_type/theory/`
- 实践与代码卷：`books/by_type/practice_code/`
- 计划中书籍：`books/by_type/planned/`

按内容领域查找：

- C++ 语言与工程：`books/by_topic/cpp_language/`
- 算法与面试：`books/by_topic/algorithms/`
- 计算系统：`books/by_topic/computer_systems/`
- AI 计算：`books/by_topic/ai_computing/`
- 计算机组成原理预留：`books/by_topic/computer_organization/`
- 操作系统预留：`books/by_topic/operating_systems/`
- 计算机网络预留：`books/by_topic/computer_networks/`

新增书籍时，先建立真实目录，再把入口登记到 `books/by_type/README.md` 和
`books/by_topic/README.md`。不要把分类目录当成构建目录。

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

生成并测试第一册实践卷：

```bash
make cpu1p-check
make cpu1p-pdf
make cpu1p-epub
make cpu1p-test
```

生成第二册 PDF 和 EPUB：

```bash
make cpu2-check
make cpu2-pdf
make cpu2-epub
make cpu2-text-count
```

统一导出第一册、第一册实践卷、第二册、第三册、第三册实践与代码卷、算法面试书和 Cpp 从零到高级：

```bash
make books-export
```

该命令会重新构建各本书的 PDF/EPUB，并把 `book-exports/` 下每本书的导出目录清理成一份不含短横线、不含加号的 PDF 和 EPUB。EPUB 不再使用 `cpu-volume-1.epub` 这类带短横线的机器名，也不在导入文件名里使用 `C++`，避免微信读书导入链路误处理特殊字符。

整理手机导出目录：

```bash
make phone-export-organize
```

手机目录使用 `/mnt/sdcard/STU/BOOKS/按卷类型/{原理卷,实践与代码卷}/书名/` 保存真实 PDF/EPUB；`按内容领域/` 只保存 Markdown 索引说明，不复制书文件，也不生成 `.txt` 索引，避免微信读书把同一本书或目录说明扫描成多份。

直接导出到手机：

```bash
make phone-books-export
```

该命令会把每本书复制到分类后的手机目录，并在复制后重新生成手机索引。

生成并测试第三册实践与代码卷：

```bash
make cpu3p-check
make cpu3p-pdf
make cpu3p-epub
make cpu3p-test
```

第三册源码阅读和完整代码清单已经并入 `cpu3p-*` 目标。`cpu3s-*` 目标仅保留为历史素材构建入口，不再进入默认导出。

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

Cpp 从零到高级也可以进入单本书目录执行：

```bash
cd books/cpp-zero-to-advanced
make check
make pdf
make epub
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
- 同一个主题需要原理、实践、源码或代码三类产物时，优先拆成独立书籍目录，再通过 `books/by_type/` 和 `books/by_topic/` 聚合入口。

## 当前第一册入口

- 第一册说明：`books/cpu-volume-1/README.md`
- 正式 LaTeX 工程：`books/cpu-volume-1/source/latex/README.md`
- 正式主稿：`books/cpu-volume-1/source/latex/main.tex`
- 16 周执行计划：`books/cpu-volume-1/materials/x86_64_hpc_ai_16_week_bootcamp.md`
- Markdown 草稿素材：`books/cpu-volume-1/materials/drafts/README.md`
- Lab 00：`books/cpu-volume-1/labs/lab00_benchmark_foundation/README.md`

## 当前第二册入口

- 第二册说明：`books/cpu-volume-2/README.md`
- 原理卷正式主稿：`books/cpu-volume-2/source/latex/main.tex`
- 实验路线：`books/cpu-volume-2/source/latex/appendices/app-a-lab-roadmap.tex`
- 硬件执行模型：`books/cpu-volume-2/source/latex/chapters/part01-hardware-foundations/`
- 单机高性能计算：`books/cpu-volume-2/source/latex/chapters/part02-single-node-hpc/`
- 多线程与同步：`books/cpu-volume-2/source/latex/chapters/part03-concurrency-synchronization/`
- 并行算法与运行时：`books/cpu-volume-2/source/latex/chapters/part04-parallel-algorithms-runtime/`
- 分布式计算：`books/cpu-volume-2/source/latex/chapters/part05-distributed-computing/`
- 早期实验素材：`books/cpu-volume-2/labs/compute_systems/`

## 当前实践与代码卷入口

- 实践与代码卷说明：`books/compute-systems-engine-code/README.md`
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
