#!/usr/bin/env python3
"""Build a compact EPUB directly from this volume's LaTeX sources."""

from __future__ import annotations

import argparse
import hashlib
import html
import os
import re
import uuid
import zipfile
from dataclasses import dataclass
from pathlib import Path


BOOK_TITLE = os.environ.get("BOOK_TITLE", "操作系统第一册")
BOOK_SUBTITLE = os.environ.get("BOOK_SUBTITLE", "硬件演进、内核对象与系统边界")
BOOK_AUTHOR = os.environ.get("BOOK_AUTHOR", "CPU Books")


@dataclass(frozen=True)
class Page:
    title: str
    filename: str
    body: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build the OS volume EPUB from LaTeX.")
    parser.add_argument("--book-dir", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "main.epub")
    return parser.parse_args()


def strip_comment(line: str) -> str:
    escaped = False
    for index, char in enumerate(line):
        if char == "\\" and not escaped:
            escaped = True
            continue
        if char == "%" and not escaped:
            return line[:index]
        escaped = False
    return line.rstrip()


def read_group(text: str, start: int) -> tuple[str, int]:
    if start >= len(text) or text[start] != "{":
        return "", start
    depth = 1
    out: list[str] = []
    index = start + 1
    while index < len(text):
        char = text[index]
        if char == "\\" and index + 1 < len(text):
            out.append(char)
            out.append(text[index + 1])
            index += 2
            continue
        if char == "{":
            depth += 1
            out.append(char)
        elif char == "}":
            depth -= 1
            if depth == 0:
                return "".join(out), index + 1
            out.append(char)
        else:
            out.append(char)
        index += 1
    return "".join(out), index


def first_arg(line: str, command: str) -> str:
    prefix = "\\" + command
    if not line.startswith(prefix):
        return ""
    index = len(prefix)
    while index < len(line) and line[index].isspace():
        index += 1
    if index < len(line) and line[index] == "{":
        value, _ = read_group(line, index)
        return value
    return ""


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
        r"\rightarrow": "->",
        r"\leftarrow": "<-",
        r"\times": "x",
        r"\ldots": "...",
        r"\dots": "...",
        r"\cpp": "C++",
    }
    for source, target in replacements.items():
        text = text.replace(source, target)
    text = re.sub(r"\\[A-Za-z]+\*?", "", text)
    return text.replace("{", "").replace("}", "").strip()


def split_two_args(text: str, index: int) -> tuple[str, str, int]:
    while index < len(text) and text[index].isspace():
        index += 1
    first, index = read_group(text, index)
    while index < len(text) and text[index].isspace():
        index += 1
    second, index = read_group(text, index)
    return first, second, index


def inline(text: str) -> str:
    out: list[str] = []
    index = 0
    while index < len(text):
        char = text[index]
        if char != "\\":
            if char == "$":
                end = text.find("$", index + 1)
                if end != -1:
                    out.append(f'<span class="math">{html.escape(latex_plain(text[index + 1:end]))}</span>')
                    index = end + 1
                    continue
            out.append(html.escape(char))
            index += 1
            continue

        if index + 1 < len(text) and text[index + 1] in "%&_#$":
            out.append(html.escape(text[index + 1]))
            index += 2
            continue

        match = re.match(r"\\([A-Za-z]+)\*?", text[index:])
        if match is None:
            index += 1
            continue
        name = match.group(1)
        index += len(match.group(0))
        if name in {"code", "filepath", "texttt", "nolinkurl"} and index < len(text) and text[index] == "{":
            raw, index = read_group(text, index)
            out.append(f"<code>{html.escape(latex_plain(raw))}</code>")
            continue
        if name in {"textbf", "emph", "textit"} and index < len(text) and text[index] == "{":
            raw, index = read_group(text, index)
            tag = "strong" if name == "textbf" else "em"
            out.append(f"<{tag}>{inline(raw)}</{tag}>")
            continue
        if name == "term" and index < len(text) and text[index] == "{":
            first, second, index = split_two_args(text, index)
            out.append(f"<strong>{inline(first)}</strong>（{inline(second)}）")
            continue
        if name in {"tightlist", "toprule", "midrule", "bottomrule"}:
            continue
        if index < len(text) and text[index] == "{":
            raw, index = read_group(text, index)
            out.append(inline(raw))
            continue
    return "".join(out)


def parse_heading(line: str) -> tuple[str, str] | None:
    for command in ("chapter", "section", "subsection", "subsubsection"):
        prefix = "\\" + command
        if line.startswith(prefix):
            index = len(prefix)
            if index < len(line) and line[index] == "*":
                index += 1
            while index < len(line) and line[index].isspace():
                index += 1
            if index < len(line) and line[index] == "{":
                title, _ = read_group(line, index)
                return command, title
    return None


