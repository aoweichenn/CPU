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
    8: ("ch08-pipeline-core.tex",),
    9: ("ch09-pipeline-hazards.tex",),
    11: ("ch11-cache-interface.tex",),
    12: ("ch12-modern-vm-tlb.tex",),
    14: ("ch14-nvme-interface.tex",),
    16: ("ch16-persistence-reliability.tex",),
    17: ("ch17-handshake-axi.tex",),
    18: ("ch18-pcie-transactions.tex",),
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
    return "".join(output)


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
    """Add subtle horizontal separators between body rows of every remaining table."""
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

    print(
        f"built {len(CHAPTERS)} chapters from {len(expected)} legacy content units; "
        "every unit assigned exactly once"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
