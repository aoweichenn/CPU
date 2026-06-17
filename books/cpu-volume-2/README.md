# CPU 计算系统教材第二册

本目录是第二册的自包含工程。第一册讲程序、二进制、x86-64、ABI、Linux 工具链和可信性能测量；第二册讲计算系统：现代 CPU 硬件原理、多核心并行、高性能计算、多线程、锁、条件变量、原子操作、内存模型、无锁数据结构、并行算法、异步 I/O 和分布式计算。AI 模型、算子开发和本地量化推理引擎放入第三册。

## 目录结构

```text
source/latex/      # 正式 LaTeX 书稿，生成 main.pdf 和 main.epub
materials/         # 草稿、素材、参考资料和扩写计划
labs/              # 与本书绑定的代码实验
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

## 贯穿项目

贯穿项目位于：

```text
labs/compute_systems/
```

当前第一阶段实现了计算系统基础实验：

- 顺序归约和并行归约
- mutex 计数器和 atomic 计数器
- bounded MPMC 队列
- CLI demo
- CTest 正确性测试
