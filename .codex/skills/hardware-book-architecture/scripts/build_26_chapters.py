#!/usr/bin/env python3
"""Build the reader-facing 26-chapter manuscript from intact legacy units."""

from __future__ import annotations

from dataclasses import dataclass
from difflib import SequenceMatcher
from pathlib import Path
import re
import sys


@dataclass(frozen=True)
class Chapter:
    number: int
    slug: str
    title: str
    opening: str
    units: tuple[tuple[str, tuple[int, ...]], ...]


@dataclass(frozen=True)
class PseudocodeSpec:
    """A stable pseudocode block selected by its first reader-visible line."""

    prefix: str
    category: str
    expected: int = 1


CHAPTERS = (
    Chapter(1, "levels-devices-digital-abstraction", "电平、器件与数字抽象", "数字硬件不是从符号 0 和 1 开始，而是从可测量的电压区间、可控制的电流路径和明确的失效边界开始。本章建立从物理节点到可靠 bit 的第一层抽象。", (("ch01-electricity-diode-switch.tex", (0, 1, 2)),)),
    Chapter(2, "gates-combinational-logic", "逻辑门与组合电路", "有了可靠电平，下一步是把受控路径组织成逻辑门，再把门组织成选择、译码和运算网络。本章始终同时追踪逻辑功能、传播延迟、负载和毛刺。", (("ch01-electricity-diode-switch.tex", (3, 4, 5)),)),
    Chapter(3, "state-clock-sequential-logic", "状态、时钟与时序逻辑", "组合逻辑只能计算当前输入，状态元件才能保存过去。本章用采样边界把锁存器、触发器、寄存器、跨时钟域和状态机连成一条连续通路。", (("ch02-feedback-latches-flipflops.tex", (0, 1, 2, 3, 4, 5)),)),
    Chapter(4, "rtl-verilog-verification", "RTL、Verilog 与验证", "RTL 的目标不是描述一串顺序动作，而是描述寄存器、组合网络及其时钟边界。本章训练把 Verilog 读回硬件，并区分仿真、综合和真实时序。", (("ch09-rtl-verilog-reading.tex", (0, 1, 2, 3, 4, 5)),)),
    Chapter(5, "numbers-arithmetic-circuits", "数值表示与算术电路", "硬件只搬运固定宽度 bit，数值含义来自解释规则。本章从补码和标志位进入加法、移位、比较、乘法与 ALU 的实现取舍。", (("ch03-number-arithmetic-circuits.tex", (0, 1, 2, 3)),)),
    Chapter(6, "isa-assembly-machine-code", "ISA、汇编与机器码", "ISA 是软件与处理器之间可长期保持的硬件契约。本章把指令编码、可见状态、条件码、调用约定和逐条执行 trace 对齐。", (("ch04-minimal-cpu.tex", (2, 3)),)),
    Chapter(7, "datapath-controller", "数据通路与控制器", "一条指令必须落实为源状态、组合路径、控制线和提交边界。本章把 ALU、寄存器、选择器、控制字和状态机接成可走读的数据通路。", (("ch03-number-arithmetic-circuits.tex", (4, 5)), ("ch04-minimal-cpu.tex", (0, 1)), ("ch-control-plane.tex", (2,)))),
    Chapter(8, "cpu-execution-pipeline", "CPU 执行：单周期、多周期与流水线", "单周期、多周期和流水线并不是三套互不相干的机器，而是对硬件复用、关键路径和吞吐的三种组织方式。本章从完整执行 trace 进入处理器实现的演进。", (("ch04-minimal-cpu.tex", (5,)), ("ch06-classic-chips-platforms.tex", (0, 1)))),
    Chapter(9, "hazards-exceptions-performance", "冒险、异常与性能", "并行执行只有在依赖、异常和提交边界都受控时才保持正确。本章把异常控制流与 CPI、吞吐、带宽和瓶颈分析放进同一套定量框架。", (("ch04-minimal-cpu.tex", (4,)), ("ch10-performance-analysis.tex", (0, 1, 2, 3, 4, 5)))),
    Chapter(10, "storage-hierarchy-overview", "存储层次与访问全景", "存储系统不是一排容量和速度数字，而是一条由不同粒度、协议和完成语义组成的访问路径。本章先建立从 load 到块 I/O 的全局坐标系。", (("ch05b-storage-cell-structures.tex", (0, 1)), ("ch05-memory-bus-io-dma.tex", (0, 1, 6)), ("ch05b-storage-cell-structures.tex", (8,)))),
    Chapter(11, "sram-register-file-cache", "SRAM、寄存器文件与 Cache", "离计算核心越近，存储越依赖低延迟、并行端口和局部性。本章从 SRAM 单元与阵列出发，建立寄存器文件和 Cache 的结构基础。", (("ch05b-storage-cell-structures.tex", (2,)),)),
    Chapter(12, "address-translation-tlb-vm", "地址转换、TLB 与虚拟内存", "处理器发出的虚拟地址必须经过权限、页表和 TLB 才能成为实际事务。本章用 80386 的历史边界进入现代地址转换路径。", (("ch06-classic-chips-platforms.tex", (2,)),)),
    Chapter(13, "dram-ddr-memory-controller", "DRAM、DDR 与内存控制器", "主存访问要穿过电容单元、感放、bank、命令调度、PHY 和训练。本章把器件结构、接口时序与控制器决策放在同一条 trace 上。", (("ch05b-storage-cell-structures.tex", (3, 4, 5)), ("ch-control-plane.tex", (3,)))),
    Chapter(14, "nand-ssd-nvme", "NAND Flash、SSD 与 NVMe", "SSD 用控制器和映射层把擦写受限的 NAND 包装成块设备。本章把单元物理、FTL、队列接口和完成通知连成一条路径。", (("ch05b-storage-cell-structures.tex", (6,)), ("ch11-peripherals-queues.tex", (0, 2)))),
    Chapter(15, "magnetic-disk-sata", "磁记录、硬盘与 SATA", "机械硬盘的访问时间由磁记录、寻道、旋转和控制器共同决定。本章把盘片物理、伺服定位、请求调度和块接口放在一起解释。", (("ch05b-storage-cell-structures.tex", (7,)), ("ch11-peripherals-queues.tex", (3,)))),
    Chapter(16, "storage-io-reliability-trace", "存储 I/O 栈、可靠性与端到端 Trace", "文件读取只有在文件层、块层、设备队列、DMA、介质和完成路径全部闭合后才真正结束。本章用端到端 trace 收束整个存储大部分。", (("ch05b-storage-cell-structures.tex", (9, 10)), ("ch11-peripherals-queues.tex", (1,)))),
    Chapter(17, "bus-address-decode-interconnect", "总线、地址译码与片上互连", "互连的核心不是一束导线，而是请求、目标选择、背压、响应和错误的事务契约。本章从地址地图进入总线与片上互连。", (("ch05-memory-bus-io-dma.tex", (2,)),)),
    Chapter(18, "pcie-high-speed-serial", "PCIe 与高速串行接口", "PCIe 把并行总线问题转换为分层数据包、链路训练和配置空间问题。本章沿枚举、BAR、事务和错误上报建立高速扩展接口的理解。", (("ch05-memory-bus-io-dma.tex", (5,)),)),
    Chapter(19, "mmio-interrupt-dma-iommu", "MMIO、中断、DMA、IOMMU 与设备队列", "CPU 和设备通过寄存器、描述符、地址转换和完成通知共享系统。本章明确每次设备事务的所有权、可见性和错误边界。", (("ch05-memory-bus-io-dma.tex", (3, 4)), ("ch-control-plane.tex", (4,)))),
    Chapter(20, "power-clock-signal-integrity", "电源、时钟与信号完整性", "逻辑图只说明理想功能，真实机器还要承受压降、抖动、反射、串扰和测量负载。本章把这些问题写成可预算、可测量的工程边界。", (("ch08-power-clock-signal-integrity.tex", (0, 1, 2, 3, 4, 5)),)),
    Chapter(21, "motherboard-firmware-boot", "主板、芯片组、固件与启动", "按下电源后，供电、时钟、复位、训练、枚举和固件必须按依赖顺序推进。本章沿启动时间线解释主板与平台分工。", (("ch06-classic-chips-platforms.tex", (3,)), ("ch-control-plane.tex", (5,)))),
    Chapter(22, "platform-control-feedback-reliability", "平台控制、反馈与可靠性", "整机由大量不同时间尺度的控制环维持稳定。本章把硬件状态机、固件策略、驱动调度、反馈控制和故障恢复统一成平台控制面。", (("ch-control-plane.tex", (0, 1, 6, 7, 8)),)),
    Chapter(23, "peripherals-network-human-interface", "外设、网络与人机接口", "介质和物理接口各不相同，但高吞吐外设普遍依赖队列、描述符、DMA 和完成通知。本章用网络与外设案例比较这些共同机制。", (("ch11-peripherals-queues.tex", (4, 5)),)),
    Chapter(24, "gpu-vram-display", "GPU、显存与显示系统", "GPU 用大量并行执行资源换取吞吐，并以命令队列、显存和 fence 接入整机。本章从执行模型一直走到显示扫描边界。", (("ch12-gpu-display.tex", (0, 1, 2, 3, 4, 5)),)),
    Chapter(25, "linking-loading-system-boundary", "链接、装载与系统软件边界", "硬件执行的是确定地址上的机器码，软件工具链负责把名字、段和重定位变成这种状态。本章把目标文件、固件、装载器和异常入口接回硬件。", (("ch06-classic-chips-platforms.tex", (4,)), ("ch13-linking-loading.tex", (0, 1, 2, 3, 4, 5)))),
    Chapter(26, "power-on-program-trace", "从上电到程序运行：搭机、全链路 Trace 与故障定位", "最后一章把全书重新连成一台可观察的机器：从供电复位到取指执行，从主存和设备访问到输出与排错。目标不是只让程序运行，而是能解释每一个边界。", (("ch07-breadboard-computer.tex", (0, 1, 2, 3, 4, 5)),)),
)


