#!/usr/bin/env python3
"""Build the reader-facing 26-chapter manuscript from intact legacy units."""

from __future__ import annotations

from dataclasses import dataclass
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
LEGACY_CHAPTER_REFERENCES = (
    ("第十三章", "NVMe 与存储 I/O 两章"),
    ("第十二章", "“冒险、异常与性能”一章"),
    ("第九章", "“电源、时钟与信号完整性”一章"),
    ("第八章", "最后的教学计算机项目"),
    ("第七章", "经典芯片与平台案例"),
    ("第五章", "存储与设备事务部分"),
    ("第四章", "处理器核心部分"),
    ("第三章", "数值与数据通路部分"),
    ("第二章", "“状态、时钟与时序逻辑”一章"),
    ("第一章", "数字硬件基础部分"),
)
REFERENCE_CLEANUPS = (
    ("经典芯片与平台案例经典芯片", "经典芯片与平台案例"),
    ("最后的教学计算机项目面包板教学机", "教学计算机项目中的面包板机"),
    ("最后的教学计算机项目教学机", "教学计算机项目"),
    ("最后的教学计算机项目的面包板计算机", "教学计算机项目中的面包板计算机"),
    ("最后的教学计算机项目的教学机", "教学计算机项目中的教学机"),
    ("最后的教学计算机项目这台机器", "教学计算机项目中的这台机器"),
)
TARGET_REPLACEMENTS = {
    2: (("独占整章", "独占一个完整部分"),),
    9: (("独占整章", "独占一个完整部分"),),
    20: (("前面七章", "前面的数字与处理器章节"),),
    25: (("前面十四章", "前面二十四章"),),
    26: (("前面七章", "前面二十五章"),),
}


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
    for old, new in REFERENCE_CLEANUPS:
        text = text.replace(old, new)
    for old, new in TARGET_REPLACEMENTS.get(chapter_number, ()):
        text = text.replace(old, new)
    return text


def main() -> int:
    root = find_root(Path.cwd().resolve())
    source_dir = root / "books/hardware-zero-to-machine/source/latex/chapters"
    output_dir = root / "books/hardware-zero-to-machine/source/latex/chapters26"
    additions_dir = output_dir / "additions"
    output_dir.mkdir(parents=True, exist_ok=True)

    source_names = sorted({name for chapter in CHAPTERS for name, _ in chapter.units})
    units = {name: split_units(source_dir / name) for name in source_names}
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
                body_parts.append(units[name][index])
        for extra_name in EXTRAS.get(chapter.number, ()):
            extra = additions_dir / extra_name
            if not extra.is_file():
                raise FileNotFoundError(f"missing chapter addition: {extra}")
            body_parts.append(extra.read_text(encoding="utf-8"))
        body = rewrite_legacy_chapter_references(
            "".join(body_parts).lstrip("\n"), chapter.number
        )
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
