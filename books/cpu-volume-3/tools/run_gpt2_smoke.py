#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import os
import shlex
import subprocess
import sys
import urllib.request
from dataclasses import dataclass
from pathlib import Path


MODEL_REPO_ID = "openai-community/gpt2"
MODEL_REVISION = "main"
MODEL_FILE_IDENTITIES = {
    "config.json": (
        665,
        "0daed7749b4f02b8f76240d5444551d7b08712dab4d0adb8239c56ba823bb7b4",
    ),
    "vocab.json": (
        1042301,
        "196139668be63f3b5d6574427317ae82f612a97c5d1cdaf36ed2256dbf636783",
    ),
    "merges.txt": (
        456318,
        "1ce1664773c50f3e0cc8842619a93edc4624525b728b188a9e0be33b7726adc5",
    ),
    "model.safetensors": (
        548105171,
        "248dfc3911869ec493c76e65bf2fcf7f615828b0254c12b473182f0f81d3a707",
    ),
}
DEFAULT_PROMPT = "Hello, my name is"
DEFAULT_MAX_NEW_TOKENS = 1
DEFAULT_TIMEOUT_SECONDS = 300
DEFAULT_BUILD_JOBS = 2


@dataclass(frozen=True)
class CommandResult:
    args: list[str]
    returncode: int
    stdout: str
    stderr: str


def script_path() -> Path:
    return Path(__file__).resolve()


def volume_dir() -> Path:
    return script_path().parents[1]


def repo_dir() -> Path:
    return script_path().parents[3]


def default_cache_dir() -> Path:
    explicit_cache = os.environ.get("LCQI_GPT2_CACHE_DIR")
    if explicit_cache:
        return Path(explicit_cache).expanduser()
    xdg_cache = os.environ.get("XDG_CACHE_HOME")
    cache_root = Path(xdg_cache).expanduser() if xdg_cache else Path.home() / ".cache"
    return cache_root / "lcqi-gpt2-smoke" / MODEL_REPO_ID.replace("/", "--")


def default_build_dir() -> Path:
    return volume_dir() / "build" / "lcqi-release"


def default_report_path() -> Path:
    return volume_dir() / "results" / "lcqi-gpt2-smoke.txt"


def command_line(args: list[str]) -> str:
    return " ".join(shlex.quote(arg) for arg in args)


