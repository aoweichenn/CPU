# 现稿到二十六章的无损迁移账本

## 状态规则

每个条目只能使用以下状态：

- `pending`：尚未迁移。
- `copied`：已进入新章节，但尚未逐项核对或构建。
- `verified`：正文、图表、公式、代码、例题、习题与答案均已核对，且新输入图构建成功。

只有全部条目达到 `verified`，才允许移除旧章节在 `main.tex` 中的入口。旧源文件继续保留，直至用户明确批准清理。

## 章节级账本

| 旧文件 | 必须保留的内容单元 | 新章 | 状态 |
|---|---|---:|---|
| `ch01-electricity-diode-switch.tex` | 电平与噪声余量；器件与受控电流路径 | 1 | verified |
| 同上 | 逻辑门；组合逻辑；延迟、负载、毛刺与检查 | 2 | verified |
| `ch02-feedback-latches-flipflops.tex` | 反馈/锁存器；触发器/采样窗口；关键路径/复位/CDC；寄存器/计数器/移位；FSM | 3 | verified |
| `ch09-rtl-verilog-reading.tex` | 电路到 RTL；组合写法；时序写法；综合边界；testbench/仿真 | 4 | verified |
| `ch03-number-arithmetic-circuits.tex` | 数值解释；ALU；进位、移位、比较与乘法 | 5 | verified |
| `ch04-minimal-cpu.tex` | 极小 ISA；可见状态、条件码与调用约定 | 6 | verified |
| `ch03-number-arithmetic-circuits.tex` | 数据通路；控制器 | 7 | verified |
| `ch04-minimal-cpu.tex` | 动作编码、控制字、可接线数据通路、整机 trace | 7–8 | verified |
| `ch06-classic-chips-platforms.tex` | 6502、Z80、8086、80386 处理器案例 | 8、12 | verified |
| `ch04-minimal-cpu.tex` | 异常、中断与精确异常；调试案例 | 9 | verified |
| `ch10-performance-analysis.tex` | 时延/吞吐/利用率；CPI；Amdahl；带宽/roofline；测量案例 | 9、26 | verified |
| `ch05-memory-bus-io-dma.tex` | 主存概览；存储层次、地址转换和设备 trace | 10–12、16 | verified |
| `ch05b-storage-cell-structures.tex` | bit→字→阵列→系统；层次总览 | 10 | verified |
| 同上 | SRAM 单元与阵列 | 11 | verified |
| 同上 | DRAM 单元/感放/阵列；DDR/PHY/训练；控制器/调度 | 13 | verified |
| 同上 | NAND、3D 堆叠与 FTL | 14 | verified |
| 同上 | 磁盘物理、伺服与磁头定位 | 15 | verified |
| 同上 | 四条读写 trace；一次 `read()` 的完整旅程 | 16 | verified |
| `ch11-peripherals-queues.tex` | 块设备与文件系统硬件接口；SSD | 14、16 | verified |
| 同上 | 机械硬盘与访问调度 | 15 | verified |
| 同上 | 网卡与网络队列；综合案例与尾延迟 | 19、23 | verified |
| `ch05-memory-bus-io-dma.tex` | 地址译码；教学总线到现代互连 | 17–18 | verified |
| 同上 | MMIO；中断；DMA | 19 | verified |
| `ch08-power-clock-signal-integrity.tex` | 电源；时钟树；传输线/反射/端接；串扰/回流/EMC；测量 | 20 | verified |
| `ch06-classic-chips-platforms.tex` | 整机启动与平台演进；目标文件、固件与可启动机器 | 21、25 | verified |
| `ch-control-plane.tex` | 控制面总览；寄存器/FSM；内存控制；平台控制器/IOMMU；主板/固件；反馈/PID；驱动/中断/调度；整机控制图 | 7、13、19、21–22 | verified |
| `ch12-gpu-display.tex` | CPU/GPU 对比；SIMD/SIMT；VRAM/HBM；命令环/fence/显示扫描；图形与通用计算 | 24 | verified |
| `ch13-linking-loading.tex` | 目标文件/符号；重定位；装载/动态链接；系统调用/异常控制流；源码到进程案例 | 25 | verified |
| `ch07-breadboard-computer.tex` | 总体架构；寄存器/ALU；内存/输出；控制单元/微码；编程/调试/扩展 | 26 | verified |
| `frontmatter/preface.tex` | 读者对象、阅读路线、章节数量与结构描述 | 全书定稿时更新 | verified |
| `backmatter/checklist.tex` | 全部检查项及与章节的引用关系 | 1–26 | verified |

