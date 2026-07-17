---
name: book-export-workflow
description: 本仓库改完任何一本书的 LaTeX 并重建 PDF 后，必须导出到手机目录 /mnt/sdcard/STU/BOOKS 的固定流程
whenToUse: 每次修改任何 books/ 下书稿、重建 PDF、提交推送之后，以及用户要求导出书到手机/微信读书时必读
---

# 书籍改动后的固定收尾：导出到手机目录

用户明确要求（2026-07-17）：**每次改完一本书并重建 PDF 后，都要导出到手机目录**，不得只提交 git 就结束。

## 标准流程

1. 改书稿 → 构建 PDF（`make <book>-pdf` 或书目录内 `make pdf`）。
2. 导出到手机目录：根目录执行 `make <book>-phone-export`。该目标会自动重建 PDF（若无变化则秒过）、复制 PDF、并运行 `phone-export-organize-allow-missing` 整理校验。
3. 若该书没有根 Makefile 的 `<book>-phone-export` 目标（目前只有 `operating-systems-volume-1` 如此），改用在书目录内执行 `make -C books/<book-id> phone-export`，然后回到根目录补一句 `make phone-export-organize-allow-missing`。

常用目标名：`cpu`（第一册）、`cpu1p`、`cpu2`、`cpu3`、`cpu3p`、`algo`、`cpp`、`cse`、`hw`、`os`（无根目标，用书内命令）。

全部书一次导出：`make phone-books-export`。

## 手机目录约定（不要破坏）

- 根：`/mnt/sdcard/STU/BOOKS`，结构为 `按卷类型/{原理卷,实践与代码卷}/<书名>/<书名>.pdf`。
- 每个书目录里**恰好一份 PDF**，文件名即书名，不含空格、`+`、任何类连字符字符——这是微信读书导入链路的硬约束，防止同一本书被扫描成多份。
- 导出后必须运行 `tools/organize_phone_exports.py`（由上述 make 目标代劳），它会清掉游离文件、旧目录和 `.epub/.md/.txt` 残留。
- `book-exports/` 是本地产物目录（`make <book>-export`），与手机目录是两条独立链路；默认导手机目录就够，除非用户另行要求。

## 易错点

- 导出脚本 `tools/export_book.py` 有白名单护栏：目标必须在 `book-exports/` 或 `/mnt/sdcard/STU/BOOKS` 下，别手写 cp 绕过它。
- 导出的是构建产物 `source/latex/main.pdf`：先构建再导出，顺序不能反。
- `cpu-volume-3-source` 是历史素材，不导出手机（已并入 cpu3p）。