def run_command(
    args: list[str],
    *,
    cwd: Path | None = None,
    timeout_seconds: int | None = None,
) -> CommandResult:
    try:
        completed = subprocess.run(
            args,
            cwd=str(cwd) if cwd else None,
            text=True,
            capture_output=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout if isinstance(exc.stdout, str) else ""
        stderr = exc.stderr if isinstance(exc.stderr, str) else ""
        return CommandResult(args=args, returncode=124, stdout=stdout, stderr=stderr)
    return CommandResult(
        args=args,
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
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
    if len(text) <= max_chars:
        return text
    return text[-max_chars:]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def model_url(file_name: str) -> str:
    return (
        f"https://huggingface.co/{MODEL_REPO_ID}/resolve/"
        f"{MODEL_REVISION}/{file_name}"
    )


def download_file(url: str, target: Path, timeout_seconds: int) -> None:
    part_path = target.with_suffix(target.suffix + ".part")
    if part_path.exists():
        part_path.unlink()
    request = urllib.request.Request(url, headers={"User-Agent": "lcqi-gpt2-smoke"})
    with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
        with part_path.open("wb") as output:
            while True:
                block = response.read(1024 * 1024)
                if not block:
                    break
                output.write(block)
    part_path.replace(target)


def file_identity_matches(path: Path, expected_bytes: int, expected_sha256: str) -> bool:
    return (
        path.exists()
        and path.stat().st_size == expected_bytes
        and sha256_file(path) == expected_sha256
    )


def ensure_model_files(cache_dir: Path, *, no_download: bool, timeout_seconds: int) -> None:
    cache_dir.mkdir(parents=True, exist_ok=True)
    missing = [
        name
        for name, (expected_bytes, expected_sha256) in MODEL_FILE_IDENTITIES.items()
        if not file_identity_matches(cache_dir / name, expected_bytes, expected_sha256)
    ]
    if not missing:
        print(f"[lcqi-gpt2] model cache hit: {cache_dir}")
        return
    if no_download:
        raise RuntimeError(
            f"missing GPT-2 model files and --no-download was set: {missing}"
        )
    for file_name in missing:
        target = cache_dir / file_name
        if target.exists():
            target.unlink()
        print(f"[lcqi-gpt2] downloading {MODEL_REPO_ID}/{file_name}")
        download_file(model_url(file_name), target, timeout_seconds)
        expected_bytes, expected_sha256 = MODEL_FILE_IDENTITIES[file_name]
        if not file_identity_matches(target, expected_bytes, expected_sha256):
            raise RuntimeError(
                f"downloaded GPT-2 file identity mismatch: {file_name}"
            )


def build_lcqi_gpt2(build_dir: Path, jobs: int) -> Path:
    configure = run_command(
        [
            "cmake",
            "-S",
            str(volume_dir()),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        timeout_seconds=DEFAULT_TIMEOUT_SECONDS,
    )
    ensure_success(configure, "configure LCQI")
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
        timeout_seconds=DEFAULT_TIMEOUT_SECONDS,
    )
    ensure_success(build, "build lcqi_gpt2")
    binary = build_dir / "labs" / "linux_cpu_inference" / "lcqi_gpt2"
    if not binary.exists():
        raise RuntimeError(f"lcqi_gpt2 binary not found: {binary}")
    return binary


def write_report(
    report_path: Path,
    *,
    cache_dir: Path,
    binary: Path,
    prompt: str,
    max_new_tokens: int,
    engine: str,
    result: CommandResult,
) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"timestamp_utc={dt.datetime.now(dt.UTC).isoformat()}",
        f"model_repo_id={MODEL_REPO_ID}",
        f"model_revision={MODEL_REVISION}",
        f"model_cache_dir={cache_dir}",
    ]
    for file_name, (expected_bytes, expected_sha256) in MODEL_FILE_IDENTITIES.items():
        path = cache_dir / file_name
        lines.append(f"file.{file_name}.bytes={path.stat().st_size}")
        lines.append(f"file.{file_name}.sha256={sha256_file(path)}")
        lines.append(f"file.{file_name}.expected_bytes={expected_bytes}")
        lines.append(f"file.{file_name}.expected_sha256={expected_sha256}")
    lines.extend(
        [
            f"binary={binary}",
            f"prompt={prompt}",
            f"max_new_tokens={max_new_tokens}",
            f"engine={engine}",
            f"command={command_line(result.args)}",
            f"exit_code={result.returncode}",
            "stdout_begin",
            result.stdout.rstrip(),
            "stdout_end",
            "stderr_begin",
            tail(result.stderr).rstrip(),
            "stderr_end",
        ]
    )
    passed = result.returncode == 0 and "generated_text " in result.stdout
    lines.append(f"validation={'PASS' if passed else 'FAIL'}")
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run LCQI self-owned GPT-2 smoke on a HuggingFace safetensors directory."
    )
    parser.add_argument("--cache-dir", type=Path, default=default_cache_dir())
    parser.add_argument("--build-dir", type=Path, default=default_build_dir())
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--max-new-tokens", type=int, default=DEFAULT_MAX_NEW_TOKENS)
    parser.add_argument("--engine", choices=("cached", "full"), default="cached")
    parser.add_argument("--benchmark", action="store_true")
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS)
    parser.add_argument("--no-download", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.max_new_tokens < 0:
            raise RuntimeError("--max-new-tokens cannot be negative")
        ensure_model_files(
            args.cache_dir,
            no_download=args.no_download,
            timeout_seconds=args.timeout_seconds,
        )
        binary = build_lcqi_gpt2(args.build_dir, args.jobs)
        command = [
            str(binary),
            "--engine",
            args.engine,
            str(args.cache_dir),
            args.prompt,
            str(args.max_new_tokens),
        ]
        if args.benchmark:
            command.insert(1, "--benchmark")
        result = run_command(command, cwd=repo_dir(), timeout_seconds=args.timeout_seconds)
        write_report(
            args.report,
            cache_dir=args.cache_dir,
            binary=binary,
            prompt=args.prompt,
            max_new_tokens=args.max_new_tokens,
            engine=args.engine,
            result=result,
        )
        print(f"[lcqi-gpt2] report: {args.report}")
        print(tail(result.stdout, max_chars=2000))
        return 0 if result.returncode == 0 else result.returncode
    except Exception as error:
        print(f"[lcqi-gpt2] error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
