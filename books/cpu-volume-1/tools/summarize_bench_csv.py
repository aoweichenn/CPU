#!/usr/bin/env python3
"""Summarize Lab 00 benchmark CSV into a Markdown table."""

from __future__ import annotations

import csv
import statistics
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class BenchmarkKey:
    name: str
    input_size: int


def usage() -> str:
    return "Usage: summarize_bench_csv.py <foundation.csv>"


def load_groups(path: Path) -> dict[BenchmarkKey, list[int]]:
    groups: dict[BenchmarkKey, list[int]] = defaultdict(list)

    with path.open(newline="") as input_file:
        reader = csv.DictReader(input_file)
        required_columns = {"name", "input_size", "elapsed_ns"}
        missing_columns = required_columns - set(reader.fieldnames or [])
        if missing_columns:
            joined = ", ".join(sorted(missing_columns))
            raise ValueError(f"missing required CSV columns: {joined}")

        for row in reader:
            key = BenchmarkKey(
                name=row["name"],
                input_size=int(row["input_size"]),
            )
            groups[key].append(int(row["elapsed_ns"]))

    return dict(groups)


def format_float(value: float) -> str:
    return f"{value:.2f}"


def print_summary(groups: dict[BenchmarkKey, list[int]]) -> None:
    print("# Lab 00 Benchmark Summary")
    print()
    print("| benchmark | input size | samples | min ns | median ns | mean ns | max ns | ns / element |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|")

    for key in sorted(groups, key=lambda item: (item.name, item.input_size)):
        values = groups[key]
        min_ns = min(values)
        median_ns = statistics.median(values)
        mean_ns = statistics.fmean(values)
        max_ns = max(values)
        ns_per_element = median_ns / key.input_size

        print(
            f"| {key.name} "
            f"| {key.input_size} "
            f"| {len(values)} "
            f"| {min_ns} "
            f"| {format_float(median_ns)} "
            f"| {format_float(mean_ns)} "
            f"| {max_ns} "
            f"| {format_float(ns_per_element)} |"
        )


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(usage(), file=sys.stderr)
        return 1

    path = Path(argv[1])
    groups = load_groups(path)
    if not groups:
        print("CSV contains no benchmark rows.", file=sys.stderr)
        return 1

    print_summary(groups)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
