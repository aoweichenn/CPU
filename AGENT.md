# Hardware Book (硬件从零到整机) — Expansion Roadmap

## Planned 20-Chapter Structure

| Ch | Title | Key Questions | End-of-chapter Project |
|----|-------|---------------|----------------------|
| 1 | 数字抽象与电气基础 | 电压、噪声裕量、阈值、负载、电平标准、受控电流路径→可靠 bit | 电平约定图、噪声裕量题 |
| 2 | 组合逻辑与布尔代数 | 真值表、NAND/NOR 完备性、译码器、mux、毛刺、传播延迟 | 逻辑化简与毛刺案例 |
| 3 | 时序逻辑、寄存器和状态机 | 锁存器、触发器、setup/hold、复位、同步器、计数器、FSM | 状态机分析表 |
| 4 | RTL 与硬件描述语言阅读 | Verilog 组合块、时序块、非阻塞赋值、综合边界、testbench | RTL 走读题 |
| 5 | 数值表示与算术电路 | 无符号、补码、定点、浮点、加法器、乘除法、标志位 | 边界向量表 |
| 6 | ISA、汇编和机器码 | 指令格式、寻址模式、条件码、调用约定、栈帧 | 汇编到机器码 trace |
| 7 | 数据通路与控制器 | 寄存器堆、ALU、mux、控制字、硬连线/微程序控制 | 控制信号表 |
| 8 | 单周期与多周期 CPU | IF/ID/EX/MEM/WB 状态边界、关键路径、等待状态 | 访存周期图 |
| 9 | 流水线、冒险和异常 | 结构/数据/控制冒险、转发、停顿、冲刷、精确异常 | 五级流水线题 |
| 10 | cache 与存储层次 | locality、tag/index/offset、替换、写策略、一致性 | cache 命中/miss 推演 |
| 11 | 虚拟内存、TLB 和页表 | 虚拟地址、页表、TLB、权限、缺页、IOMMU | 地址转换案例 |
| 12 | DRAM/DDR 与内存控制器 | bank、row buffer、refresh、ECC、训练、调度 | DRAM 访问时序图 |
| 13 | 总线、互连和 PCIe | 片选、valid/ready、AXI/APB、PCIe TLP、MSI、AER | PCIe 枚举路径 |
| 14 | MMIO、中断、DMA 和 IOMMU | 设备寄存器、中断控制器、DMA 描述符、cache 可见性 | 设备事务 trace |
| 15 | SSD、硬盘、网络与外设队列 | NVMe、SATA、描述符环、错误恢复、尾延迟 | NVMe/网卡队列题 |
| 16 | GPU、VRAM 和显示系统 | SIMT、shader、命令缓冲、fence、帧缓冲 | GPU 命令路径图 |
| 17 | 主板、固件、启动和平台管理 | VRM、UEFI、PCH、DDR 训练、ACPI、EC | 启动 bring-up 日志 |
| 18 | 链接、装载和系统软件边界 | ELF、重定位、动态链接、loader、系统调用 | ELF/loader 纸面项目 |
| 19 | 性能分析与定量方法 | CPI、Amdahl、roofline、cache miss 代价、瓶颈定位 | 性能分析题 |
| 20 | 综合纸面项目与故障案例集 | 全链路 trace：开机→读文件→执行→设备→GPU→错误恢复 | 整机规格与排错报告 |

## Current 13-Chapter → Planned 20-Chapter Mapping

- ch01 (电平、开关、逻辑门与组合电路) → 规划 ch1-2
- ch02 (状态、时钟与同步边界) → 规划 ch3
- ch03 (数值、ALU、数据通路与控制器) → 规划 ch5, ch7
- ch04 (最小 CPU 与控制 trace) → 规划 ch6, ch8, ch9
- ch05 (主存、总线、I/O 与 DMA) → 规划 ch10-15
- ch06 (经典芯片、启动与平台演进) → 规划 ch16, ch17
- ch07 (动手搭一台 8-bit 教学计算机) → 综合实践章
- ch08 (电源、时钟与信号完整性) → 规划 ch1, ch3 工程延伸
- ch09 (RTL 与 Verilog 阅读入门) → 规划 ch4
- ch10 (性能分析与定量方法) → 规划 ch19
- ch11 (外设与队列) → 规划 ch15
- ch12 (GPU 与显示系统) → 规划 ch16
- ch13 (链接、装载和系统软件边界) → 规划 ch18
- 尚未覆盖 → 规划 ch20

## Expansion Priority

先补 ch6-15：ISA/汇编 → 数据通路 → 流水线 → cache/TLB → DRAM → PCIe → MMIO/DMA → SSD/网络 → GPU → 主板 → 链接装载 → 性能分析

## Per-Chapter Quality Standard

1. 开头给出本章要解决的硬件问题
2. 1-3 张机制图（数据路径、控制路径、状态边界、错误路径）
3. 至少一个真实案例（8086 总线、RISC-V、cache miss、NVMe、GPU fence）
4. 章末习题：概念题、trace 题、边界题、故障定位题
5. 参考解答落实到机器细节
