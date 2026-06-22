# 当前会话状态

导出时间：2026-06-23，Asia/Shanghai

## 仓库

```text
repo: /home/aoweichen/demos/STU/CPP/BASE/CPU
branch: main
latest committed before this export: 5b66bc8 Refine CPU volume 1 formal prose
remote push target used previously: ssh://git@ssh.github.com/aoweichenn/CPU.git main:master
```

## 第一册产物

```text
books/cpu-volume-1/source/latex/main.pdf 4264081 bytes
books/cpu-volume-1/source/latex/main.epub 623738 bytes
```

导出副本：

```text
book-exports/从C++到计算系统第一册/main.pdf 4264081 bytes
book-exports/从C++到计算系统第一册/main.epub 623738 bytes
book-exports/从C++到计算系统第一册/从C++到计算系统第一册.pdf 4264081 bytes
book-exports/从C++到计算系统第一册/从C++到计算系统第一册.epub 623738 bytes
```

## 第一册文本统计

统计命令：

```bash
python3 books/cpu-volume-1/source/latex/scripts/count_text_units.py --chapters-only
python3 books/cpu-volume-1/source/latex/scripts/count_text_units.py
```

结果：

```text
chapters: 336793 units (324194 CJK + 12599 EN)
book total: 345223 units (332204 CJK + 13019 EN)
```

## 已通过检查

```bash
cd books/cpu-volume-1/source/latex
make pdf
make epub
rg -n -F 'Overfull \\hbox' main.log || true
```

结果：

```text
make pdf: passed
make epub: passed
Overfull hbox scan: no matches
EPUB image_entries: 0
```

正式性扫描：

```bash
rg -n "课堂讲解|报告模板|毕业|教材训练|教学实验|教学级|教师视角|写作目标|规模目标|反凑数|交付物标准|大师级|从零到精通|高质量教材|经典教材式|质量底线|读者自学|学习复盘|完成本章后|LaTeX 写作规范|扩写节奏" books/cpu-volume-1/source/latex || true
```

结果：无匹配。

## 后续推荐动作

另一台机器拉取后，如果继续做书稿：

1. 先检查 `git status --short`。
2. 如果继续第一册，先读：
   - `books/cpu-volume-1/source/latex/frontmatter/preface.tex`
   - `books/cpu-volume-1/source/latex/outline/book-architecture.tex`
   - `books/cpu-volume-1/source/latex/chapters/part00-foundations/ch00-mental-model.tex`
3. 如果继续第二册，先读最新章节和 `books/cpu-volume-2/source/latex` 构建脚本。
4. 修改后必须构建 PDF/EPUB，并导出对应 `book-exports/...` 目录。
