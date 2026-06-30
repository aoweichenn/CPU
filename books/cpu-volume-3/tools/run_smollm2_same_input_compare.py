#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import os
import re
import shlex
import statistics
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import run_smollm2_small_smoke


DEFAULT_PROMPT = "Hello"
DEFAULT_MAX_NEW_TOKENS = 2
DEFAULT_ROUNDS = 5
DEFAULT_BUILD_JOBS = 2
DEFAULT_TIMEOUT_SECONDS = 300
LCQI_TARGET = "lcqi_llama_gguf"
LLAMA_SIMPLE_TARGET = "llama-simple"
LLAMA_BENCH_TARGET = "llama-bench"
LLAMA_TOKENIZE_TARGET = "llama-tokenize"
LCQI_Q4_DIRECT_ENV = "LCQI_LLAMA_Q4_DIRECT"
LCQI_GGML_DIRECT_ENV = "LCQI_LLAMA_GGML_DIRECT"
LCQI_Q4_DIRECT_OFF = "0"
LCQI_GGML_DIRECT_ON = "1"
LCQI_SUMMARY_KEYS = [
    "benchmark_load_ms",
    "benchmark_prefill_ms",
    "derived_prefill_ms_per_prompt_token",
    "derived_prefill_prompt_tokens_per_second",
    "benchmark_decode_ms",
    "derived_decode_ms_per_step",
    "benchmark_decode_tokens_per_second",
    "benchmark_worker_count",
    "derived_f32_weight_inflation_ratio",
    "derived_direct_quantized_weight_byte_share",
    "benchmark_hotspot_rms_norm_ms",
    "benchmark_hotspot_attention_ms",
    "benchmark_hotspot_rope_ms",
    "benchmark_hotspot_wq_ms",
    "benchmark_hotspot_wk_ms",
    "benchmark_hotspot_wv_ms",
    "benchmark_hotspot_wo_ms",
    "benchmark_hotspot_w_gate_ms",
    "benchmark_hotspot_w_up_ms",
    "benchmark_hotspot_w_down_ms",
    "benchmark_hotspot_lm_head_ms",
    "benchmark_hotspot_q4_k_direct_ms",
    "benchmark_hotspot_ggml_direct_ms",
    "benchmark_hotspot_f32_fallback_ms",
    "benchmark_hotspot_q4_k_direct_calls",
    "benchmark_hotspot_ggml_direct_calls",
    "benchmark_hotspot_f32_fallback_calls",
    "benchmark_q4_k_direct_tensors",
    "benchmark_ggml_direct_tensors",
    "benchmark_f32_fallback_tensors",
]


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
    return volume_dir() / "results" / "lcqi-smollm2-same-input-compare-caw.txt"


def command_line(args: list[str]) -> str:
    return " ".join(shlex.quote(arg) for arg in args)


def tail(text: str, max_chars: int = 4000) -> str:
    return text if len(text) <= max_chars else text[-max_chars:]


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
        metrics={},
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


def smollm2_chat_prompt(user_prompt: str) -> str:
    return (
        "<|im_start|>system\n"
        "You are a helpful AI assistant named SmolLM, trained by Hugging Face"
        "<|im_end|>\n"
        "<|im_start|>user\n"
        f"{user_prompt}"
        "<|im_end|>\n"
        "<|im_start|>assistant\n"
    )


def escape_newlines(text: str) -> str:
    return text.replace("\\", "\\\\").replace("\n", "\\n")


def parse_int_list(value: str) -> list[int]:
    ids: list[int] = []
    for item in value.replace(",", " ").split():
        ids.append(int(item))
    return ids


def parse_lcqi_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    generated_ids: list[int] = []
    prompt_ids: list[int] = []
    for line in stdout.splitlines():
        key, sep, value = line.partition(" ")
        if not sep:
            continue
        if key in {
            "mode",
            "weight_execution",
            "architecture",
            "model_name",
            "layers",
            "hidden_size",
            "query_heads",
            "kv_heads",
            "head_dim",
            "vocab_size",
            "tie_lm_head_to_embedding",
            "predicted_first_token",
            "benchmark_manifest_ms",
            "benchmark_weight_load_ms",
            "benchmark_load_ms",
            "benchmark_prefill_ms",
            "benchmark_decode_ms",
            "benchmark_total_ms",
            "benchmark_prompt_tokens",
            "benchmark_generated_tokens",
            "benchmark_prefill_steps",
            "benchmark_decode_steps",
            "benchmark_worker_count",
            "benchmark_first_token_tokens_per_second",
            "benchmark_decode_tokens_per_second",
            "benchmark_kv_cache_bytes",
            "benchmark_quantized_weight_bytes",
            "benchmark_f32_weight_bytes",
            "benchmark_direct_quantized_weight_bytes",
            "benchmark_fallback_dequantized_weight_bytes",
            "benchmark_tensors_loaded",
            "benchmark_q4_k_direct_tensors",
            "benchmark_ggml_direct_tensors",
            "benchmark_f32_fallback_tensors",
            "benchmark_hotspot_rms_norm_ms",
            "benchmark_hotspot_attention_ms",
            "benchmark_hotspot_rope_ms",
            "benchmark_hotspot_wq_ms",
            "benchmark_hotspot_wk_ms",
            "benchmark_hotspot_wv_ms",
            "benchmark_hotspot_wo_ms",
            "benchmark_hotspot_w_gate_ms",
            "benchmark_hotspot_w_up_ms",
            "benchmark_hotspot_w_down_ms",
            "benchmark_hotspot_lm_head_ms",
            "benchmark_hotspot_q4_k_direct_ms",
            "benchmark_hotspot_ggml_direct_ms",
            "benchmark_hotspot_f32_fallback_ms",
            "benchmark_hotspot_q4_k_direct_calls",
            "benchmark_hotspot_ggml_direct_calls",
            "benchmark_hotspot_f32_fallback_calls",
        }:
            metrics[key] = value.strip()
        elif key == "prompt_ids":
            prompt_ids = parse_int_list(value)
            metrics["prompt_ids"] = " ".join(str(item) for item in prompt_ids)
            metrics["prompt_token_count_from_ids"] = str(len(prompt_ids))
        elif key == "generated_ids":
            generated_ids = parse_int_list(value)
            metrics["generated_ids"] = " ".join(str(item) for item in generated_ids)
            metrics["generated_token_count_from_ids"] = str(len(generated_ids))
    if prompt_ids and generated_ids and len(generated_ids) >= len(prompt_ids):
        new_ids = generated_ids[len(prompt_ids):]
        metrics["generated_new_ids"] = " ".join(str(item) for item in new_ids)
    add_lcqi_derived_metrics(metrics)
    return metrics


