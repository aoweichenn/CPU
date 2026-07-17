# Computer Organization and Operating Systems Volume 1

《计算机组成原理与操作系统》第一版工程。它把组成原理作为 OS 的前半部，从数字电路、CPU、总线、MMIO、DMA、IOMMU、特权级、MMU 和中断控制器讲起，再进入资源抽象、并发控制、隔离、I/O 和恢复。正式书稿使用 LaTeX 编写，并按“从失败案例进入、拆开概念、给出机制、落到证据”的节奏组织每个主题。

## 正文结构

本册固定为 12 个编号章。硬件基础单独作为第 2 章，其他硬件、源码和实验深讲材料只能嵌入相关主章，不能再扩成第 13 章、第 14 章一类的并列目录。

12 个主章是：

1. 操作系统地图与契约
2. 硬件基础：CPU、内存、总线和设备
3. 硬件事实怎样逼出内核对象
4. 进程、线程与调度
5. 系统调用、权限与内核边界
6. exec、ELF 装载与用户栈
7. 虚拟内存与地址空间
8. mmap、page cache 与文件映射
9. 设备驱动、中断下半部与 DMA
10. 文件、设备与 I/O
11. socket、网络入口与内核边界
12. 安全、隔离与最小权限

## 目录

- `source/latex/`：LaTeX 正式主稿、章节、前言和附录。
- `source/latex/chapters/`：主线章节，以及跟具体主章绑定的源码级补充材料。
- `source/latex/supplements/hardware/`：硬件深讲素材，供计算机组成/硬件补充卷或选读使用，不直接插入第二章后面。
- `source/latex/supplements/pre-os/`：裸机、单片机、中断、定时器和启动前置素材，可作为“从裸机到内核入口”选读。
- `source/latex/supplements/models/`：C++20 模型、x86-64 契约模型、第一阶段验收和硬件到 OS 对象映射，优先归入实践与代码卷。

补充材料的入口在书末“补充材料阅读地图”。主线正文不得把这些 detail 重新作为隐藏章节塞回第二章或第三章后面。

## 常用命令

```bash
make check
make pdf
make phone-export
```
