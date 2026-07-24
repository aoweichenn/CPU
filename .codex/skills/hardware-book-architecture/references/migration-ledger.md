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

## 2026-07 互连、固件与平台可靠性深化批次

本批次沿“请求进入互连、设备完成事务、固件建立平台、错误被检测并恢复”的通路补齐量化例子。新增内容先记为 `copied`；生成、内容审计、全书构建、代表页目视检查和手机导出均通过后再改为 `verified`。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 17 章 | NoC 分包、虚通道、credit 账本、通道依赖死锁、QoS 与端到端时延 | verified |
| 第 18 章 | PCIe MPS/MRRS、完成拆分、credit 窗口、排序属性、ACS/PASID 与恢复边界 | verified |
| 第 21 章 | UEFI 阶段资源边界、DRAM 训练 trace、可信启动、ACPI/设备树核对与 A/B 更新 | verified |
| 第 22 章 | ECC 综合征、scrub 预算、错误遏制、阈值升级、共同故障域与恢复验收 | verified |

## 2026-07 结构归位与现代接口补全批次

本批次不删除任何既有正文、练习或答案。章节顺序调整以“主讲机制在前、历史与跨层案例在后”为原则；归属错误的练习迁回主讲章，超过每章核心题数量的原题按综合题子题完整保留。新增技术内容以官方架构或协议资料为核对入口。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 7、8、10、25 章 | 主线顺序、跨章归属、旧层级编号与过渡关系 | verified |
| 全部 26 章 | 核心练习数量收束，全部原题与答案作为独立题或综合题子题保留 | verified |
| 第 8 章 | SIMD/向量状态、lane、掩码、向量访存、精确异常、上下文与 SMT 共享边界 | verified |
| 第 9 章 | 瞬态执行、微架构侧信道、隔离措施、性能与威胁边界 | verified |
| 第 18 章 | CXL.io/cache/mem、设备内存、Chiplet 与 UCIe 封装内事务 | verified |
| 第 23 章 | Ethernet MAC--PHY 边界、PCS/PMA/PMD、协商、训练、FEC 与故障回溯 | verified |
| 全书后置资料 | 处理器、推测执行、PCIe/CXL/UCIe、NVMe、UEFI、Ethernet 与 USB 官方资料入口 | verified |

## 2026-07 硅实现、物理外设与专用通路补全批次

本批次补齐“RTL 如何成为芯片、数字事务如何穿过模拟与物理接口、专用硬件如何接回统一内存与完成语义”三类断点。所有内容继续进入现有二十六章，不新增章节容器，不删除既有正文与习题。每个图必须先明确要证明的机制，再按分层图、泳道图或状态链表达，并经过 PDF 页面目视检查。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 4 章 | ASIC 后端、签核、流片、封装与首次上电的完整实现链 | verified |
| 第 4 章 | FPGA 的 LUT、互连、BRAM、DSP、时钟资源、位流与配置启动 | verified |
| 第 4 章 | scan/ATPG、MBIST/LBIST、JTAG 边界扫描、晶圆测试、分档与熔丝修复 | verified |
| 第 14 章 | UFS/eMMC/SD 主机接口、新型非易失存储、光存储与磁带边界 | verified |
| 第 19 章 | TEE、内存加密、完整性保护、机密虚拟机与调试/密钥生命周期 | verified |
| 第 20 章 | 电池、充电、PMIC、USB-PD、电源路径选择与掉电边界 | verified |
| 第 23 章 | Wi-Fi/蓝牙的 MAC、基带、RF 前端、天线、关联与错误回溯 | verified |
| 第 23 章 | RDMA/RoCE、SmartNIC/DPU、PTP 硬件时间戳与数据中心完成语义 | verified |
| 第 23 章 | CAN/LIN 的仲裁、电气边界、错误计数、总线关闭与恢复 | verified |
| 第 24 章 | HDMI/DisplayPort/eDP、TCON、LCD/OLED 驱动与像素可见边界 | verified |
| 第 24 章 | CMOS 图像传感器、MIPI CSI、ISP、帧缓冲与曝光时间线 | verified |
| 第 24 章 | NPU 的脉动阵列、片上 SRAM、量化、DMA、命令队列与完成边界 | verified |

