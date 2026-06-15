# LaTeX 正式书稿工程

这是本项目的正式教材工程。当前 PDF 是系列教材第一册，正文保留已经写成的 12 章；后续现代 CPU、SIMD、HPC 和 AI 算子内容进入第二册规划。

Markdown 目录 `docs/book/` 保留为草稿、实验说明和内容素材库；正式排版、章节编号、交叉引用、术语表、索引、习题和附录以本目录为准。

## 构建要求

推荐安装 TeX Live 完整版或至少包含中文支持的发行版：

```bash
sudo apt update
sudo apt install texlive-full latexmk
```

当前环境已配置 `xelatex` 和 `latexmk`，可以直接生成 PDF。

## 构建命令

安装 LaTeX 后运行：

```bash
cd book-latex
latexmk -xelatex -interaction=nonstopmode main.tex
```

也可以使用工程提供的 Makefile：

```bash
cd book-latex
make check
make pdf
make epub
```

清理：

```bash
latexmk -C
```

## 目录结构

```text
book-latex/
  main.tex
  preamble/
  frontmatter/
  chapters/
  appendices/
  backmatter/
  outline/
```

## 排版规则

当前 PDF 采用接近经典系统教材的克制版式：

- 目录只保留部和章，章内小节不进入目录。
- 正文只保留章编号，章内主题标题不显示 `2.x`、`6.x` 这类密集编号。
- 正文章标统一使用中文“第 N 章”；附录使用“附录 A”。
- 超链接不显示蓝色边框或蓝色文字，目录点线使用弱灰色。
- 提示、误区、深入理解等块使用黑灰教材风格，不使用彩色网页卡片。
- 实验和习题块使用白底左线式材料块，避免大面积灰底和框套框。
- 代码清单使用 Maple Mono NL NF CN，保持等宽缩进、细线分隔和长行换行标记。
- EPUB 由 `scripts/build_epub.py` 从正式 LaTeX 源生成，导航只保留前置内容、部、章、附录和术语表，适合手机阅读；构建时会尽量嵌入 Maple Mono NL NF CN，保持代码块观感。

## 写作规则

每章必须满足：

- 从具体问题开始。
- 解释机制为什么存在。
- 解释程序员抽象和底层实现模型。
- 给最小代码例子。
- 从源码走到汇编、IR、二进制或性能证据。
- 包含实验、习题、常见误区、验收标准。
- 尽量包含贯穿案例、可复现实验和真实系统源码阅读入口；Linux 源码案例见 `appendices/app-f-linux-source-reading.tex`。
- 关键术语使用 `\term{中文}{English}` 或 `\engterm{English}` 标注。

更详细规则见：

- `docs/book/book-style-guide.md`
- `book-latex/outline/book-architecture.tex`

## 与 16 周计划的关系

16 周计划是学习执行路线，LaTeX 书稿是知识体系。

学习时：

```text
16 周计划决定每天做什么
LaTeX 书稿解释为什么和怎么理解
labs/ 提供代码实验
docs/reports/ 保存报告
```
