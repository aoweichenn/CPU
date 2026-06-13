# LaTeX 正式书稿工程

这是本项目的正式教材工程，目标是长期写成百万字级系统教材。

Markdown 目录 `docs/book/` 保留为草稿、实验说明和内容素材库；正式排版、章节编号、交叉引用、术语表、索引、习题和附录以本目录为准。

## 构建要求

推荐安装 TeX Live 完整版或至少包含中文支持的发行版：

```bash
sudo apt update
sudo apt install texlive-full latexmk
```

当前环境检查显示尚未安装 `xelatex`、`lualatex`、`latexmk`，所以本机暂时不能生成 PDF。

## 构建命令

安装 LaTeX 后运行：

```bash
cd book-latex
latexmk -xelatex -interaction=nonstopmode main.tex
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

## 写作规则

每章必须满足：

- 从具体问题开始。
- 解释机制为什么存在。
- 解释程序员抽象和底层实现模型。
- 给最小代码例子。
- 从源码走到汇编、IR、二进制或性能证据。
- 包含实验、习题、常见误区、验收标准。
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