def add_lcqi_derived_metrics(metrics: dict[str, str]) -> None:
    prefill_ms = metric_as_float(metrics, "benchmark_prefill_ms")
    prompt_tokens = metric_as_float(metrics, "benchmark_prompt_tokens")
    decode_ms = metric_as_float(metrics, "benchmark_decode_ms")
    decode_steps = metric_as_float(metrics, "benchmark_decode_steps")
    quantized_bytes = metric_as_float(metrics, "benchmark_quantized_weight_bytes")
    f32_bytes = metric_as_float(metrics, "benchmark_f32_weight_bytes")
    direct_bytes = metric_as_float(metrics, "benchmark_direct_quantized_weight_bytes")
    if prefill_ms is not None and prefill_ms > 0.0 and prompt_tokens is not None:
        metrics["derived_prefill_prompt_tokens_per_second"] = (
            f"{prompt_tokens * 1000.0 / prefill_ms:.6f}"
        )
        metrics["derived_prefill_ms_per_prompt_token"] = (
            f"{prefill_ms / prompt_tokens:.6f}"
        )
    if decode_ms is not None and decode_ms > 0.0 and decode_steps is not None:
        metrics["derived_decode_steps_per_second"] = f"{decode_steps * 1000.0 / decode_ms:.6f}"
        metrics["derived_decode_ms_per_step"] = f"{decode_ms / max(decode_steps, 1.0):.6f}"
    if quantized_bytes is not None and quantized_bytes > 0.0 and f32_bytes is not None:
        metrics["derived_f32_weight_inflation_ratio"] = f"{f32_bytes / quantized_bytes:.6f}"
    if quantized_bytes is not None and quantized_bytes > 0.0 and direct_bytes is not None:
        metrics["derived_direct_quantized_weight_byte_share"] = (
            f"{direct_bytes / quantized_bytes:.6f}"
        )


