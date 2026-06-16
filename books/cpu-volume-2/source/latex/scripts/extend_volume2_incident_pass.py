from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from expand_volume2_600k import CHAPTERS  # noqa: E402


MARKER_BEGIN = "% BEGIN VOLUME2_600K_INCIDENT_PASS"
MARKER_END = "% END VOLUME2_600K_INCIDENT_PASS"


def render(chapter) -> str:
    first = chapter.mechanisms[0]
    second = chapter.mechanisms[1]
    third = chapter.mechanisms[2]
    last = chapter.mechanisms[-1]
    case = chapter.cases[-1]
    return "\n\n".join(
        item.strip()
        for item in (
            MARKER_BEGIN,
            "\\topic{事故复盘式走查}",
            f"为了把本章内容真正串起来，可以把{chapter.running_problem}写成一次小型事故复盘。"
            f"事故现象不是一句“系统慢了”或“结果错了”，而要具体到可观察事实：哪一批输入、哪个阶段、哪一个指标、哪一种失败注入触发了问题。"
            f"如果现象描述不具体，后面的{first.name}、{second.name}和{third.name}就只能变成猜测。",
            f"复盘第一步是还原朴素设计。写清当时为什么选择它，它解决了什么简单问题，又默认了哪些条件。"
            f"很多坏设计一开始并不坏，只是从小输入、小并发、无失败的条件迁移到{chapter.running_problem}时，没有同步升级边界。"
            f"这也是本册反复强调从朴素方案出发的原因：只有理解朴素方案为什么成立，才能准确说明它为什么在新条件下失败。",
            f"复盘第二步是列出候选假设，但每个假设都必须有可证伪实验。"
            f"如果怀疑{first.name}相关路径，就构造只改变这一因素的对照；如果怀疑{second.name}，就记录它对应的状态或指标；如果怀疑{third.name}，就找出它在代码里的所有入口和退出点。"
            "不能把所有怀疑同时改掉。一次修改多个因素会让复盘变成经验叙事，而不是工程证据。",
            f"复盘第三步是修复。修复不等于把代码改到通过测试，而是把不变量写回系统。"
            f"本章的不变量来自“{chapter.main_failure}”这个失败主题。"
            "因此修复说明要写清：新增字段保护什么，新增队列容量限制什么，新增版本号拒绝什么，新增 reference 比较什么，新增指标暴露什么。"
            "如果修复无法对应到不变量，就很可能只是局部补丁。",
            f"复盘第四步是验收。以“{case.name}”为例，验收不应只跑正常输入，还要跑边界输入、压力输入和错误输入。"
            f"正常输入证明功能还在，边界输入证明不变量没有被破坏，压力输入证明成本模型仍然成立，错误输入证明失败不会越过提交边界。"
            "验收报告要保留原始命令和数据，否则下一次回归无法判断问题是否真正修复。",
            f"最后要写“未解决问题”。例如{last.name}相关边界可能还没有在当前设备上完全验证，某些 Linux 计数器可能因为权限不可用，某些 NUMA 结论可能因为硬件不支持只能停留在设计层。"
            "把未验证风险写出来不是降低质量，而是防止教材把假设写成事实。"
            "高质量书籍要敢于说明证据边界，这比把每个结论都写成绝对经验更可靠。",
            "读者完成本章后，可以用这份复盘模板检查自己的学习结果：能否描述事故，能否还原朴素设计，能否提出可证伪假设，能否把修复绑定到不变量，能否设计验收矩阵，能否写出未验证风险。"
            "如果六项都能完成，本章知识就已经从概念记忆转成工程判断。"
            "如果只能完成其中一两项，就应回到本章前面的案例重新走一遍，而不是急着进入下一章。",
            MARKER_END,
            "",
        )
    )


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    changed = 0
    for chapter in CHAPTERS:
        path = root / chapter.path
        text = path.read_text(encoding="utf-8")
        if MARKER_BEGIN in text:
            print(f"already incident-expanded {chapter.path}")
            continue
        if chapter.anchor not in text:
            raise RuntimeError(f"anchor not found in {path}: {chapter.anchor}")
        text = text.replace(chapter.anchor, f"{render(chapter)}\n{chapter.anchor}", 1)
        path.write_text(text, encoding="utf-8")
        changed += 1
        print(f"incident-expanded {chapter.path}")
    print(f"changed chapters: {changed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