## 2026-07 薄弱章节机制深化批次

本批次根据全书内容密度与机制链完整性审计，扩充第 12、17、18、22 章。扩写只增加读者内容，不替换或删减既有材料；不增加章末练习数量。每个新增通路都要明确状态、请求、完成、失效与恢复边界，并在最终 PDF 中逐页检查图文间距。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 12 章 | 内存属性、A/D 位并发、大页拆合、NUMA 页面迁移与页框生命周期 | verified |
| 第 17 章 | 目录一致性、home node、探测完成、虚通道依赖与跨电源/复位域桥接 | verified |
| 第 18 章 | PCIe 交换拓扑、retimer、热插拔、DPC、SR-IOV、ATS/PRI/PASID 与失效闭环 | verified |
| 第 22 章 | EC/BMC 带外管理、传感器与主机隔离、FIT/MTBF/MTTR、可用性和共同故障预算 | verified |

## 2026-07 指令入口、设备搬运与固件所有权深化批次

本批次继续补齐三条跨层通路：机器码从取指边界进入合法译码并在陷阱后恢复，离散页框经 IOMMU 和 scatter-gather 描述符交给设备并在完成后回收，平台从复位 strap 与不可变启动根进入可更新固件并在操作系统接管后限制运行时固件权限。只增加读者内容与机制图，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 6 章 | 指令边界、取指窗口、合法译码、扩展门控、自修改代码同步、陷阱入口与返回状态 | verified |
| 第 19 章 | scatter-gather DMA、IOVA 映射、部分完成、interrupt remapping、posted interrupt 与向量生命周期 | verified |
| 第 21 章 | 复位 strap、不可变 Boot ROM、SPI Flash 区域保护、SMM/运行时固件所有权与恢复边界 | verified |

## 2026-07 算术提交、微码推进与 SATA 完成语义深化批次

本批次补齐三类“内部步骤很多、软件只看到一次完成”的事务：浮点执行单元从分类、对阶和尾数运算走到一次确定舍入与异常状态，微码序列从入口地址逐项推进并在可重启边界提交，AHCI/SATA 命令从主存描述符进入 NCQ 槽位并在数据、状态和错误恢复均闭合后释放所有权。只新增正文、机制图和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 5 章 | 浮点执行流水、分类与旁路、GRS 舍入、FMA 单次舍入、异常标志与可复现性边界 | verified |
| 第 7 章 | 微序列器、控制存储入口与下一地址、等待状态、可重启微操作、assist 与补丁验证边界 | verified |
| 第 15 章 | AHCI 命令表与 PRDT、NCQ tag 生命周期、FIS 数据/完成链、错误分层、端口复位与持久化边界 | verified |

## 2026-07 特权交接与块 I/O 可靠性深化批次

本批次针对“已有概括但零基础读者难以独立走通”的两处边界继续扩写：处理器从用户态陷入特权态时，明确保存哪些架构状态、由谁切栈、怎样返回；存储请求从页缓存进入块层时，明确怎样形成 BIO、合并为 request、取得硬件 tag 并沿完成路径回收。校验 RAID 进一步用逐字节异或演算解释小写、write hole、降级读与重建窗口。只新增正文、机制图、正文表格和正式资料入口，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 6 章 | RISC-V 特权状态、trap CSR、用户栈到内核栈、系统调用返回、release/acquire、FENCE 与原子读改写 | verified |
| 第 16 章 | 页缓存、文件块映射、BIO/request、blk-mq、scatter-gather DMA、tag 完成、RAID 校验小写、降级读与重建 | verified |

