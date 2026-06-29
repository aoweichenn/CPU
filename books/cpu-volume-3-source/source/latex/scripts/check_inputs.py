from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    main_tex = root / "main.tex"
    missing: list[Path] = []

    for line in main_tex.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped.startswith("\\input{"):
            continue
        name = stripped.split("{", 1)[1].split("}", 1)[0]
        path = root / f"{name}.tex"
        if not path.exists():
            missing.append(path)

    if missing:
        print("missing input files:")
        for path in missing:
            print(path)
        return 1

    print("all input files exist")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
