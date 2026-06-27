#!/usr/bin/env python3
"""Build a phone-friendly EPUB from the project's LaTeX book sources.

This is intentionally a small project-specific converter, not a general
LaTeX engine. It supports the macros and environments used by this book and
keeps the EPUB navigation at the same coarse level as the PDF table of
contents: front matter, parts, chapters, appendices, and glossary.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import os
import re
import subprocess
import sys
import uuid
import zipfile
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


BOOK_TITLE = os.environ.get("BOOK_TITLE", "从 C++ 到机器执行：第一册")
BOOK_SUBTITLE = os.environ.get("BOOK_SUBTITLE", "底层原理、汇编接口与可信性能测量")
BOOK_AUTHOR = os.environ.get("BOOK_AUTHOR", "CPU Performance Study")
BOOK_LANG = os.environ.get("BOOK_LANG", "zh-CN")
BOOK_IMPORT_TITLE = os.environ.get("BOOK_IMPORT_TITLE", BOOK_TITLE)
BOOK_ID_SEED = os.environ.get("BOOK_ID_SEED", BOOK_IMPORT_TITLE)
EPUB_LINEARIZE_TABLES = os.environ.get("EPUB_LINEARIZE_TABLES", "").lower() in {"1", "true", "yes", "on"}
WECHAT_COMPATIBLE = os.environ.get("EPUB_WECHAT_COMPATIBLE", "").lower() in {"1", "true", "yes", "on"}
EPUB_ASCII_TITLES = os.environ.get("EPUB_ASCII_TITLES", "").lower() in {"1", "true", "yes", "on"}

BOX_TITLES = {
    "keyidea": "核心思想",
    "mentalmodel": "心智模型",
    "warningbox": "常见误区",
    "labbox": "实验",
    "exercisebox": "习题与作业",
    "deepdive": "深入理解",
}

SKIP_ENVIRONMENTS = {
    "pdfdiagram",
    "systemdiagram",
    "tikzpicture",
}

LIST_ENV = {
    "itemize": "ul",
    "enumerate": "ol",
    "description": "dl",
}

CPP_KEYWORDS = {
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "break",
    "case",
    "catch",
    "class",
    "co_await",
    "co_return",
    "co_yield",
    "compl",
    "concept",
    "const",
    "consteval",
    "constexpr",
    "constinit",
    "const_cast",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "using",
    "virtual",
    "volatile",
    "while",
    "xor",
    "xor_eq",
}

CPP_TYPES = {
    "array",
    "bool",
    "char",
    "deque",
    "double",
    "float",
    "int",
    "int64_t",
    "long",
    "map",
    "optional",
    "pair",
    "priority_queue",
    "queue",
    "set",
    "shared_ptr",
    "short",
    "size_t",
    "span",
    "stack",
    "std",
    "string",
    "string_view",
    "tuple",
    "uint64_t",
    "unique_ptr",
    "unordered_map",
    "unordered_set",
    "variant",
    "vector",
    "void",
}

CPP_DIRECTIVES = {
    "define",
    "elif",
    "else",
    "endif",
    "error",
    "if",
    "ifdef",
    "ifndef",
    "include",
    "pragma",
    "undef",
    "warning",
}


@dataclass
class SourceEntry:
    kind: str
    source: Path | None = None
    title: str = ""
    label: str = ""
    chapter_no: int | None = None
    part_id: str | None = None
    state: str = "main"
    appendix: bool = False
    output: str = ""
    nav_title: str = ""


@dataclass
class PartNode:
    ident: str
    title: str
    output: str
    children: list[SourceEntry] = field(default_factory=list)


@dataclass(frozen=True)
class EmbeddedFont:
    source: Path
    href: str
    family: str
    style: str
    weight: str
    media_type: str


@dataclass(frozen=True)
class EpubAsset:
    source: Path
    href: str
    media_type: str


class LatexInline:
    def __init__(self) -> None:
        self.today = datetime.now(timezone.utc).date().isoformat()

    def convert(self, text: str) -> str:
        text = text.replace("\n", " ")
        return self._parse(text, 0, len(text))

    def _parse(self, text: str, start: int, end: int) -> str:
        out: list[str] = []
        i = start
        while i < end:
            ch = text[i]
            if ch == "\\":
                rendered, i = self._parse_command(text, i, end)
                out.append(rendered)
                continue
            if ch == "$":
                j = find_unescaped(text, "$", i + 1, end)
                if j != -1:
                    out.append(f'<span class="math">{html.escape(latex_math_plain(text[i + 1:j]))}</span>')
                    i = j + 1
                    continue
            if ch == "~":
                out.append("&nbsp;")
            elif ch in "{}":
                out.append(html.escape(ch))
            else:
                out.append(html.escape(ch))
            i += 1
        return "".join(out)

    def _parse_command(self, text: str, i: int, end: int) -> tuple[str, int]:
        if i + 1 >= end:
            return "\\", i + 1

        nxt = text[i + 1]
        if nxt in "%&_#$":
            return html.escape(nxt), i + 2
        if nxt in "{}":
            return html.escape(nxt), i + 2
        if nxt == "\\":
            return "<br />", i + 2

        j = i + 1
        while j < end and text[j].isalpha():
            j += 1
        name = text[i + 1:j]
        if not name:
            return html.escape(nxt), i + 2
        if j < end and text[j] == "*":
            name += "*"
            j += 1

        j = skip_ws(text, j, end)
        if j < end and text[j] == "[":
            _, j = read_group(text, j, "[", "]")
            j = skip_ws(text, j, end)

        no_arg = {
            "LaTeX": "LaTeX",
            "TeX": "TeX",
            "today": self.today,
            "rightarrow": "→",
            "leftarrow": "←",
            "to": "→",
            "ldots": "…",
            "dots": "…",
            "quad": " ",
            "qquad": " ",
            "times": "×",
            "le": "≤",
            "ge": "≥",
            "neq": "≠",
            "sim": "∼",
            "log": "log",
            "cpp": "C++",
            "cpp20": "C++20",
            "cppTwenty": "C++20",
            "n": "\n",
            "t": "\t",
        }
        if name in no_arg:
            return html.escape(no_arg[name]), j

        one_arg = {
            "code": "code",
            "codeesc": "code",
            "cmd": "code",
            "filepath": "code",
            "register": "code",
            "instruction": "code",
            "texttt": "code",
            "nolinkurl": "code",
            "engterm": "strong",
            "concept": "strong",
            "textbf": "strong",
            "emph": "em",
            "textit": "em",
            "makecell": "span",
        }
        math_arg = {
            "complexity",
            "ensuremath",
            "mathrm",
            "mathit",
            "mathbf",
        }
        if name in math_arg and j < end and text[j] == "{":
            raw, j = read_group(text, j)
            return f'<span class="math">{html.escape(latex_math_plain(raw))}</span>', j

        if name in one_arg and j < end and text[j] == "{":
            raw, j = read_group(text, j)
            converted = self.convert(raw)
            tag = one_arg[name]
            if tag == "code":
                return f"<code>{html.escape(latex_plain(raw))}</code>", j
            if tag == "span":
                return converted.replace("\\\\", "<br />"), j
            return f"<{tag}>{converted}</{tag}>", j

        if name == "term" and j < end and text[j] == "{":
            first, j = read_group(text, j)
            j = skip_ws(text, j, end)
            second = ""
            if j < end and text[j] == "{":
                second, j = read_group(text, j)
            zh = self.convert(first)
            en = self.convert(second)
            return f"<strong>{zh}</strong>（{en}）", j

        if name == "href" and j < end and text[j] == "{":
            url, j = read_group(text, j)
            j = skip_ws(text, j, end)
            label = url
            if j < end and text[j] == "{":
                label, j = read_group(text, j)
            return f'<a href="{html.escape(latex_plain(url), quote=True)}">{self.convert(label)}</a>', j

        if name == "url" and j < end and text[j] == "{":
            url, j = read_group(text, j)
            safe = html.escape(latex_plain(url), quote=True)
            return f'<a href="{safe}">{html.escape(latex_plain(url))}</a>', j

        if name in {"index", "label", "ref", "pageref", "cite"} and j < end and text[j] == "{":
            _, j = read_group(text, j)
            return "", j

        if j < end and text[j] == "{":
            raw, j = read_group(text, j)
            return self.convert(raw), j

        return "", j


class LatexBlockConverter:
    def __init__(
        self,
        inline: LatexInline,
        heading_prefix: str = "",
        chapter_no: int | None = None,
        linearize_tables: bool = False,
        legacy_markup: bool = False,
        book_dir: Path | None = None,
        assets: dict[Path, EpubAsset] | None = None,
    ) -> None:
        self.inline = inline
        self.heading_prefix = heading_prefix
        self.chapter_no = chapter_no
        self.linearize_tables = linearize_tables
        self.legacy_markup = legacy_markup
        self.book_dir = book_dir
        self.assets = assets
        self.box_tag = "div" if legacy_markup else "aside"
        self.section_no = 0
        self.out: list[str] = []
        self.paragraph: list[str] = []
        self.list_stack: list[dict[str, object]] = []

    def convert(self, source: str) -> str:
        lines = preprocess_lines(source)
        i = 0
        while i < len(lines):
            raw_line = lines[i]
            line = raw_line.strip()

            if not line:
                self.flush_paragraph()
                i += 1
                continue

            begin = re.match(r"\\begin\{([^}]+)\}(.*)", line)
            if begin:
                env = begin.group(1)
                if env in SKIP_ENVIRONMENTS:
                    _, i = collect_environment(lines, i, env)
                    self.flush_paragraph()
                    continue
                if env in {"lstlisting", "verbatim"}:
                    code, i = collect_environment(lines, i, env)
                    self.flush_paragraph()
                    self.out.append(render_code_block(code.rstrip(), env, begin.group(2)))
                    continue
                if env == "tabular":
                    table, i = collect_environment(lines, i, env)
                    self.flush_paragraph()
                    self.out.append(render_table(table, self.inline, linearize=self.linearize_tables))
                    continue
                if env in LIST_ENV:
                    self.flush_paragraph()
                    tag = LIST_ENV[env]
                    self.out.append(f"<{tag}>")
                    self.list_stack.append({"tag": tag, "li_open": False})
                    i += 1
                    continue
                if env in BOX_TITLES:
                    self.flush_paragraph()
                    title = BOX_TITLES[env]
                    self.out.append(
                        f'<{self.box_tag} class="box {env}"><p class="box-title">{html.escape(title)}</p>'
                    )
                    i += 1
                    continue
                if env == "principlebox":
                    self.flush_paragraph()
                    title = "原则"
                    rest = begin.group(2).strip()
                    if rest.startswith("{"):
                        raw_title, _ = read_group(rest, 0)
                        title = latex_plain(raw_title) or title
                    self.out.append(
                        f'<{self.box_tag} class="box {env}"><p class="box-title">{self.inline.convert(title)}</p>'
                    )
                    i += 1
                    continue
                if env in {"center", "minipage", "titlepage"}:
                    self.flush_paragraph()
                    i += 1
                    continue

            end_match = re.match(r"\\end\{([^}]+)\}", line)
            if end_match:
                env = end_match.group(1)
                self.flush_paragraph()
                if env in LIST_ENV:
                    self.close_list()
                elif env in BOX_TITLES:
                    self.out.append(f"</{self.box_tag}>")
                elif env == "principlebox":
                    self.out.append(f"</{self.box_tag}>")
                i += 1
                continue

            heading = parse_heading(line)
            if heading:
                level, title = heading
                self.flush_paragraph()
                if level == "chapter":
                    self.section_no = 0
                    text = f"{self.heading_prefix} {self.inline.convert(title)}".strip()
                    self.out.append(f"<h1>{text}</h1>")
                elif level == "section":
                    self.section_no += 1
                    text = self.inline.convert(title)
                    if self.chapter_no is not None:
                        number = f"{self.chapter_no}.{self.section_no}"
                        self.out.append(f'<h2><span class="section-number">{number}</span> {text}</h2>')
                    else:
                        self.out.append(f"<h2>{text}</h2>")
                elif level == "subsection":
                    self.out.append(f"<h3>{self.inline.convert(title)}</h3>")
                elif level == "topic":
                    text = self.inline.convert(title)
                    if self.legacy_markup:
                        self.out.append(f'<p class="topic-heading"><strong>{text}</strong></p>')
                    else:
                        self.out.append(f'<h4 class="topic-heading">{text}</h4>')
                i += 1
                continue

            if line.startswith("\\addcontentsline"):
                i += 1
                continue

            figure = parse_book_figure_command(line)
            if figure is not None:
                self.flush_paragraph()
                path, caption = figure
                if self.book_dir is not None and self.assets is not None:
                    self.out.append(render_book_figure(self.book_dir, self.assets, path, caption, self.inline))
                i += 1
                continue

            if line.startswith("\\item"):
                self.flush_paragraph()
                self.open_item(line)
                i += 1
                continue

            if line in {"\\toprule", "\\midrule", "\\bottomrule", "\\hline"}:
                i += 1
                continue

            cleaned = line
            if cleaned in {"\\frontmatter", "\\mainmatter", "\\backmatter", "\\appendix", "\\printindex"}:
                i += 1
                continue

            self.paragraph.append(cleaned)
            i += 1

        self.flush_paragraph()
        while self.list_stack:
            self.close_list()
        return "\n".join(self.out)

    def flush_paragraph(self) -> None:
        if not self.paragraph:
            return
        text = " ".join(part.strip() for part in self.paragraph if part.strip())
        self.paragraph.clear()
        if not text:
            return
        converted = self.inline.convert(text)
        if self.list_stack and self.list_stack[-1]["li_open"]:
            self.out.append(f"<p>{converted}</p>")
        else:
            self.out.append(f"<p>{converted}</p>")

    def open_item(self, line: str) -> None:
        if not self.list_stack:
            self.out.append("<ul>")
            self.list_stack.append({"tag": "ul", "li_open": False})
        top = self.list_stack[-1]
        tag = str(top["tag"])
        if top["li_open"]:
            self.out.append("</dd>" if tag == "dl" else "</li>")
        content = line[len("\\item"):].strip()
        if tag == "dl":
            label = ""
            if content.startswith("["):
                label, pos = read_group(content, 0, "[", "]")
                content = content[pos:].strip()
            self.out.append(f"<dt>{self.inline.convert(label)}</dt><dd>")
        else:
            self.out.append("<li>")
        top["li_open"] = True
        if content:
            self.out.append(self.inline.convert(content))

    def close_list(self) -> None:
        if not self.list_stack:
            return
        top = self.list_stack.pop()
        tag = str(top["tag"])
        if top["li_open"]:
            self.out.append("</dd>" if tag == "dl" else "</li>")
        self.out.append(f"</{tag}>")


def skip_ws(text: str, i: int, end: int | None = None) -> int:
    end = len(text) if end is None else end
    while i < end and text[i].isspace():
        i += 1
    return i


def find_unescaped(text: str, needle: str, start: int, end: int) -> int:
    i = start
    while i < end:
        if text[i] == needle and (i == 0 or text[i - 1] != "\\"):
            return i
        i += 1
    return -1


def read_group(text: str, i: int, open_ch: str = "{", close_ch: str = "}") -> tuple[str, int]:
    if i >= len(text) or text[i] != open_ch:
        return "", i
    depth = 1
    j = i + 1
    out: list[str] = []
    while j < len(text):
        ch = text[j]
        if ch == "\\" and j + 1 < len(text):
            out.append(ch)
            out.append(text[j + 1])
            j += 2
            continue
        if ch == open_ch:
            depth += 1
            out.append(ch)
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return "".join(out), j + 1
            out.append(ch)
        else:
            out.append(ch)
        j += 1
    return "".join(out), j


def strip_comment(line: str) -> str:
    escaped = False
    for idx, ch in enumerate(line):
        if ch == "\\" and not escaped:
            escaped = True
            continue
        if ch == "%" and not escaped:
            return line[:idx]
        escaped = False
    return line


def preprocess_lines(source: str) -> list[str]:
    lines = source.splitlines()
    result: list[str] = []
    verbatim_env: str | None = None
    for line in lines:
        stripped = line.strip()
        if verbatim_env:
            result.append(line.rstrip("\n"))
            if stripped == rf"\end{{{verbatim_env}}}":
                verbatim_env = None
            continue
        begin = re.match(r"\\begin\{(lstlisting|verbatim)\}", stripped)
        if begin:
            verbatim_env = begin.group(1)
            result.append(line.rstrip("\n"))
            continue
        result.append(strip_comment(line).rstrip())
    return result


def collect_environment(lines: list[str], start: int, env: str) -> tuple[str, int]:
    body: list[str] = []
    i = start + 1
    end_marker = rf"\end{{{env}}}"
    while i < len(lines):
        if lines[i].strip() == end_marker:
            return "\n".join(body), i + 1
        body.append(lines[i])
        i += 1
    return "\n".join(body), i


def render_code_block(code: str, env: str, options: str) -> str:
    language = ""
    language_match = re.search(r"language\s*=\s*([^,\]]+)", options)
    if language_match:
        language = language_match.group(1).strip().lower()
    if env == "lstlisting" and language in {"c++", "cpp"}:
        return f'<pre class="code-block language-cpp"><code>{highlight_cpp(code)}</code></pre>'
    return f'<pre class="code-block"><code>{html.escape(code)}</code></pre>'


def parse_book_figure_command(line: str) -> tuple[str, str] | None:
    prefix = r"\bookfigure"
    if not line.startswith(prefix):
        return None
    i = skip_ws(line, len(prefix))
    if i < len(line) and line[i] == "[":
        _, i = read_group(line, i, "[", "]")
        i = skip_ws(line, i)
    if i >= len(line) or line[i] != "{":
        return None
    path, i = read_group(line, i)
    i = skip_ws(line, i)
    caption = ""
    if i < len(line) and line[i] == "{":
        caption, _ = read_group(line, i)
    return latex_plain(path), caption


def render_book_figure(
    book_dir: Path,
    assets: dict[Path, EpubAsset],
    path: str,
    caption: str,
    inline: LatexInline,
) -> str:
    source = (book_dir / path).resolve()
    asset = register_epub_asset(assets, source)
    alt = html.escape(latex_plain(caption), quote=True)
    rendered_caption = inline.convert(caption)
    return (
        '<div class="book-figure">'
        f'<img src="{html.escape(asset.href, quote=True)}" alt="{alt}" />'
        f'<p class="figure-caption">图示：{rendered_caption}</p>'
        "</div>"
    )


def register_epub_asset(assets: dict[Path, EpubAsset], source: Path) -> EpubAsset:
    if source in assets:
        return assets[source]
    if not source.exists() or not source.is_file():
        raise FileNotFoundError(source)
    suffix = source.suffix.lower()
    media_type = image_media_type(suffix)
    used_hrefs = {asset.href for asset in assets.values()}
    safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "-", source.name)
    href = f"images/{safe_name}"
    if href in used_hrefs:
        digest = hashlib.sha1(str(source).encode("utf-8")).hexdigest()[:8]
        href = f"images/{source.stem}-{digest}{suffix}"
    asset = EpubAsset(source=source, href=href, media_type=media_type)
    assets[source] = asset
    return asset


def image_media_type(suffix: str) -> str:
    mapping = {
        ".apng": "image/apng",
        ".avif": "image/avif",
        ".bmp": "image/bmp",
        ".gif": "image/gif",
        ".jpeg": "image/jpeg",
        ".jpg": "image/jpeg",
        ".png": "image/png",
        ".svg": "image/svg+xml",
        ".webp": "image/webp",
    }
    if suffix not in mapping:
        raise ValueError(f"unsupported image asset type: {suffix}")
    return mapping[suffix]


def highlight_cpp(code: str) -> str:
    highlighted = [highlight_cpp_line(line) for line in code.splitlines()]
    return "\n".join(highlighted)


def highlight_cpp_line(line: str) -> str:
    stripped = line.lstrip()
    if stripped.startswith("#"):
        leading = line[: len(line) - len(stripped)]
        return html.escape(leading) + highlight_directive_line(stripped)

    out: list[str] = []
    i = 0
    while i < len(line):
        ch = line[i]
        if ch == "/" and i + 1 < len(line) and line[i + 1] == "/":
            out.append(span("comment", line[i:]))
            break
        if ch == '"':
            token, i = collect_quoted_token(line, i, '"')
            out.append(span("string", token))
            continue
        if ch == "'":
            token, i = collect_quoted_token(line, i, "'")
            out.append(span("string", token))
            continue
        if ch.isdigit():
            token, i = collect_number_token(line, i)
            out.append(span("number", token))
            continue
        if ch == "_" or ch.isalpha():
            token, i = collect_identifier_token(line, i)
            if token in CPP_KEYWORDS:
                out.append(span("keyword", token))
            elif token in CPP_TYPES:
                out.append(span("type", token))
            else:
                out.append(html.escape(token))
            continue
        out.append(html.escape(ch))
        i += 1
    return "".join(out)


def highlight_directive_line(line: str) -> str:
    match = re.match(r"(#\s*)([A-Za-z_]\w*)", line)
    if not match:
        return span("directive", line)
    out = [span("directive", match.group(1))]
    directive = match.group(2)
    if directive in CPP_DIRECTIVES:
        out.append(span("directive", directive))
    else:
        out.append(html.escape(directive))
    rest = line[match.end() :]
    if rest:
        out.append(highlight_cpp_line(rest))
    return "".join(out)


def collect_quoted_token(line: str, start: int, quote: str) -> tuple[str, int]:
    i = start + 1
    escaped = False
    while i < len(line):
        ch = line[i]
        if ch == quote and not escaped:
            return line[start : i + 1], i + 1
        escaped = ch == "\\" and not escaped
        if ch != "\\":
            escaped = False
        i += 1
    return line[start:], len(line)


def collect_number_token(line: str, start: int) -> tuple[str, int]:
    i = start
    while i < len(line) and re.match(r"[0-9A-Fa-fxXuUlL'.]", line[i]):
        i += 1
    return line[start:i], i


def collect_identifier_token(line: str, start: int) -> tuple[str, int]:
    i = start
    while i < len(line) and (line[i] == "_" or line[i].isalnum()):
        i += 1
    return line[start:i], i


def span(css_class: str, text: str) -> str:
    return f'<span class="tok-{css_class}">{html.escape(text)}</span>'


def parse_heading(line: str) -> tuple[str, str] | None:
    for level in ("chapter", "section", "subsection", "topic"):
        prefix = "\\" + level
        if line.startswith(prefix):
            i = len(prefix)
            if i < len(line) and line[i] == "*":
                i += 1
            i = skip_ws(line, i)
            if i < len(line) and line[i] == "{":
                title, _ = read_group(line, i)
                return level, title
    return None


def render_table(source: str, inline: LatexInline, linearize: bool = False) -> str:
    cleaned = re.sub(r"\\(toprule|midrule|bottomrule|hline)", "\n", source)
    cleaned = cleaned.replace(r"\tabularnewline", r"\\")
    rows = [row.strip() for row in re.split(r"\\\\", cleaned) if row.strip()]
    parsed_rows: list[list[str]] = []
    for row in rows:
        if row.startswith(r"\end"):
            continue
        cells = [cell.strip() for cell in split_table_row(row)]
        if cells:
            parsed_rows.append(cells)

    if linearize:
        return render_linearized_table(parsed_rows, inline)

    out = ['<table class="book-table">']
    for row_index, cells in enumerate(parsed_rows):
        tag = "th" if row_index == 0 else "td"
        out.append("<tr>")
        for cell in cells:
            out.append(f"<{tag}>{inline.convert(cell)}</{tag}>")
        out.append("</tr>")
    out.append("</table>")
    return "\n".join(out)


def render_linearized_table(rows: list[list[str]], inline: LatexInline) -> str:
    if not rows:
        return ""

    out = ['<dl class="book-table-list">']
    for row in rows:
        label = row[0]
        detail = "；".join(cell for cell in row[1:] if cell)
        if not detail:
            out.append(f"<dt>{inline.convert(label)}</dt>")
            continue
        out.append(f"<dt>{inline.convert(label)}</dt>")
        out.append(f"<dd>{inline.convert(detail)}</dd>")
    out.append("</dl>")
    return "\n".join(out)


def split_table_row(row: str) -> list[str]:
    cells: list[str] = []
    current: list[str] = []
    depth = 0
    i = 0
    while i < len(row):
        ch = row[i]
        if ch == "\\" and i + 1 < len(row):
            current.append(ch)
            current.append(row[i + 1])
            i += 2
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth = max(0, depth - 1)
        if ch == "&" and depth == 0:
            cells.append("".join(current))
            current = []
        else:
            current.append(ch)
        i += 1
    cells.append("".join(current))
    return cells


def latex_plain(text: str) -> str:
    replacements = {
        r"\_": "_",
        r"\%": "%",
        r"\&": "&",
        r"\#": "#",
        r"\$": "$",
        r"\{": "{",
        r"\}": "}",
        r"\textbackslash{}": "\\",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    return text.strip()


def latex_math_plain(text: str) -> str:
    replacements = {
        r"\cdot": "×",
        r"\times": "×",
        r"\log": " log ",
        r"\ln": " ln ",
        r"\sqrt": " sqrt ",
        r"\alpha": "α",
        r"\beta": "β",
        r"\gamma": "γ",
        r"\delta": "δ",
        r"\epsilon": "ε",
        r"\theta": "θ",
        r"\Theta": "Θ",
        r"\lambda": "λ",
        r"\mu": "μ",
        r"\pi": "π",
        r"\Sigma": "Σ",
        r"\sum": "Σ",
        r"\Omega": "Ω",
        r"\omega": "ω",
        r"\infty": "∞",
        r"\leq": "≤",
        r"\le": "≤",
        r"\geq": "≥",
        r"\ge": "≥",
        r"\neq": "≠",
        r"\ne": "≠",
        r"\approx": "≈",
        r"\sim": "∼",
        r"\to": "→",
        r"\rightarrow": "→",
        r"\leftarrow": "←",
        r"\lceil": "⌈",
        r"\rceil": "⌉",
        r"\lfloor": "⌊",
        r"\rfloor": "⌋",
        r"\{": "{",
        r"\}": "}",
        r"\_": "_",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    text = re.sub(r"\\(?:left|right)\s*", "", text)
    text = re.sub(r"\\[A-Za-z@]+\*?", "", text)
    text = text.replace("{", "").replace("}", "")
    text = re.sub(r"\s+", " ", text)
    text = text.replace(" ^", "^").replace("^ ", "^")
    text = text.replace(" _", "_").replace("_ ", "_")
    return text.strip()


def collect_entries(
    book_dir: Path,
    include_cover: bool = True,
    legacy_names: bool = False,
) -> tuple[list[SourceEntry], list[PartNode]]:
    main = (book_dir / "main.tex").read_text(encoding="utf-8")
    entries: list[SourceEntry] = []
    if include_cover:
        entries.append(SourceEntry(kind="cover", title=BOOK_TITLE, nav_title="封面", output="cover.xhtml"))
    parts: list[PartNode] = []
    state = "preamble"
    current_part: PartNode | None = None
    appendix = False
    chapter_no = 0
    appendix_no = 0
    output_no = 0

    def next_output_name(prefix: str = "item") -> str:
        nonlocal output_no
        output_no += 1
        return f"{prefix}-{output_no:03d}.xhtml"

    for raw in main.splitlines():
        line = strip_comment(raw).strip()
        if line == r"\frontmatter":
            state = "frontmatter"
            continue
        if line == r"\mainmatter":
            state = "mainmatter"
            continue
        if line == r"\appendix":
            state = "appendix"
            appendix = True
            current_part = None
            continue
        if line == r"\backmatter":
            state = "backmatter"
            current_part = None
            appendix = False
            continue
        if line.startswith(r"\part"):
            title = command_first_arg(line, "part")
            ident = f"part-{len(parts) + 1}"
            output = next_output_name("part") if legacy_names else f"{ident}.xhtml"
            current_part = PartNode(ident=ident, title=title, output=output)
            parts.append(current_part)
            entries.append(SourceEntry(kind="part", title=title, nav_title=title, output=output, part_id=ident))
            continue
        if line.startswith(r"\input"):
            name = command_first_arg(line, "input")
            if not name or state == "preamble":
                continue
            source = book_dir / f"{name}.tex"
            if name == "frontmatter/title":
                continue
            if not source.exists():
                raise FileNotFoundError(source)
            title = find_source_title(source)
            label = ""
            if state == "mainmatter":
                chapter_no += 1
                label = f"第 {chapter_no} 章"
            elif appendix:
                label = f"附录 {chr(ord('A') + appendix_no)}"
                appendix_no += 1
            output = next_output_name() if legacy_names else safe_output_name(source, book_dir)
            nav_title = f"{label} {title}".strip()
            entry = SourceEntry(
                kind="chapter",
                source=source,
                title=title,
                label=label,
                chapter_no=chapter_no if state == "mainmatter" else None,
                part_id=current_part.ident if current_part else None,
                state=state,
                appendix=appendix,
                output=output,
                nav_title=nav_title,
            )
            entries.append(entry)
            if current_part is not None:
                current_part.children.append(entry)
    return entries, parts


def command_first_arg(line: str, command: str) -> str:
    prefix = "\\" + command
    if not line.startswith(prefix):
        return ""
    i = skip_ws(line, len(prefix))
    if i < len(line) and line[i] == "{":
        value, _ = read_group(line, i)
        return value
    return ""


def find_source_title(source: Path) -> str:
    for line in source.read_text(encoding="utf-8").splitlines()[:30]:
        heading = parse_heading(line.strip())
        if heading:
            return latex_plain(heading[1])
    return source.stem


def safe_output_name(source: Path, book_dir: Path) -> str:
    rel = source.relative_to(book_dir).with_suffix("")
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", "-".join(rel.parts)) + ".xhtml"


def display_title(title: str, fallback: str) -> str:
    if not EPUB_ASCII_TITLES:
        return title
    return fallback


def xhtml_page(title: str, body: str, legacy_markup: bool = False, inline_css: str = "", file_title: str = "") -> str:
    page_title = display_title(title, file_title or "item")
    doctype = "" if legacy_markup else "<!DOCTYPE html>"
    charset_meta = (
        '<meta http-equiv="Content-Type" content="application/xhtml+xml; charset=utf-8" />'
        if legacy_markup
        else '<meta charset="utf-8" />'
    )
    stylesheet_markup = (
        f'  <style type="text/css">\n{html.escape(inline_css)}\n  </style>'
        if inline_css
        else '  <link rel="stylesheet" type="text/css" href="styles/book.css" />'
    )
    page = f'''<?xml version="1.0" encoding="utf-8"?>
{doctype}
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="{BOOK_LANG}" lang="{BOOK_LANG}">
<head>
  {charset_meta}
  <title>{html.escape(page_title)}</title>
{stylesheet_markup}
</head>
<body>
{body}
</body>
</html>
'''
    if legacy_markup:
        page = normalize_legacy_xhtml(page)
    return page


def normalize_legacy_xhtml(page: str) -> str:
    """Keep XHTML lines short for importers that use fragile line-based parsing."""
    page = page.replace("</p><p", "</p>\n<p")
    page = page.replace("</li><li", "</li>\n<li")
    page = page.replace("</dt><dd", "</dt>\n<dd")
    page = page.replace("</dd><dt", "</dd>\n<dt")
    page = page.replace("</div><div", "</div>\n<div")
    page = page.replace("</pre><p", "</pre>\n<p")
    page = page.replace("</h1><h2", "</h1>\n<h2")
    page = page.replace("</h2><p", "</h2>\n<p")
    return page


def cover_page(legacy_markup: bool = False, inline_css: str = "") -> str:
    wrapper = "div" if legacy_markup else "section"
    body = f'''
<{wrapper} class="cover">
  <h1>{html.escape(BOOK_TITLE)}</h1>
  <p class="subtitle">{html.escape(BOOK_SUBTITLE)}</p>
  <p class="author">{html.escape(BOOK_AUTHOR)}</p>
</{wrapper}>
'''
    return xhtml_page(BOOK_TITLE, body, legacy_markup=legacy_markup, inline_css=inline_css, file_title="cover")


def part_page(part: PartNode, legacy_markup: bool = False, inline_css: str = "") -> str:
    wrapper = "div" if legacy_markup else "section"
    items = "\n".join(
        f'<li><a href="{html.escape(child.output, quote=True)}">{html.escape(child.nav_title)}</a></li>'
        for child in part.children
    )
    body = f'''
<{wrapper} class="part-page">
  <p class="part-label">部分</p>
  <h1>{html.escape(part.title)}</h1>
  <ol>{items}</ol>
</{wrapper}>
'''
    return xhtml_page(part.title, body, legacy_markup=legacy_markup, inline_css=inline_css, file_title=part.output)


def convert_source(entry: SourceEntry, legacy_markup: bool = False, inline_css: str = "") -> str:
    assert entry.source is not None
    book_dir = find_book_dir(entry.source)
    source = expand_source_inputs(entry.source, book_dir)
    converter = LatexBlockConverter(
        LatexInline(),
        heading_prefix=entry.label,
        chapter_no=entry.chapter_no,
        linearize_tables=EPUB_LINEARIZE_TABLES,
        legacy_markup=legacy_markup,
        book_dir=book_dir,
        assets=EPUB_ASSETS,
    )
    body = converter.convert(source)
    title = entry.nav_title or entry.title
    if legacy_markup and entry.kind == "chapter" and entry.label and "<h1>" not in body:
        body = f"<h1>{LatexInline().convert(title)}</h1>\n{body}"
    return xhtml_page(title, body, legacy_markup=legacy_markup, inline_css=inline_css, file_title=entry.output)


def find_book_dir(source: Path) -> Path:
    for candidate in (source.parent, *source.parents):
        if (candidate / "main.tex").exists():
            return candidate
    raise FileNotFoundError(f"cannot find LaTeX book root for {source}")


def expand_source_inputs(source: Path, book_dir: Path, seen: set[Path] | None = None) -> str:
    r"""Inline chapter-local ``\input`` files before EPUB conversion.

    The PDF build and text counter already let LaTeX recursively read files.
    EPUB generation converts each top-level chapter file independently, so
    chapter-local supplements must be expanded here to keep phone and PDF
    editions consistent while preserving the coarse navigation.
    """
    seen = set() if seen is None else seen
    resolved = source.resolve()
    if resolved in seen:
        return ""
    seen.add(resolved)

    expanded: list[str] = []
    for raw in source.read_text(encoding="utf-8").splitlines():
        line = strip_comment(raw).strip()
        if line.startswith(r"\input"):
            name = command_first_arg(line, "input")
            child = book_dir / f"{name}.tex"
            if child.exists():
                expanded.append(expand_source_inputs(child, book_dir, seen))
                continue
        expanded.append(raw)
    return "\n".join(expanded)


def build_nav(entries: list[SourceEntry], parts: list[PartNode]) -> str:
    front = [e for e in entries if e.kind == "chapter" and e.state == "frontmatter"]
    appendices = [e for e in entries if e.kind == "chapter" and e.state == "appendix"]
    back = [e for e in entries if e.kind == "chapter" and e.state == "backmatter"]

    lines = ["<ol>"]
    if any(e.kind == "cover" for e in entries):
        lines.append('<li><a href="cover.xhtml">封面</a></li>')
    for entry in front:
        lines.append(f'<li><a href="{html.escape(entry.output, quote=True)}">{html.escape(entry.nav_title)}</a></li>')
    for part in parts:
        lines.append(f'<li><a href="{html.escape(part.output, quote=True)}">{html.escape(part.title)}</a><ol>')
        for child in part.children:
            lines.append(f'<li><a href="{html.escape(child.output, quote=True)}">{html.escape(child.nav_title)}</a></li>')
        lines.append("</ol></li>")
    if appendices:
        lines.append("<li>附录<ol>")
        for entry in appendices:
            lines.append(f'<li><a href="{html.escape(entry.output, quote=True)}">{html.escape(entry.nav_title)}</a></li>')
        lines.append("</ol></li>")
    for entry in back:
        lines.append(f'<li><a href="{html.escape(entry.output, quote=True)}">{html.escape(entry.nav_title)}</a></li>')
    lines.append("</ol>")

    body = f'''
<nav epub:type="toc" id="toc">
  <h1>目录</h1>
  {"".join(lines)}
</nav>
'''
    return f'''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang="{BOOK_LANG}" lang="{BOOK_LANG}">
<head>
  <meta charset="utf-8" />
  <title>目录</title>
  <link rel="stylesheet" type="text/css" href="styles/book.css" />
</head>
<body>
{body}
</body>
</html>
'''


def build_legacy_toc(entries: list[SourceEntry], parts: list[PartNode], inline_css: str) -> str:
    front = [e for e in entries if e.kind == "chapter" and e.state == "frontmatter"]
    appendices = [e for e in entries if e.kind == "chapter" and e.state == "appendix"]
    back = [e for e in entries if e.kind == "chapter" and e.state == "backmatter"]

    lines = ["<h1>目录</h1>", "<ol>"]
    for entry in front:
        lines.append(f'<li><a href="{html.escape(entry.output, quote=True)}">{html.escape(entry.nav_title)}</a></li>')
    for part in parts:
        lines.append(f'<li><a href="{html.escape(part.output, quote=True)}">{html.escape(part.title)}</a>')
        if part.children:
            lines.append("<ol>")
            for child in part.children:
                lines.append(f'<li><a href="{html.escape(child.output, quote=True)}">{html.escape(child.nav_title)}</a></li>')
            lines.append("</ol>")
        lines.append("</li>")
    if appendices:
        lines.append("<li>附录<ol>")
        for entry in appendices:
            lines.append(f'<li><a href="{html.escape(entry.output, quote=True)}">{html.escape(entry.nav_title)}</a></li>')
        lines.append("</ol></li>")
    for entry in back:
        lines.append(f'<li><a href="{html.escape(entry.output, quote=True)}">{html.escape(entry.nav_title)}</a></li>')
    lines.append("</ol>")
    return xhtml_page("目录", "\n".join(lines), legacy_markup=True, inline_css=inline_css, file_title="toc")


def flatten_nav(entries: list[SourceEntry]) -> list[SourceEntry]:
    return [entry for entry in entries if entry.kind in {"cover", "part", "chapter"}]


def build_ncx(entries: list[SourceEntry], uid: str) -> str:
    nav_points = []
    for idx, entry in enumerate(flatten_nav(entries), start=1):
        title = entry.nav_title or entry.title
        nav_points.append(
            f'''<navPoint id="navPoint-{idx}" playOrder="{idx}">
  <navLabel><text>{html.escape(title)}</text></navLabel>
  <content src="{html.escape(entry.output, quote=True)}"/>
</navPoint>'''
        )
    return f'''<?xml version="1.0" encoding="utf-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <head>
    <meta name="dtb:uid" content="{uid}"/>
    <meta name="dtb:depth" content="2"/>
    <meta name="dtb:totalPageCount" content="0"/>
    <meta name="dtb:maxPageNumber" content="0"/>
  </head>
  <docTitle><text>{html.escape(BOOK_IMPORT_TITLE)}</text></docTitle>
  <navMap>
    {"".join(nav_points)}
  </navMap>
</ncx>
'''


def build_opf(
    entries: list[SourceEntry],
    uid: str,
    modified: str,
    fonts: list[EmbeddedFont],
    assets: list[EpubAsset],
    epub_version: str = "3.0",
    include_css: bool = True,
    include_legacy_toc: bool = False,
    include_legacy_toc_in_spine: bool = True,
) -> str:
    manifest_items = ['<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>']
    if include_css:
        manifest_items.append('<item id="css" href="styles/book.css" media-type="text/css"/>')
    if include_legacy_toc:
        manifest_items.append('<item id="toc-html" href="toc.xhtml" media-type="application/xhtml+xml"/>')
    if epub_version == "3.0":
        manifest_items.insert(0, '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>')
    for idx, font in enumerate(fonts, start=1):
        manifest_items.append(
            f'<item id="font-{idx}" href="{html.escape(font.href, quote=True)}" media-type="{font.media_type}"/>'
        )
    for idx, asset in enumerate(assets, start=1):
        manifest_items.append(
            f'<item id="asset-{idx}" href="{html.escape(asset.href, quote=True)}" media-type="{asset.media_type}"/>'
        )
    spine_items = []
    if include_legacy_toc and include_legacy_toc_in_spine:
        spine_items.append('<itemref idref="toc-html"/>')
    first_content_href = ""
    for idx, entry in enumerate(flatten_nav(entries), start=1):
        item_id = f"item-{idx}"
        manifest_items.append(
            f'<item id="{item_id}" href="{html.escape(entry.output, quote=True)}" media-type="application/xhtml+xml"/>'
        )
        spine_items.append(f'<itemref idref="{item_id}"/>')
        if not first_content_href:
            first_content_href = entry.output

    manifest_block = "\n    ".join(manifest_items)
    spine_block = "\n    ".join(spine_items)

    if epub_version == "2.0":
        guide = ""
        if include_legacy_toc:
            guide = f'''
  <guide>
    <reference type="toc" title="目录" href="toc.xhtml"/>
    <reference type="text" title="正文" href="{html.escape(first_content_href, quote=True)}"/>
  </guide>'''
        return f'''<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="bookid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:identifier id="bookid" opf:scheme="UUID">{uid}</dc:identifier>
    <dc:title>{html.escape(BOOK_IMPORT_TITLE)}</dc:title>
    <dc:creator>{html.escape(BOOK_AUTHOR)}</dc:creator>
    <dc:language>{BOOK_LANG}</dc:language>
    <dc:date>{modified[:10]}</dc:date>
  </metadata>
  <manifest>
    {manifest_block}
  </manifest>
  <spine toc="ncx">
    {spine_block}
  </spine>{guide}
</package>
'''

    return f'''<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="bookid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">urn:uuid:{uid}</dc:identifier>
    <dc:title>{html.escape(BOOK_IMPORT_TITLE)}</dc:title>
    <dc:creator>{html.escape(BOOK_AUTHOR)}</dc:creator>
    <dc:language>{BOOK_LANG}</dc:language>
    <meta property="dcterms:modified">{modified}</meta>
  </metadata>
  <manifest>
    {manifest_block}
  </manifest>
  <spine toc="ncx">
    {spine_block}
  </spine>
</package>
'''


def stylesheet(fonts: list[EmbeddedFont]) -> str:
    mono_font_stack = (
        '"Maple Mono NL NF CN", "Maple Mono", "Source Code Pro", monospace'
        if fonts
        else 'ui-monospace, "Cascadia Mono", "Noto Sans Mono", "Liberation Mono", monospace'
    )
    font_faces = "\n".join(
        f'''@font-face {{
  font-family: "{font.family}";
  font-style: {font.style};
  font-weight: {font.weight};
  src: url("../{font.href}");
}}'''
        for font in fonts
    )
    base_css = """
