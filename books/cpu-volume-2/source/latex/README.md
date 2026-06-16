# LaTeX 正式书稿工程

这是 `books/cpu-volume-2` 的正式教材工程。第二册主题是 Linux 上的现代 CPU 性能工程、多核心并发、缓存和 TLB、SIMD、软件优化、AI 基础、算子开发，以及本地 CPU 量化推理引擎实现。

## 构建命令

```bash
make check
make pdf
make epub
make text-count
```

生成文件：

- `main.pdf`
- `main.epub`

## 排版规则

- 版式、字体、代码块、目录层级和 EPUB 约束沿用第一册。
- 目录只保留部和章，章内小节不进入目录。
- 正文使用编号小节串联内容，避免碎片化罗列。
- 代码使用 Maple Mono NL NF CN，保持等宽缩进、语法颜色和行号。
- EPUB 不生成封面页，不包含图片文件或图片标签，不输出 XHTML 表格。

## 写作规则

- 原理先行，再给代码和实验。
- 性能结论必须配合计算量、内存流量、输入形状和测量边界。
- C++ 示例使用位宽明确的整数类型，例如 `std::int32_t`、`std::int64_t` 和 `std::uint64_t`。
- 不把源码阅读写成函数列表，要围绕数据结构、热路径、benchmark 和工程报告展开。
- 贯穿项目是 `labs/linux_cpu_inference/`，后续扩写要让章节内容能落到这个项目上。
