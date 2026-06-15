from __future__ import annotations

from pathlib import Path


REQUIRED_PATHS = [
    "README.md",
    "Makefile",
    "source/markdown/main.md",
    "source/markdown/chapters/00-preface-and-method.md",
    "source/markdown/chapters/01-three-month-plan.md",
    "source/markdown/chapters/02-complexity-and-bruteforce.md",
    "source/markdown/chapters/03-cpp-containers.md",
    "source/markdown/chapters/04-array-two-pointers-sliding-window.md",
    "source/markdown/chapters/05-hash-table-and-prefix-sum.md",
    "source/markdown/chapters/06-stack-queue-heap.md",
    "source/markdown/chapters/07-binary-search.md",
    "source/markdown/chapters/08-tree-and-graph.md",
    "source/markdown/appendices/a-leetcode-manual.md",
    "materials/problem-map.md",
    "materials/writing-standard.md",
    "reports/weekly-report-template.md",
]


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    missing = [path for path in REQUIRED_PATHS if not (root / path).exists()]
    if missing:
        print("missing required files:")
        for path in missing:
            print(path)
        return 1

    main = root / "source/markdown/main.md"
    text = main.read_text(encoding="utf-8")
    missing_links = []
    for path in REQUIRED_PATHS:
        if path.startswith("source/markdown/chapters/") or path.startswith("source/markdown/appendices/"):
            relative = path.removeprefix("source/markdown/")
            if relative not in text:
                missing_links.append(relative)

    if missing_links:
        print("main.md does not reference:")
        for path in missing_links:
            print(path)
        return 1

    print("algorithm interview book structure ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