body {
  color: #111;
  background: #fff;
  font-family: serif;
  line-height: 1.72;
  margin: 0;
  padding: 0 0.8em;
}
h1, h2, h3 {
  line-height: 1.25;
  margin: 1.2em 0 0.45em;
}
h1 {
  font-size: 1.65em;
  border-bottom: 1px solid #d6d6d6;
  padding-bottom: 0.25em;
}
h2 {
  font-size: 1.22em;
}
.section-number {
  color: #2a6f97;
  margin-right: 0.35em;
}
h3 {
  font-size: 1.05em;
}
p {
  margin: 0.62em 0;
  text-indent: 2em;
}
h1 + p,
h2 + p,
h3 + p {
  text-indent: 2em;
}
code {
  font-family: __MONO_FONT_STACK__;
  font-size: 0.95em;
  background: #f1f3f5;
  color: #174e70;
  padding: 0.04em 0.2em;
  border-radius: 2px;
}
pre {
  font-family: __MONO_FONT_STACK__;
  font-size: 0.92em;
  line-height: 1.52;
  white-space: pre;
  overflow-x: auto;
  overflow-y: hidden;
  tab-size: 4;
  background: #f7f8fa;
  color: #111111;
  border: 1px solid #c9d1d9;
  border-left: 0.6em solid #eef1f5;
  border-radius: 3px;
  margin: 0.9em 0;
  padding: 0.78em 0.82em;
}
pre code {
  background: transparent;
  color: inherit;
  padding: 0;
  border-radius: 0;
}
.tok-keyword {
  color: #0033b3;
  font-weight: 700;
}
.tok-type {
  color: #067d7a;
}
.tok-string {
  color: #067d17;
}
.tok-comment {
  color: #8c8c8c;
  font-style: italic;
}
.tok-directive {
  color: #871094;
}
.tok-number {
  color: #1750eb;
}
ul, ol {
  margin: 0.55em 0 0.75em 1.4em;
  padding: 0;
}
li {
  margin: 0.25em 0;
}
.box {
  border-left: 3px solid #c8cdd2;
  background: #fafaf8;
  margin: 1em 0;
  padding: 0.45em 0.8em;
}
.box p {
  text-indent: 0;
}
.box-title {
  font-weight: bold;
  margin-top: 0;
}
.book-table {
  width: 100%;
  border-collapse: collapse;
  margin: 1em 0;
  font-size: 0.9em;
}
.book-table th,
.book-table td {
  border-top: 1px solid #d8d8d8;
  padding: 0.35em 0.4em;
  vertical-align: top;
}
.book-table th {
  font-weight: bold;
  background: #f7f7f5;
}
.book-table-list {
  margin: 0.85em 0;
  padding: 0;
}
.book-table-list dt {
  font-weight: 700;
  color: #174e70;
  margin: 0.55em 0 0.12em;
}
.book-table-list dd {
  margin: 0 0 0.5em 1.2em;
  padding: 0;
}
.book-table-list dd p {
  text-indent: 0;
}
.book-figure {
  margin: 1em 0;
  text-align: center;
  page-break-inside: avoid;
}
.book-figure img {
  display: block;
  width: auto;
  max-width: 100%;
  max-height: 22em;
  margin: 0 auto;
}
.figure-caption {
  text-indent: 0;
  text-align: left;
  color: #5b5f66;
  font-size: 0.9em;
  line-height: 1.45;
  margin: 0.45em 0 0;
}
.cover {
  min-height: 80vh;
  display: flex;
  flex-direction: column;
  justify-content: center;
  text-align: center;
}
.cover h1 {
  border: none;
  font-size: 2em;
}
.subtitle,
.author,
.part-label {
  text-indent: 0;
  text-align: center;
  color: #555;
}
.part-page h1 {
  text-align: center;
  border: none;
}
.math {
  font-family: serif;
}
a {
  color: inherit;
}
"""
    base_css = base_css.replace("__MONO_FONT_STACK__", mono_font_stack)
    return f"{font_faces}\n\n{base_css}"


def legacy_stylesheet() -> str:
    return """
