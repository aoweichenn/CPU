#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import statistics
import subprocess
from dataclasses import dataclass
from pathlib import Path


DEFAULT_PROMPT = "Hello, my name is"
DEFAULT_TOKEN_COUNTS = "4,16"
DEFAULT_ROUNDS = 3
DEFAULT_BUILD_JOBS = 2
DEFAULT_TIMEOUT_SECONDS = 900

METRIC_PREFIXES = ("benchmark_", "hotspot_")


@dataclass(frozen=True)
class HotspotSample:
    max_new_tokens: int
    round_id: int
    metrics: dict[str, float]
    generated_text: str


def script_path() -> Path:
    return Path(__file__).resolve()


def repo_dir() -> Path:
    return script_path().parents[3]


def volume_dir() -> Path:
    return script_path().parents[1]


def practice_dir() -> Path:
    return repo_dir() / "books" / "cpu-volume-3-practice"


def default_build_dir() -> Path:
    return volume_dir() / "build" / "lcqi-hotspot-profile"


def default_model_dir() -> Path:
    return Path.home() / ".cache" / "lcqi-gpt2-smoke" / "openai-community--gpt2"


def default_report_path() -> Path:
    return volume_dir() / "results" / "lcqi-gpt2-hotspot-profile.txt"


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


def ensure_success(completed: subprocess.CompletedProcess[str], action: str) -> None:
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


def build_binary(build_dir: Path, jobs: int, timeout_seconds: int) -> Path:
    configure = run_command(
        [
            "cmake",
            "-S",
            str(practice_dir()),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        cwd=repo_dir(),
        timeout_seconds=timeout_seconds,
    )
    ensure_success(configure, "configure hotspot profile build")
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
        cwd=repo_dir(),
        timeout_seconds=timeout_seconds,
    )
    ensure_success(build, "build lcqi_gpt2")
    binary = build_dir / "linux_cpu_inference" / "lcqi_gpt2"
    if not binary.exists():
        raise RuntimeError(f"lcqi_gpt2 binary not found: {binary}")
    return binary


def parse_numeric_list(value: str) -> list[int]:
    result: list[int] = []
    for part in value.split(","):
        stripped = part.strip()
        if not stripped:
            continue
        parsed = int(stripped)
        if parsed <= 0:
            raise RuntimeError("--token-counts entries must be positive")
        result.append(parsed)
    if not result:
        raise RuntimeError("--token-counts must contain at least one positive integer")
    return result


def parse_stdout(stdout: str) -> tuple[dict[str, float], str]:
    metrics: dict[str, float] = {}
    generated_text = ""
    for line in stdout.splitlines():
        key, _, value = line.partition(" ")
        if key.startswith(METRIC_PREFIXES):
            metrics[key] = float(value)
        elif key == "generated_text":
            generated_text = value
    required = {
        "benchmark_generate_ms",
        "benchmark_worker_count",
        "hotspot_total_step_ms",
        "hotspot_lm_head_pct",
        "hotspot_mlp_fc_pct",
        "hotspot_mlp_projection_pct",
        "hotspot_qkv_projection_pct",
        "hotspot_attention_pct",
    }
    missing = sorted(required - metrics.keys())
    if missing:
        raise RuntimeError(f"missing hotspot metrics: {missing}")
    if not generated_text:
        raise RuntimeError("missing generated_text")
    return metrics, generated_text


def run_sample(
    binary: Path,
    *,
    model_dir: Path,
    prompt: str,
    max_new_tokens: int,
    round_id: int,
    threads: int,
    timeout_seconds: int,
) -> HotspotSample:
    completed = run_command(
        [
            str(binary),
            "--profile-hotspots",
            "--engine",
            "cached",
            "--threads",
            str(threads),
            str(model_dir),
            prompt,
            str(max_new_tokens),
        ],
        cwd=repo_dir(),
        timeout_seconds=timeout_seconds,
    )
    ensure_success(completed, f"run hotspot profile tokens={max_new_tokens}")
    metrics, generated_text = parse_stdout(completed.stdout)
    return HotspotSample(
        max_new_tokens=max_new_tokens,
        round_id=round_id,
        metrics=metrics,
        generated_text=generated_text,
    )