SECTION_RE = re.compile(r"(?m)^\\section\{")
NUMBERED_SECTION_RE = re.compile(r"^(\\section\{)(?:〇|[一二三四五六七八九十]+)、")
CHINESE_NUMERALS = ("一", "二", "三", "四", "五", "六", "七", "八", "九", "十", "十一", "十二")
CHINESE_TO_ARABIC = {number: str(index) for index, number in enumerate(CHINESE_NUMERALS, start=1)}
EXTRAS = {
    1: ("ch01-chapter-exercises.tex",),
    5: ("ch05-chapter-exercises.tex",),
    6: ("ch06-isa-real-world.tex",),
    8: ("ch08-pipeline-core.tex",),
    9: ("ch09-pipeline-hazards.tex",),
    11: ("ch11-cache-interface.tex",),
    12: ("ch12-modern-vm-tlb.tex",),
    13: ("ch13-chapter-exercises.tex",),
    14: ("ch14-nvme-interface.tex",),
    15: ("ch15-chapter-exercises.tex",),
    16: ("ch16-persistence-reliability.tex",),
    17: ("ch17-handshake-axi.tex",),
    18: ("ch18-pcie-transactions.tex",),
    19: ("ch19-device-queues.tex",),
    21: ("ch21-platform-recovery.tex",),
    22: ("ch22-ras-reliability.tex",),
    23: ("ch23-usb-human-interface.tex",),
}
DIAGRAM_OVERRIDES = {
    6: (("这台最小 ISA 的完整数据通路", "ch06-minimal-isa-datapath.tex"),),
    13: (("内存控制器内部流水线", "ch13-memory-controller-pipeline.tex"),),
    14: (
        ("NVMe 命令处理流程", "ch14-nvme-command-flow.tex"),
        ("磨损均衡前后的擦除次数分布", "ch14-wear-leveling-balance.tex"),
    ),
    15: (
        ("HDD 整机框图", "ch15-hdd-signal-path.tex"),
        ("ZBR 区位布局", "ch15-zbr-zone-layout.tex"),
    ),
    16: (("从 read() 到寄存器的完整路径", "ch16-read-end-to-end.tex"),),
    21: (("台式机主板布局与三大分配网络", "ch21-motherboard-distribution.tex"),),
    26: (("J 的条件选通逻辑", "ch26-conditional-jump-select.tex"),),
}
LEGACY_CHAPTER_REFERENCES = (
    ("第十三章", "存储与外设队列案例"),
    ("第十二章", "“冒险、异常与性能”一章"),
    ("第九章", "“电源、时钟与信号完整性”一章"),
    ("第八章", "本书末章的教学计算机项目"),
    ("第七章", "经典芯片与平台案例"),
    ("第五章", "存储与设备事务相关章节"),
    ("第四章", "处理器核心相关章节"),
    ("第三章", "算术与数据通路相关章节"),
    ("第二章", "“状态、时钟与时序逻辑”一章"),
    ("第一章", "“数字硬件基础”部分"),
)
LEGACY_TOPIC_REFERENCES = (
    ("经典芯片与平台演进章", "经典芯片与平台演进案例"),
    ("经典芯片与平台章", "经典芯片与平台案例"),
    ("存储器件结构章", "存储器件相关章节"),
    ("主存与总线章", "存储与互连相关章节"),
    ("外设队列章", "“外设、网络与人机接口”一章"),
    ("电源时钟章", "“电源、时钟与信号完整性”一章"),
)
REFERENCE_CLEANUPS = (
    ("现象上一节", "该现象前文"),
    ("经典芯片与平台案例经典芯片", "经典芯片与平台案例"),
    ("存储与外设队列案例的 NVMe", "存储与外设队列案例中的 NVMe"),
    ("存储与外设队列案例 NVMe", "存储与外设队列案例中的 NVMe"),
    ("列mux", "列 mux"),
    ("R高", "R 高"),
    ("本书末章的教学计算机项目面包板教学机", "教学计算机项目中的面包板机"),
    ("本书末章的教学计算机项目教学机", "教学计算机项目"),
    ("本书末章的教学计算机项目的面包板计算机", "教学计算机项目中的面包板计算机"),
    ("本书末章的教学计算机项目的教学机", "教学计算机项目中的教学机"),
    ("本书末章的教学计算机项目这台机器", "教学计算机项目中的这台机器"),
)
TARGET_REPLACEMENTS = {
    2: (("独占整章", "独占一个完整部分"),),
    9: (("独占整章", "独占一个完整部分"),),
    13: (("Fine-granularity (REFfg)", r"Fine-\allowbreak granularity (REFfg)"),),
    20: (("前面七章", "前面的数字与处理器章节"),),
    25: (("前面十四章", "前面二十四章"),),
    26: (("前面七章", "前面二十五章"),),
}

