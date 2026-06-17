from pathlib import Path


def collect_inputs(root: Path, tex_file: Path, visited: set[Path], missing: list[Path]) -> None:
    if tex_file in visited:
        return
    visited.add(tex_file)

    for line in tex_file.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped.startswith("\\input{"):
            continue
        name = stripped.split("{", 1)[1].split("}", 1)[0]
        path = root / f"{name}.tex"
        if not path.exists():
            missing.append(path)
            continue
        collect_inputs(root, path, visited, missing)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    main_tex = root / "main.tex"
    markdown_source = root.parent / "markdown"
    missing: list[Path] = []
    visited: set[Path] = set()

    collect_inputs(root, main_tex, visited, missing)

    if missing:
        print("missing input files:")
        for path in missing:
            print(path)
        return 1

    if markdown_source.exists():
        print(f"formal Markdown source should not exist: {markdown_source}")
        return 1

    print(f"all {len(visited)} LaTeX input files exist")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
