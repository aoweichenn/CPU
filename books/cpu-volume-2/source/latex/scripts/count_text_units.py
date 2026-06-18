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
    "systemdiagram",
    "tikzpicture",
}

INLINE_CODE_COMMANDS = {
    "code",
    "cmd",
    "filepath",
    "register",
    "instruction",
    "url",
    "nolinkurl",
}

CJK_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]")
ENGLISH_WORD_RE = re.compile(r"[A-Za-z][A-Za-z0-9]*(?:[-'][A-Za-z0-9]+)*")


@dataclass(frozen=True)
class CountResult:
    path: Path
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
    text = re.sub(r"\\engterm\s*\{([^{}]*)\}", r"\1", text)
    text = re.sub(r"\\href\s*\{[^{}]*\}\s*\{([^{}]*)\}", r"\1", text)
    text = re.sub(r"\\topic\s*\{([^{}]*)\}", r"\1", text)
    return text


def strip_math(text: str) -> str:
    text = re.sub(r"\\\[(.*?)\\\]", " ", text, flags=re.DOTALL)
    text = re.sub(r"\\\((.*?)\\\)", " ", text, flags=re.DOTALL)
    text = re.sub(r"\$\$(.*?)\$\$", " ", text, flags=re.DOTALL)
    text = re.sub(r"\$(?:\\.|[^$])*\$", " ", text)
    return text


def strip_latex_markup(text: str) -> str:
    text = re.sub(r"\\(?:begin|end)\s*\{[^{}]*\}", " ", text)
    text = re.sub(r"\\(?:documentclass|usepackage|input|includegraphics)\s*(?:\[[^\]]*\])?\s*\{[^{}]*\}", " ", text)
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


def count_file(path: Path) -> CountResult:
    text = readable_text(path.read_text(encoding="utf-8"))
    cjk_chars = len(CJK_RE.findall(text))
    english_words = len(ENGLISH_WORD_RE.findall(text))
    return CountResult(path=path, cjk_chars=cjk_chars, english_words=english_words)


def input_paths(root: Path, main_tex: Path) -> list[Path]:
    seen: set[Path] = set()
    ordered: list[Path] = []

    def visit(path: Path) -> None:
        resolved = path.resolve()
        if resolved in seen:
            return
        seen.add(resolved)
        ordered.append(path)
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r"\\input\{([^{}]+)\}", text):
            child = root / f"{match.group(1)}.tex"
            if child.exists():
                visit(child)

    visit(main_tex)
    return ordered


def category(path: Path) -> str:
    parts = set(path.parts)
    if "chapters" in parts:
        return "chapters"
    if "frontmatter" in parts or "outline" in parts:
        return "frontmatter"
    if "appendices" in parts or "backmatter" in parts:
        return "appendices"
    return "other"


def is_book_content(path: Path) -> bool:
    return category(path) in {"frontmatter", "chapters", "appendices"}


def print_table(results: list[CountResult], root: Path) -> None:
    print("units = Chinese Han characters + English words; code/listings/verbatim/inline code macros excluded")
    print()
    print(f"{'units':>8} {'CJK':>8} {'EN':>8}  file")
    for result in results:
        rel = result.path.relative_to(root)
        print(f"{result.total:8d} {result.cjk_chars:8d} {result.english_words:8d}  {rel}")

    print()
    for name in ("frontmatter", "chapters", "appendices"):
        group = [result for result in results if category(result.path) == name]
        if not group:
            continue
        total = sum(result.total for result in group)
        cjk = sum(result.cjk_chars for result in group)
        english = sum(result.english_words for result in group)
        print(f"{name:>12}: {total:8d} units ({cjk} CJK + {english} EN)")

    ignored = [result for result in results if category(result.path) == "other"]
    if ignored:
        total = sum(result.total for result in ignored)
        cjk = sum(result.cjk_chars for result in ignored)
        english = sum(result.english_words for result in ignored)
        print(f"{'ignored':>12}: {total:8d} units ({cjk} CJK + {english} EN)")

    counted = [result for result in results if is_book_content(result.path)]
    total = sum(result.total for result in counted)
    cjk = sum(result.cjk_chars for result in counted)
    english = sum(result.english_words for result in counted)
    print(f"{'book total':>12}: {total:8d} units ({cjk} CJK + {english} EN)")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Count readable book size as Chinese Han characters plus English words, excluding code."
    )
    parser.add_argument("--chapters-only", action="store_true", help="count only files under chapters/")
    parser.add_argument("--min", type=int, default=0, help="fail if counted units are below this minimum")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    paths = input_paths(root, root / "main.tex")
    paths = [path for path in paths if path.suffix == ".tex"]
    if args.chapters_only:
        paths = [path for path in paths if "chapters" in path.parts]

    results = [count_file(path) for path in paths]
    print_table(results, root)

    total = sum(result.total for result in results if is_book_content(result.path))
    if args.min and total < args.min:
        print()
        print(f"FAILED: {total} units < required minimum {args.min} units")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