# Only algorithmic descriptions belong here.  Real Verilog/C/assembly examples,
# equations, terminal transcripts and data layouts keep their native comments.
# The expected counts make a moved, deleted or accidentally duplicated block fail
# the build instead of silently losing its teaching annotation.
PSEUDOCODE_SPECS: dict[int, tuple[PseudocodeSpec, ...]] = {
    1: (PseudocodeSpec("if A can drive through a diode:", "decision"),),
    2: (
        PseudocodeSpec("if sel = 0, Y = A", "decision"),
        PseudocodeSpec("if A = 0: Y = B", "decision"),
    ),
    3: (
        PseudocodeSpec("on clock edge:", "sequential", 3),
        PseudocodeSpec("next = current + 1", "sequential"),
        PseudocodeSpec("f = Q3 XOR Q2", "sequential"),
        PseudocodeSpec("case (S):", "state"),
        PseudocodeSpec("for each bit:", "transaction"),
        PseudocodeSpec("button pipeline:", "procedure"),
    ),
    5: (PseudocodeSpec("if op = ADD:", "decision"),),
    6: (
        PseudocodeSpec("CALL target, stack grows downward:", "trace"),
        PseudocodeSpec("MOV R3, [R1 + 8]", "trace"),
        PseudocodeSpec("MOV [R1 + 8], R3", "trace"),
    ),
    7: (
        PseudocodeSpec("state EXEC_ADD:", "state"),
        PseudocodeSpec("; 微程序示意：ADD 与条件跳转 BEQ 各对应一段微指令", "microcode"),
        PseudocodeSpec("S0: read_sel_a=R1, read_sel_b=R2", "microcode", 2),
        PseudocodeSpec("addr = base(a) + i * 4", "trace"),
        PseudocodeSpec("status = MMIO[UART_STAT]", "transaction"),
        PseudocodeSpec("if length > buffer_capacity:", "decision"),
        PseudocodeSpec("product = 0", "algorithm"),
        PseudocodeSpec("microinstruction sketch:", "microcode"),
        PseudocodeSpec("ADD Rd, Rs:", "microcode"),
    ),
    8: (
        PseudocodeSpec("branch predicted not taken:", "trace"),
        PseudocodeSpec("8086 memory read sketch:", "transaction"),
        PseudocodeSpec("8086 memory write sketch:", "transaction"),
        PseudocodeSpec("IF:  PC -> instruction memory", "trace"),
    ),
    9: (
        PseudocodeSpec("faulting load:", "trace"),
        PseudocodeSpec("朴素版(i-j-k 顺序, float):", "algorithm"),
    ),
    10: (
        PseudocodeSpec("SRAM read:", "transaction"),
        PseudocodeSpec("单 bank，两次访问落在同一 bank 的不同行:", "trace"),
        PseudocodeSpec("prepare DMA descriptor:", "transaction"),
        PseudocodeSpec("receive ring:", "transaction"),
        PseudocodeSpec("receive path:", "transaction"),
    ),
    11: (
        PseudocodeSpec("READ OPERATION (step by step):", "transaction"),
        PseudocodeSpec("WRITE OPERATION (writing 0 to cell", "transaction"),
        PseudocodeSpec("ECC SEC-DED for 64-bit data word:", "algorithm"),
    ),
    12: (
        PseudocodeSpec("effective address -> segmentation", "trace"),
        PseudocodeSpec("80386-style address walk:", "transaction"),
    ),
    13: (
        PseudocodeSpec("// 1. Precharge: BL = BLbar", "transaction"),
        PseudocodeSpec("Row buffer HIT  (same row, different column):", "trace"),
        PseudocodeSpec("for each DQ bit i:", "training", 2),
        PseudocodeSpec("for vref = VREF_MIN", "training"),
        PseudocodeSpec("每周期调度决策:", "state"),
        PseudocodeSpec("读写批处理算法:", "algorithm"),
        PseudocodeSpec("ECC 读路径:", "transaction"),
    ),
    14: (
        PseudocodeSpec("Erase operation (block erase):", "procedure"),
        PseudocodeSpec("Read operation (one page):", "transaction"),
        PseudocodeSpec("ISPP algorithm (per page):", "algorithm"),
        PseudocodeSpec("Read disturb mechanism:", "recovery"),
        PseudocodeSpec("Bad block management:", "recovery"),
        PseudocodeSpec("Garbage Collection (greedy algorithm):", "algorithm"),
        PseudocodeSpec("Dynamic wear leveling:", "algorithm"),
        PseudocodeSpec("Power-loss protection mechanism:", "recovery"),
        PseudocodeSpec("NVMe Write command flow:", "transaction"),
        PseudocodeSpec("Read retry sequence:", "recovery"),
        PseudocodeSpec("Soft-bit read for LDPC:", "training"),
        PseudocodeSpec("LDPC iterative decoding (simplified):", "algorithm"),
        PseudocodeSpec("host memory: write SQ entry", "transaction"),
    ),
    15: (
        PseudocodeSpec("TFC control loop:", "control_loop"),
        PseudocodeSpec("seek state machine:", "state"),
        PseudocodeSpec("track-following loop", "control_loop"),
        PseudocodeSpec("NCQ optimization example:", "algorithm"),
    ),
    18: (PseudocodeSpec("handshake rule:", "transaction"),),
    19: (
        PseudocodeSpec("device command sequence:", "transaction"),
        PseudocodeSpec("I2C register read transaction:", "transaction"),
        PseudocodeSpec("SPI mode-0 byte read", "transaction"),
        PseudocodeSpec("DMA receive sketch:", "transaction"),
        PseudocodeSpec("scan_bus(bus):", "algorithm"),
    ),
    20: (
        PseudocodeSpec("纹波测量流程", "procedure"),
        PseudocodeSpec("偶发故障排查顺序", "diagnostic"),
        PseudocodeSpec("面包板阶段就要养成的测量与设计习惯", "diagnostic"),
    ),
    21: (PseudocodeSpec("minimal whole-machine program:", "trace"),),
    23: (PseudocodeSpec("1. 驱动把 64 B 命令项写入本核 SQ", "transaction"),),
    24: (PseudocodeSpec("驱动侧提交一批命令的序列：", "transaction"),),
    25: (
        PseudocodeSpec("reset_handler:", "procedure"),
        PseudocodeSpec("reset trace:", "trace"),
        PseudocodeSpec("protected-mode checklist:", "procedure"),
        PseudocodeSpec("M2 counter loop:", "algorithm"),
        PseudocodeSpec("M3 trace example:", "trace"),
        PseudocodeSpec("timer interrupt arrives while A runs", "trace"),
    ),
    26: (
        PseudocodeSpec("STA 的 T3 一拍之内", "trace"),
        PseudocodeSpec("1. 停机：", "procedure"),
        PseudocodeSpec("T0  CO+MI", "microcode"),
        PseudocodeSpec("W = [0x0000] * 128", "microcode"),
        PseudocodeSpec("while 行为与预期不符:", "diagnostic"),
    ),
}

PSEUDOCODE_COMMENTS: dict[str, tuple[str, str]] = {
    "decision": (
        "先判断条件，再只执行命中的分支；分支顺序同时表达优先级。",
        "所有输入稳定后才读取结果；未覆盖的条件必须有明确默认行为。",
    ),
    "sequential": (
        "右侧先由旧状态计算，所有状态只在有效时钟沿同时更新。",
        "条件未命中或处于两个时钟沿之间时，寄存器保持原值。",
    ),
    "state": (
        "当前状态与输入共同选择动作和下一状态，先确认每个分支的优先级。",
        "本拍只计算 next，状态在时钟边界更新；等待和错误都要有去向。",
    ),
    "transaction": (
        "逐行追踪发起者、所有权和可见性；请求被接受不等于事务完成。",
        "以采样沿、状态位或 completion 为准，再允许消费者使用结果。",
    ),
    "trace": (
        "按行追踪数据来源、经过的部件和最终写入目标。",
        "只有标出的时钟沿、阶段或异常入口会改变体系结构可见状态。",
    ),
    "microcode": (
        "每个 u/T 行代表一个控制节拍，同一行内的控制信号并行生效。",
        "next 决定下一微地址；等待或错误时不得提前进入写回。",
    ),
    "algorithm": (
        "每轮只推进一个元素或一个控制步，并保持中间状态的一致性。",
        "退出条件成立后才提交结果；空集合、越界和失败要单独处理。",
    ),
    "training": (
        "逐点扫描延迟或阈值并记录连续通过窗口，不能只看单个通过点。",
        "训练结束取稳定窗口中心；没有有效窗口时进入明确失败路径。",
    ),
    "control_loop": (
        "每个采样周期先测量、再计算误差，最后更新执行器。",
        "输出必须限幅；测量无效或误差失控时转入安全状态。",
    ),
    "recovery": (
        "先检测失效并保存仍可信的数据，再切换到备用位置或重试路径。",
        "更新映射或元数据后必须校验；掉电重启应能判断最后有效版本。",
    ),
    "procedure": (
        "按步骤依次执行，前一步的稳定结果是后一步的前提。",
        "每一步都保留可观察读回；不满足预期就停在当前层排错。",
    ),
    "diagnostic": (
        "先固定现象和测量点，再做一次单变量改动。",
        "改动后回到上一边界复测，确认故障没有迁移到下一级。",
    ),
}
PSEUDOCODE_ANNOTATION_RE = re.compile(r"^(?://|;|#) 注释（[^）]+）：")
LSTLISTING_RE = re.compile(
    r"(\\begin\{lstlisting\}(?:\[[^\n]*\])?\n)(.*?)(\\end\{lstlisting\})",
    re.DOTALL,
)


def find_root(start: Path) -> Path:
    marker = Path("books/hardware-zero-to-machine/source/latex/main.tex")
    for candidate in (start, *start.parents):
        if (candidate / marker).is_file():
            return candidate
    raise FileNotFoundError(f"cannot find repository root from {start}")


