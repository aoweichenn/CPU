#!/usr/bin/env python3
"""Verify that the OS book follows the live sibling OS project."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
DEFAULT_PROJECT = REPO.parent / "os"
DEFAULT_BOOK_SPINE = (
    REPO
    / "books"
    / "operating-systems-volume-1"
    / "source"
    / "latex"
    / "frontmatter"
    / "project-spine.tex"
)
DEFAULT_SNAPSHOT = (
    Path(__file__).resolve().parents[1] / "references" / "os-project-snapshot.json"
)

ROADMAP_ROW_RE = re.compile(
    r"^\|\s*(v\d+(?:\.\d+)*)\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|",
    re.MULTILINE,
)
ROADMAP_HEADING_RE = re.compile(
    r"^#{2,6}\s+(v\d+(?:\.\d+)*)(?=\s|$)",
    re.MULTILINE,
)
README_CURRENT_RE = re.compile(
    r"当前状态：`(v\d+(?:\.\d+)*)(?:[^`]*)`\s*已完成"
)
BOOK_CURRENT_RE = re.compile(
    r"案例当前已经完成\s+v\d+(?:\.\d+)*\s+至\s+(v\d+(?:\.\d+)*)"
)
VERSION_RE = re.compile(r"\bv\d+(?:\.\d+)*\b")
CMAKE_PROJECT_VERSION_RE = re.compile(
    r"\bproject\s*\([^)]*?\bVERSION\s+(\d+(?:\.\d+)+)",
    re.DOTALL,
)

TRACKED_ROOTS = ("docs", "scripts", "source", "tests", "tools")
TRACKED_ROOT_FILES = (
    "CMakeLists.txt",
    "CMakePresets.json",
    "README.md",
)
IGNORED_PARTS = {
    "__pycache__",
    ".pytest_cache",
    "build",
}
IGNORED_SUFFIXES = {
    ".o",
    ".obj",
    ".a",
    ".so",
    ".pyc",
    ".pdf",
    ".png",
    ".jpg",
    ".jpeg",
    ".webp",
}


def version_key(version: str) -> tuple[int, ...]:
    return tuple(int(part) for part in version.removeprefix("v").split("."))


def normalized_version(version: str) -> str:
    parts = [int(part) for part in version.removeprefix("v").split(".")]
    while len(parts) > 2 and parts[-1] == 0:
        parts.pop()
    return "v" + ".".join(str(part) for part in parts)


def read_text(path: Path) -> str:
    if not path.is_file():
        raise FileNotFoundError(path)
    return path.read_text(encoding="utf-8")


def tracked_files(project: Path) -> list[Path]:
    files: set[Path] = set()
    for name in TRACKED_ROOT_FILES:
        path = project / name
        if path.is_file():
            files.add(path)

    for root_name in TRACKED_ROOTS:
        root = project / root_name
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(project)
            if any(part in IGNORED_PARTS for part in relative.parts):
                continue
            if path.suffix.lower() in IGNORED_SUFFIXES:
                continue
            files.add(path)

    return sorted(files, key=lambda path: path.relative_to(project).as_posix())


def content_fingerprint(project: Path, files: list[Path]) -> str:
    digest = hashlib.sha256()
    for path in files:
        relative = path.relative_to(project).as_posix().encode("utf-8")
        digest.update(relative)
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def file_fingerprints(project: Path, files: list[Path]) -> dict[str, str]:
    # The full aggregate digest is the publication gate. Per-file prefixes are
    # retained only to identify changed paths when that gate trips.
    return {
        path.relative_to(project).as_posix(): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()[:16]
        for path in files
    }


def git_head(project: Path) -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(project), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unavailable"


def roadmap_state(project: Path) -> tuple[list[str], list[str]]:
    roadmap = read_text(project / "docs" / "roadmap.md")
    rows = ROADMAP_ROW_RE.findall(roadmap)
    if not rows:
        raise ValueError("docs/roadmap.md 中没有可识别的版本表")

    versions = {
        version
        for version, _, _ in rows
    }
    versions.update(ROADMAP_HEADING_RE.findall(roadmap))
    completed = [
        version
        for version, _, status in rows
        if "完成" in status
    ]
    if not completed:
        raise ValueError("docs/roadmap.md 中没有已完成里程碑")

    ordered_versions = sorted(versions, key=version_key)
    completed.sort(key=version_key)
    return ordered_versions, completed


def live_state(project: Path) -> dict[str, object]:
    if not project.is_dir():
        raise FileNotFoundError(project)

    versions, completed = roadmap_state(project)
    files = tracked_files(project)
    if not files:
        raise ValueError(f"没有找到可审计项目文件：{project}")

    readme = read_text(project / "README.md")
    current_match = README_CURRENT_RE.search(readme)
    if current_match is None:
        raise ValueError("README.md 中没有可识别的当前完成版本")

    cmake = read_text(project / "CMakeLists.txt")
    declared_match = CMAKE_PROJECT_VERSION_RE.search(cmake)
    if declared_match is None:
        raise ValueError("CMakeLists.txt 中没有可识别的项目版本")

    return {
        "schema_version": 2,
        "project_relative_path": "../os",
        "git_head": git_head(project),
        "tracked_file_count": len(files),
        "content_sha256": content_fingerprint(project, files),
        "files": file_fingerprints(project, files),
        "latest_completed": completed[-1],
        "readme_completed": current_match.group(1),
        "declared_version": current_match.group(1),
        "cmake_version": normalized_version(declared_match.group(1)),
        "roadmap_versions": versions,
        "completed_versions": completed,
    }


def book_state(book_spine: Path) -> tuple[str, set[str]]:
    text = read_text(book_spine)
    current_match = BOOK_CURRENT_RE.search(text)
    if current_match is None:
        raise ValueError(f"书稿没有明确声明当前已完成版本：{book_spine}")
    return current_match.group(1), set(VERSION_RE.findall(text))


def compare_snapshot(
    live: dict[str, object],
    expected: dict[str, object],
) -> list[str]:
    errors: list[str] = []
    live_files = dict(live.get("files", {}))
    expected_files = dict(expected.get("files", {}))
    added = sorted(set(live_files) - set(expected_files))
    removed = sorted(set(expected_files) - set(live_files))
    modified = sorted(
        path
        for path in set(live_files) & set(expected_files)
        if live_files[path] != expected_files[path]
    )
    for label, paths in (
        ("新增", added),
        ("删除", removed),
        ("修改", modified),
    ):
        if not paths:
            continue
        shown = ", ".join(paths[:12])
        suffix = f"；另有 {len(paths) - 12} 个" if len(paths) > 12 else ""
        errors.append(f"项目{label}文件：{shown}{suffix}")

    for key in (
        "tracked_file_count",
        "content_sha256",
        "latest_completed",
        "declared_version",
        "cmake_version",
        "roadmap_versions",
        "completed_versions",
    ):
        if live.get(key) != expected.get(key):
            errors.append(
                f"项目快照字段 {key} 已变化："
                f"记录={expected.get(key)!r}，实时={live.get(key)!r}"
            )
    return errors


def audit(
    project: Path,
    book_spine: Path,
    snapshot_path: Path,
    *,
    compare_recorded_snapshot: bool = True,
) -> tuple[dict[str, object], list[str]]:
    live = live_state(project)
    errors: list[str] = []

    if live["latest_completed"] != live["readme_completed"]:
        errors.append(
            "项目 README 与 roadmap 的当前完成版本不一致："
            f"README={live['readme_completed']}，"
            f"roadmap={live['latest_completed']}"
        )

    for version in live["completed_versions"]:
        release = project / "docs" / "releases" / f"{version}.md"
        if not release.is_file():
            errors.append(f"已完成版本缺少发布证据：{release}")

    book_current, book_versions = book_state(book_spine)
    if book_current != live["latest_completed"]:
        errors.append(
            "书稿项目主线落后于实时项目："
            f"书稿={book_current}，项目={live['latest_completed']}"
        )

    missing_versions = [
        version
        for version in live["roadmap_versions"]
        if version not in book_versions
    ]
    if missing_versions:
        errors.append(
            "书稿项目主线缺少 roadmap 版本："
            + ", ".join(missing_versions)
        )

    if str(live["declared_version"]) not in book_versions:
        errors.append(
            "书稿项目主线没有覆盖工程当前开发版本："
            f"{live['declared_version']}"
        )

    if compare_recorded_snapshot:
        expected = json.loads(read_text(snapshot_path))
        errors.extend(compare_snapshot(live, expected))
    return live, errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--project",
        type=Path,
        default=DEFAULT_PROJECT,
        help="实时 OS 项目目录",
    )
    parser.add_argument(
        "--book-spine",
        type=Path,
        default=DEFAULT_BOOK_SPINE,
        help="书稿项目主线文件",
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        default=DEFAULT_SNAPSHOT,
        help="上次完成书稿同步后的项目快照",
    )
    parser.add_argument(
        "--emit-snapshot",
        action="store_true",
        help="输出实时项目快照，供完成书稿同步后更新记录",
    )
    parser.add_argument(
        "--book-only",
        action="store_true",
        help="只检查实时项目与书稿边界，允许在更新快照前构建书稿",
    )
    args = parser.parse_args()

    project = args.project.resolve()
    book_spine = args.book_spine.resolve()
    snapshot = args.snapshot.resolve()

    try:
        if args.emit_snapshot:
            print(
                json.dumps(
                    live_state(project),
                    ensure_ascii=False,
                    indent=2,
                )
            )
            return 0

        live, errors = audit(
            project,
            book_spine,
            snapshot,
            compare_recorded_snapshot=not args.book_only,
        )
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as error:
        print(f"OS 项目主线同步检查失败：{error}")
        return 1

    if errors:
        print("OS 项目主线已经变化，发布书稿前必须重新审计并同步：")
        for error in errors:
            print(f"- {error}")
        print(
            "同步完成后运行 --emit-snapshot，"
            "用输出更新 skill 中的快照，再重新执行本检查。"
        )
        return 1

    mode = "书稿边界有效" if args.book_only else "同步有效"
    print(
        f"OS 项目主线{mode}："
        f"完成到 {live['latest_completed']}，"
        f"CMake 元数据 {live['cmake_version']}，"
        f"审计 {live['tracked_file_count']} 个项目文件，"
        f"指纹 {str(live['content_sha256'])[:12]}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