def parse_llama_simple_metrics(stderr: str, stdout: str, full_prompt: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    parse_llama_perf_metrics(stderr, metrics)
    clean_stdout = strip_ansi(stdout).replace("\r", "")
    metrics["stdout_prompt_prefix_match"] = str(clean_stdout.startswith(full_prompt)).lower()
    if clean_stdout.startswith(full_prompt):
        generated = clean_stdout[len(full_prompt):].strip()
        metrics["generated_suffix"] = escape_newlines(generated)
    decoded_match = re.search(
        r"main:\s+decoded\s+(?P<count>\d+)\s+tokens\s+in\s+(?P<seconds>[0-9.]+)\s+s,"
        r"\s+speed:\s+(?P<tps>[0-9.]+)\s+t/s",
        stderr,
    )
    if decoded_match:
        metrics["llama_simple_decoded_tokens"] = decoded_match.group("count")
        metrics["llama_simple_main_seconds"] = decoded_match.group("seconds")
        metrics["llama_simple_main_tokens_per_second"] = decoded_match.group("tps")
    return metrics


def parse_llama_perf_metrics(text: str, metrics: dict[str, str]) -> None:
    load_match = re.search(
        r"llama_(?:perf_context_print|print_timings):\s+load time\s*=\s*"
        r"(?P<ms>[0-9.]+)\s*ms",
        text,
    )
    if load_match:
        metrics["llama_load_ms"] = load_match.group("ms")

    prompt_match = re.search(
        r"llama_(?:perf_context_print|print_timings):\s+prompt eval time\s*=\s*"
        r"(?P<ms>[0-9.]+)\s*ms\s*/\s*(?P<count>\d+)\s+tokens\s*"
        r"\(\s*(?P<ms_per>[0-9.]+)\s*ms per token,\s*"
        r"(?P<tps>[0-9.]+)\s*tokens per second\s*\)",
        text,
    )
    if prompt_match:
        metrics["llama_prompt_eval_ms"] = prompt_match.group("ms")
        metrics["llama_prompt_eval_tokens"] = prompt_match.group("count")
        metrics["llama_prompt_eval_ms_per_token"] = prompt_match.group("ms_per")
        metrics["llama_prompt_eval_tokens_per_second"] = prompt_match.group("tps")

    eval_match = re.search(
        r"llama_(?:perf_context_print|print_timings):\s+eval time\s*=\s*"
        r"(?P<ms>[0-9.]+)\s*ms\s*/\s*(?P<count>\d+)\s+runs\s*"
        r"\(\s*(?P<ms_per>[0-9.]+)\s*ms per token,\s*"
        r"(?P<tps>[0-9.]+)\s*tokens per second\s*\)",
        text,
    )
    if eval_match:
        metrics["llama_eval_ms"] = eval_match.group("ms")
        metrics["llama_eval_runs"] = eval_match.group("count")
        metrics["llama_eval_ms_per_run"] = eval_match.group("ms_per")
        metrics["llama_eval_tokens_per_second"] = eval_match.group("tps")

    total_match = re.search(
        r"llama_(?:perf_context_print|print_timings):\s+total time\s*=\s*"
        r"(?P<ms>[0-9.]+)\s*ms(?:\s*/\s*(?P<count>\d+)\s+tokens)?",
        text,
    )
    if total_match:
        metrics["llama_total_ms"] = total_match.group("ms")
        if total_match.group("count"):
            metrics["llama_total_tokens"] = total_match.group("count")


def parse_llama_bench_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    for line in stdout.splitlines():
        stripped = line.strip()
        if not stripped.startswith("|"):
            continue
        columns = [column.strip() for column in stripped.strip("|").split("|")]
        if len(columns) != 7 or columns[0] == "model" or columns[0].startswith("-"):
            continue
        metrics["llama_bench_model"] = columns[0]
        metrics["llama_bench_size"] = columns[1]
        metrics["llama_bench_params"] = columns[2]
        metrics["llama_bench_backend"] = columns[3]
        metrics["llama_bench_threads"] = columns[4]
        test_name = columns[5]
        tokens_per_second = re.sub(r"\s+.*$", "", columns[6])
        if test_name.startswith("pp"):
            metrics["llama_bench_prefill_test"] = test_name
            metrics["llama_bench_prefill_tokens_per_second"] = tokens_per_second
        elif test_name.startswith("tg"):
            metrics["llama_bench_decode_test"] = test_name
            metrics["llama_bench_decode_tokens_per_second"] = tokens_per_second
    return metrics


def parse_llama_tokenize_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    ids_match = re.search(r"\[(?P<ids>[0-9,\s-]+)\]", stdout)
    if ids_match:
        ids = parse_int_list(ids_match.group("ids"))
        metrics["llama_tokenize_ids"] = " ".join(str(item) for item in ids)
        metrics["llama_tokenize_token_count_from_ids"] = str(len(ids))
    count_match = re.search(r"Total number of tokens:\s*(?P<count>\d+)", stdout)
    if count_match:
        metrics["llama_tokenize_show_count"] = count_match.group("count")
    return metrics


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)


def metric_as_float(metrics: dict[str, str], key: str) -> float | None:
    value = metrics.get(key)
    if value is None:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def metric_values(runs: list[CommandResult], key: str) -> list[float]:
    values: list[float] = []
    for run in runs:
        value = metric_as_float(run.metrics, key)
        if value is not None:
            values.append(value)
    return values


def median_metric(runs: list[CommandResult], key: str) -> float | None:
    values = metric_values(runs, key)
    return statistics.median(values) if values else None


def format_summary(name: str, runs: list[CommandResult], keys: list[str]) -> list[str]:
    lines = [f"[summary:{name}]", f"rounds={len(runs)}"]
    for key in keys:
        values = metric_values(runs, key)
        if not values:
            continue
        lines.append(
            f"{key}_median={statistics.median(values):.6f} "
            f"{key}_min={min(values):.6f} "
            f"{key}_max={max(values):.6f}"
        )
    return lines


def build_lcqi(build_dir: Path, jobs: int, timeout_seconds: int) -> Path:
    configure = run_command(
        "cmake_configure_lcqi",
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
        "cmake_build_lcqi_llama_gguf",
        ["cmake", "--build", str(build_dir), "--target", LCQI_TARGET, "-j", str(jobs)],
        timeout_seconds=timeout_seconds,
    )
    ensure_success(build, "build LCQI llama GGUF target")
    binary = build_dir / "labs" / "linux_cpu_inference" / LCQI_TARGET
    if not binary.exists() or not os.access(binary, os.X_OK):
        raise RuntimeError(f"LCQI binary not found or not executable: {binary}")
    return binary