## 2026-07 队列、共享内存与以太网物理路径案例深化批次

本批次继续处理“术语已经出现，但零基础读者还难以从具体状态推导结果”的三处接口。第 14 章用小深度环形队列和跨页缓冲案例展开 NVMe 的队首、队尾、phase、CID、PRP 与复位代际；第 18 章用同一 Cache line 的所有权案例区分 PCIe DMA、CXL.cache 与 CXL.mem，并把 chiplet 请求拆到协议适配、链路可靠性和封装 PHY；第 23 章用最小帧和普通数据帧的线速预算连接 MAC、PCS、PMA、PMD、FCS 与接收队列。只增加正文、算例、机制图和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 14 章 | NVMe 环形 SQ/CQ、doorbell、phase/CID、PRP 跨页映射、超时与 generation 回收边界 | verified |
| 第 18 章 | PCIe DMA 与 CXL 一致性对照、home agent、Cache line 所有权、CXL.mem 可见边界、chiplet 协议栈与故障定位 | verified |
| 第 23 章 | Ethernet 帧上线预算、MAC/PCS/PMA/PMD 表示转换、FCS/FEC 边界、RX DMA 与分层错误证据 | verified |

## 2026-07 零基础完整事务案例深化批次

本批次把已经出现的术语组织成零基础读者可以逐状态手推的完整事务。新增内容只进入现有二十六章，不减少正文、图表、案例、习题或答案，也不增加章末练习数量。第 5 章闭合有符号整数除法的预处理、迭代、异常与提交；第 12 章从程序申请虚拟区走到首次触碰、TLB、Cache、COW 与回收；第 17 章逐拍展开 AXI 五通道和 AXI--APB 桥；第 19 章用四槽网卡接收环闭合 DMA、IOMMU、MSI-X 与缓冲回收；第 22 章用安全 P-state 切换闭合电压、频率、温度与故障回退。所有机制图按宽松分层、泳道或状态链绘制，并在最终 PDF 中逐页检查。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 5 章 | 有符号整数除法、符号归一化、商余数规则、除零、最小负数除以负一、可变时延与精确提交 | verified |
| 第 12 章 | `malloc` 虚拟区、首次触碰、需求零页、PTE/TLB/Cache、A/D 位、NUMA、COW、unmap 与页框回收 | verified |
| 第 17 章 | AXI 五通道逐拍写、双 ID 交错读、背压、`LAST`、响应归属、AXI--APB 桥接与错误闭合 | verified |
| 第 19 章 | 四槽网卡 RX 环、缓冲发布、IOMMU/DMA、完成所有权、MSI-X、屏蔽重查、代际与回收 | verified |
| 第 22 章 | P-state 安全升降序、VRM 斜率、时钟确认、热惯性、硬件保护、失败回退与完成确认 | verified |

## 2026-07 电气、跨时钟、DRAM 与显示可见性案例深化批次

本批次继续把跨边界知识组织成可逐状态核对的完整案例。第 1 章从 5V 板外信号进入 3.3V 输入脚，闭合阈值兼容、串联限流、钳位、掉电反灌与失效处理；第 3 章用四槽异步 FIFO 展开双口存储、二进制/格雷指针、保守空满、独立复位与重新对齐；第 13 章用一条 64B Cache line 读请求展开行冲突、PRE/ACT/RD、DQS 突发、ECC 与返回提交；第 24 章用 60Hz 双缓冲展开命令提交、渲染 fence、VBlank 翻页、逐行扫描、面板响应与错过截止时刻的整帧代价。只增加正文、机制图和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 1 章 | 5V 外部信号进入 3.3V 输入的阈值、限流、钳位、掉电反灌、保护失效与测量闭环 | verified |
| 第 3 章 | 四槽异步 FIFO、双口存储、格雷指针、保守空满、跨域可见性、独立复位与代际恢复 | verified |
| 第 13 章 | 64B DDR4 Cache line 读、行冲突、PRE/ACT/RD、BL8、DQS、ECC、返回排序与错误闭合 | verified |
| 第 24 章 | 60Hz 双缓冲、GPU fence、VBlank 翻页、扫描完成、面板响应、错帧代价与显示恢复 | verified |

