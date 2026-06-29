#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import shutil
import statistics
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


DEFAULT_BASELINE_REF = "4febf3f"
DEFAULT_PROMPT = "Hello, my name is"
DEFAULT_MAX_NEW_TOKENS = 4
DEFAULT_ROUNDS = 3
DEFAULT_BUILD_JOBS = 2
DEFAULT_TIMEOUT_SECONDS = 300

METRIC_KEYS = [
    "benchmark_prefill_ms",
    "benchmark_decode_ms",
    "benchmark_generate_ms",
    "benchmark_generate_tokens_per_second",
    "benchmark_decode_tokens_per_second",
]


@dataclass(frozen=True)
class RunSample:
    variant: str
    engine: str
    round_id: int
    metrics: dict[str, float]
    generated_text: str


def script_path() -> Path:
    return Path(__file__).resolve()


def repo_dir() -> Path:
    return script_path().parents[3]


def volume_dir() -> Path:
    return script_path().parents[1]


def practice_dir(root: Path) -> Path:
    return root / "books" / "cpu-volume-3-practice"


def default_report_path() -> Path:
    return volume_dir() / "results" / "lcqi-gpt2-optimization-ab.txt"


def default_model_dir() -> Path:
    return Path.home() / ".cache" / "lcqi-gpt2-smoke" / "openai-community--gpt2"


def run_command(
    args: list[str],
    *,
    cwd: Path,
    timeout_seconds: int,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        cwd=cwd,
        text=True,
        capture_output=True,
        timeout=timeout_seconds,
        check=False,
    )


def ensure_success(
    completed: subprocess.CompletedProcess[str],
    action: str,
) -> None:
    if completed.returncode == 0:
        return
    command = (
        " ".join(completed.args)
        if isinstance(completed.args, list)
        else str(completed.args)
    )
    raise RuntimeError(
        f"{action} failed with exit code {completed.returncode}\n"
        f"command: {command}\n"
        f"stdout tail:\n{completed.stdout[-2000:]}\n"
        f"stderr tail:\n{completed.stderr[-2000:]}"
    )


def git_text(args: list[str]) -> str:
    completed = run_command(["git", *args], cwd=repo_dir(), timeout_seconds=60)
    ensure_success(completed, "git command")
    return completed.stdout.strip()


def working_tree_dirty() -> bool:
    completed = run_command(
        ["git", "status", "--porcelain"],
        cwd=repo_dir(),
        timeout_seconds=60,
    )
    ensure_success(completed, "git status")
    return bool(completed.stdout.strip())


def build_binary(root: Path, build_dir: Path, jobs: int, timeout_seconds: int) -> Path:
    configure = run_command(
        [
            "cmake",
            "-S",
            str(practice_dir(root)),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        cwd=root,
        timeout_seconds=timeout_seconds,
    )
    ensure_success(configure, f"configure {root}")
    build = run_command(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--target",
            "lcqi_gpt2",
            "-j",
            str(jobs),
        ],
        cwd=root,
        timeout_seconds=timeout_seconds,
    )
    ensure_success(build, f"build {root}")
    binary = build_dir / "linux_cpu_inference" / "lcqi_gpt2"
    if not binary.exists():
        raise RuntimeError(f"lcqi_gpt2 binary not found: {binary}")
    return binary


def parse_metrics(stdout: str) -> tuple[dict[str, float], str]:
    metrics: dict[str, float] = {}
    generated_text = ""
    for line in stdout.splitlines():
        key, _, value = line.partition(" ")
        if key in METRIC_KEYS:
            metrics[key] = float(value)
        elif key == "generated_text":
            generated_text = value
    missing = sorted(set(METRIC_KEYS) - metrics.keys())
    if missing:
        raise RuntimeError(f"missing benchmark metrics: {missing}")
    return metrics, generated_text


def run_sample(
    binary: Path,
    *,
    variant: str,
    engine: str,
    round_id: int,
    model_dir: Path,
    prompt: str,
    max_new_tokens: int,
    timeout_seconds: int,
) -> RunSample:
    completed = run_command(
        [
            str(binary),
            "--benchmark",
            "--engine",
            engine,
            str(model_dir),
            prompt,
            str(max_new_tokens),
        ],
        cwd=repo_dir(),
        timeout_seconds=timeout_seconds,
    )
    ensure_success(completed, f"run {variant} {engine}")
    metrics, generated_text = parse_metrics(completed.stdout)
    return RunSample(
        variant=variant,
        engine=engine,
        round_id=round_id,
        metrics=metrics,
        generated_text=generated_text,
    )


def summarize(samples: list[RunSample]) -> list[str]:
    lines: list[str] = []
    variants = sorted({sample.variant for sample in samples})
    engines = sorted({sample.engine for sample in samples})
    for variant in variants:
        for engine in engines:
            selected = [
                sample for sample in samples
                if sample.variant == variant and sample.engine == engine
            ]
            if not selected:
                continue
            lines.append(f"[{variant} {engine}]")
            for key in METRIC_KEYS:
                values = [sample.metrics[key] for sample in selected]
                lines.append(
                    f"{key}=median:{statistics.median(values):.6g},"
                    f"min:{min(values):.6g},max:{max(values):.6g}"
                )
            texts = sorted({sample.generated_text for sample in selected})
            lines.append(f"generated_text={texts[0] if len(texts) == 1 else texts}")
            lines.append("")
    return lines