def split_units(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    match = re.search(r"(?m)^\\chapter\{[^\n]*\}\s*\n?", text)
    if not match:
        raise ValueError(f"missing chapter heading: {path}")
    body = text[match.end():]
    starts = [match.start() for match in SECTION_RE.finditer(body)]
    if not starts:
        return [body]
    return [body[: starts[0]], *[body[start:end] for start, end in zip(starts, starts[1:] + [len(body)])]]


def section_topics(path: Path) -> tuple[dict[str, str], tuple[str | None, ...]]:
    """Return old section-number topics and the topic owned by each split unit."""
    text = path.read_text(encoding="utf-8")
    headings = re.findall(r"(?m)^\\section\{([^}]+)\}", text)
    by_number: dict[str, str] = {}
    ordered: list[str | None] = [None]
    for heading in headings:
        match = re.match(r"(〇|[一二三四五六七八九十]+)、(.+)", heading)
        if not match:
            raise ValueError(f"section heading lacks a Chinese number: {path}: {heading}")
        number, topic = match.groups()
        by_number[number] = topic
        ordered.append(topic)
    return by_number, tuple(ordered)


def named_section_reference(topic: str) -> str:
    return f"“{topic}”一节"


def rewrite_legacy_section_references(
    text: str,
    source_name: str,
    unit_index: int,
    topics_by_number: dict[str, dict[str, str]],
    topics_by_unit: dict[str, tuple[str | None, ...]],
) -> str:
    """Turn pre-migration numeric and relative references into stable topic names."""
    number_map = topics_by_number[source_name]
    alternatives = "|".join(sorted(map(re.escape, number_map), key=len, reverse=True))

    def replace_section_symbol(match: re.Match[str]) -> str:
        return named_section_reference(number_map[match.group(1)])

    text = re.sub(rf"[ \t]*§[ \t]*({alternatives})", replace_section_symbol, text)
    text = re.sub(
        rf"本章第[ \t]*({alternatives})[ \t]*节",
        replace_section_symbol,
        text,
    )

    arabic = {
        CHINESE_TO_ARABIC[number]: number
        for number in number_map
        if number in CHINESE_TO_ARABIC
    }

    def replace_arabic_section(match: re.Match[str]) -> str:
        number = arabic.get(match.group(1))
        return named_section_reference(number_map[number]) if number else match.group(0)

    text = re.sub(
        r"本章第[ \t]*([0-9]+)[ \t]*节", replace_arabic_section, text
    )

    ordered = topics_by_unit[source_name]
    if unit_index > 0 and ordered[unit_index - 1]:
        text = text.replace("上一节", named_section_reference(ordered[unit_index - 1]))
    if unit_index + 1 < len(ordered) and ordered[unit_index + 1]:
        text = text.replace("下一节", named_section_reference(ordered[unit_index + 1]))
    text = re.sub(
        r"一节[ \t]+(?=[\u4e00-\u9fff，。；：、！？）])", "一节", text
    )
    return text


INLINE_LITERAL_RE = re.compile(
    r"(\\(?:code|texttt|url|path)\{[^{}]*\})"
)
PROSE_QUOTE_RE = re.compile(r'"([^"]*)"')

# Reader-facing phrases that were vivid in draft form but are too colloquial for a
# technical textbook.  Keep these exact and context-sensitive: the replacement
# names the physical constraint, consumed resource, or visible failure instead of
# applying a generic synonym to every occurrence of a word.
EDITORIAL_PHRASE_REPLACEMENTS = (
    ("分支分歧吃掉吞吐；缺第三条，同步吃掉一切", "分支分歧降低吞吐；缺第三条，同步开销会主导总体性能"),
    ("砍掉的道失去的是总吞吐", "关闭的通道降低的是总吞吐"),
    ("喂给片元着色器", "传递给片元着色器"),
    ("第二个决定是被 16 字节的预算逼出来的", "第二个决定来自 16 字节预算的约束"),
    ("盯住有效控制线", "观察有效控制线"),
    ("把剩下的可能砍掉一半", "将剩余可能性排除一半"),
    ("比按兴趣硬扛更能走到终点", "比仅凭兴趣勉强坚持更有助于完成学习路径"),
    ("不被普通 cache 吞掉", "不被普通 cache 延迟或合并"),
    ("库被白白扫过", "库扫描早于未解析符号出现"),
    ("内核内部的 \\code{-14} 被包装层吃掉", "内核内部的 \\code{-14} 由包装层转换，不直接暴露给程序"),
    ("偏斜吃掉时序预算", "偏斜占用时序预算"),
    ("偏斜吃掉预算", "偏斜占用预算"),
    ("被偏斜和抖动合计吃掉 1.7ns", "其中 1.7ns 被偏斜和抖动占用"),
    ("容限约 1.2V，被吃掉大半", "约 1.2V 的容限已消耗大半"),
    ("正是被这道题逼出来的形状", "正是这一约束所要求的布局"),
    ("还能联合作战", "还可联合使用"),
    ("条件命中时吐出一个脉冲", "条件命中时输出一个脉冲"),
    ("偶发故障的最后一层隐身术就此解除", "由此可以进一步定位偶发故障"),
    ("为什么人人谈眼图", "为什么眼图是千兆位链路的核心观测方法"),
    ("抖动和噪声各吃掉一大块之后，眼睛只剩一条缝", "抖动和噪声分别占用部分裕量后，有效眼开度显著缩小"),
    ("建立时间被吃掉了", "建立时间裕量不足"),
    ("时钟偏斜吃掉建立时间裕量", "时钟偏斜压缩建立时间裕量"),
    ("右规把一位挤进 guard", "右规把一位移入 guard 位"),
    ("全部塞进一个周期", "全部安排在一个周期内"),
    ("边界值才会逼出", "边界值才能暴露"),
    ("被空块耗尽逼出来的同步 GC", "空块耗尽时触发的同步 GC"),
    ("被空块耗尽逼出来的", "由空块耗尽触发的"),
    ("缓存打穿后的“缓外速度”", "缓存空间耗尽后的介质持续写入速度"),
    ("写放大吃掉的是寿命预算", "写放大会消耗寿命预算"),
    ("剩下的块继续挨打", "其余块继续承担擦写负载"),
    ("热块先被擦穿", "热块先达到擦除寿命上限"),
    ("先撞到 P/E 预算", "先达到 P/E 次数上限"),
    ("少数块先擦穿", "少数块先达到寿命上限"),
    ("搬进擦除次数多的块里“养老”", "搬入擦除次数较多的块中长期存放"),
    ("里“养老”", "中长期存放"),
    ("寿命的公平是花钱买的", "寿命均衡以额外写入为代价"),
    ("当宝贝搬走", "作为有效数据搬移"),
    ("写放大白白上涨", "写放大无谓增加"),
    ("白白丢弃", "不必要地丢弃"),
    ("吞掉有效脉冲", "滤除有效脉冲"),
    ("吞掉有效边沿", "滤除有效边沿"),
    ("会吃掉电压余量", "会消耗电压余量"),
    ("余量都可能被吃掉一部分", "余量都可能减少一部分"),
    ("后一个沿会被吞掉", "后一个边沿可能无法被识别"),
    ("被格式位宽“吃掉”了", "因格式位宽限制而无法保留"),
    ("被静默吞掉", "被静默丢失"),
    ("结果看起来像“小数被吃掉”", "结果看起来像“小数没有参与结果”"),
    ("加上的 1 却被格式位宽吃掉", "加上的 1 却因格式位宽限制而无法保留"),
    ("为什么大数加小数容易“吃掉”小数", "为什么大数与小数相加时，小数容易在结果中消失"),
    ("那一次“白做的减法”", "那一次需要撤销的减法"),
    ("白白耗电", "产生不必要的静态功耗"),
    ("二极管每级吃掉 0.7V", "二极管每级引入 0.7V 压降"),
    ("等于白白多串了一级门", "等效于额外串联一级门"),
    ("几十毫瓦白白发热", "产生几十毫瓦不必要的发热"),
    ("直接给踩坑演算", "给出一个容易出错的示例"),
    ("同时喂给两者", "同时输入两者"),
    ("若 MMIO 被写回 cache 吞掉", "若 MMIO 写入仅停留在写回 cache 中"),
    ("把它塞进每个取指周期", "把它安排在每个取指周期"),
    ("足以吞掉大半个数据窗口", "足以占用大半个数据窗口"),
    ("直接砍掉明细里最贵", "直接消除明细中代价最高"),
    ("撞到的物理瓶颈", "达到的物理瓶颈"),
    ("被白白浪费", "无法得到有效利用"),
    ("把大部分收益吃掉", "抵消大部分收益"),
    ("打穿缓存之后", "写入超过缓存承载能力之后"),
    ("缓存打穿后", "缓存空间耗尽后"),
    ("吞掉一部分理论吞吐", "使理论吞吐降低"),
    ("存储系统吃掉 48\\%，分支吃掉", "存储系统占 48\\%，分支占"),
    ("吃掉一半 IPC", "使 IPC 降低约一半"),
    ("吃掉的周期", "占用的周期"),
    ("继续死磕同一个部件的", "继续提高同一个部件的"),
    ("都撞在表的前几行", "都受表中前几行所示占比限制"),
    ("遮羞布盖不住的负载一直存在", "缓存无法隐藏所有负载的存储时延"),
    ("这套机制是遮羞布，不是解药——它把墙的代价藏起来，藏的条件是程序有局部性。", "这套机制只能在程序具有局部性时缓解内存墙的影响，无法消除处理器与存储器之间的速度差距。"),
    ("墙重新露头", "内存墙效应重新显现"),
    ("遮羞布更厚；无局部性的负载照旧撞墙", "可延后内存墙出现；无局部性的负载仍受内存墙限制"),
    ("满打满算约一万次", "理论上最多约一万次"),
    ("直接吃掉预算的十分之一", "直接占用预算的十分之一"),
    ("吃掉预算的十分之一", "占用预算的十分之一"),
    ("翻车现场", "错误案例"),
    ("远端请求可能饿死", "远端请求可能长期得不到服务"),
    ("防止饿死", "防止请求长期得不到服务"),
    ("饿死被时间上限封死", "时间上限避免了请求长期得不到服务"),
    ("（饿死）", "（饥饿，starvation）"),
    ("差（可饿死）", "差（可发生饥饿）"),
    ("反转与饿死", "反转与饥饿"),
    ("从另一头榨容量", "从磁道布局方向提高容量"),
    ("靠文件系统日志兜底", "由文件系统日志保证一致性"),
    ("电容兜底", "由电容提供掉电保护"),
    ("条曲线兜底", "条曲线提供保持力保障"),
    ("须自己兜底", "须自行处理恢复"),
    ("反馈收拾其余一切", "反馈处理其余扰动"),
    ("受摔受损", "因跌落受损"),
    ("被仪器扔掉", "无法被仪器保留"),
    ("拍一拍板子行为就变", "轻触板子时行为改变"),
    ("记忆会骗人，日志不会", "人的记忆可能产生偏差，日志则能保留测量记录"),
    ("真正干活的执行单元", "真正执行运算的单元"),
    ("线程在干活", "线程在执行"),
    ("按命令干活", "按命令执行"),
    ("CPU 发单、设备干活", "CPU 提交命令、设备执行"),
    ("一条 lane 干活", "一条 lane 参与执行"),
    ("替谁干活", "代表谁执行"),
    ("内核替你干活", "内核代为执行"),
    ("锁得太多会拖垮整机的内存子系统", "锁页过多会严重占用整机内存资源"),
    ("哪怕整个卡死", "即使完全停止响应"),
    ("读出的就是垃圾", "读出的内容没有定义"),
    ("容器隔离的抓手", "容器隔离的实施点"),
    ("最先摔倒的地方", "最容易产生误解的地方"),
    ("跑一遍掐个表", "运行一次并记录墙钟时间"),
    ("偶尔被狠狠干扰一次", "偶尔受到一次显著干扰"),
    ("结论先打回重测", "结论应先重新测量"),
    ("最狠", "最显著"),
    ("攒批攒得越狠越好", "批量越大越好"),
    ("把帧预算啃掉一块", "占用一部分帧预算"),
    ("都扛不住", "也无法满足吞吐要求"),
    ("有效载荷就被咬一口", "有效载荷都会相应减少"),
    ("代码自己在被啃", "程序代码正在被意外改写"),
    ("从六十四个嫌疑里揪出一个", "从六十四个候选点中定位一个"),
    ("把眼高被串扰与电源噪声啃", "眼高受串扰与电源噪声压缩"),
    ("眼高被串扰与电源噪声啃", "眼高受串扰与电源噪声压缩"),
    ("眼宽被抖动与偏斜啃", "眼宽受抖动与偏斜压缩"),
    ("真正打满", "充分利用"),
    ("三笔演算把差距钉死", "三项计算可以量化这一差距"),
    ("总线保持器兜底", "由总线保持器维持电平"),
    ("不能交给器件去兜底", "不能依赖器件代为保证"),
    ("没人驱动时谁来兜底", "无人驱动时如何维持确定电平"),
    ("把“兜底”直接做进了电气结构里", "将默认电平直接落实在电气结构中"),
    ("补 \\code{else} 兜底", "补 \\code{else} 覆盖默认路径"),
    ("把兜底散在各分支末尾", "把默认赋值散在各分支末尾"),
    ("// 兜底：无锁存器", "// 默认路径：避免推断锁存器"),
    ("没有类型系统替你兜底", "没有类型系统代为保证正确性"),
    ("指望换页兜底", "依赖换页机制补偿容量不足"),
    ("没有硬件自动兜底", "没有硬件自动管理机制"),
    ("做错时没有兜底", "安排不当时也没有自动补偿机制"),
    ("做错时没有硬件自动兜底", "安排不当时也没有硬件自动补偿机制"),
    ("内核替它干活", "内核代为执行"),
    ("按钟干活", "按显示时钟运行"),
    ("饿死的典型情形", "饥饿的典型情形"),
    ("饿死被时", "请求长期得不到服务的情况被时"),
    ("一眼见底", "清晰可见"),
    ("还有一招 SLC 缓存", "还可使用 SLC 缓存"),
    ("同一招", "同一方法"),
    ("这一招", "这一方法"),
    ("一招：", "一种方法是："),
    ("后台 GC 主机无感", "后台 GC 通常不影响主机请求时延"),
    ("都攒一点无效页，又都剩一堆有效页", "都会积累少量无效页，同时保留大量有效页"),
    ("两者做足", "两者均充分实施"),
    ("分层做足", "按层级充分配置"),
    ("直接撞墙", "直接受到内存墙限制"),
    ("寄存器溢写还债", "寄存器溢写增加访存开销"),
    ("用 load/store 溢写还债", "因 load/store 溢写而增加访存开销"),
    ("容器隔离的抓手", "容器隔离的实施机制"),
    ("随手一关了事", "在未分析时间上界的情况下关闭中断"),
    ("随手把 RAM 写花", "意外改写 RAM"),
    ("随手可碰", "均可直接访问"),
    ("线程廉价到可以按数据量随手开", "线程创建成本足够低，可按数据规模配置"),
    ("工程推论随手可得", "由此可以直接得到工程推论"),
    ("不是随手排布", "不是任意排布"),
    ("随手跳过", "直接忽略"),
    ("例子随手就能造", "很容易构造相应示例"),
    ("随手\n", "直接\n"),
    ("不是一堆孤立门的随意堆叠", "不是孤立逻辑门的任意堆叠"),
    ("一堆专用信号线", "大量专用信号线"),
    ("一颗 CPU 加一堆外设", "一颗 CPU 加若干外设"),
    ("一堆“噪声”", "一组无规律的“噪声”"),
    ("一堆位号", "大量缺乏语义的位号"),
    ("按上式 WA 逼近 10", "按上式计算可得 WA 接近 10"),
    ("少数块先被擦穿", "少数块先达到擦除寿命上限"),
    ("寿命的公平本身是花钱买的", "均衡寿命分布本身需要额外写入"),
    ("工程上常用由总线保持器维持电平", "工程上常用总线保持器维持电平"),
    ("时延差拉大，cache 就从一级加到三级，容量越给越大；带宽差拉大，预取器、乱序窗口、MSHR 就越堆越多。", "随着时延差扩大，cache 从一级扩展到多级且容量不断增加；随着带宽差扩大，预取器、乱序窗口和 MSHR 的规模也相应增加。"),
    ("视频处理把帧数据过一遍就走，AI 推理把权重流一遍就走，大数据扫描把表读一遍就走", "视频处理仅顺序访问一次帧数据，AI 推理仅顺序读取一次权重，大数据扫描也仅顺序读取一次数据表"),
    ("猜错的预取", "错误预取"),
    ("已经算过这笔得失", "已经量化过这一权衡"),
    ("按局部性把负载分类，墙的表现一目了然", "按局部性对负载分类后，可以明确区分内存墙对不同负载的影响"),
    ("不是洁癖，是数量级不允许", "这并非风格偏好，而是由访问时延的数量级差异决定的"),
    ("看 $v$ 的下场就知道", "可以从有效页比例 $v$ 看出"),
    ("把失效撒到所有块上", "使失效页分散到所有块"),
    ("前者藏在空闲", "前者发生在空闲阶段"),
    ("P/E 预算就被\n多吃几倍", "介质承担的 P/E 消耗就相应增加"),
    ("工程上的缓冲垫是预留空间", "工程上的缓解措施是预留空间"),
    ("拿一部分闪存按 SLC\n模式用", "将一部分闪存配置为 SLC\n模式"),
    ("突发写先进缓存、空闲时再腾进 TLC", "突发写入先进入缓存、空闲时再迁移至 TLC"),
    ("“缓外速度”才是介质的真速度", "缓存外持续写入速度才反映介质的持续写入能力"),
    ("动态磨损\n均衡只盯活跃数据", "动态磨损\n均衡仅处理活跃数据"),
    ("新写\n入自然流向年轻块", "新写\n入自然流向擦除次数较少的块"),
    ("静态磨\n损均衡连冷数据也搬", "静态磨\n损均衡也会迁移冷数据"),
    ("把年轻块换出来轮换", "使擦除次数较少的块重新加入分配"),
    ("消费盘多做动态加有\n限的静态", "消费级 SSD 通常采用动态磨损均衡，并辅以有\n限的静态磨损均衡"),
    ("坏块管理走的是同一条备用通道", "坏块管理同样依赖备用块池"),
    ("使用中长出的坏块", "使用过程中产生的坏块"),
    ("都由备用块池顶替，顶替一次，\n可用块与预留空间就少一分", "均由备用块替换；每替换一个坏块，\n可用块数量与预留空间都会相应减少"),
    ("请求长期得不到服务的情况被时\n间上限封死", "时间上限可避免请求长期得不到服务"),
    ("把重活推迟到中断返回之后", "把耗时较长的工作推迟到中断返回之后"),
    ("都要人动手安排", "均需要软件显式安排"),
    ("都要人动\n手安排", "均需要软件显式安\n排"),
    ("做对时效率更高", "安排合理时效率更高"),
    ("硬件 cache 自动平滑", "硬件 cache 自动管理"),
    ("时延隐藏的兵力", "可用于隐藏时延的并发线程数"),
    ("这笔账第二节谈占用率时已算过", "这一关系已在第二节讨论占用率时量化"),
    ("全是显式的", "都由程序显式指定"),
    ("把 cache 管理从硬件手里拿出来交给了程序员", "将 cache 管理从硬件自动控制转为程序员显式控制"),
    ("前者发生在空闲阶段\n里，后者变成时延尖峰", "前者发生在空闲阶段，后者表现为请求时延尖峰"),
    ("同步 GC 则直接拖慢当次写", "同步 GC 则直接增加当次写入时延"),
    ("定期把长期不改写的数据挪到", "定期将长期不改写的数据迁移到"),
    ("消费级 SSD 通常采用动态磨损均衡，并辅以有\n限的静态磨损均衡，企业盘两者均充分实施", "消费级 SSD 通常采用动态磨损均衡，并辅以有\n限的静态磨损均衡；企业级 SSD 则会更充分地实施两类均衡"),
    ("两类均衡；注意均衡搬运", "两类均衡。需要注意的是，均衡搬运"),
    ("GPU 把近端两级的控制权交给人", "GPU 将近端两级交由程序员显式管理"),
    ("最刺眼的差别", "最明显的差别"),
    ("写得规整的循环自然被硬\n件照顾", "访问模式规整的循环可由硬\n件 cache 自动利用局部性"),
    ("内核的变量一多\n就超额", "内核变量过多时会\n超出配额"),
    ("性能无声下滑", "性能随之下降"),
    ("块内线程围着这块数据复用几十上百\n次", "块内线程可将这块数据复用几十至上百\n次"),
    ("做错时没有硬件自动管理机制", "安排不当时也没有硬件自动补偿机制"),
    ("分配器管空间", "分配器管理空间"),
    ("开一条逼着回流绕路的缝", "开槽使回流绕行"),
    ("在于逼着回流画\n大圈", "在于回流必须沿槽端绕行，从而形成\n大环路"),
    ("需求的形状把结构逼成了这样", "这种结构由任务特征决定"),
    ("需\n求的形状把结构逼成了这样", "任务\n特征决定了这种结构"),
    ("直逼本底噪声", "接近本底噪声"),
    ("扩 RAM，连锁倒逼同一处", "扩 RAM，多个约束汇合到同一格式改动"),
    ("低 4 位放不下地址\\\\倒逼两字节格式", "低 4 位无法容纳地址\\\\因此需要两字节格式"),
    ("低 4 位放不下地址，同样倒逼两字节格式", "低 4 位无法容纳地址，因此同样需要两字节格式"),
    ("ADI\\\\烧两行控制字", "ADI\\\\新增两行控制字"),
    ("ADI 只烧两行控制字，最便宜", "ADI 只需新增两行控制字，成本最低"),
    ("CALL/RET 与栈要新增 SP 计数器模块，最贵", "CALL/RET 与栈需要新增 SP 计数器模块，成本最高"),
    ("不加模块只算账", "不增加模块，只核算时序"),
    ("三个目标各有走法", "三个目标各有相应的实现路径"),
    ("钉死", "限定"),
    ("写死", "固定"),
)


def polish_prose_chunk(text: str) -> str:
    literals: list[str] = []

    def protect_literal(match: re.Match[str]) -> str:
        literals.append(match.group(0))
        return f"\ue000{len(literals) - 1}\ue001"

    text = INLINE_LITERAL_RE.sub(protect_literal, text)
    text = PROSE_QUOTE_RE.sub(r"“\1”", text)
    text = re.sub(r"(?<=[\u4e00-\u9fff])(?=[A-Za-z0-9])", " ", text)
    text = re.sub(r"(?<=[A-Za-z0-9])(?=[\u4e00-\u9fff])", " ", text)
    for index, literal in enumerate(literals):
        text = text.replace(f"\ue000{index}\ue001", literal)
    return text


def polish_reader_text(text: str) -> str:
    """Apply typography fixes to prose while leaving literal code blocks intact."""
    output: list[str] = []
    prose_buffer: list[str] = []
    literal_depth = 0
    for line in text.splitlines(keepends=True):
        if re.search(r"\\begin\{(?:lstlisting|verbatim)\}", line):
            output.append(polish_prose_chunk("".join(prose_buffer)))
            prose_buffer.clear()
            literal_depth += 1
            output.append(line)
            continue
        if literal_depth:
            output.append(line)
            if re.search(r"\\end\{(?:lstlisting|verbatim)\}", line):
                literal_depth -= 1
            continue
        prose_buffer.append(line)
    if literal_depth:
        raise ValueError("unterminated literal environment")
    output.append(polish_prose_chunk("".join(prose_buffer)))
    polished = "".join(output)
    for old, new in EDITORIAL_PHRASE_REPLACEMENTS:
        polished = polished.replace(old, new)
    return polished


def renumber_sections(text: str) -> str:
    index = 0
    output: list[str] = []
    for line in text.splitlines(keepends=True):
        if line.startswith("\\section{"):
            if index >= len(CHINESE_NUMERALS):
                raise ValueError("chapter has more section blocks than supported")
            title_and_end = line[len("\\section{"):]
            title_and_end = re.sub(r"^(?:〇|[一二三四五六七八九十]+)、", "", title_and_end)
            line = f"\\section{{{CHINESE_NUMERALS[index]}、{title_and_end}"
            index += 1
        output.append(line)
    return "".join(output)


def rewrite_legacy_chapter_references(text: str, chapter_number: int) -> str:
    for old, new in LEGACY_CHAPTER_REFERENCES:
        text = text.replace(old, new)
    for old, new in LEGACY_TOPIC_REFERENCES:
        text = text.replace(old, new)
    for old, new in REFERENCE_CLEANUPS:
        text = text.replace(old, new)
    for old, new in TARGET_REPLACEMENTS.get(chapter_number, ()):
        text = text.replace(old, new)
    return text


def apply_diagram_overrides(text: str, chapter_number: int, overrides_dir: Path) -> str:
    """Replace selected legacy figures while preserving their prose and captions."""
    for caption_start, override_name in DIAGRAM_OVERRIDES.get(chapter_number, ()):
        marker = text.find(r"\diagramnote{" + caption_start)
        if marker < 0:
            raise ValueError(
                f"diagram override marker not found in chapter {chapter_number}: {caption_start}"
            )
        begin = text.rfind(r"\begin{pdfdiagram", 0, marker)
        end_token = r"\end{pdfdiagram}"
        end = text.find(end_token, begin, marker)
        if begin < 0 or end < 0:
            raise ValueError(
                f"diagram environment not found for chapter {chapter_number}: {caption_start}"
            )
        end += len(end_token)
        replacement_path = overrides_dir / override_name
        if not replacement_path.is_file():
            raise FileNotFoundError(f"missing diagram override: {replacement_path}")
        replacement = replacement_path.read_text(encoding="utf-8").strip()
        text = text[:begin] + replacement + text[end:]
    return text


ROW_END_RE = re.compile(r"\\\\(?:\[[^]]*\])?\s*$")
CHAPTER_END_SECTION_RE = re.compile(
    r"(?m)^\\section\*\{(章末练习|参考解答[^}]*)\}"
)
LONGTABLE_BEGIN_RE = re.compile(r"\\begin\{longtable\}\{[^\n]+\}")


def split_table_cells(row: str) -> list[str]:
    """Split a simple longtable row while preserving escaped ampersands."""
    cells: list[str] = []
    start = 0
    for index, character in enumerate(row):
        if character != "&":
            continue
        backslashes = 0
        cursor = index - 1
        while cursor >= 0 and row[cursor] == "\\":
            backslashes += 1
            cursor -= 1
        if backslashes % 2 == 0:
            cells.append(row[start:index].strip())
            start = index + 1
    cells.append(row[start:].strip())
    return cells


def parse_simple_longtable(table: str, chapter_number: int, section_title: str) -> list[list[str]]:
    """Read the three-column chapter-end tables without changing cell content."""
    begin = LONGTABLE_BEGIN_RE.search(table)
    if not begin:
        raise ValueError(f"chapter {chapter_number} {section_title}: malformed longtable")
    body = table[begin.end(): table.rfind(r"\end{longtable}")]
    rows: list[list[str]] = []
    buffer: list[str] = []
    for raw_line in body.splitlines():
        line = raw_line.strip()
        if not line or line in (r"\toprule", r"\midrule", r"\bottomrule"):
            continue
        buffer.append(line)
        joined = " ".join(buffer)
        if not ROW_END_RE.search(joined):
            continue
        row = ROW_END_RE.sub("", joined).strip()
        cells = split_table_cells(row)
        if len(cells) != 3:
            raise ValueError(
                f"chapter {chapter_number} {section_title}: expected 3 cells, got {len(cells)}"
            )
        rows.append(cells)
        buffer.clear()
    if buffer:
        raise ValueError(f"chapter {chapter_number} {section_title}: unterminated table row")
    if len(rows) < 2:
        raise ValueError(f"chapter {chapter_number} {section_title}: table has no exercises")
    return rows


def format_chapter_end_exercises(text: str, chapter_number: int) -> str:
    """Turn chapter-end exercise/answer tables into numbered textbook problems."""
    sections = list(CHAPTER_END_SECTION_RE.finditer(text))
    records: list[dict[str, object]] = []
    for index, section in enumerate(sections):
        section_title = section.group(1)
        section_end = sections[index + 1].start() if index + 1 < len(sections) else len(text)
        begin = LONGTABLE_BEGIN_RE.search(text, section.end(), section_end)
        if not begin:
            raise ValueError(f"chapter {chapter_number} {section_title}: missing longtable")
        end_token = r"\end{longtable}"
        end = text.find(end_token, begin.end(), section_end)
        if end < 0:
            raise ValueError(f"chapter {chapter_number} {section_title}: unterminated longtable")
        end += len(end_token)
        rows = parse_simple_longtable(text[begin.start():end], chapter_number, section_title)
        records.append(
            {
                "start": begin.start(),
                "end": end,
                "title": section_title,
                "header": rows[0],
                "entries": rows[1:],
            }
        )

    if len(records) % 2:
        raise ValueError(f"chapter {chapter_number}: unpaired exercise/answer section")

    replacements: list[tuple[int, int, str]] = []
    for pair_index in range(0, len(records), 2):
        exercise_record = records[pair_index]
        answer_record = records[pair_index + 1]
        if exercise_record["title"] != "章末练习" or not str(answer_record["title"]).startswith("参考解答"):
            raise ValueError(f"chapter {chapter_number}: exercise/answer sections are out of order")
        exercises = exercise_record["entries"]
        answers = answer_record["entries"]
        assert isinstance(exercises, list) and isinstance(answers, list)
        if len(exercises) != len(answers):
            raise ValueError(
                f"chapter {chapter_number}: {len(exercises)} exercises but {len(answers)} answers"
            )

        def title_key(title: str) -> str:
            key = re.sub(r"[\s：:（）()、，,/-]", "", title).lower()
            suffixes = ("读写周期", "寄存器", "顺序", "计算", "分析", "拍数", "事务序列", "序列", "周期")
            for suffix in suffixes:
                if key.endswith(suffix):
                    key = key[: -len(suffix)]
                    break
            return key

        unmatched = set(range(len(answers)))
        ordered_answers: list[list[str]] = []
        for exercise in exercises:
            exercise_key = title_key(exercise[0])

            def score(answer_index: int) -> tuple[float, int]:
                answer_key = title_key(answers[answer_index][0])
                exact_bonus = 2.0 if exercise_key == answer_key else 0.0
                containment_bonus = 0.5 if exercise_key in answer_key or answer_key in exercise_key else 0.0
                similarity = SequenceMatcher(None, exercise_key, answer_key).ratio()
                return exact_bonus + containment_bonus + similarity, -answer_index

            answer_index = max(unmatched, key=score)
            best_score = score(answer_index)[0]
            if best_score < 0.5:
                raise ValueError(
                    f"chapter {chapter_number}: cannot match answer {answers[answer_index][0]!r} "
                    f"to exercise {exercise[0]!r}"
                )
            ordered_answers.append(answers[answer_index])
            unmatched.remove(answer_index)

        exercise_header = exercise_record["header"]
        answer_header = answer_record["header"]
        assert isinstance(exercise_header, list) and isinstance(answer_header, list)
        exercise_rendered: list[str] = []
        answer_rendered: list[str] = []
        for exercise, answer in zip(exercises, ordered_answers):
            title, body, check = exercise
            exercise_rendered.extend(
                (
                    f"\\begin{{chapterexercise}}{{{title}}}",
                    body,
                    f"\\exercisecheck{{{exercise_header[2]}}}{{{check}}}",
                    r"\end{chapterexercise}",
                    "",
                )
            )
            _, body, check = answer
            answer_rendered.extend(
                (
                    f"\\begin{{chapteranswer}}{{{title}}}",
                    body,
                    f"\\answercheck{{{answer_header[2]}}}{{{check}}}",
                    r"\end{chapteranswer}",
                    "",
                )
            )
        replacements.extend(
            (
                (
                    int(exercise_record["start"]),
                    int(exercise_record["end"]),
                    "\n".join(exercise_rendered).rstrip(),
                ),
                (
                    int(answer_record["start"]),
                    int(answer_record["end"]),
                    "\n".join(answer_rendered).rstrip(),
                ),
            )
        )

    for start, end, replacement in reversed(replacements):
        text = text[:start] + replacement + text[end:]
    return text


def parse_longtable_rows(body: str) -> list[str]:
    """Return header and data rows from the manuscript's simple longtable dialect."""
    rows: list[str] = []
    buffer: list[str] = []
    for raw_line in body.splitlines():
        line = raw_line.strip()
        if not line or line in (r"\toprule", r"\midrule", r"\bottomrule"):
            continue
        buffer.append(line)
        joined = " ".join(buffer)
        if not ROW_END_RE.search(joined):
            continue
        rows.append(joined.strip())
        buffer.clear()
    if buffer:
        raise ValueError("unterminated row in oversized longtable")
    return rows


def split_oversized_longtables(text: str) -> str:
    """Break page-filling tables into readable chunks with repeated context."""
    end_token = r"\end{longtable}"
    position = 0
    output: list[str] = []
    while begin := LONGTABLE_BEGIN_RE.search(text, position):
        end = text.find(end_token, begin.end())
        if end < 0:
            raise ValueError("unterminated longtable while splitting oversized tables")
        output.append(text[position:begin.start()])
        body = text[begin.end():end]
        rows = parse_longtable_rows(body)
        if len(rows) < 2:
            raise ValueError("longtable has no data rows")
        header, data_rows = rows[0], rows[1:]
        source_size = sum(len(ROW_END_RE.sub("", row)) for row in data_rows)
        needs_split = len(data_rows) >= 11 or (len(data_rows) >= 5 and source_size >= 1100)
        chunks: list[list[str]] = []
        current: list[str] = []
        current_size = 0
        if needs_split:
            for row in data_rows:
                row_size = len(ROW_END_RE.sub("", row))
                if current and (len(current) >= 7 or current_size + row_size > 720):
                    chunks.append(current)
                    current = []
                    current_size = 0
                current.append(row)
                current_size += row_size
            if current:
                chunks.append(current)
        if len(chunks) <= 1:
            output.append(text[begin.start():end + len(end_token)])
            position = end + len(end_token)
            continue

        spec = begin.group(0)
        rendered_chunks: list[str] = []
        total = len(chunks)
        for chunk_index, chunk in enumerate(chunks, start=1):
            first_cell = split_table_cells(ROW_END_RE.sub("", chunk[0]))[0]
            last_cell = split_table_cells(ROW_END_RE.sub("", chunk[-1]))[0]
            # A spanning summary row contains \multicolumn, which is valid only
            # inside an alignment.  Never copy that command into the prose note.
            if first_cell.startswith(r"\multicolumn"):
                first_cell = "汇总行"
            if last_cell.startswith(r"\multicolumn"):
                last_cell = "汇总行"
            rendered_chunks.extend(
                (
                    f"\\tablechunkintro{{{chunk_index}}}{{{total}}}{{{first_cell}}}{{{last_cell}}}",
                    spec,
                    r"\toprule",
                    header,
                    r"\midrule",
                    *chunk,
                    r"\bottomrule",
                    end_token,
                    "",
                )
            )
        output.append("\n".join(rendered_chunks).rstrip())
        position = end + len(end_token)
    output.append(text[position:])
    return "".join(output)


def add_longtable_row_rules(text: str) -> str:
    """Add clearly visible horizontal separators between body rows of every table."""
    end_token = r"\end{longtable}"
    position = 0
    output: list[str] = []
    while begin := LONGTABLE_BEGIN_RE.search(text, position):
        end = text.find(end_token, begin.end())
        if end < 0:
            raise ValueError("unterminated longtable while adding row rules")
        output.append(text[position:begin.end()])
        body = text[begin.end():end]
        lines = body.splitlines(keepends=True)
        styled: list[str] = []
        for index, line in enumerate(lines):
            stripped = line.rstrip("\r\n")
            row_match = re.search(r"(\\\\(?:\[[^]]*\])?)\s*$", stripped)
            if row_match:
                next_content = ""
                for following in lines[index + 1:]:
                    if following.strip():
                        next_content = following.strip()
                        break
                if next_content not in (r"\midrule", r"\bottomrule"):
                    stripped = stripped[:row_match.end()] + r"\tablerowrule"
                    newline = line[len(line.rstrip("\r\n")):]
                    line = stripped + newline
            styled.append(line)
        output.append("".join(styled))
        output.append(end_token)
        position = end + len(end_token)
    output.append(text[position:])
    return "".join(output)


def style_checklist_tables(text: str) -> str:
    """Render intact checklist content with the book's strong row/column grid."""

    def replace_columns(match: re.Match[str]) -> str:
        spec = match.group(1).replace("p{", "L{")
        return rf"\begin{{longtable}}{{{spec}}}"

    styled = re.sub(r"\\begin\{longtable\}\{([^\n]+)\}", replace_columns, text)
    return add_longtable_row_rules(styled)


def _annotation_token(body: str) -> str:
    """Use a familiar comment token without changing listing language metadata."""
    if body.startswith(";"):
        return ";"
    if body.startswith("W = [0x0000] * 128"):
        return "#"
    return "//"


def _strip_pseudocode_annotation(body: str) -> tuple[str, int]:
    """Return the original listing body and number of generated comment lines."""
    lines = body.splitlines(keepends=True)
    count = 0
    while count < len(lines) and PSEUDOCODE_ANNOTATION_RE.match(
        lines[count].rstrip("\r\n")
    ):
        count += 1
    return "".join(lines[count:]), count


def annotate_pseudocode_listings(text: str, chapter_number: int) -> str:
    """Add two teaching comments to every registered pseudocode listing."""
    specs = PSEUDOCODE_SPECS.get(chapter_number, ())
    counts = {spec: 0 for spec in specs}

    def replace(match: re.Match[str]) -> str:
        begin, body, end = match.groups()
        original, prior_annotations = _strip_pseudocode_annotation(body)
        matched = [spec for spec in specs if original.startswith(spec.prefix)]
        if len(matched) > 1:
            prefixes = [spec.prefix for spec in matched]
            raise ValueError(
                f"chapter {chapter_number}: ambiguous pseudocode listing {prefixes}"
            )
        if not matched:
            if prior_annotations:
                raise ValueError(
                    f"chapter {chapter_number}: orphan pseudocode annotation before "
                    f"{original.splitlines()[0] if original else '<empty>'}"
                )
            return match.group(0)

        spec = matched[0]
        counts[spec] += 1
        if prior_annotations:
            if prior_annotations != 2:
                raise ValueError(
                    f"chapter {chapter_number}: {spec.prefix!r} has "
                    f"{prior_annotations} generated annotations, expected 2"
                )
            return match.group(0)

        token = _annotation_token(original)
        comments = PSEUDOCODE_COMMENTS[spec.category]
        annotation = "\n".join(
            f"{token} 注释（{label}）：{comment}"
            for label, comment in zip(("推进", "边界"), comments)
        )
        return f"{begin}{annotation}\n{original}{end}"

    annotated = LSTLISTING_RE.sub(replace, text)
    mismatches = [
        f"{spec.prefix!r}: expected {spec.expected}, found {counts[spec]}"
        for spec in specs
        if counts[spec] != spec.expected
    ]
    if mismatches:
        raise ValueError(
            f"chapter {chapter_number}: pseudocode inventory mismatch: "
            + "; ".join(mismatches)
        )
    return annotated


def audit_pseudocode_annotations(text: str, chapter_number: int) -> int:
    """Fail unless each registered block has exactly two generated comments."""
    specs = PSEUDOCODE_SPECS.get(chapter_number, ())
    counts = {spec: 0 for spec in specs}
    annotated_blocks = 0

    for match in LSTLISTING_RE.finditer(text):
        body = match.group(2)
        original, annotation_count = _strip_pseudocode_annotation(body)
        matched = [spec for spec in specs if original.startswith(spec.prefix)]
        if len(matched) > 1:
            raise ValueError(
                f"chapter {chapter_number}: ambiguous annotated pseudocode block"
            )
        if not matched:
            if annotation_count:
                raise ValueError(
                    f"chapter {chapter_number}: generated annotation has no inventory entry"
                )
            continue
        spec = matched[0]
        counts[spec] += 1
        annotated_blocks += 1
        if annotation_count != 2:
            raise ValueError(
                f"chapter {chapter_number}: {spec.prefix!r} has "
                f"{annotation_count} generated comments, expected 2"
            )

    mismatches = [
        f"{spec.prefix!r}: expected {spec.expected}, found {counts[spec]}"
        for spec in specs
        if counts[spec] != spec.expected
    ]
    if mismatches:
        raise ValueError(
            f"chapter {chapter_number}: pseudocode audit mismatch: "
            + "; ".join(mismatches)
        )
    return annotated_blocks


def main() -> int:
    root = find_root(Path.cwd().resolve())
    source_dir = root / "books/hardware-zero-to-machine/source/latex/chapters"
    output_dir = root / "books/hardware-zero-to-machine/source/latex/chapters26"
    additions_dir = output_dir / "additions"
    overrides_dir = output_dir / "diagram-overrides"
    output_dir.mkdir(parents=True, exist_ok=True)

    source_names = sorted({name for chapter in CHAPTERS for name, _ in chapter.units})
    units = {name: split_units(source_dir / name) for name in source_names}
    topic_data = {name: section_topics(source_dir / name) for name in source_names}
    topics_by_number = {name: data[0] for name, data in topic_data.items()}
    topics_by_unit = {name: data[1] for name, data in topic_data.items()}
    expected = {(name, index) for name, blocks in units.items() for index in range(len(blocks))}
    assigned: list[tuple[str, int]] = []

    expected_outputs: set[Path] = set()
    for chapter in CHAPTERS:
        body_parts: list[str] = []
        for name, indices in chapter.units:
            for index in indices:
                if index >= len(units[name]):
                    raise IndexError(f"invalid unit {name}:{index}")
                assigned.append((name, index))
                body_parts.append(
                    rewrite_legacy_section_references(
                        units[name][index],
                        name,
                        index,
                        topics_by_number,
                        topics_by_unit,
                    )
                )
        for extra_name in EXTRAS.get(chapter.number, ()):
            extra = additions_dir / extra_name
            if not extra.is_file():
                raise FileNotFoundError(f"missing chapter addition: {extra}")
            body_parts.append(extra.read_text(encoding="utf-8"))
        body = rewrite_legacy_chapter_references(
            "".join(body_parts).lstrip("\n"), chapter.number
        )
        body = apply_diagram_overrides(body, chapter.number, overrides_dir)
        body = annotate_pseudocode_listings(body, chapter.number)
        audit_pseudocode_annotations(body, chapter.number)
        body = format_chapter_end_exercises(body, chapter.number)
        body = split_oversized_longtables(body)
        body = add_longtable_row_rules(body)
        body = renumber_sections(body)
        output = output_dir / f"ch{chapter.number:02d}-{chapter.slug}.tex"
        expected_outputs.add(output)
        text = (
            f"\\chapter{{{chapter.title}}}\n\n"
            "\\begin{keyidea}\n"
            f"{chapter.opening}\n"
            "\\end{keyidea}\n\n"
            f"{body}"
        )
        text = polish_reader_text(text)
        text = text.rstrip() + "\n"
        output.write_text(text, encoding="utf-8")

    duplicates = sorted({item for item in assigned if assigned.count(item) > 1})
    missing = sorted(expected - set(assigned))
    extra = sorted(set(assigned) - expected)
    if duplicates or missing or extra:
        for output in expected_outputs:
            output.unlink(missing_ok=True)
        print(f"coverage failure: duplicates={duplicates} missing={missing} extra={extra}", file=sys.stderr)
        return 1

    for stale in output_dir.glob("ch*.tex"):
        if stale not in expected_outputs:
            stale.unlink()

    checklist_source = source_dir.parent / "backmatter/checklist.tex"
    checklist_output = source_dir.parent / "backmatter/checklist-rendered.tex"
    checklist_output.write_text(
        style_checklist_tables(checklist_source.read_text(encoding="utf-8")),
        encoding="utf-8",
    )

    print(
        f"built {len(CHAPTERS)} chapters from {len(expected)} legacy content units; "
        "every unit assigned exactly once"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
