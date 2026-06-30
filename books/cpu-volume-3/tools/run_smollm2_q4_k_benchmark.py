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

import run_smollm2_small_smoke


DEFAULT_BUILD_JOBS = 2
DEFAULT_REPEAT = 20
DEFAULT_TIMEOUT_SECONDS = 300
BENCH_TARGET = "lcqi_gguf_q4_bench"


@dataclass(frozen=True)
class CommandResult:
    name: str
    args: list[str]
    returncode: int
    stdout: str
    stderr: str
    metrics: dict[str, str]


def volume_dir() -> Path:
    return Path(__file__).resolve().parents[1]


def repo_dir() -> Path:
    return Path(__file__).resolve().parents[3]


def default_build_dir() -> Path:
    return volume_dir() / "build" / "lcqi-release"


def default_report_path() -> Path:
    return volume_dir() / "results" / "lcqi-smollm2-q4-k-benchmark.txt"


def command_line(args: list[str]) -> str:
    return " ".join(shlex.quote(arg) for arg in args)


def run_command(
    name: str,
    args: list[str],
    *,
    timeout_seconds: int,
    cwd: Path | None = None,
    env: dict[str, str] | None = None,
) -> CommandResult:
    command_env = os.environ.copy()
    if env:
        command_env.update(env)
    try:
        completed = subprocess.run(
            args,
            cwd=str(cwd) if cwd else str(repo_dir()),
            env=command_env,
            text=True,
            capture_output=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout if isinstance(exc.stdout, str) else ""
        stderr = exc.stderr if isinstance(exc.stderr, str) else ""
        return CommandResult(name, args, 124, stdout, stderr, {})
    return CommandResult(
        name=name,
        args=args,
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
        metrics=parse_key_value_metrics(completed.stdout),
    )


def ensure_success(result: CommandResult, action: str) -> None:
    if result.returncode == 0:
        return
    raise RuntimeError(
        f"{action} failed with exit code {result.returncode}\n"
        f"command: {command_line(result.args)}\n"
        f"stdout tail:\n{tail(result.stdout)}\n"
        f"stderr tail:\n{tail(result.stderr)}"
    )


def tail(text: str, max_chars: int = 4000) -> str:
    return text if len(text) <= max_chars else text[-max_chars:]


def parse_key_value_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    for line in stdout.splitlines():
        key, sep, value = line.partition("=")
        if sep:
            metrics[key.strip()] = value.strip()
    return metrics


def parse_llama_bench_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    for line in stdout.splitlines():
        stripped = line.strip()
        if not stripped.startswith("|"):
            continue
        columns = [column.strip() for column in stripped.strip("|").split("|")]
        if len(columns) != 7 or columns[0] == "model" or columns[0].startswith("-"):
            continue
        metrics["llama_model"] = columns[0]
        metrics["llama_size"] = columns[1]
        metrics["llama_params"] = columns[2]
        metrics["llama_backend"] = columns[3]
        metrics["llama_threads"] = columns[4]
        tokens_per_second = re.sub(r"\s+.*$", "", columns[6])
        if columns[5].startswith("pp"):
            metrics["llama_prefill_test"] = columns[5]
            metrics["llama_prefill_tps"] = tokens_per_second
        if columns[5].startswith("tg"):
            metrics["llama_decode_test"] = columns[5]
            metrics["llama_decode_tps"] = tokens_per_second
    return metrics


def build_lcqi(build_dir: Path, jobs: int, timeout_seconds: int) -> Path:
    configure = run_command(
        "cmake_configure",
        [
            "cmake",
            "-S",
            str(volume_dir()),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        timeout_seconds=timeout_seconds,
    )
    ensure_success(configure, "configure LCQI")
    build = run_command(
        "cmake_build_lcqi_gguf_q4_bench",
        [
            "cmake",
            "--build",
            str(build_dir),
            "--target",
            BENCH_TARGET,
            "-j",
            str(jobs),
        ],
        timeout_seconds=timeout_seconds,
    )
    ensure_success(build, "build LCQI Q4_K benchmark")
    binary = build_dir / "labs" / "linux_cpu_inference" / BENCH_TARGET
    if not binary.exists() or not os.access(binary, os.X_OK):
        raise RuntimeError(f"LCQI Q4_K benchmark binary not found: {binary}")
    return binary


def llama_bench_binary(cache_dir: Path) -> Path:
    return cache_dir / "llama.cpp" / "build" / "bin" / "llama-bench"


def model_path(cache_dir: Path) -> Path:
    return cache_dir / "models" / run_smollm2_small_smoke.MODEL_FILE_NAME


def run_lcqi_benchmark(
    binary: Path,
    model: Path,
    *,
    name: str,
    tensor: str,
    repeat: int,
    timeout_seconds: int,
    env: dict[str, str] | None = None,
) -> CommandResult:
    return run_command(
        name,
        [str(binary), str(model), tensor, str(repeat)],
        timeout_seconds=timeout_seconds,
        env=env,
    )


def run_llama_bench(cache_dir: Path, timeout_seconds: int) -> CommandResult | None:
    binary = llama_bench_binary(cache_dir)
    model = model_path(cache_dir)
    if not binary.exists() or not os.access(binary, os.X_OK) or not model.exists():
        return None
    result = run_command(
        "llama_cpp_smollm2_q4_k_m",
        [
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
        ],
        timeout_seconds=timeout_seconds,
    )
    return CommandResult(
        result.name,
        result.args,
        result.returncode,
        result.stdout,
        result.stderr,
        parse_llama_bench_metrics(result.stdout),
    )


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


def write_report(path: Path, runs: list[CommandResult], *, model: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    model_matches, actual_sha256, actual_bytes = run_smollm2_small_smoke.verify_model_file(model)
    lines = [
        "LCQI SmolLM2 Q4_K Benchmark",
        "",
        f"timestamp_utc={dt.datetime.now(dt.UTC).isoformat()}",
        f"repo_commit={current_commit()}",
        f"working_tree_dirty={working_tree_dirty()}",
        f"model={model}",
        f"model_source={run_smollm2_small_smoke.MODEL_GGUF_REPO_ID}",
        f"model_file={run_smollm2_small_smoke.MODEL_FILE_NAME}",
        f"model_expected_bytes={run_smollm2_small_smoke.MODEL_EXPECTED_BYTES}",
        f"model_actual_bytes={actual_bytes}",
        f"model_expected_sha256={run_smollm2_small_smoke.MODEL_EXPECTED_SHA256}",
        f"model_actual_sha256={actual_sha256}",
        f"model_identity_match={str(model_matches).lower()}",
        f"llama_cpp_commit={run_smollm2_small_smoke.LLAMA_CPP_COMMIT}",
        "",
        "scope=LCQI reads the same SmolLM2 Q4_K_M GGUF used by llama.cpp and runs a "
        "single real Q4_K tensor matvec slice. llama.cpp numbers are end-to-end mature "
        "engine context, not same-function microkernel parity.",
        "",
    ]
    for run in runs:
        lines.extend(
            [
                f"[{run.name}]",
                f"returncode={run.returncode}",
                f"command={command_line(run.args)}",
            ]
        )
        for key in sorted(run.metrics):
            lines.append(f"{key}={run.metrics[key]}")
        lines.extend(
            [
                "stdout_tail:",
                tail(run.stdout.strip(), 2400),
                "stderr_tail:",
                tail(run.stderr.strip(), 2400),
                "",
            ]
        )
    scalar = next((run for run in runs if run.name == "lcqi_smollm2_q4_k_scalar"), None)
    auto = next((run for run in runs if run.name == "lcqi_smollm2_q4_k_auto"), None)
    if scalar is not None and auto is not None:
        scalar_matvec = metric_as_float(scalar.metrics, "q4_q8_matvec_average_us")
        auto_matvec = metric_as_float(auto.metrics, "q4_q8_matvec_average_us")
        scalar_total = metric_as_float(scalar.metrics, "q4_q8_total_average_us")
        auto_total = metric_as_float(auto.metrics, "q4_q8_total_average_us")
        if scalar_matvec is not None and auto_matvec is not None and auto_matvec > 0.0:
            lines.append("[lcqi_auto_vs_scalar]")
            lines.append(f"q4_q8_matvec_speedup_auto_vs_scalar={scalar_matvec / auto_matvec:.3f}")
            if scalar_total is not None and auto_total is not None and auto_total > 0.0:
                lines.append(f"q4_q8_total_speedup_auto_vs_scalar={scalar_total / auto_total:.3f}")
            lines.append("")
    while lines and lines[-1] == "":
        lines.pop()
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def metric_as_float(metrics: dict[str, str], key: str) -> float | None:
    value = metrics.get(key)
    if value is None:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Benchmark LCQI Q4_K matvec on the same SmolLM2 GGUF used by llama.cpp."
    )
    parser.add_argument("--cache-dir", type=Path, default=run_smollm2_small_smoke.default_cache_dir())
    parser.add_argument("--build-dir", type=Path, default=default_build_dir())
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--tensor", default="auto")
    parser.add_argument("--repeat", type=int, default=DEFAULT_REPEAT)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--no-download", action="store_true")
    parser.add_argument("--skip-llama", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.repeat <= 0:
        print("[lcqi-smollm2-q4] --repeat must be positive", file=sys.stderr)
        return 1
    try:
        cache_dir = args.cache_dir.expanduser().resolve()
        model = model_path(cache_dir)
        run_smollm2_small_smoke.download_model(
            model,
            force_download=False,
            no_download=args.no_download,
        )
        binary = build_lcqi(args.build_dir, args.jobs, args.timeout_seconds)
        runs = [
            run_lcqi_benchmark(
                binary,
                model,
                name="lcqi_smollm2_q4_k_scalar",
                tensor=args.tensor,
                repeat=args.repeat,
                timeout_seconds=args.timeout_seconds,
                env={"LCQI_Q4K_BACKEND": "scalar"},
            ),
            run_lcqi_benchmark(
                binary,
                model,
                name="lcqi_smollm2_q4_k_auto",
                tensor=args.tensor,
                repeat=args.repeat,
                timeout_seconds=args.timeout_seconds,
            )
        ]
        if not args.skip_llama:
            llama = run_llama_bench(cache_dir, args.timeout_seconds)
            if llama is not None:
                runs.append(llama)
        write_report(args.report, runs, model=model)
    except Exception as error:
        print(f"[lcqi-smollm2-q4] error: {error}", file=sys.stderr)
        return 1

    for run in runs:
        print(f"{run.name}: returncode={run.returncode}")
        for key in sorted(run.metrics):
            print(f"  {key}={run.metrics[key]}")
    print(f"report={args.report}")
    return 0 if all(run.returncode == 0 for run in runs) else 1


if __name__ == "__main__":
    raise SystemExit(main())
