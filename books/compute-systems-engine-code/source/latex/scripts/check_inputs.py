#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main.tex"
pattern = re.compile(r"\\input\{([^}]+)\}")

missing = []
for match in pattern.finditer(MAIN.read_text(encoding="utf-8")):
    target = ROOT / f"{match.group(1)}.tex"
    if not target.exists():
        missing.append(str(target.relative_to(ROOT)))

if missing:
    for item in missing:
        print(f"missing input: {item}")
    sys.exit(1)

print("all inputs exist")