def ensure_llama_targets(
    cache_dir: Path,
    *,
    jobs: int,
    skip_build: bool,
    timeout_seconds: int,
) -> tuple[Path, Path, Path]:
    run_smollm2_small_smoke.ensure_llama_simple_chat(
        cache_dir,
        jobs=jobs,
        skip_build=skip_build,
    )
    build_dir = cache_dir / "llama.cpp" / "build"
    binaries = {
        LLAMA_SIMPLE_TARGET: build_dir / "bin" / LLAMA_SIMPLE_TARGET,
        LLAMA_BENCH_TARGET: build_dir / "bin" / LLAMA_BENCH_TARGET,
        LLAMA_TOKENIZE_TARGET: build_dir / "bin" / LLAMA_TOKENIZE_TARGET,
    }
    for target, binary in binaries.items():
        if binary.exists() and os.access(binary, os.X_OK):
            continue
        if skip_build:
            raise RuntimeError(f"{target} is missing and --skip-llama-build was set: {binary}")
        build = run_command(
            f"cmake_build_{target}",
            ["cmake", "--build", str(build_dir), "--target", target, "-j", str(jobs)],
            timeout_seconds=timeout_seconds,
        )
        ensure_success(build, f"build llama.cpp target {target}")
        if not binary.exists() or not os.access(binary, os.X_OK):
            raise RuntimeError(f"{target} was not produced: {binary}")
    return (
        binaries[LLAMA_SIMPLE_TARGET],
        binaries[LLAMA_BENCH_TARGET],
        binaries[LLAMA_TOKENIZE_TARGET],
    )


def run_lcqi_round(
    binary: Path,
    model_path: Path,
    *,
    user_prompt: str,
    max_new_tokens: int,
    round_index: int,
    timeout_seconds: int,
    name_prefix: str = "lcqi_round",
    env: dict[str, str] | None = None,
    threads: int | None = None,
) -> CommandResult:
    command = [
        str(binary),
        str(model_path),
        "--prompt",
        user_prompt,
        "--max-new",
        str(max_new_tokens),
        "--benchmark",
        "--decode-text",
    ]
    if threads is not None:
        command.extend(["--threads", str(threads)])
    result = run_command(
        f"{name_prefix}_{round_index}",
        command,
        timeout_seconds=timeout_seconds,
        env=env,
    )
    return CommandResult(
        result.name,
        result.args,
        result.returncode,
        result.stdout,
        result.stderr,
        parse_lcqi_metrics(result.stdout),
    )


def run_llama_simple_round(
    binary: Path,
    model_path: Path,
    *,
    full_prompt: str,
    max_new_tokens: int,
    round_index: int,
    timeout_seconds: int,
) -> CommandResult:
    command = [
        str(binary),
        "-m",
        str(model_path),
        "-ngl",
        "0",
        "-n",
        str(max_new_tokens),
        full_prompt,
    ]
    result = run_command(
        f"llama_simple_round_{round_index}",
        command,
        timeout_seconds=timeout_seconds,
        env={"LC_ALL": "C"},
    )
    return CommandResult(
        result.name,
        result.args,
        result.returncode,
        result.stdout,
        result.stderr,
        parse_llama_simple_metrics(result.stderr, result.stdout, full_prompt),
    )


def run_llama_tokenize(
    binary: Path,
    model_path: Path,
    *,
    full_prompt: str,
    timeout_seconds: int,
) -> CommandResult:
    command = [
        str(binary),
        "-m",
        str(model_path),
        "--ids",
        "--show-count",
        "--log-disable",
        "-p",
        full_prompt,
    ]
    result = run_command(
        "llama_tokenize_same_prompt",
        command,
        timeout_seconds=timeout_seconds,
        env={"LC_ALL": "C"},
    )
    return CommandResult(
        result.name,
        result.args,
        result.returncode,
        result.stdout,
        result.stderr,
        parse_llama_tokenize_metrics(result.stdout),
    )