## 2026-07 缓存写入、平台上电与音频流案例深化批次

本批次继续把已有概念收束为可以从零逐状态追踪的事务。第 11 章用一次命中脏牺牲行的 write-allocate 写缺失，闭合 store buffer、MSHR、写回缓冲、独占请求、填充合并与全局可见性；第 21 章用 ATX 类平台从待机电源、按键、主电源、VRM、时钟和复位走到复位向量首取指；第 23 章用 48 kHz、双声道、32-bit slot 的全双工音频流，闭合 codec、I²S/TDM、FIFO、DMA period、环形缓冲、时钟漂移与 xrun 恢复。只增加正文、机制图和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 11 章 | write-allocate 写缺失、脏牺牲行、RFO/GetM、填充合并、写入可见性与失败恢复 | verified |
| 第 21 章 | 待机电源、按键门控、PS\_ON\#、主电源与 VRM、时钟锁定、复位释放和首取指 | verified |
| 第 23 章 | 48 kHz 全双工音频、I²S/TDM、DMA period/ring、时钟漂移、xrun 与代际恢复 | verified |

## 2026-07 DRAM 写入、虚拟机切换、多核上线与主板制造案例深化批次

本批次补齐四条仍缺少完整状态链的关键通路：64B Cache line 怎样从写回队列穿过 DDR 写突发并落实到单元，处理器怎样在 guest 与 VMM 之间完成 VM Entry/Exit，启动核怎样逐个发布并验收其他核心，以及主板怎样从设计资料经过制造装联走到受控首轮上电。只增加正文、机制图和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 13 章 | 64B DDR4 Cache line 写回、行冲突、PRE/ACT/WR、DQS/BL8、ECC、tWR、可见性与恢复边界 | verified |
| 第 19 章 | VM Entry/Exit、两阶段地址转换、退出证据、模拟或重试、PC 推进、虚拟中断与失效恢复 | verified |
| 第 21 章 | 启动核、每核启动记录、唤醒机制、trampoline、每核状态、online 确认与隔离失败核 | verified |
| 第 21 章 | 原理图、PCB、制造资料、制板、装联、AOI/X-ray/ICT/JTAG 与受限流首轮上电 | verified |

## 2026-07 高级内存、虚拟化、NUMA 与产品化深化批次

本批次在既有章节内继续补齐四条面向复杂平台的闭环通路：DDR5 的双子通道写突发怎样分别跨越链路 ECC 与片内 ECC，并在温压漂移或错误计数越限后完成排空、重训练和恢复；L2 虚拟机怎样经过 L1 控制意图与 L0 物理所有权进入处理器，以及运行中的虚拟机怎样通过脏页追踪迁往另一台主机而不产生双主；多插槽平台怎样发布 NUMA 拓扑、上线远端处理器与内存，并按所有权的逆序安全下线 CPU；一块通过首轮上电的主板怎样继续经过 EVT、DVT、PVT、EMC、良率爬坡与失效分析进入受控量产。只增加正文、机制图、算例和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 13 章 | DDR5 32-bit 子通道 64B 写、BL16、链路 ECC 与 on-die ECC、运行期裕量监视、排空、重训练、RFM/ECS/PPR 与降级恢复 | verified |
| 第 19 章 | L0/L1/L2 嵌套虚拟化、控制结构合成、嵌套退出路由、两级第二阶段翻译、脏页追踪、预复制、停机交接与单一所有权 | verified |
| 第 21 章 | 多插槽拓扑发现、MADT/SRAT/SLIT/HMAT、远端内存上线、逐插槽核心发布、CPU offline、物理热移除与失败回滚 | verified |
| 第 21 章 | EVT/DVT/PVT 阶段门、EMC 预扫与认证、首过良率、测试覆盖、失效证据链、根因修正与量产版本闭环 | verified |