def collect_environment(lines: list[str], start: int, env: str) -> tuple[list[str], int]:
    body: list[str] = []
    end_marker = rf"\end{{{env}}}"
    index = start + 1
    while index < len(lines):
        if lines[index].strip() == end_marker:
            return body, index + 1
        body.append(lines[index].rstrip())
        index += 1
    return body, index


def split_table_row(row: str) -> list[str]:
    cells: list[str] = []
    current: list[str] = []
    depth = 0
    index = 0
    while index < len(row):
        char = row[index]
        if char == "\\" and index + 1 < len(row):
            current.append(char)
            current.append(row[index + 1])
            index += 2
            continue
        if char == "{":
            depth += 1
        elif char == "}":
            depth = max(0, depth - 1)
        if char == "&" and depth == 0:
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(char)
        index += 1
    cells.append("".join(current).strip())
    return cells


def render_table(lines: list[str]) -> str:
    source = "\n".join(lines)
    source = re.sub(r"\\(toprule|midrule|bottomrule|hline)", "\n", source)
    rows = [row.strip() for row in re.split(r"\\\\", source) if row.strip()]
    rendered = ['<table class="book-table">']
    for row_index, row in enumerate(rows):
        if row.startswith("\\"):
            continue
        cells = split_table_row(row)
        tag = "th" if row_index == 0 else "td"
        rendered.append("<tr>")
        for cell in cells:
            rendered.append(f"<{tag}>{inline(cell)}</{tag}>")
        rendered.append("</tr>")
    rendered.append("</table>")
    return "\n".join(rendered)


def convert_latex(source: str, heading_shift: int = 0, book_dir: Path | None = None) -> str:
    lines = [strip_comment(line) for line in source.splitlines()]
    out: list[str] = []
    paragraph: list[str] = []
    list_stack: list[str] = []

    def flush_paragraph() -> None:
        if not paragraph:
            return
        text = " ".join(part.strip() for part in paragraph if part.strip())
        paragraph.clear()
        if text:
            out.append(f"<p>{inline(text)}</p>")

    def close_lists() -> None:
        while list_stack:
            out.append(f"</{list_stack.pop()}>")

    index = 0
    while index < len(lines):
        raw = lines[index]
        line = raw.strip()
        if not line:
            flush_paragraph()
            index += 1
            continue
        begin = re.match(r"\\begin\{([^}]+)\}", line)
        if begin:
            env = begin.group(1)
            if env in {"lstlisting", "verbatim"}:
                body, index = collect_environment(lines, index, env)
                flush_paragraph()
                out.append(f'<pre><code>{html.escape(chr(10).join(body))}</code></pre>')
                continue
            if env in {"longtable", "tabular"}:
                body, index = collect_environment(lines, index, env)
                flush_paragraph()
                out.append(render_table(body))
                continue
            if env in {"itemize", "enumerate"}:
                flush_paragraph()
                tag = "ul" if env == "itemize" else "ol"
                out.append(f"<{tag}>")
                list_stack.append(tag)
                index += 1
                continue
            if env in {"keyidea", "warningbox", "rubricbox"}:
                flush_paragraph()
                out.append(f'<aside class="box {env}">')
                index += 1
                continue
        end = re.match(r"\\end\{([^}]+)\}", line)
        if end:
            env = end.group(1)
            flush_paragraph()
            if env in {"itemize", "enumerate"} and list_stack:
                out.append(f"</{list_stack.pop()}>")
            elif env in {"keyidea", "warningbox", "rubricbox"}:
                out.append("</aside>")
            index += 1
            continue
        heading = parse_heading(line)
        if heading is not None:
            flush_paragraph()
            command, title = heading
            base = {"chapter": 1, "section": 2, "subsection": 3, "subsubsection": 4}[command]
            level = min(6, base + heading_shift)
            out.append(f"<h{level}>{inline(title)}</h{level}>")
            index += 1
            continue
        if line.startswith(r"\item"):
            flush_paragraph()
            content = line[len(r"\item") :].strip()
            out.append(f"<li>{inline(content)}</li>")
            index += 1
            continue
        if line.startswith(r"\lstinputlisting"):
            flush_paragraph()
            path_start = line.find("{")
            if path_start != -1 and book_dir is not None:
                path_name, _ = read_group(line, path_start)
                listing_path = book_dir / path_name
                code = listing_path.read_text(encoding="utf-8")
                out.append(f"<pre><code>{html.escape(code)}</code></pre>")
            index += 1
            continue
        if line.startswith("\\") and line in {
            r"\frontmatter",
            r"\mainmatter",
            r"\backmatter",
            r"\tableofcontents",
            r"\tightlist",
        }:
            index += 1
            continue
        if line.startswith(("\\documentclass", "\\input", "\\setcounter", "\\begin{document}", "\\end{document}")):
            index += 1
            continue
        paragraph.append(line)
        index += 1
    flush_paragraph()
    close_lists()
    return "\n".join(out)


def chapter_slug(number: int) -> str:
    return f"chapter-{number:02d}.xhtml"


