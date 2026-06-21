# 从 C++ 到计算系统：第二册

本目录是第二册原理卷的自包含工程。第一册讲程序、二进制、x86-64、ABI、Linux 工具链和可信性能测量；第二册讲计算系统原理：现代 CPU 硬件、多核心并行、高性能计算、多线程、锁、条件变量、原子操作、内存模型、无锁数据结构、并行算法、运行时、异步 I/O 和分布式计算。

第二册正文不再把完整 C++ 工程实现塞入每章，而是用伪代码、关键片段、状态表、机制图和实验矩阵讲清原理。完整代码、测试、benchmark、故障注入和实验报告进入配套代码实践卷：

```text
books/compute-systems-engine-code/
```

AI 模型、算子开发和本地量化推理引擎放入第三册。

## 目录结构

```text
source/latex/      # 正式 LaTeX 书稿，生成 main.pdf 和 main.epub
materials/         # 草稿、素材、参考资料和扩写计划
labs/              # 早期代码实验和本书配套实践素材
reports/           # 实验报告模板
results/           # 实验输出
tools/             # 本书专用脚本
```

## 生成书籍

```bash
make check
make pdf
make epub
make text-count
```

生成文件位置：

- `source/latex/main.pdf`
- `source/latex/main.epub`

## 本书维护规则

- 正式成书以 `source/latex/` 为准。
- 本书只维护一个正式版本，不再并行维护 Markdown 版本。
- 排版、字体、代码样式、目录层级和 EPUB 约束沿用第一册。
- 章节写作顺序保持“原理先行、模型约束、代码证据、测量验证、源码入口”。
- C++ 示例统一使用位宽明确的整数类型，例如 `std::int32_t`、`std::int64_t` 和 `std::uint64_t`。
- 所有性能结论必须说明输入规模、数据布局、缓存行为、指令约束和测量边界。

## 贯穿项目与代码卷

贯穿项目统一命名为 `Compute Systems Engine`。在本目录中，它作为原理主线出现；完整工程实现由配套代码实践卷维护：

```text
books/compute-systems-engine-code/
```

本目录仍保留早期实验素材：

```text
labs/compute_systems/
```

这些实验可作为代码卷的迁移起点，但不再代表完整实践卷结构。当前早期实验包含：

- 顺序归约和并行归约
- mutex 计数器和 atomic 计数器
- bounded MPMC 队列
- CLI demo
- CTest 正确性测试