def summarize(samples: list[HotspotSample]) -> list[str]:
    lines: list[str] = []
    metric_keys = [
        "benchmark_load_ms",
        "benchmark_generate_ms",
        "benchmark_generate_tokens_per_second",
        "benchmark_decode_tokens_per_second",
        "hotspot_total_step_ms",
        "benchmark_worker_count",
        "hotspot_lm_head_pct",
        "hotspot_mlp_fc_pct",
        "hotspot_mlp_projection_pct",
        "hotspot_qkv_projection_pct",
        "hotspot_attention_projection_pct",
        "hotspot_attention_pct",
    ]
    for token_count in sorted({sample.max_new_tokens for sample in samples}):
        selected = [sample for sample in samples if sample.max_new_tokens == token_count]
        lines.append(f"[summary max_new_tokens={token_count}]")
        for key in metric_keys:
            values = [sample.metrics[key] for sample in selected if key in sample.metrics]
            if not values:
                continue
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
    samples: list[HotspotSample],
    *,
    model_dir: Path,
    prompt: str,
    token_counts: list[int],
    rounds: int,
    threads: int,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "LCQI GPT-2 Hotspot Profile",
        "",
        f"timestamp_utc={dt.datetime.now(dt.UTC).isoformat()}",
        f"model_dir={model_dir}",
        f"prompt={prompt}",
        f"token_counts={','.join(str(value) for value in token_counts)}",
        f"rounds={rounds}",
        f"requested_threads={threads}",
        "engine=cached",
        "build=CMAKE_BUILD_TYPE=Release through books/cpu-volume-3-practice",
        "",
        "[samples]",
    ]
    for sample in samples:
        metric_text = ",".join(
            f"{key}:{sample.metrics[key]:.6g}"
            for key in sorted(sample.metrics)
            if key.startswith(METRIC_PREFIXES)
        )
        lines.append(
            f"round={sample.round_id},max_new_tokens={sample.max_new_tokens},"
            f"{metric_text},generated_text={sample.generated_text}"
        )
    lines.append("")
    lines.extend(summarize(samples))
    while lines and lines[-1] == "":
        lines.pop()
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run LCQI GPT-2 cached hotspot profile.")
    parser.add_argument("--model-dir", type=Path, default=default_model_dir())
    parser.add_argument("--build-dir", type=Path, default=default_build_dir())
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--token-counts", default=DEFAULT_TOKEN_COUNTS)
    parser.add_argument("--rounds", type=int, default=DEFAULT_ROUNDS)
    parser.add_argument("--threads", type=int, default=0)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.rounds <= 0:
        print("[lcqi-hotspot] --rounds must be positive")
        return 1
    if args.threads < 0:
        print("[lcqi-hotspot] --threads cannot be negative")
        return 1
    if not args.model_dir.exists():
        print(f"[lcqi-hotspot] model directory not found: {args.model_dir}")
        return 1
    token_counts = parse_numeric_list(args.token_counts)
    binary = build_binary(args.build_dir, args.jobs, args.timeout_seconds)
    samples: list[HotspotSample] = []
    for round_id in range(1, args.rounds + 1):
        for token_count in token_counts:
            sample = run_sample(
                binary,
                model_dir=args.model_dir,
                prompt=args.prompt,
                max_new_tokens=token_count,
                round_id=round_id,
                threads=args.threads,
                timeout_seconds=args.timeout_seconds,
            )
            samples.append(sample)
            print(
                f"round={round_id} max_new_tokens={token_count} "
                f"generate_ms={sample.metrics['benchmark_generate_ms']:.3f} "
                f"workers={sample.metrics['benchmark_worker_count']:.0f} "
                f"lm_head_pct={sample.metrics['hotspot_lm_head_pct']:.3f} "
                f"mlp_pct={sample.metrics['hotspot_mlp_fc_pct'] + sample.metrics['hotspot_mlp_projection_pct']:.3f}"
            )
    write_report(
        args.report,
        samples,
        model_dir=args.model_dir,
        prompt=args.prompt,
        token_counts=token_counts,
        rounds=args.rounds,
        threads=args.threads,
    )
    print(f"report={args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