def write_report(
    path: Path,
    samples: list[RunSample],
    *,
    baseline_ref: str,
    baseline_commit: str,
    current_commit: str,
    prompt: str,
    max_new_tokens: int,
    rounds: int,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "LCQI GPT-2 Optimization A/B",
        "",
        f"timestamp_utc={dt.datetime.now(dt.UTC).isoformat()}",
        f"baseline_ref={baseline_ref}",
        f"baseline_commit={baseline_commit}",
        f"current_commit={current_commit}",
        f"current_working_tree_dirty={str(working_tree_dirty()).lower()}",
        f"prompt={prompt}",
        f"max_new_tokens={max_new_tokens}",
        f"rounds={rounds}",
        "",
        "scope=Both variants are built through books/cpu-volume-3-practice with "
        "CMAKE_BUILD_TYPE=Release and run on the same GPT-2 F32 safetensors model. "
        "The llama.cpp comparison remains in lcqi-gpt2-benchmark-compare.txt; this "
        "report isolates LCQI before/after code changes.",
        "",
        "[samples]",
    ]
    for sample in samples:
        metric_text = ",".join(
            f"{key}:{sample.metrics[key]:.6g}" for key in METRIC_KEYS
        )
        lines.append(
            f"round={sample.round_id},variant={sample.variant},engine={sample.engine},"
            f"{metric_text},generated_text={sample.generated_text}"
        )
    lines.append("")
    lines.extend(summarize(samples))
    while lines and lines[-1] == "":
        lines.pop()
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run LCQI GPT-2 before/after A/B benchmark.")
    parser.add_argument("--baseline-ref", default=DEFAULT_BASELINE_REF)
    parser.add_argument("--model-dir", type=Path, default=default_model_dir())
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--max-new-tokens", type=int, default=DEFAULT_MAX_NEW_TOKENS)
    parser.add_argument("--rounds", type=int, default=DEFAULT_ROUNDS)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.rounds <= 0:
        print("[lcqi-ab] --rounds must be positive", file=sys.stderr)
        return 1
    if args.max_new_tokens < 0:
        print("[lcqi-ab] --max-new-tokens cannot be negative", file=sys.stderr)
        return 1
    if not args.model_dir.exists():
        print(f"[lcqi-ab] model directory not found: {args.model_dir}", file=sys.stderr)
        return 1

    baseline_commit = git_text(["rev-parse", "--short", args.baseline_ref])
    current_commit = git_text(["rev-parse", "--short", "HEAD"])
    with tempfile.TemporaryDirectory(prefix="lcqi-gpt2-ab-") as tmp:
        tmp_path = Path(tmp)
        baseline_root = tmp_path / "baseline"
        current_build = tmp_path / "current-build"
        baseline_build = tmp_path / "baseline-build"
        ensure_success(
            run_command(
                ["git", "worktree", "add", "--detach", str(baseline_root), args.baseline_ref],
                cwd=repo_dir(),
                timeout_seconds=args.timeout_seconds,
            ),
            "create baseline worktree",
        )
        try:
            baseline_binary = build_binary(
                baseline_root,
                baseline_build,
                args.jobs,
                args.timeout_seconds,
            )
            current_binary = build_binary(
                repo_dir(),
                current_build,
                args.jobs,
                args.timeout_seconds,
            )

            samples: list[RunSample] = []
            for round_id in range(1, args.rounds + 1):
                for variant, binary in [
                    ("baseline", baseline_binary),
                    ("current", current_binary),
                ]:
                    for engine in ["full", "cached"]:
                        sample = run_sample(
                            binary,
                            variant=variant,
                            engine=engine,
                            round_id=round_id,
                            model_dir=args.model_dir,
                            prompt=args.prompt,
                            max_new_tokens=args.max_new_tokens,
                            timeout_seconds=args.timeout_seconds,
                        )
                        samples.append(sample)
                        print(
                            f"round={round_id} variant={variant} engine={engine} "
                            f"generate_ms={sample.metrics['benchmark_generate_ms']:.3f} "
                            f"gen_tps={sample.metrics['benchmark_generate_tokens_per_second']:.5f}"
                        )
            write_report(
                args.report,
                samples,
                baseline_ref=args.baseline_ref,
                baseline_commit=baseline_commit,
                current_commit=current_commit,
                prompt=args.prompt,
                max_new_tokens=args.max_new_tokens,
                rounds=args.rounds,
            )
        finally:
            subprocess.run(
                ["git", "worktree", "remove", "--force", str(baseline_root)],
                cwd=repo_dir(),
                text=True,
                capture_output=True,
                check=False,
            )
    print(f"report={args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
