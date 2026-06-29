#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import run_gpt2_smoke


DEFAULT_PROMPT = "Hello, my name is"
DEFAULT_MAX_NEW_TOKENS = 4
DEFAULT_BUILD_JOBS = 2
DEFAULT_TIMEOUT_SECONDS = 300
DEFAULT_SMOLLM2_CACHE_DIR = Path.home() / ".cache" / "lcqi-smollm2-small-smoke"
SMOLLM2_GGUF = "SmolLM2-135M-Instruct-Q4_K_M.gguf"


@dataclass(frozen=True)
class EngineRun:
    name: str
    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    metrics: dict[str, str]


def volume_dir() -> Path:
    return Path(__file__).resolve().parents[1]


def repo_dir() -> Path:
    return Path(__file__).resolve().parents[3]


def default_report_path() -> Path:
    return volume_dir() / "results" / "lcqi-gpt2-benchmark-compare.txt"


def default_build_dir() -> Path:
    return volume_dir() / "build" / "lcqi-release"


def command_line(args: list[str]) -> str:
    return " ".join(shlex.quote(arg) for arg in args)


def tail(text: str, max_chars: int = 4000) -> str:
    return text if len(text) <= max_chars else text[-max_chars:]


def run_command(args: list[str], *, timeout_seconds: int) -> tuple[int, str, str]:
    try:
        completed = subprocess.run(
            args,
            cwd=repo_dir(),
            text=True,
            capture_output=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout if isinstance(exc.stdout, str) else ""
        stderr = exc.stderr if isinstance(exc.stderr, str) else ""
        return 124, stdout, stderr
    return completed.returncode, completed.stdout, completed.stderr


def parse_lcqi_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    for line in stdout.splitlines():
        if not line:
            continue
        key, _, value = line.partition(" ")
        if key in {
            "engine",
            "generated_text",
            "benchmark_load_ms",
            "benchmark_tokenize_ms",
            "benchmark_prefill_ms",
            "benchmark_decode_ms",
            "benchmark_generate_ms",
            "benchmark_total_ms",
            "benchmark_prompt_tokens",
            "benchmark_generated_tokens",
            "benchmark_prefill_steps",
            "benchmark_decode_steps",
            "benchmark_generate_tokens_per_second",
            "benchmark_decode_tokens_per_second",
            "benchmark_kv_cache_bytes",
        }:
            metrics[key] = value
    return metrics


def run_lcqi(
    binary: Path,
    model_dir: Path,
    *,
    engine: str,
    prompt: str,
    max_new_tokens: int,
    timeout_seconds: int,
) -> EngineRun:
    command = [
        str(binary),
        "--benchmark",
        "--engine",
        engine,
        str(model_dir),
        prompt,
        str(max_new_tokens),
    ]
    returncode, stdout, stderr = run_command(command, timeout_seconds=timeout_seconds)
    return EngineRun(
        name=f"lcqi_{engine}",
        command=command,
        returncode=returncode,
        stdout=stdout,
        stderr=stderr,
        metrics=parse_lcqi_metrics(stdout),
    )


def llama_bench_binary(cache_dir: Path) -> Path:
    return cache_dir / "llama.cpp" / "build" / "bin" / "llama-bench"


def smollm2_model_path(cache_dir: Path) -> Path:
    return cache_dir / "models" / SMOLLM2_GGUF


def parse_llama_bench_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    lines = [line.strip() for line in stdout.splitlines() if line.strip()]
    for line in lines:
        if not line.startswith("|"):
            continue
        columns = [column.strip() for column in line.strip("|").split("|")]
        if len(columns) != 7 or columns[0] == "model" or columns[0].startswith("-"):
            continue
        metrics["llama_model"] = columns[0]
        metrics["llama_size"] = columns[1]
        metrics["llama_params"] = columns[2]
        metrics["llama_backend"] = columns[3]
        metrics["llama_threads"] = columns[4]
        test_name = columns[5]
        tokens_per_second = re.sub(r"\s+.*$", "", columns[6])
        if test_name.startswith("pp"):
            metrics["llama_prefill_test"] = test_name
            metrics["llama_prefill_tps"] = tokens_per_second
        elif test_name.startswith("tg"):
            metrics["llama_decode_test"] = test_name
            metrics["llama_decode_tps"] = tokens_per_second
    return metrics


def run_llama_bench(cache_dir: Path, *, timeout_seconds: int) -> EngineRun | None:
    binary = llama_bench_binary(cache_dir)
    model = smollm2_model_path(cache_dir)
    if not binary.exists() or not os.access(binary, os.X_OK) or not model.exists():
        return None
    command = [
        str(binary),
        "-m",
        str(model),
        "-ngl",
        "0",
        "-p",
        "5",
        "-n",
        "4",
        "-r",
        "1",
    ]
    returncode, stdout, stderr = run_command(command, timeout_seconds=timeout_seconds)
    return EngineRun(
        name="llama_cpp_smollm2_q4_k_m",
        command=command,
        returncode=returncode,
        stdout=stdout,
        stderr=stderr,
        metrics=parse_llama_bench_metrics(stdout),
    )


def write_report(path: Path, runs: list[EngineRun], *, prompt: str, max_new_tokens: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "LCQI GPT-2 Benchmark Compare",
        "",
        f"timestamp_utc={dt.datetime.now(dt.UTC).isoformat()}",
        f"repo_commit={current_commit()}",
        f"working_tree_dirty={working_tree_dirty()}",
        f"python={sys.version.split()[0]}",
        f"prompt={prompt}",
        f"max_new_tokens={max_new_tokens}",
        "",
        "scope=LCQI full_prefix and cached_kv run the same GPT-2 F32 safetensors model. "
        "llama.cpp uses SmolLM2-135M-Instruct Q4_K_M as an external mature-engine reference, "
        "so its numbers are engineering context, not same-model quality ranking.",
        "",
    ]
    for run in runs:
        lines.extend(
            [
                f"[{run.name}]",
                f"returncode={run.returncode}",
                f"command={command_line(run.command)}",
            ]
        )
        for key in sorted(run.metrics):
            lines.append(f"{key}={run.metrics[key]}")
        lines.extend(
            [
                "stdout_tail:",
                tail(run.stdout.strip(), 2000),
                "stderr_tail:",
                tail(run.stderr.strip(), 2000),
                "",
            ]
        )
    while lines and lines[-1] == "":
        lines.pop()
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def current_commit() -> str:
    result = subprocess.run(
        ["git", "-C", str(repo_dir()), "rev-parse", "--short", "HEAD"],
        text=True,
        capture_output=True,
        check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def working_tree_dirty() -> str:
    result = subprocess.run(
        ["git", "-C", str(repo_dir()), "status", "--porcelain"],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        return "unknown"
    return "true" if result.stdout.strip() else "false"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare LCQI GPT-2 full-prefix and KV-cache paths, with optional llama.cpp context."
    )
    parser.add_argument("--cache-dir", type=Path, default=run_gpt2_smoke.default_cache_dir())
    parser.add_argument("--build-dir", type=Path, default=default_build_dir())
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--max-new-tokens", type=int, default=DEFAULT_MAX_NEW_TOKENS)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--no-download", action="store_true")
    parser.add_argument("--skip-llama", action="store_true")
    parser.add_argument("--smollm2-cache-dir", type=Path, default=DEFAULT_SMOLLM2_CACHE_DIR)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.max_new_tokens < 0:
        print("[lcqi-compare] --max-new-tokens cannot be negative", file=sys.stderr)
        return 1
    try:
        run_gpt2_smoke.ensure_model_files(
            args.cache_dir,
            no_download=args.no_download,
            timeout_seconds=args.timeout_seconds,
        )
        binary = run_gpt2_smoke.build_lcqi_gpt2(args.build_dir, args.jobs)
        runs = [
            run_lcqi(
                binary,
                args.cache_dir,
                engine="full",
                prompt=args.prompt,
                max_new_tokens=args.max_new_tokens,
                timeout_seconds=args.timeout_seconds,
            ),
            run_lcqi(
                binary,
                args.cache_dir,
                engine="cached",
                prompt=args.prompt,
                max_new_tokens=args.max_new_tokens,
                timeout_seconds=args.timeout_seconds,
            ),
        ]
        if not args.skip_llama:
            llama_run = run_llama_bench(
                args.smollm2_cache_dir.expanduser().resolve(),
                timeout_seconds=args.timeout_seconds,
            )
            if llama_run is not None:
                runs.append(llama_run)
        write_report(args.report, runs, prompt=args.prompt, max_new_tokens=args.max_new_tokens)
    except Exception as error:
        print(f"[lcqi-compare] error: {error}", file=sys.stderr)
        return 1

    for run in runs:
        print(f"{run.name}: returncode={run.returncode}")
        for key in sorted(run.metrics):
            print(f"  {key}={run.metrics[key]}")
    print(f"report={args.report}")
    return 0 if all(run.returncode == 0 for run in runs) else 1


if __name__ == "__main__":
    raise SystemExit(main())