body {
  line-height: 1.7;
  margin: 0;
  padding: 0 0.8em;
}
h1 {
  font-size: 1.5em;
  margin: 1.1em 0 0.6em;
}
h2, h3 {
  font-size: 1.15em;
  margin: 1em 0 0.5em;
}
p {
  margin: 0.6em 0;
  text-indent: 2em;
}
pre {
  font-family: monospace;
  font-size: 0.9em;
  line-height: 1.45;
  white-space: pre-wrap;
  margin: 0.8em 0;
}
code {
  font-family: monospace;
}
ul, ol {
  margin: 0.6em 0 0.8em 1.4em;
}
li {
  margin: 0.25em 0;
}
.box, .book-table-list {
  margin: 0.9em 0;
}
.book-figure {
  margin: 0.9em 0;
  text-align: center;
}
.book-figure img {
  width: auto;
  max-width: 100%;
  max-height: 20em;
}
.figure-caption {
  text-indent: 0;
  font-size: 0.9em;
}
.box-title {
  font-weight: bold;
  text-indent: 0;
}
""".strip()


def collect_embedded_fonts() -> list[EmbeddedFont]:
    specs = [
        ("Maple Mono NL NF CN", "Maple Mono NL NF CN", "normal", "400"),
        ("Maple Mono NL NF CN:style=Bold", "Maple Mono NL NF CN", "normal", "700"),
        ("Maple Mono NL NF CN:style=Italic", "Maple Mono NL NF CN", "italic", "400"),
    ]
    fonts: list[EmbeddedFont] = []
    seen: set[Path] = set()
    for query, family, style, weight in specs:
        path = font_path(query)
        if path is None or path in seen:
            continue
        if "MapleMono" not in path.name and "Maple" not in str(path):
            continue
        seen.add(path)
        suffix = path.suffix.lower()
        media_type = "font/ttf" if suffix == ".ttf" else "font/otf"
        fonts.append(
            EmbeddedFont(
                source=path,
                href=f"fonts/{path.name}",
                family=family,
                style=style,
                weight=weight,
                media_type=media_type,
            )
        )
    return fonts


def font_path(query: str) -> Path | None:
    try:
        result = subprocess.run(
            ["fc-match", "-f", "%{file}\n", query],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except FileNotFoundError:
        return None
    path = Path(result.stdout.strip())
    if path.exists() and path.is_file():
        return path.resolve()
    return None


def container_xml() -> str:
    return '''<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
'''


def zip_info(name: str, compress_type: int = zipfile.ZIP_DEFLATED) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
    info.compress_type = compress_type
    info.create_system = 0
    info.external_attr = 0
    return info


IMAGE_EXTENSIONS = {".apng", ".avif", ".bmp", ".gif", ".jpeg", ".jpg", ".png", ".svg", ".webp"}
IMAGE_MARKUP = re.compile(
    r"(<\s*(?:img|svg|picture|source|figure)\b|image/|data:image|background-image|cover-image)",
    re.IGNORECASE,
)
EPUB_ASSETS: dict[Path, EpubAsset] = {}


def validate_no_embedded_images(output: Path) -> None:
    bad_files: list[str] = []
    bad_markup: list[str] = []
    with zipfile.ZipFile(output) as archive:
        for name in archive.namelist():
            suffix = Path(name).suffix.lower()
            if suffix in IMAGE_EXTENSIONS:
                bad_files.append(name)
                continue
            if suffix not in {".css", ".opf", ".xhtml", ".xml"}:
                continue
            content = archive.read(name).decode("utf-8", errors="replace")
            if IMAGE_MARKUP.search(content):
                bad_markup.append(name)
    if bad_files or bad_markup:
        details = []
        if bad_files:
            details.append("image files: " + ", ".join(bad_files))
        if bad_markup:
            details.append("image markup: " + ", ".join(bad_markup))
        raise RuntimeError("EPUB contains image content; " + "; ".join(details))


def build_epub(
    book_dir: Path,
    output: Path,
    include_cover: bool = True,
    forbid_images: bool = False,
    linearize_tables: bool | None = None,
    wechat_compatible: bool = False,
    embed_fonts: bool = True,
) -> None:
    global EPUB_LINEARIZE_TABLES, WECHAT_COMPATIBLE
    if linearize_tables is not None:
        EPUB_LINEARIZE_TABLES = linearize_tables
    if wechat_compatible:
        WECHAT_COMPATIBLE = True
    EPUB_ASSETS.clear()

    entries, parts = collect_entries(book_dir, include_cover=include_cover, legacy_names=WECHAT_COMPATIBLE)
    fonts = [] if WECHAT_COMPATIBLE or not embed_fonts else collect_embedded_fonts()
    inline_css = legacy_stylesheet() if WECHAT_COMPATIBLE else ""
    modified = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    pages: dict[str, str] = {}
    if include_cover:
        pages["cover.xhtml"] = cover_page(legacy_markup=WECHAT_COMPATIBLE, inline_css=inline_css)
    if WECHAT_COMPATIBLE:
        pages["toc.xhtml"] = build_legacy_toc(entries, parts, inline_css=inline_css)
    part_by_output = {part.output: part for part in parts}
    for entry in entries:
        if entry.kind == "cover":
            continue
        if entry.kind == "part":
            pages[entry.output] = part_page(
                part_by_output[entry.output],
                legacy_markup=WECHAT_COMPATIBLE,
                inline_css=inline_css,
            )
        elif entry.kind == "chapter":
            pages[entry.output] = convert_source(entry, legacy_markup=WECHAT_COMPATIBLE, inline_css=inline_css)

    content_digest = hashlib.sha256()
    content_digest.update(BOOK_ID_SEED.encode("utf-8"))
    for name in sorted(pages):
        content_digest.update(name.encode("utf-8"))
        content_digest.update(pages[name].encode("utf-8"))
    uid = str(uuid.uuid5(uuid.NAMESPACE_URL, f"https://github.com/aoweichenn/CPU/{content_digest.hexdigest()}"))

    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        archive.writestr(zip_info("mimetype", zipfile.ZIP_STORED), "application/epub+zip")
        archive.writestr(zip_info("META-INF/container.xml"), container_xml())
        if not WECHAT_COMPATIBLE:
            archive.writestr(zip_info("OEBPS/styles/book.css"), stylesheet(fonts))
        if not WECHAT_COMPATIBLE:
            archive.writestr(zip_info("OEBPS/nav.xhtml"), build_nav(entries, parts))
        archive.writestr(zip_info("OEBPS/toc.ncx"), build_ncx(entries, uid))
        epub_version = "2.0" if WECHAT_COMPATIBLE else "3.0"
        archive.writestr(
            zip_info("OEBPS/content.opf"),
            build_opf(
                entries,
                uid,
                modified,
                fonts,
                list(EPUB_ASSETS.values()),
                epub_version=epub_version,
                include_css=not WECHAT_COMPATIBLE,
                include_legacy_toc=WECHAT_COMPATIBLE,
                include_legacy_toc_in_spine=not WECHAT_COMPATIBLE,
            ),
        )
        for font in fonts:
            archive.writestr(zip_info(f"OEBPS/{font.href}"), font.source.read_bytes())
        for asset in EPUB_ASSETS.values():
            archive.writestr(zip_info(f"OEBPS/{asset.href}"), asset.source.read_bytes())
        for name, content in pages.items():
            archive.writestr(zip_info(f"OEBPS/{name}"), content)
    if forbid_images:
        validate_no_embedded_images(output)


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build the CPU book EPUB.")
    parser.add_argument(
        "--book-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Path to the LaTeX book directory.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "main.epub",
        help="Output EPUB path.",
    )
    parser.add_argument(
        "--no-cover-page",
        action="store_true",
        help="Do not add a generated XHTML cover page to the EPUB spine or navigation.",
    )
    parser.add_argument(
        "--forbid-images",
        action="store_true",
        help="Fail the build if the EPUB contains image files or image markup.",
    )
    parser.add_argument(
        "--linearize-tables",
        action="store_true",
        default=EPUB_LINEARIZE_TABLES,
        help="Render LaTeX tabular environments as text definition lists instead of XHTML tables.",
    )
    parser.add_argument(
        "--wechat-compatible",
        action="store_true",
        default=WECHAT_COMPATIBLE,
        help="Generate a conservative EPUB2-style package for WeChat Reading import.",
    )
    parser.add_argument(
        "--no-embed-fonts",
        action="store_true",
        help="Do not embed local font files in the EPUB.",
    )
    return parser.parse_args(list(argv))


def main(argv: Iterable[str]) -> int:
    args = parse_args(argv)
    build_epub(
        args.book_dir.resolve(),
        args.output.resolve(),
        include_cover=not args.no_cover_page,
        forbid_images=args.forbid_images,
        linearize_tables=args.linearize_tables,
        wechat_compatible=args.wechat_compatible,
        embed_fonts=not args.no_embed_fonts,
    )
    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
