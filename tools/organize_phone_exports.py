#!/usr/bin/env python3
"""Organize phone book exports without duplicating PDF/EPUB files."""

from __future__ import annotations

import argparse
import filecmp
import shutil
from dataclasses import dataclass
from pathlib import Path


PHONE_EXPORT_ROOT = Path("/mnt/sdcard/STU/BOOKS")
PDF_EPUB_SUFFIXES = {".pdf", ".epub"}
HYPHEN_LIKE_CHARS = ("-", "‐", "‑", "‒", "–", "—", "―", "－", "−")
INDEX_FILE_NAME = "README.md"
STALE_INDEX_FILE_NAME = "目录.txt"


@dataclass(frozen=True)
class Book:
    title: str
    volume_type: str
    topics: tuple[str, ...]


BOOKS = (
    Book("Cpp从零到高级", "原理卷", ("Cpp语言与工程",)),
    Book("算法刷题与Cpp面试教材", "原理卷", ("算法与面试",)),
    Book("从Cpp到计算系统第一册", "原理卷", ("计算系统",)),
    Book("从Cpp到计算系统第二册", "原理卷", ("计算系统",)),
    Book("从Cpp到AI计算第三册", "原理卷", ("AI计算",)),
    Book("从Cpp到计算系统第一册实践卷", "实践卷", ("计算系统",)),
    Book("从Cpp到AI计算第三册实践卷", "实践卷", ("AI计算",)),
    Book("从Cpp到AI计算第三册源码卷", "代码卷", ("AI计算",)),
    Book("计算系统引擎代码实践卷", "代码卷", ("计算系统",)),
)

VOLUME_TYPES = ("原理卷", "实践卷", "代码卷", "规划卷")
TOPICS = (
    "Cpp语言与工程",
    "算法与面试",
    "计算系统",
    "AI计算",
    "计算机组成原理",
    "操作系统",
    "计算机网络",
)
PLANNED_BOOKS = ("计算机组成原理", "操作系统", "计算机网络")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=PHONE_EXPORT_ROOT)
    return parser.parse_args()


def canonical_book_dir(root: Path, book: Book) -> Path:
    return root / "按卷类型" / book.volume_type / book.title


def ensure_safe_title(title: str) -> None:
    if any(ch in title for ch in (" ", "+", "/", "\\")):
        raise SystemExit(f"unsafe export title: {title}")
    if any(ch in title for ch in HYPHEN_LIKE_CHARS):
        raise SystemExit(f"export title must not contain hyphen-like characters: {title}")


def ensure_expected_book_files(book_dir: Path, title: str) -> None:
    expected_names = {f"{title}.pdf", f"{title}.epub"}
    existing_files = [path for path in book_dir.iterdir() if path.is_file()]
    existing_names = {path.name for path in existing_files}
    missing = sorted(expected_names - existing_names)
    if missing:
        raise SystemExit(f"missing export files in {book_dir}: {', '.join(missing)}")

    for path in existing_files:
        if path.name not in expected_names:
            path.unlink()

    subdirs = [path for path in book_dir.iterdir() if path.is_dir()]
    if subdirs:
        names = ", ".join(path.name for path in subdirs)
        raise SystemExit(f"unexpected subdirectories in book export {book_dir}: {names}")

    final_files = sorted(path.name for path in book_dir.iterdir() if path.is_file())
    if final_files != sorted(expected_names):
        raise SystemExit(f"unexpected export contents in {book_dir}: {final_files}")


def compare_book_dirs(left: Path, right: Path, title: str) -> bool:
    for suffix in (".pdf", ".epub"):
        left_file = left / f"{title}{suffix}"
        right_file = right / f"{title}{suffix}"
        if not left_file.is_file() or not right_file.is_file():
            return False
        if not filecmp.cmp(left_file, right_file, shallow=False):
            return False
    return True


def move_or_merge_book(root: Path, book: Book) -> Path:
    ensure_safe_title(book.title)
    source = root / book.title
    dest = canonical_book_dir(root, book)
    dest.parent.mkdir(parents=True, exist_ok=True)

    if source.exists() and dest.exists() and source.resolve() != dest.resolve():
        ensure_expected_book_files(source, book.title)
        ensure_expected_book_files(dest, book.title)
        if not compare_book_dirs(source, dest, book.title):
            raise SystemExit(f"duplicate book dirs differ: {source} and {dest}")
        shutil.rmtree(source)
    elif source.exists() and not dest.exists():
        shutil.move(str(source), str(dest))

    if not dest.is_dir():
        raise SystemExit(f"missing phone export directory for {book.title}: {dest}")
    ensure_expected_book_files(dest, book.title)
    return dest


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def clean_index_trees(root: Path) -> None:
    for dirname in ("按内容领域",):
        path = root / dirname
        if path.exists():
            shutil.rmtree(path)
    for path in root.rglob(STALE_INDEX_FILE_NAME):
        if path.is_file():
            path.unlink()


