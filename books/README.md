# Books

这个目录是全仓库的书库根。每一本书都必须有自己的目录，不能把正文、实验、工具和结果直接散落在仓库根目录。

当前书籍：

| book id | status | description |
|---|---|---|
| `cpu-volume-1` | active | CPU 底层原理教材第一册：程序、C++、二进制、x86-64、汇编、Linux 工具链和性能测量。 |
| `cpu-volume-1-practice` | active | 第一册配套实践卷：Executable Evidence Kit、ELF64 解析、ABI/符号观察、Linux 工具链、GTest/GMock、Google Benchmark 和研究报告。 |
| `cpu-volume-2` | active | 从 C++ 到计算系统第二册原理卷：硬件原理、多核心并行、高性能计算、多线程、同步、无锁数据结构、并行算法、运行时、异步 I/O 和分布式计算。 |
| `compute-systems-engine-code` | active | 第二册配套代码实践卷：围绕 Compute Systems Engine 实现完整 C++20 工程、测试、benchmark、故障注入和报告。 |
| `cpu-volume-3` | planning | CPU 与 AI 计算教材第三册：AI 模型、张量、算子开发、量化和本地 CPU 推理引擎。 |
| `cpu-volume-3-practice` | active | 第三册配套实践卷：Linux 本地量化推理引擎、KV Cache、reference trace、7B 账本、服务 SLO 和验收门禁。 |
| `cpu-volume-3-source` | active | 第三册源码卷：LCQI 本地 CPU 推理工程完整源码、测试、benchmark、工具脚本和阅读路径。 |
| `algorithm-interview` | active | 算法刷题与 C++ 面试教材：LaTeX 正式书稿，覆盖数据结构、算法原理、暴力到优化、C++ 容器、力扣题单、训练路线和面试表达。 |
| `cpp-zero-to-advanced` | active | Cpp 从零到高级教材：执行模型、类型、RAII、标准库、泛型、构建测试、性能、并发和综合项目。 |

## 分类入口

分类目录只提供入口和索引，真实书籍工程仍在 `books/<book-id>/`。构建、导出、测试和正文中的源码引用不要改到分类目录。

按卷类型：

| type | path | purpose |
|---|---|---|
| 原理卷 | `by_type/theory/` | 主教材和概念推导。 |
| 实践卷 | `by_type/practice/` | 分阶段任务、实验步骤、验收标准和报告。 |
| 源码卷或代码卷 | `by_type/code/` | 完整源码、构建入口、测试、benchmark 和阅读路径。 |
| 计划中 | `by_type/planned/` | 后续新卷的命名和拆分计划。 |

按内容领域：

| topic | path | current or planned scope |
|---|---|---|
| C++ 语言与工程 | `by_topic/cpp_language/` | C++ 从零到高级。 |
| 算法与面试 | `by_topic/algorithms/` | 算法刷题与 C++ 面试。 |
| 计算系统 | `by_topic/computer_systems/` | 从 C++ 到计算系统第一册、第二册及其实践或代码卷。 |
| AI 计算 | `by_topic/ai_computing/` | 第三册、第三册实践卷和源码卷。 |
| 计算机组成原理 | `by_topic/computer_organization/` | 后续组成原理原理卷、实践卷和源码卷预留。 |
| 操作系统 | `by_topic/operating_systems/` | 后续 OS 原理卷、实践卷和源码卷预留。 |
| 计算机网络 | `by_topic/computer_networks/` | 后续网络原理卷、实践卷和源码卷预留。 |

## 书籍目录规范

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

目录含义：

- `source/`：正式成书源码，例如 LaTeX、Markdown、脚本生成工程。
- `materials/`：草稿、素材、参考资料、课程计划、题单。
- `labs/`：和本书绑定的代码实验。
- `reports/`：报告模板、读书报告、实验报告。
- `results/`：实验结果和生成数据，通常只跟踪 `.gitkeep` 或精选样例。
- `tools/`：本书专用工具脚本。

跨书共享的脚本、模板或风格文件必须先证明有复用价值，再提升到仓库级目录，避免为了抽象而抽象。

如果一本主题书需要拆出实践卷、源码卷或代码卷，优先创建独立真实目录，再在 `by_type/` 和 `by_topic/` 添加入口。分类入口不能承载正文和实验，避免构建路径、导出路径和书内源码清单互相漂移。