def build_pages(book_dir: Path) -> list[Page]:
    main = (book_dir / "main.tex").read_text(encoding="utf-8")
    pages: list[Page] = []
    current_title = "前言"
    current_body: list[str] = []
    chapter_index = 0
    in_document = False

    def flush_page() -> None:
        nonlocal current_body
        if not current_body:
            return
        filename = "frontmatter.xhtml" if chapter_index == 0 else chapter_slug(chapter_index)
        pages.append(Page(current_title, filename, "\n".join(current_body)))
        current_body = []

    for raw in main.splitlines():
        line = strip_comment(raw).strip()
        if line == r"\begin{document}":
            in_document = True
            continue
        if not in_document:
            continue
        if line == r"\end{document}":
            break
        if line.startswith(r"\tableofcontents"):
            continue
        if line.startswith(r"\chapter{"):
            flush_page()
            title = first_arg(line, "chapter")
            chapter_index += 1
            current_title = latex_plain(title)
            current_body.append(f"<h1>{inline(title)}</h1>")
            continue
        if line.startswith(r"\inputtopic{"):
            name = first_arg(line, "inputtopic")
            path = book_dir / f"{name}.tex"
            current_body.append(convert_latex(path.read_text(encoding="utf-8"), heading_shift=1, book_dir=book_dir))
            continue
        if line.startswith(r"\input{"):
            name = first_arg(line, "input")
            if name == "frontmatter/title":
                continue
            path = book_dir / f"{name}.tex"
            current_body.append(convert_latex(path.read_text(encoding="utf-8"), heading_shift=0, book_dir=book_dir))
            continue
        if line in {r"\frontmatter", r"\mainmatter", r"\backmatter"}:
            continue
    flush_page()
    return pages


def xhtml_page(title: str, body: str) -> str:
    return f'''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN">
<head>
  <meta charset="utf-8"/>
  <title>{html.escape(title)}</title>
  <link rel="stylesheet" type="text/css" href="style.css"/>
</head>
<body>
{body}
</body>
</html>
'''


def nav_page(pages: list[Page]) -> str:
    items = "\n".join(
        f'<li><a href="{html.escape(page.filename, quote=True)}">{html.escape(page.title)}</a></li>'
        for page in pages
    )
    return xhtml_page(BOOK_TITLE, f'<nav epub:type="toc" id="toc"><h1>{html.escape(BOOK_TITLE)}</h1><ol>{items}</ol></nav>')


def content_opf(pages: list[Page], book_id: uuid.UUID) -> str:
    manifest_items = [
        '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>',
        '<item id="style" href="style.css" media-type="text/css"/>',
    ]
    spine_items: list[str] = []
    for index, page in enumerate(pages):
        item_id = f"item{index}"
        manifest_items.append(
            f'<item id="{item_id}" href="{html.escape(page.filename, quote=True)}" media-type="application/xhtml+xml"/>'
        )
        spine_items.append(f'<itemref idref="{item_id}"/>')
    return f'''<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="bookid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">urn:uuid:{book_id}</dc:identifier>
    <dc:title>{html.escape(BOOK_TITLE)}</dc:title>
    <dc:creator>{html.escape(BOOK_AUTHOR)}</dc:creator>
    <dc:language>zh-CN</dc:language>
  </metadata>
  <manifest>
    {chr(10).join(manifest_items)}
  </manifest>
  <spine>
    {chr(10).join(spine_items)}
  </spine>
</package>
'''


def write_epub(output: Path, pages: list[Page]) -> None:
    seed = hashlib.sha256("".join(page.body for page in pages).encode("utf-8")).hexdigest()
    book_id = uuid.uuid5(uuid.NAMESPACE_URL, f"os-volume-1:{seed}")
    container = '''<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
'''
    css = '''
body { font-family: sans-serif; line-height: 1.65; margin: 1.2em; color: #171717; }
h1, h2, h3, h4 { line-height: 1.25; }
code { font-family: monospace; background: #f3f4f6; padding: 0.05em 0.2em; }
pre { white-space: pre-wrap; background: #f3f4f6; padding: 0.8em; border: 1px solid #d4d4d8; }
table { border-collapse: collapse; width: 100%; margin: 1em 0; }
th, td { border: 1px solid #d4d4d8; padding: 0.35em; vertical-align: top; }
.box { border-left: 0.25em solid #334155; padding: 0.5em 0.8em; background: #f8fafc; }
'''
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        archive.writestr("META-INF/container.xml", container)
        archive.writestr("OEBPS/content.opf", content_opf(pages, book_id))
        archive.writestr("OEBPS/nav.xhtml", nav_page(pages))
        archive.writestr("OEBPS/style.css", css)
        for page in pages:
            archive.writestr(f"OEBPS/{page.filename}", xhtml_page(page.title, page.body))


def main() -> int:
    args = parse_args()
    pages = build_pages(args.book_dir.resolve())
    write_epub(args.output.resolve(), pages)
    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
