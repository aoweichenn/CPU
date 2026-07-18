---
name: hardware-book-style
description: 《硬件从零到整机》写作规范——经典教材口吻（禁审计腔词汇）与统一 x86-64 Intel 汇编语法。改写或新增该书任何章节内容时必须遵守。
---

# 《硬件从零到整机》写作规范

手稿位置：`books/hardware-zero-to-machine/source/latex/`（chapters/、frontmatter/、backmatter/）。

## 一、口吻：经典教材陈述式（2026-07 用户明确要求）

对标 CSAPP / Patterson&Hennessy / Harris&Harris 中译本：先陈述机制事实，再解释原因，最后说明意义。**禁止审计腔词汇**：

| 禁用 | 改为（按语境） |
|---|---|
| 验收 / 可验收 / 验收证据 / 验收重点 / 通过标准 / 验收点 | 检查、观察、预期行为、实现要点、调试点，或重写句子 |
| 证据（"波形证据""ALU 证据"） | 删除或换成具体对象（"波形图""ALU 的输出"） |
| 账本 / 账是平的 / 对账 | 过程、序列、逐项核对 |
| （硬件）合同 | 约定 |
| 可观察边界 | 观察点、接口 |
| 落到 / 绑在一起 / 压成 / 跑飞 | 书面化表达（对应到 / 联系起来 / 编码为 / 失去控制） |

- 改写=换表达，**不删技术内容、不改数值结论**；表格只改列名和措辞，技术数据行不动。
- `trace` 是标准术语可保留，每章首次出现建议加"执行轨迹"注释。
- 图内 TikZ 标签、`\diagramnote`、章末练习与解答同样适用。

## 二、指令集：统一 x86-64 Intel 语法

- 目的操作数在前，内存写作 `[基址+偏移]`；软件级示例用真实寄存器名（`rax/rdi/rsi/rcx/rbx/rsp/rbp`，小写）。
- ch04 教学 ISA 只换助记符，**16-bit 编码 / hex / opcode / 字段布局 / trace 数值一律不变**：

| 旧助记符 | 新（x86-64 Intel） | 编码 |
|---|---|---|
| `LDI Rd, imm8` | `MOV Rd, imm8` | `0010 d x imm8` 不变 |
| `SLI Rd,Ra,imm4` | `SHL Rd, imm4`（注明编码 a=d） | `0111 d a imm4 --` 不变 |
| `LOAD Rd,[Ra+o]` | `MOV Rd, [Ra+o]` | `0011` 不变 |
| `STORE Rs,[Ra+o]` | `MOV [Ra+o], Rs` | `0100` 不变 |
| `ADD Rd,Ra,Rb` | `ADD Rd, Rb`（注明 a=d，二操作数） | `0001` 不变 |
| `JZ Rr, +off` | 保留（注明：比较+跳转融合，x86-64 对应 `test/cmp`+`jz`） | `0101` 不变 |
| `JMP/CALL/RET` | 不变 | 不变 |

- 教学硬件寄存器名保留 `R0–R7`（3-bit 字段）；纯软件示例（循环、调用约定、栈帧）用 x86-64 全名。
- ch03 的 ALU 运算名（ADD/SUB/AND/OR/SLL/SLT）是运算选择名，不是程序语法，保留。
- ch05 泛指的 load/store 体系结构术语保留；具体指令示例改 `MOV`。8086 `IN/OUT`、ch06 的 6502/Z80/8086 真实历史指令集保持原样。

## 三、改完必做

1. 根目录 `make hw-pdf`：零 `^! ` 错误、零 Overfull。
2. 改动涉及图或版式时 `mutool draw` 渲染目检。
3. `make hw-phone-export` 导出手机（见 book-export-workflow 技能）。
4. `git add books/hardware-zero-to-machine/ && git commit && git push origin main`（见 repo-git-workflow 技能）。
