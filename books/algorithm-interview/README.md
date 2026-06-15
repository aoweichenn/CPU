# 算法刷题与 C++ 面试教材

这是一本面向刷题找工作的 LaTeX 教材。目标不是堆题解，而是把每类算法讲成一条可以复用的推理链：

```text
题目条件 -> 暴力枚举 -> 重复工作在哪里 -> 数据结构选择 -> 可维护的不变量 -> 优化算法 -> C++ 实现 -> 复杂度证明 -> 变形题
```

本书正式主版本放在 `source/latex/`，使用 XeLaTeX 构建，排版要求和 CPU 第一册保持同一类经典教材风格。这里不维护 Markdown 正式稿；后续如果生成 EPUB，也从 LaTeX 主稿派生。

## 目录结构

```text
source/latex/        # 正式 LaTeX 书稿
materials/           # 题单、写作标准、扩写计划
labs/cpp20-examples/ # 可编译 C++20 示例和测试
reports/             # 周报和复盘模板
results/             # 运行结果，默认不提交批量输出
```

## 当前目标

- 制定 3 个月刷题计划。
- 数据结构单独成章讲清楚，数组、字符串、链表、栈、队列、哈希表、堆、树、图、并查集都要和算法应用绑定。
- 每个算法都讲清楚暴力版本、优化版本、多解法版本。
- 解释为什么能优化，而不是只给结论。
- 用 C++20 展示代码，强调容器用法、复杂度、迭代器失效、内存和常见坑。
- 每类算法绑定力扣案例题目、变形题、复盘模板和面试表达方式。

## 命令

检查 LaTeX 书稿结构：

```bash
make check
```

生成 PDF：

```bash
make pdf
```

构建并测试 C++ 示例：

```bash
make test
```

## 已有入口

- 正式主稿：`source/latex/main.tex`
- 三个月计划：`source/latex/chapters/part00-method/ch01-three-month-plan.tex`
- 数据结构主线：`source/latex/chapters/part01-data-structures/`
- 核心算法主线：`source/latex/chapters/part02-core-algorithms/`
- 题单地图：`materials/problem-map.md`
- 写作标准：`materials/writing-standard.md`
- 示例代码：`labs/cpp20-examples/`

## 质量规则

每个算法章节必须包含：

- 问题模型：输入、输出、约束、隐藏条件。
- 暴力解：枚举对象、正确性、复杂度、为什么慢。
- 优化动机：重复工作、单调性、局部状态、可交换性或最优子结构。
- 数据结构选择：结构保存什么、查询什么、更新什么，复杂度是多少。
- 优化解：不变量、边界、证明、复杂度。
- C++ 实现：容器选择、API 用法、常见坑。
- 力扣案例：基础题、标准题、变形题、综合题。
- 练习路线：当天题、复盘题、一周后回访题。
- 面试表达：如何在白板或在线面试里讲清楚。