def write_root_index(root: Path) -> None:
    lines = [
        "手机导出目录",
        "",
        "真实 PDF/EPUB 只放在：按卷类型/原理卷、按卷类型/实践卷、按卷类型/代码卷。",
        "按内容领域只放索引说明，不复制书文件，避免微信读书把同一本书识别成多本。",
        "每本书目录内只保留一个同名 PDF 和一个同名 EPUB。",
        "",
        "当前分组：",
    ]
    for volume_type in VOLUME_TYPES:
        lines.append(f"- 按卷类型/{volume_type}")
    lines.append("- 按内容领域")
    write_text(root / INDEX_FILE_NAME, "\n".join(lines) + "\n")


def write_type_indexes(root: Path) -> None:
    write_text(
        root / "按卷类型" / INDEX_FILE_NAME,
        "\n".join(
            [
                "按卷类型",
                "",
                "真实 PDF/EPUB 保存在本目录下的原理卷、实践卷、代码卷。",
                "每本书目录只保留一个同名 PDF 和一个同名 EPUB。",
            ]
        )
        + "\n",
    )

    books_by_type = {
        volume_type: [book for book in BOOKS if book.volume_type == volume_type]
        for volume_type in VOLUME_TYPES
    }
    for volume_type, books in books_by_type.items():
        type_dir = root / "按卷类型" / volume_type
        type_dir.mkdir(parents=True, exist_ok=True)
        lines = [volume_type, ""]
        if books:
            lines.append("本目录保存真实书文件：")
            for book in books:
                lines.append(f"- {book.title}/{book.title}.pdf")
                lines.append(f"- {book.title}/{book.title}.epub")
        else:
            lines.append("当前没有已导出的书。")
            if volume_type == "规划卷":
                lines.append("")
                lines.append("预留方向：")
                for title in PLANNED_BOOKS:
                    lines.append(f"- {title}")
        write_text(type_dir / INDEX_FILE_NAME, "\n".join(lines) + "\n")


def write_topic_indexes(root: Path) -> None:
    topic_root = root / "按内容领域"
    topic_root.mkdir(parents=True, exist_ok=True)
    write_text(
        topic_root / INDEX_FILE_NAME,
        "\n".join(
            [
                "按内容领域",
                "",
                "这里是索引层，不保存 PDF/EPUB。",
                "真实文件位置以每个领域目录里的路径为准。",
            ]
        )
        + "\n",
    )

    for topic in TOPICS:
        topic_books = [book for book in BOOKS if topic in book.topics]
        lines = [topic, "", "真实文件位置："]
        if topic_books:
            for book in topic_books:
                path = Path("..") / ".." / "按卷类型" / book.volume_type / book.title
                lines.append(f"- {book.title}: {path.as_posix()}")
        else:
            lines.append("- 暂未导出，目录已预留。")
        write_text(topic_root / topic / INDEX_FILE_NAME, "\n".join(lines) + "\n")


def ensure_no_duplicate_exports(root: Path) -> None:
    allowed_book_dirs = {canonical_book_dir(root, book).resolve() for book in BOOKS}
    for book in BOOKS:
        legacy_dir = root / book.title
        if legacy_dir.exists():
            raise SystemExit(f"legacy top-level book directory still exists: {legacy_dir}")

    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in PDF_EPUB_SUFFIXES:
            continue
        parent = path.parent.resolve()
        if parent not in allowed_book_dirs:
            raise SystemExit(f"unexpected PDF/EPUB outside canonical book dirs: {path}")


def main() -> int:
    args = parse_args()
    root = args.root
    root.mkdir(parents=True, exist_ok=True)

    for volume_type in VOLUME_TYPES:
        (root / "按卷类型" / volume_type).mkdir(parents=True, exist_ok=True)

    for book in BOOKS:
        move_or_merge_book(root, book)

    clean_index_trees(root)
    write_root_index(root)
    write_type_indexes(root)
    write_topic_indexes(root)
    ensure_no_duplicate_exports(root)

    print(root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