def run_llama_bench(
    binary: Path,
    model_path: Path,
    *,
    prompt_tokens: int,
    max_new_tokens: int,
    rounds: int,
    timeout_seconds: int,
) -> CommandResult:
    command = [
        str(binary),
        "-m",
        str(model_path),
        "-ngl",
        "0",
        "-p",
        str(prompt_tokens),
        "-n",
        str(max_new_tokens),
        "-r",
        str(rounds),
    ]
    result = run_command(
        "llama_bench_same_token_count",
        command,
        timeout_seconds=timeout_seconds,
        env={"LC_ALL": "C"},
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


def command_output(args: list[str]) -> str:
    completed = subprocess.run(args, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        return "unknown"
    return completed.stdout.strip()


def write_report(
    report_path: Path,
    *,
    cache_dir: Path,
    model_path: Path,
    user_prompt: str,
    full_prompt: str,
    max_new_tokens: int,
    lcqi_runs: list[CommandResult],
    lcqi_serial_runs: list[CommandResult],
    lcqi_q4_direct_off_runs: list[CommandResult],
    lcqi_ggml_direct_runs: list[CommandResult],
    llama_tokenize: CommandResult,
    llama_simple_runs: list[CommandResult],
    llama_bench: CommandResult,
) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    model_matches, actual_sha256, actual_bytes = run_smollm2_small_smoke.verify_model_file(
        model_path
    )
    lines = [
        "LCQI vs llama.cpp SmolLM2 Same Input Compare",
        "",
        f"timestamp_utc={dt.datetime.now(dt.UTC).isoformat()}",
        f"repo_commit={current_commit()}",
        f"working_tree_dirty={working_tree_dirty()}",
        f"host={command_output(['hostname'])}",
        f"uname={command_output(['uname', '-a'])}",
        f"nproc={command_output(['nproc'])}",
        f"python={sys.version.split()[0]}",
        f"cache_dir={cache_dir}",
        "",
        f"model_source={run_smollm2_small_smoke.MODEL_GGUF_REPO_ID}",
        f"model_file={run_smollm2_small_smoke.MODEL_FILE_NAME}",
        f"model_path={model_path}",
        f"model_expected_bytes={run_smollm2_small_smoke.MODEL_EXPECTED_BYTES}",
        f"model_actual_bytes={actual_bytes}",
        f"model_expected_sha256={run_smollm2_small_smoke.MODEL_EXPECTED_SHA256}",
        f"model_actual_sha256={actual_sha256}",
        f"model_identity_match={str(model_matches).lower()}",
        f"llama_cpp_commit={run_smollm2_small_smoke.LLAMA_CPP_COMMIT}",
        "",
        f"user_prompt={escape_newlines(user_prompt)}",
        f"max_new_tokens={max_new_tokens}",
        f"expanded_chat_prompt={escape_newlines(full_prompt)}",
        "",
        "scope=LCQI uses its internal SmolLM2 chat-prompt builder from the same user "
        "prompt. llama-simple receives the expanded chat prompt string directly. "
        "The same-input check is prompt-token-count equality plus matching visible "
        "generated suffix; llama-bench is reported separately as same-token-count "
        "synthetic throughput, not same text.",
        "",
    ]

    if lcqi_q4_direct_off_runs:
        lines.extend(format_summary(
            "lcqi_q4_direct_off_same_input",
            lcqi_q4_direct_off_runs,
            LCQI_SUMMARY_KEYS,
        ))
        lines.append("")

    if lcqi_serial_runs:
        lines.extend(format_summary(
            "lcqi_serial_same_input",
            lcqi_serial_runs,
            LCQI_SUMMARY_KEYS,
        ))
        lines.append("")

    lines.extend(format_summary(
        "lcqi_same_input",
        lcqi_runs,
        LCQI_SUMMARY_KEYS,
    ))
    lines.append("")
    if lcqi_ggml_direct_runs:
        lines.extend(format_summary(
            "lcqi_ggml_direct_experimental_same_input",
            lcqi_ggml_direct_runs,
            LCQI_SUMMARY_KEYS,
        ))
        lines.append("")
    lines.extend(format_summary(
        "llama_simple_same_input",
        llama_simple_runs,
        [
            "llama_load_ms",
            "llama_prompt_eval_ms",
            "llama_prompt_eval_ms_per_token",
            "llama_prompt_eval_tokens_per_second",
            "llama_eval_ms",
            "llama_eval_ms_per_run",
            "llama_eval_tokens_per_second",
            "llama_total_ms",
        ],
    ))
    lines.append("")

    add_ratio_summary(
        lines,
        lcqi_runs,
        llama_tokenize,
        llama_simple_runs,
        llama_bench,
        lcqi_serial_runs,
        lcqi_q4_direct_off_runs,
        lcqi_ggml_direct_runs,
    )

    for run in (
        lcqi_q4_direct_off_runs +
        lcqi_serial_runs +
        lcqi_runs +
        lcqi_ggml_direct_runs +
        [llama_tokenize] +
        llama_simple_runs +
        [llama_bench]
    ):
        lines.extend(
            [
                "",
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
                tail(strip_ansi(run.stdout).strip(), 3000),
                "stderr_tail:",
                tail(strip_ansi(run.stderr).strip(), 3000),
            ]
        )

    while lines and lines[-1] == "":
        lines.pop()
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def add_ratio_summary(
    lines: list[str],
    lcqi_runs: list[CommandResult],
    llama_tokenize: CommandResult,
    llama_simple_runs: list[CommandResult],
    llama_bench: CommandResult,
    lcqi_serial_runs: list[CommandResult],
    lcqi_q4_direct_off_runs: list[CommandResult],
    lcqi_ggml_direct_runs: list[CommandResult],
) -> None:
    lcqi_prompt_tokens = median_metric(lcqi_runs, "benchmark_prompt_tokens")
    lcqi_prompt_ids = lcqi_runs[0].metrics.get("prompt_ids") if lcqi_runs else None
    llama_tokenize_ids = llama_tokenize.metrics.get("llama_tokenize_ids")
    llama_tokenize_count = metric_as_float(llama_tokenize.metrics, "llama_tokenize_show_count")
    llama_prompt_tokens = median_metric(llama_simple_runs, "llama_prompt_eval_tokens")
    lcqi_prefill = median_metric(lcqi_runs, "benchmark_prefill_ms")
    llama_prompt = median_metric(llama_simple_runs, "llama_prompt_eval_ms")
    lcqi_decode_step = median_metric(lcqi_runs, "derived_decode_ms_per_step")
    llama_eval_step = median_metric(llama_simple_runs, "llama_eval_ms_per_run")
    lcqi_weight_ratio = median_metric(lcqi_runs, "derived_f32_weight_inflation_ratio")
    lcqi_direct_share = median_metric(lcqi_runs, "derived_direct_quantized_weight_byte_share")
    bench_pp_tps = metric_as_float(llama_bench.metrics, "llama_bench_prefill_tokens_per_second")
    bench_tg_tps = metric_as_float(llama_bench.metrics, "llama_bench_decode_tokens_per_second")

    lines.extend(["[interpretation]", f"prompt_tokens_lcqi_median={lcqi_prompt_tokens}"])
    lines.append(f"prompt_tokens_llama_tokenize={llama_tokenize_count}")
    lines.append(f"prompt_tokens_llama_simple_median={llama_prompt_tokens}")
    if lcqi_prompt_tokens == llama_prompt_tokens == llama_tokenize_count:
        lines.append("same_input_prompt_token_count_match=true")
    else:
        lines.append("same_input_prompt_token_count_match=false")
    lines.append(
        "same_input_prompt_token_ids_match="
        f"{str(lcqi_prompt_ids == llama_tokenize_ids).lower()}"
    )
    if lcqi_prefill is not None and llama_prompt is not None and llama_prompt > 0.0:
        lines.append(f"prefill_ms_ratio_lcqi_over_llama_simple={lcqi_prefill / llama_prompt:.6f}")
    if lcqi_decode_step is not None and llama_eval_step is not None and llama_eval_step > 0.0:
        lines.append(f"decode_step_ms_ratio_lcqi_over_llama_simple={lcqi_decode_step / llama_eval_step:.6f}")
    if lcqi_weight_ratio is not None:
        lines.append(f"lcqi_f32_dequantized_weight_inflation_ratio={lcqi_weight_ratio:.6f}")
    if lcqi_direct_share is not None:
        lines.append(f"lcqi_direct_quantized_weight_byte_share={lcqi_direct_share:.6f}")
    add_lcqi_threaded_ratios(lines, lcqi_serial_runs, lcqi_runs)
    add_lcqi_ab_ratios(lines, lcqi_q4_direct_off_runs, lcqi_runs)
    add_lcqi_ggml_experimental_ratios(lines, lcqi_runs, lcqi_ggml_direct_runs)
    if bench_pp_tps is not None:
        lines.append(f"llama_bench_same_token_count_prefill_tps={bench_pp_tps:.6f}")
    if bench_tg_tps is not None:
        lines.append(f"llama_bench_same_token_count_decode_tps={bench_tg_tps:.6f}")
    lines.append(
        "root_cause=LCQI keeps shape-compatible Q4_K matrices in the default direct "
        "quantized path. The experimental Q5_0/Q6_K/Q8_0 direct path increases "
        "quantized-byte coverage, but its current scalar block kernels are slower "
        "than the dequantized f32 AVX2 fallback, so it is not enabled by default. "
        "The default LCQI run now uses a persistent row worker pool for large f32 "
        "fallback matrices; --threads 1 keeps the serial reference. llama.cpp "
        "still batches prompt evaluation, uses ggml graph scheduling, tuned low-bit "
        "CPU kernels, prefetch/thread scheduling, and more compact KV/cache execution."
    )


def add_lcqi_threaded_ratios(
    lines: list[str],
    lcqi_serial_runs: list[CommandResult],
    lcqi_runs: list[CommandResult],
) -> None:
    if not lcqi_serial_runs:
        return
    serial_prefill = median_metric(lcqi_serial_runs, "benchmark_prefill_ms")
    threaded_prefill = median_metric(lcqi_runs, "benchmark_prefill_ms")
    serial_decode = median_metric(lcqi_serial_runs, "derived_decode_ms_per_step")
    threaded_decode = median_metric(lcqi_runs, "derived_decode_ms_per_step")
    serial_f32 = median_metric(lcqi_serial_runs, "benchmark_hotspot_f32_fallback_ms")
    threaded_f32 = median_metric(lcqi_runs, "benchmark_hotspot_f32_fallback_ms")
    worker_count = median_metric(lcqi_runs, "benchmark_worker_count")
    if worker_count is not None:
        lines.append(f"lcqi_threaded_worker_count_median={worker_count:.0f}")
    if serial_prefill is not None and threaded_prefill is not None and threaded_prefill > 0.0:
        lines.append(
            "lcqi_threaded_prefill_speedup_serial_over_threaded="
            f"{serial_prefill / threaded_prefill:.6f}"
        )
    if serial_decode is not None and threaded_decode is not None and threaded_decode > 0.0:
        lines.append(
            "lcqi_threaded_decode_step_speedup_serial_over_threaded="
            f"{serial_decode / threaded_decode:.6f}"
        )
    if serial_f32 is not None and threaded_f32 is not None and threaded_f32 > 0.0:
        lines.append(
            "lcqi_threaded_f32_hotspot_speedup_serial_over_threaded="
            f"{serial_f32 / threaded_f32:.6f}"
        )


def add_lcqi_ab_ratios(
    lines: list[str],
    lcqi_q4_direct_off_runs: list[CommandResult],
    lcqi_runs: list[CommandResult],
) -> None:
    if not lcqi_q4_direct_off_runs:
        return
    off_prefill = median_metric(lcqi_q4_direct_off_runs, "benchmark_prefill_ms")
    on_prefill = median_metric(lcqi_runs, "benchmark_prefill_ms")
    off_decode = median_metric(lcqi_q4_direct_off_runs, "derived_decode_ms_per_step")
    on_decode = median_metric(lcqi_runs, "derived_decode_ms_per_step")
    off_down = median_metric(lcqi_q4_direct_off_runs, "benchmark_hotspot_w_down_ms")
    on_down = median_metric(lcqi_runs, "benchmark_hotspot_w_down_ms")
    off_f32_bytes = median_metric(lcqi_q4_direct_off_runs, "benchmark_f32_weight_bytes")
    on_f32_bytes = median_metric(lcqi_runs, "benchmark_f32_weight_bytes")
    if off_prefill is not None and on_prefill is not None and on_prefill > 0.0:
        lines.append(f"lcqi_q4_direct_prefill_speedup_off_over_on={off_prefill / on_prefill:.6f}")
    if off_decode is not None and on_decode is not None and on_decode > 0.0:
        lines.append(f"lcqi_q4_direct_decode_step_speedup_off_over_on={off_decode / on_decode:.6f}")
    if off_down is not None and on_down is not None and on_down > 0.0:
        lines.append(f"lcqi_q4_direct_w_down_speedup_off_over_on={off_down / on_down:.6f}")
    if off_f32_bytes is not None and on_f32_bytes is not None:
        lines.append(f"lcqi_q4_direct_f32_weight_bytes_saved={off_f32_bytes - on_f32_bytes:.0f}")


def add_lcqi_ggml_experimental_ratios(
    lines: list[str],
    lcqi_runs: list[CommandResult],
    lcqi_ggml_direct_runs: list[CommandResult],
) -> None:
    if not lcqi_ggml_direct_runs:
        return
    default_prefill = median_metric(lcqi_runs, "benchmark_prefill_ms")
    ggml_prefill = median_metric(lcqi_ggml_direct_runs, "benchmark_prefill_ms")
    default_decode = median_metric(lcqi_runs, "derived_decode_ms_per_step")
    ggml_decode = median_metric(lcqi_ggml_direct_runs, "derived_decode_ms_per_step")
    default_direct_share = median_metric(lcqi_runs, "derived_direct_quantized_weight_byte_share")
    ggml_direct_share = median_metric(lcqi_ggml_direct_runs, "derived_direct_quantized_weight_byte_share")
    ggml_hotspot = median_metric(lcqi_ggml_direct_runs, "benchmark_hotspot_ggml_direct_ms")
    if default_prefill is not None and ggml_prefill is not None and ggml_prefill > 0.0:
        lines.append(
            "lcqi_ggml_experimental_prefill_speedup_default_over_experimental="
            f"{default_prefill / ggml_prefill:.6f}"
        )
    if default_decode is not None and ggml_decode is not None and ggml_decode > 0.0:
        lines.append(
            "lcqi_ggml_experimental_decode_step_speedup_default_over_experimental="
            f"{default_decode / ggml_decode:.6f}"
        )
    if default_direct_share is not None and ggml_direct_share is not None:
        lines.append(
            "lcqi_ggml_experimental_direct_share_delta="
            f"{ggml_direct_share - default_direct_share:.6f}"
        )
    if ggml_hotspot is not None:
        lines.append(f"lcqi_ggml_experimental_hotspot_ms={ggml_hotspot:.6f}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run same-input SmolLM2 comparison between LCQI and llama.cpp."
    )
    parser.add_argument("--cache-dir", type=Path, default=run_smollm2_small_smoke.default_cache_dir())
    parser.add_argument("--model-path", type=Path, default=None)
    parser.add_argument("--build-dir", type=Path, default=default_build_dir())
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--max-new", type=int, default=DEFAULT_MAX_NEW_TOKENS)
    parser.add_argument("--rounds", type=int, default=DEFAULT_ROUNDS)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS)
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--force-download", action="store_true")
    parser.add_argument("--no-download", action="store_true")
    parser.add_argument("--skip-llama-build", action="store_true")
    parser.add_argument("--include-lcqi-q4-direct-off", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--include-lcqi-ggml-direct", action=argparse.BooleanOptionalAction, default=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.rounds <= 0:
        print("[lcqi-same-input] --rounds must be positive", file=sys.stderr)
        return 1
    if args.max_new < 0:
        print("[lcqi-same-input] --max-new cannot be negative", file=sys.stderr)
        return 1

    cache_dir = args.cache_dir.expanduser().resolve()
    model_path = (
        args.model_path.expanduser().resolve()
        if args.model_path
        else cache_dir / "models" / run_smollm2_small_smoke.MODEL_FILE_NAME
    )
    build_dir = args.build_dir.expanduser().resolve()
    report_path = args.report.expanduser().resolve()
    full_prompt = smollm2_chat_prompt(args.prompt)

    try:
        run_smollm2_small_smoke.download_model(
            model_path,
            force_download=args.force_download,
            no_download=args.no_download,
        )
        lcqi_binary = build_lcqi(build_dir, args.jobs, args.timeout_seconds)
        llama_simple_binary, llama_bench_binary, llama_tokenize_binary = ensure_llama_targets(
            cache_dir,
            jobs=args.jobs,
            skip_build=args.skip_llama_build,
            timeout_seconds=args.timeout_seconds,
        )

        lcqi_q4_direct_off_runs: list[CommandResult] = []
        lcqi_serial_runs: list[CommandResult] = []
        lcqi_runs: list[CommandResult] = []
        lcqi_ggml_direct_runs: list[CommandResult] = []
        llama_simple_runs: list[CommandResult] = []
        for round_index in range(1, args.rounds + 1):
            if args.include_lcqi_q4_direct_off:
                print(f"[lcqi-same-input] LCQI Q4 direct off round {round_index}/{args.rounds}")
                lcqi_q4_direct_off_runs.append(
                    run_lcqi_round(
                        lcqi_binary,
                        model_path,
                        user_prompt=args.prompt,
                        max_new_tokens=args.max_new,
                        round_index=round_index,
                        timeout_seconds=args.timeout_seconds,
                        name_prefix="lcqi_q4_direct_off_round",
                        env={LCQI_Q4_DIRECT_ENV: LCQI_Q4_DIRECT_OFF},
                    )
                )
                ensure_success(
                    lcqi_q4_direct_off_runs[-1],
                    "run LCQI Q4 direct off same-input round",
                )
            print(f"[lcqi-same-input] LCQI serial round {round_index}/{args.rounds}")
            lcqi_serial_runs.append(
                run_lcqi_round(
                    lcqi_binary,
                    model_path,
                    user_prompt=args.prompt,
                    max_new_tokens=args.max_new,
                    round_index=round_index,
                    timeout_seconds=args.timeout_seconds,
                    name_prefix="lcqi_serial_round",
                    threads=1,
                )
            )
            ensure_success(lcqi_serial_runs[-1], "run LCQI serial same-input round")
            print(f"[lcqi-same-input] LCQI round {round_index}/{args.rounds}")
            lcqi_runs.append(
                run_lcqi_round(
                    lcqi_binary,
                    model_path,
                    user_prompt=args.prompt,
                    max_new_tokens=args.max_new,
                    round_index=round_index,
                    timeout_seconds=args.timeout_seconds,
                )
            )
            ensure_success(lcqi_runs[-1], "run LCQI same-input round")
            if args.include_lcqi_ggml_direct:
                print(f"[lcqi-same-input] LCQI GGML direct experimental round {round_index}/{args.rounds}")
                lcqi_ggml_direct_runs.append(
                    run_lcqi_round(
                        lcqi_binary,
                        model_path,
                        user_prompt=args.prompt,
                        max_new_tokens=args.max_new,
                        round_index=round_index,
                        timeout_seconds=args.timeout_seconds,
                        name_prefix="lcqi_ggml_direct_experimental_round",
                        env={LCQI_GGML_DIRECT_ENV: LCQI_GGML_DIRECT_ON},
                    )
                )
                ensure_success(
                    lcqi_ggml_direct_runs[-1],
                    "run LCQI GGML direct experimental same-input round",
                )
            print(f"[lcqi-same-input] llama-simple round {round_index}/{args.rounds}")
            llama_simple_runs.append(
                run_llama_simple_round(
                    llama_simple_binary,
                    model_path,
                    full_prompt=full_prompt,
                    max_new_tokens=args.max_new,
                    round_index=round_index,
                    timeout_seconds=args.timeout_seconds,
                )
            )
            ensure_success(llama_simple_runs[-1], "run llama-simple same-input round")

        prompt_tokens = int(lcqi_runs[0].metrics["benchmark_prompt_tokens"])
        print("[lcqi-same-input] llama-tokenize prompt identity check")
        llama_tokenize = run_llama_tokenize(
            llama_tokenize_binary,
            model_path,
            full_prompt=full_prompt,
            timeout_seconds=args.timeout_seconds,
        )
        ensure_success(llama_tokenize, "run llama-tokenize same-input check")
        print("[lcqi-same-input] llama-bench same-token-count reference")
        llama_bench = run_llama_bench(
            llama_bench_binary,
            model_path,
            prompt_tokens=prompt_tokens,
            max_new_tokens=args.max_new,
            rounds=args.rounds,
            timeout_seconds=args.timeout_seconds,
        )
        ensure_success(llama_bench, "run llama-bench same-token-count reference")

        write_report(
            report_path,
            cache_dir=cache_dir,
            model_path=model_path,
            user_prompt=args.prompt,
            full_prompt=full_prompt,
            max_new_tokens=args.max_new,
            lcqi_runs=lcqi_runs,
            lcqi_serial_runs=lcqi_serial_runs,
            lcqi_q4_direct_off_runs=lcqi_q4_direct_off_runs,
            lcqi_ggml_direct_runs=lcqi_ggml_direct_runs,
            llama_tokenize=llama_tokenize,
            llama_simple_runs=llama_simple_runs,
            llama_bench=llama_bench,
        )
    except Exception as error:
        print(f"[lcqi-same-input] ERROR: {error}", file=sys.stderr)
        return 1

    print(f"report={report_path}")
    for line in format_summary(
        "lcqi_same_input",
        lcqi_runs,
        ["benchmark_prefill_ms", "derived_prefill_prompt_tokens_per_second", "derived_decode_ms_per_step"],
    ):
        print(line)
    for line in format_summary(
        "llama_simple_same_input",
        llama_simple_runs,
        ["llama_prompt_eval_ms", "llama_prompt_eval_tokens_per_second", "llama_eval_ms_per_run"],
    ):
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
