from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


CODE_ENVIRONMENTS = {
    "lstlisting",
    "verbatim",
    "Verbatim",
    "codeblock",
    "tcblisting",
}

INLINE_CODE_COMMANDS = {
    "code",
    "cmd",
    "filepath",
    "url",
    "nolinkurl",
    "complexity",
}

CJK_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]")
ENGLISH_WORD_RE = re.compile(r"[A-Za-z][A-Za-z0-9]*(?:[-'][A-Za-z0-9]+)*")


@dataclass(frozen=True)
class CountResult:
    path: Path
    matter: str
    cjk_chars: int
    english_words: int

    @property
    def total(self) -> int:
        return self.cjk_chars + self.english_words


def strip_comments(text: str) -> str:
    lines: list[str] = []
    for line in text.splitlines():
        out: list[str] = []
        escaped = False
        for ch in line:
            if ch == "\\" and not escaped:
                escaped = True
                out.append(ch)
                continue
            if ch == "%" and not escaped:
                break
            escaped = False
            out.append(ch)
        lines.append("".join(out))
    return "\n".join(lines)


def strip_code_environments(text: str) -> str:
    for env in CODE_ENVIRONMENTS:
        pattern = re.compile(
            rf"\\begin\{{{re.escape(env)}\}}(?:\[[^\]]*\])?.*?\\end\{{{re.escape(env)}\}}",
            re.DOTALL,
        )
        text = pattern.sub(" ", text)
    return text


def remove_command_arg(text: str, command: str) -> str:
    pattern = re.compile(rf"\\{command}\s*(?:\[[^\]]*\])?\{{[^{{}}]*\}}")
    previous = None
    while previous != text:
        previous = text
        text = pattern.sub(" ", text)
    return text


def normalize_text_macros(text: str) -> str:
    text = re.sub(r"\\term\s*\{([^{}]*)\}\s*\{([^{}]*)\}", r"\1 \2", text)
    text = re.sub(r"\\href\s*\{[^{}]*\}\s*\{([^{}]*)\}", r"\1", text)
    return text


def strip_math(text: str) -> str:
    text = re.sub(r"\\\[(.*?)\\\]", " ", text, flags=re.DOTALL)
    text = re.sub(r"\\\((.*?)\\\)", " ", text, flags=re.DOTALL)
    text = re.sub(r"\$\$(.*?)\$\$", " ", text, flags=re.DOTALL)
    text = re.sub(r"\$(?:\\.|[^$])*\$", " ", text)
    return text


def strip_latex_markup(text: str) -> str:
    text = re.sub(r"\\(?:begin|end)\s*\{[^{}]*\}", " ", text)
    text = re.sub(
        r"\\(?:documentclass|usepackage|input|includegraphics)\s*(?:\[[^\]]*\])?\s*\{[^{}]*\}",
        " ",
        text,
    )
    text = re.sub(r"\\[A-Za-z@]+\*?(?:\[[^\]]*\])?", " ", text)
    text = re.sub(r"\\.", " ", text)
    text = text.replace("{", " ").replace("}", " ")
    text = text.replace("~", " ")
    return text


def readable_text(tex: str) -> str:
    text = strip_comments(tex)
    text = strip_code_environments(text)
    for command in INLINE_CODE_COMMANDS:
        text = remove_command_arg(text, command)
    text = normalize_text_macros(text)
    text = strip_math(text)
    text = strip_latex_markup(text)
    return text


@dataclass(frozen=True)
class InputItem:
    path: Path
    matter: str


def count_file(item: InputItem) -> CountResult:
    path = item.path
    text = readable_text(path.read_text(encoding="utf-8"))
    return CountResult(
        path=path,
        matter=item.matter,
        cjk_chars=len(CJK_RE.findall(text)),
        english_words=len(ENGLISH_WORD_RE.findall(text)),
    )


def update_matter(line: str, matter: str) -> str:
    if r"\frontmatter" in line:
        return "frontmatter"
    if r"\mainmatter" in line:
        return "mainmatter"
    if r"\appendix" in line:
        return "appendices"
    if r"\backmatter" in line:
        return "backmatter"
    return matter


def input_items(root: Path, main_tex: Path) -> list[InputItem]:
    seen: set[tuple[Path, str]] = set()
    ordered: list[InputItem] = []

    def visit(path: Path, matter: str) -> None:
        resolved = path.resolve()
        key = (resolved, matter)
        if key in seen:
            return
        seen.add(key)
        ordered.append(InputItem(path=path, matter=matter))

        current_matter = matter
        for line in path.read_text(encoding="utf-8").splitlines():
            current_matter = update_matter(line, current_matter)
            for match in re.finditer(r"\\input\{([^{}]+)\}", line):
                child = root / f"{match.group(1)}.tex"
                if child.exists():
                    visit(child, current_matter)

    visit(main_tex, "other")
    return ordered


def category(result: CountResult) -> str:
    return result.matter


def is_book_content(result: CountResult) -> bool:
    return result.matter == "mainmatter"


def print_table(results: list[CountResult], root: Path) -> None:
    print("units = Chinese Han characters + English words; code, formulas, and inline code macros excluded")
    print()
    print(f"{'units':>8} {'CJK':>8} {'EN':>8} {'matter':>12}  file")
    for result in results:
        rel = result.path.relative_to(root)
        print(
            f"{result.total:8d} {result.cjk_chars:8d} "
            f"{result.english_words:8d} {result.matter:>12}  {rel}"
        )

    print()
    for name in ("frontmatter", "mainmatter", "appendices", "backmatter"):
        group = [result for result in results if category(result) == name]
        if not group:
            continue
        total = sum(result.total for result in group)
        cjk = sum(result.cjk_chars for result in group)
        english = sum(result.english_words for result in group)
        print(f"{name:>12}: {total:8d} units ({cjk} CJK + {english} EN)")

    counted = [result for result in results if is_book_content(result)]
    total = sum(result.total for result in counted)
    cjk = sum(result.cjk_chars for result in counted)
    english = sum(result.english_words for result in counted)
    print(f"{'main text':>12}: {total:8d} units ({cjk} CJK + {english} EN)")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Count readable book size as Chinese Han characters plus English words, excluding code."
    )
    parser.add_argument("--chapters-only", action="store_true", help="count only files under chapters/")
    parser.add_argument("--min", type=int, default=0, help="fail if counted units are below this minimum")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    items = input_items(root, root / "main.tex")
    items = [item for item in items if item.path.suffix == ".tex"]
    if args.chapters_only:
        items = [
            item
            for item in items
            if "chapters" in item.path.parts and item.matter == "mainmatter"
        ]

    results = [count_file(item) for item in items]
    print_table(results, root)

    total = sum(result.total for result in results if is_book_content(result))
    if args.min and total < args.min:
        print()
        print(f"FAILED: {total} units < required minimum {args.min} units")
        print(f"remaining: {args.min - total} units")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
