#!/usr/bin/env python3
"""Build a phone-friendly EPUB3 from the project's LaTeX book sources.

This is intentionally a small project-specific converter, not a general
LaTeX engine. It supports the macros and environments used by this book and
keeps the EPUB navigation at the same coarse level as the PDF table of
contents: front matter, parts, chapters, appendices, and glossary.
"""

from __future__ import annotations

import argparse
import html
import re
import subprocess
import sys
import uuid
import zipfile
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


BOOK_TITLE = "从 C++ 到机器执行：第一册"
BOOK_SUBTITLE = "底层原理、汇编接口与可信性能测量"
BOOK_AUTHOR = "CPU Performance Study"
BOOK_LANG = "zh-CN"

BOX_TITLES = {
    "keyidea": "核心思想",
    "mentalmodel": "心智模型",
    "warningbox": "常见误区",
    "labbox": "实验",
    "exercisebox": "习题与作业",
    "deepdive": "深入理解",
}

LIST_ENV = {
    "itemize": "ul",
    "enumerate": "ol",
    "description": "dl",
}


@dataclass
class SourceEntry:
    kind: str
    source: Path | None = None
    title: str = ""
    label: str = ""
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
                    out.append(f'<span class="math">{html.escape(text[i + 1:j])}</span>')
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
            "n": "\n",
            "t": "\t",
        }
        if name in no_arg:
            return html.escape(no_arg[name]), j

        one_arg = {
            "code": "code",
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
    def __init__(self, inline: LatexInline, heading_prefix: str = "") -> None:
        self.inline = inline
        self.heading_prefix = heading_prefix
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
                if env in {"lstlisting", "verbatim"}:
                    code, i = collect_environment(lines, i, env)
                    self.flush_paragraph()
                    self.out.append(f'<pre><code>{html.escape(code.rstrip())}</code></pre>')
                    continue
                if env == "tabular":
                    table, i = collect_environment(lines, i, env)
                    self.flush_paragraph()
                    self.out.append(render_table(table, self.inline))
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
                    self.out.append(f'<aside class="box {env}"><p class="box-title">{html.escape(title)}</p>')
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
                    self.out.append("</aside>")
                i += 1
                continue

            heading = parse_heading(line)
            if heading:
                level, title = heading
                self.flush_paragraph()
                if level == "chapter":
                    text = f"{self.heading_prefix} {self.inline.convert(title)}".strip()
                    self.out.append(f"<h1>{text}</h1>")
                elif level == "section":
                    self.out.append(f"<h2>{self.inline.convert(title)}</h2>")
                elif level == "subsection":
                    self.out.append(f"<h3>{self.inline.convert(title)}</h3>")
                i += 1
                continue

            if line.startswith("\\addcontentsline"):
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


def parse_heading(line: str) -> tuple[str, str] | None:
    for level in ("chapter", "section", "subsection"):
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


def render_table(source: str, inline: LatexInline) -> str:
    cleaned = re.sub(r"\\(toprule|midrule|bottomrule|hline)", "\n", source)
    cleaned = cleaned.replace(r"\tabularnewline", r"\\")
    rows = [row.strip() for row in re.split(r"\\\\", cleaned) if row.strip()]
    out = ['<table class="book-table">']
    for row_index, row in enumerate(rows):
        if row.startswith(r"\end"):
            continue
        cells = [cell.strip() for cell in split_table_row(row)]
        if not cells:
            continue
        tag = "th" if row_index == 0 else "td"
        out.append("<tr>")
        for cell in cells:
            out.append(f"<{tag}>{inline.convert(cell)}</{tag}>")
        out.append("</tr>")
    out.append("</table>")
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


def collect_entries(book_dir: Path) -> tuple[list[SourceEntry], list[PartNode]]:
    main = (book_dir / "main.tex").read_text(encoding="utf-8")
    entries: list[SourceEntry] = [
        SourceEntry(kind="cover", title=BOOK_TITLE, nav_title="封面", output="cover.xhtml")
    ]
    parts: list[PartNode] = []
    state = "preamble"
    current_part: PartNode | None = None
    appendix = False
    chapter_no = 0
    appendix_no = 0

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
            output = f"{ident}.xhtml"
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
            output = safe_output_name(source, book_dir)
            nav_title = f"{label} {title}".strip()
            entry = SourceEntry(
                kind="chapter",
                source=source,
                title=title,
                label=label,
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


def xhtml_page(title: str, body: str) -> str:
    return f'''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="{BOOK_LANG}" lang="{BOOK_LANG}">
<head>
  <meta charset="utf-8" />
  <title>{html.escape(title)}</title>
  <link rel="stylesheet" type="text/css" href="styles/book.css" />
</head>
<body>
{body}
</body>
</html>
'''


def cover_page() -> str:
    body = f'''
<section class="cover">
  <h1>{html.escape(BOOK_TITLE)}</h1>
  <p class="subtitle">{html.escape(BOOK_SUBTITLE)}</p>
  <p class="author">{html.escape(BOOK_AUTHOR)}</p>
</section>
'''
    return xhtml_page(BOOK_TITLE, body)


def part_page(part: PartNode) -> str:
    items = "\n".join(
        f'<li><a href="{html.escape(child.output, quote=True)}">{html.escape(child.nav_title)}</a></li>'
        for child in part.children
    )
    body = f'''
<section class="part-page">
  <p class="part-label">部分</p>
  <h1>{html.escape(part.title)}</h1>
  <ol>{items}</ol>
</section>
'''
    return xhtml_page(part.title, body)


def convert_source(entry: SourceEntry) -> str:
    assert entry.source is not None
    source = entry.source.read_text(encoding="utf-8")
    converter = LatexBlockConverter(LatexInline(), heading_prefix=entry.label)
    body = converter.convert(source)
    return xhtml_page(entry.nav_title or entry.title, body)


def build_nav(entries: list[SourceEntry], parts: list[PartNode]) -> str:
    front = [e for e in entries if e.kind == "chapter" and e.state == "frontmatter"]
    appendices = [e for e in entries if e.kind == "chapter" and e.state == "appendix"]
    back = [e for e in entries if e.kind == "chapter" and e.state == "backmatter"]

    lines = ['<ol>', '<li><a href="cover.xhtml">封面</a></li>']
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
  <docTitle><text>{html.escape(BOOK_TITLE)}</text></docTitle>
  <navMap>
    {"".join(nav_points)}
  </navMap>
</ncx>
'''


def build_opf(entries: list[SourceEntry], uid: str, modified: str, fonts: list[EmbeddedFont]) -> str:
    manifest_items = [
        '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>',
        '<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>',
        '<item id="css" href="styles/book.css" media-type="text/css"/>',
    ]
    for idx, font in enumerate(fonts, start=1):
        manifest_items.append(
            f'<item id="font-{idx}" href="{html.escape(font.href, quote=True)}" media-type="{font.media_type}"/>'
        )
    spine_items = []
    for idx, entry in enumerate(flatten_nav(entries), start=1):
        item_id = f"item-{idx}"
        manifest_items.append(
            f'<item id="{item_id}" href="{html.escape(entry.output, quote=True)}" media-type="application/xhtml+xml"/>'
        )
        spine_items.append(f'<itemref idref="{item_id}"/>')

    return f'''<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="bookid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">urn:uuid:{uid}</dc:identifier>
    <dc:title>{html.escape(BOOK_TITLE)}</dc:title>
    <dc:creator>{html.escape(BOOK_AUTHOR)}</dc:creator>
    <dc:language>{BOOK_LANG}</dc:language>
    <meta property="dcterms:modified">{modified}</meta>
  </metadata>
  <manifest>
    {"".join(manifest_items)}
  </manifest>
  <spine toc="ncx">
    {"".join(spine_items)}
  </spine>
</package>
'''


def stylesheet(fonts: list[EmbeddedFont]) -> str:
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
h3 {
  font-size: 1.05em;
}
p {
  margin: 0.62em 0;
  text-indent: 2em;
}
code {
  font-family: "Maple Mono NL NF CN", "Source Code Pro", monospace;
  font-size: 0.92em;
  background: #f6f6f4;
  padding: 0.04em 0.18em;
  border-radius: 2px;
}
pre {
  font-family: "Maple Mono NL NF CN", "Source Code Pro", monospace;
  font-size: 0.86em;
  line-height: 1.45;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  background: #fbfbfa;
  border-top: 1px solid #c8cdd2;
  border-bottom: 1px solid #c8cdd2;
  margin: 0.85em 0;
  padding: 0.75em 0.7em;
}
pre code {
  background: transparent;
  padding: 0;
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
    return f"{font_faces}\n\n{base_css}"


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


def build_epub(book_dir: Path, output: Path) -> None:
    entries, parts = collect_entries(book_dir)
    fonts = collect_embedded_fonts()
    uid = str(uuid.uuid5(uuid.NAMESPACE_URL, f"https://github.com/aoweichenn/CPU/{BOOK_TITLE}"))
    modified = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    pages: dict[str, str] = {"cover.xhtml": cover_page()}
    part_by_output = {part.output: part for part in parts}
    for entry in entries:
        if entry.kind == "cover":
            continue
        if entry.kind == "part":
            pages[entry.output] = part_page(part_by_output[entry.output])
        elif entry.kind == "chapter":
            pages[entry.output] = convert_source(entry)

    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        mimetype = zipfile.ZipInfo("mimetype")
        mimetype.compress_type = zipfile.ZIP_STORED
        archive.writestr(mimetype, "application/epub+zip")
        archive.writestr("META-INF/container.xml", container_xml(), compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("OEBPS/styles/book.css", stylesheet(fonts), compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("OEBPS/nav.xhtml", build_nav(entries, parts), compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("OEBPS/toc.ncx", build_ncx(entries, uid), compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("OEBPS/content.opf", build_opf(entries, uid, modified, fonts), compress_type=zipfile.ZIP_DEFLATED)
        for font in fonts:
            archive.write(font.source, f"OEBPS/{font.href}", compress_type=zipfile.ZIP_DEFLATED)
        for name, content in pages.items():
            archive.writestr(f"OEBPS/{name}", content, compress_type=zipfile.ZIP_DEFLATED)


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
    return parser.parse_args(list(argv))


def main(argv: Iterable[str]) -> int:
    args = parse_args(argv)
    build_epub(args.book_dir.resolve(), args.output.resolve())
    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