## 2026-07 现代存储、证明、老化与片上追踪深化批次

本批次补齐四条高级机制链：主机怎样通过 ZNS/FDP 参与 SSD 数据放置，NVMe 队列怎样跨 fabric 并在 ANA 多路径切换中保持单一所有权，TPM 度量怎样通过带新鲜挑战的 Quote 成为远程可验证证据，器件与封装怎样从长期应力走到现场故障，以及片上 trace 怎样受过滤、缓冲、时间同步和安全权限约束。只增加正文、机制图、算例和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 14 章 | ZNS zone 状态、写指针、Zone Append、Reset/恢复、FDP 的 RU/RG 与放置标识 | verified |
| 第 16 章 | NVMe-oF 的命令/数据/完成边界、RDMA/TCP 传输、ANA、多路径切换与持久化 | verified |
| 第 19 章 | TPM Quote、nonce、AK、事件日志重放、策略版本、秘密释放与升级恢复 | verified |
| 第 22 章 | EM、BTI、HCI、TDDB、热循环、加速寿命试验、降额与现场维护闭环 | verified |
| 第 26 章 | PMU、Intel PT/CoreSight、trace source/fabric/sink、缓冲溢出、时间同步与跨层定位 | verified |

## 2026-07 物理硬件安全与功能安全深化批次

本批次补齐逻辑安全边界之外的物理攻击面，以及可靠性之外的功能安全完成语义。第 19 章从熵源、根密钥和生命周期控制进入侧信道、故障注入与 DRAM 扰动；第 22 章从安全目标和故障容忍时间进入确定性执行、锁步比较、安全岛与受控降级；第 26 章用一次传感器卡死案例把采样、DMA、计算、独立监控、执行器门控和证据保存连接成可验收时间线。只新增正文、机制图、算例和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 19 章 | 熵源健康检测、条件化、根密钥派生、调试生命周期、侧信道、故障注入与 Rowhammer 防护链 | verified |
| 第 22 章 | 安全目标、FTTI 预算、确定性硬件、锁步、安全岛、诊断覆盖、降级运行与安全状态 | verified |
| 第 26 章 | 传感器卡死后的跨层检测、截止时间内执行器关闭、证据链与恢复验收 | verified |

## 2026-07 紧凑章节端到端案例深化批次

本批次针对内容覆盖已经完整、但案例密度与章节跨度仍不均衡的第 12、17、18、23 章补充可手算事务。第 12 章把一次 Sv39 store 从 TLB miss 走到页表项原子更新和架构提交；第 17 章把 Mesh NoC 的 flit、hop、credit、目标服务与返回包列入同一时延账；第 18 章把 ECAM 地址、BDF、BAR 分配、posted write 和 AER/DPC 证据连成设备生命周期；第 23 章把 IMU 采样、FIFO 水位、SPI/DMA、时间戳回绕、溢出与代际恢复连成数据链。只新增正文、机制图、算例和正文表格，不增加章末练习，也不改变既有迁移单元归属。

| 增补范围 | 内容单元 | 状态 |
|---|---|---|
| 第 12 章 | Sv39 精确地址拆分、三级 PTE 读取、权限检查、A/D 原子更新、TLB 填充、数据访问与 fault 重试 | verified |
| 第 17 章 | 4×3 Mesh 路由、flit 串行化、hop/credit/竞争/目标服务预算、响应归属与超时恢复 | verified |
| 第 18 章 | ECAM 配置地址、BDF 发现、BAR 掩码与资源分配、posted write 可见性、AER/DPC 隔离与重枚举 | verified |
| 第 23 章 | IMU 采样序号、FIFO 水位、SPI burst、DMA 环、时间戳回绕、溢出证据与 generation 恢复 | verified |