## 每条账本的核对清单

迁移一个条目时，逐项确认：

1. 所有自然段和脚注存在。
2. 所有 `figure`、`table`、公式、代码块、波形和 trace 存在。
3. 所有 `label`、`ref`、引用和资源路径仍可解析。
4. 所有概念题、计算题、边界题、排错题和参考答案存在。
5. 重叠内容已经标注主讲位置，但没有擅自删减。
6. 新章可从 `main.tex` 到达，并通过现有 `check` 与 PDF/EPUB 构建。
7. 章节开头、过渡和小结已适配新上下文；这些新增文字不得替代原内容。

## 批次建议

按依赖顺序迁移，且每批完成后构建：

1. 第 1–4 章：数字基础与 RTL。
2. 第 5–9 章：处理器核心。
3. 第 10–16 章：完整存储大部分。
4. 第 17–19 章：互连与设备事务。
5. 第 20–22 章：主板与平台。
6. 第 23–25 章：外设、GPU 与软件边界。
7. 第 26 章、前言、目录桥接和全书检查表。

## 2026-07 内容增补批次

本批次只新增读者内容，不改变既有 98 个迁移单元的归属。每个增补项仍按“正文、图表、练习、答案、构建”五项核对。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 17 章 | 仲裁、burst、outstanding、协议桥、跨时钟域、crossbar/NoC、QoS 与错误闭合 | verified |
| 第 18 章 | PCIe PHY/LTSSM、数据链路重放、credit、TLP、枚举、BAR、MSI-X、AER 与带宽演算 | verified |
| 第 12 章 | 现代多级页表、TLB 微结构、权限、缺页、shootdown、大页与嵌套转换 | verified |
| 第 11 章 | Cache 命中路径、替换与写回、MSHR、预取、包含关系、MESI、伪共享与 ECC | verified |
| 第 16 章 | 持久化顺序、日志/COW、RAID、写洞、校验清洗、重建与错误传播 | verified |
| 第 22 章 | RAS、机器检查、看门狗、遥测、故障隔离、降级运行与恢复层级 | verified |
| 第 23 章 | USB/xHCI、HID、音频时钟、传感器采样和外设故障定位 | verified |
| 第 6 章 | 真实 ISA 对照、寻址、字节序、特权/原子指令与机器码 trace | verified |
| 第 1、5、6、11--19、21、22 章 | 概念、trace、边界条件、故障定位练习及机器级答案 | verified |

## 2026-07 非存储部分连贯性增补批次

本批次继续保留全部既有内容，并把所有迁移来源中的练习与解答统一移动到真正章末。新增内容先记为 `copied`，只有正文、练习、答案、全书构建和 PDF 抽检全部通过后才改为 `verified`。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 4 章 | lint、时序约束、STA、CDC/RDC、形式性质与实现闭环 | verified |
| 第 8 章 | 流水有效位、可变时延、分支预测、逐拍控制 trace | verified |
| 第 23 章 | 传感器模拟前端、ADC、采样、I2C/SPI、FIFO/DMA 与时间戳 | verified |
| 第 24 章 | CPU/GPU 可见性、页面迁移、设备缺页、fence 与显示完成域 | verified |
| 第 26 章 | 教学机到现代整机映射、首次运行全链路 trace、证据关联与恢复 | verified |
| 全部 26 章 | 汇总迁移来源中的练习与解答，并放置于实际章末 | verified |

## 2026-07 现代处理器与存储前端深化批次

本批次不改变章节数量和既有单元归属。新增内容以“具体状态、逐拍或逐事件 trace、数值演算、并发边界、故障恢复、练习答案”为验收单位；新增文件进入生成图、章节流审计和全书构建后，方可标记为 `verified`。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 8 章 | 超标量前端、寄存器重命名、发射队列、ROB、LSQ、按序退休与误预测恢复 | verified |
| 第 9 章 | 乱序执行中的精确异常、加载/存储排序、重放、提交带宽与性能代价 | verified |
| 第 10 章 | 多核内存模型、store buffer、acquire/release、屏障与 DMA 可见性对照 | verified |
| 第 11 章 | AMAT、冲突 miss、非阻塞 Cache、MESI 暂态、目录一致性与 litmus trace | verified |
| 第 12 章 | 真实 PTE 位字段、页表内存成本、TLB reach、page-walk cache、COW 与 shootdown 竞争 | verified |
