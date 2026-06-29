#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import os
import re
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


MODEL_SOURCE_ID = "HuggingFaceTB/SmolLM2-135M-Instruct"
MODEL_GGUF_REPO_ID = "bartowski/SmolLM2-135M-Instruct-GGUF"
MODEL_FILE_NAME = "SmolLM2-135M-Instruct-Q4_K_M.gguf"
MODEL_URL = (
    "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF"
    f"/resolve/main/{MODEL_FILE_NAME}"
)
MODEL_EXPECTED_BYTES = 105454432
MODEL_EXPECTED_SHA256 = (
    "2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d"
)
LLAMA_CPP_REPO_URL = "https://github.com/ggml-org/llama.cpp.git"
LLAMA_CPP_COMMIT = "b3fed31b99f9bd37725833674252bccb429bb183"
LLAMA_TARGET = "llama-simple-chat"
DEFAULT_PROMPT = "What is 2+3? Answer in one short sentence."
DEFAULT_CONTEXT_TOKENS = 512
DEFAULT_TIMEOUT_SECONDS = 180
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
    explicit_cache = os.environ.get("LCQI_SMOLLM2_CACHE_DIR")
    if explicit_cache:
        return Path(explicit_cache).expanduser()
    xdg_cache = os.environ.get("XDG_CACHE_HOME")
    cache_root = Path(xdg_cache).expanduser() if xdg_cache else Path.home() / ".cache"
    return cache_root / "lcqi-smollm2-small-smoke"


def default_report_path() -> Path:
    return volume_dir() / "results" / "lcqi-smollm2-small-model-smoke.txt"


def command_line(args: list[str]) -> str:
    return " ".join(shlex.quote(arg) for arg in args)


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required tool not found in PATH: {name}")
    return path


def run_command(
    args: list[str],
    *,
    cwd: Path | None = None,
    input_text: str | None = None,
    timeout_seconds: int | None = None,
) -> CommandResult:
    try:
        completed = subprocess.run(
            args,
            cwd=str(cwd) if cwd else None,
            input=input_text,
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


def verify_model_file(path: Path) -> tuple[bool, str, int]:
    if not path.exists():
        return False, "", 0
    actual_bytes = path.stat().st_size
    actual_sha = sha256_file(path)
    return (
        actual_bytes == MODEL_EXPECTED_BYTES
        and actual_sha == MODEL_EXPECTED_SHA256,
        actual_sha,
        actual_bytes,
    )


def download_model(model_path: Path, *, force_download: bool, no_download: bool) -> None:
    model_path.parent.mkdir(parents=True, exist_ok=True)

    if force_download and model_path.exists():
        model_path.unlink()

    is_valid, actual_sha, actual_bytes = verify_model_file(model_path)
    if is_valid:
        print(f"[lcqi-smoke] model cache hit: {model_path}")
        return

    if model_path.exists():
        print(
            "[lcqi-smoke] cached model hash mismatch, redownloading: "
            f"bytes={actual_bytes}, sha256={actual_sha}"
        )
        model_path.unlink()

    if no_download:
        raise RuntimeError(
            f"model file is missing or invalid and --no-download was set: {model_path}"
        )

    require_tool("curl")
    part_path = model_path.with_suffix(model_path.suffix + ".part")
    if part_path.exists():
        part_path.unlink()

    print(f"[lcqi-smoke] downloading small GGUF model: {MODEL_GGUF_REPO_ID}/{MODEL_FILE_NAME}")
    result = run_command(
        [
            "curl",
            "-L",
            "--fail",
            "--retry",
            "3",
            "-o",
            str(part_path),
            MODEL_URL,
        ]
    )
    ensure_success(result, "download SmolLM2 GGUF")

    downloaded_ok, downloaded_sha, downloaded_bytes = verify_model_file(part_path)
    if not downloaded_ok:
        raise RuntimeError(
            "downloaded model did not match expected identity: "
            f"bytes={downloaded_bytes}, sha256={downloaded_sha}"
        )
    part_path.replace(model_path)
    print(f"[lcqi-smoke] model verified: sha256={MODEL_EXPECTED_SHA256}")


def ensure_llama_simple_chat(cache_dir: Path, *, jobs: int, skip_build: bool) -> Path:
    cached_binary = cache_dir / "llama.cpp" / "build" / "bin" / LLAMA_TARGET
    source_dir = cache_dir / "llama.cpp"
    build_dir = source_dir / "build"
    if cached_binary.exists() and os.access(cached_binary, os.X_OK) and is_pinned_llama_checkout(source_dir):
        print(f"[lcqi-smoke] llama.cpp binary cache hit: {cached_binary}")
        return cached_binary

    if skip_build:
        raise RuntimeError(f"llama.cpp binary is missing and --skip-build was set: {cached_binary}")

    require_tool("git")
    require_tool("cmake")

    cache_dir.mkdir(parents=True, exist_ok=True)

    if not (source_dir / ".git").exists():
        print(f"[lcqi-smoke] cloning llama.cpp into cache: {source_dir}")
        result = run_command(
            [
                "git",
                "clone",
                "--filter=blob:none",
                "--no-checkout",
                LLAMA_CPP_REPO_URL,
                str(source_dir),
            ]
        )
        ensure_success(result, "clone llama.cpp")

    print(f"[lcqi-smoke] checking out pinned llama.cpp commit: {LLAMA_CPP_COMMIT}")
    fetch = run_command(
        ["git", "-C", str(source_dir), "fetch", "--depth", "1", "origin", LLAMA_CPP_COMMIT]
    )
    ensure_success(fetch, "fetch pinned llama.cpp commit")
    checkout = run_command(["git", "-C", str(source_dir), "checkout", "--detach", LLAMA_CPP_COMMIT])
    ensure_success(checkout, "checkout pinned llama.cpp commit")

    print(f"[lcqi-smoke] configuring llama.cpp build: {build_dir}")
    configure = run_command(
        [
            "cmake",
            "-S",
            str(source_dir),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DLLAMA_BUILD_TESTS=OFF",
            "-DLLAMA_BUILD_EXAMPLES=ON",
            "-DLLAMA_BUILD_SERVER=OFF",
            "-DGGML_NATIVE=OFF",
        ]
    )
    ensure_success(configure, "configure llama.cpp")

    print(f"[lcqi-smoke] building {LLAMA_TARGET} with -j{jobs}")
    build = run_command(
        ["cmake", "--build", str(build_dir), "--target", LLAMA_TARGET, "-j", str(jobs)]
    )
    ensure_success(build, f"build {LLAMA_TARGET}")

    if not cached_binary.exists():
        raise RuntimeError(f"expected binary was not produced: {cached_binary}")
    return cached_binary


def is_pinned_llama_checkout(source_dir: Path) -> bool:
    if not (source_dir / ".git").exists():
        return False
    result = run_command(["git", "-C", str(source_dir), "rev-parse", "HEAD"])
    if result.returncode != 0:
        return False
    return result.stdout.strip() == LLAMA_CPP_COMMIT


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)


def extract_response(stdout: str) -> str:
    cleaned = strip_ansi(stdout).replace("\r", "")
    chunks = [chunk.strip() for chunk in re.split(r"(?:^|\n)> ?", cleaned) if chunk.strip()]
    if not chunks:
        return ""
    return chunks[0]


def run_model_smoke(
    llama_binary: Path,
    model_path: Path,
    *,
    prompt: str,
    context_tokens: int,
    timeout_seconds: int,
) -> tuple[CommandResult, str]:
    command = [
        str(llama_binary),
        "-m",
        str(model_path),
        "-ngl",
        "0",
        "-c",
        str(context_tokens),
    ]
    print("[lcqi-smoke] running real small model generation")
    result = run_command(
        command,
        input_text=prompt.rstrip("\n") + "\n\n",
        timeout_seconds=timeout_seconds,
    )
    ensure_success(result, "run small model smoke")

    response = extract_response(result.stdout)
    if not response:
        raise RuntimeError(
            "small model smoke exited successfully but produced no assistant text\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result, response


def current_repo_commit() -> str:
    result = run_command(["git", "-C", str(repo_dir()), "rev-parse", "--short", "HEAD"])
    if result.returncode != 0:
        return "unknown"
    return result.stdout.strip()


def current_uname() -> str:
    result = run_command(["uname", "-a"])
    if result.returncode != 0:
        return "unknown"
    return result.stdout.strip()


def write_report(
    report_path: Path,
    *,
    cache_dir: Path,
    llama_binary: Path,
    model_path: Path,
    prompt: str,
    result: CommandResult,
    response: str,
) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    clean_stdout = strip_ansi(result.stdout).strip()
    report = "\n".join(
        [
            "LCQI SmolLM2 Small Model Smoke",
            "",
            f"timestamp_utc={dt.datetime.now(dt.UTC).isoformat()}",
            f"repo_commit={current_repo_commit()}",
            f"host={current_uname()}",
            f"python={sys.version.split()[0]}",
            f"cache_dir={cache_dir}",
            "",
            f"model_source_id={MODEL_SOURCE_ID}",
            f"model_gguf_repo_id={MODEL_GGUF_REPO_ID}",
            f"model_file={MODEL_FILE_NAME}",
            f"model_url={MODEL_URL}",
            f"model_expected_bytes={MODEL_EXPECTED_BYTES}",
            f"model_expected_sha256={MODEL_EXPECTED_SHA256}",
            f"model_path={model_path}",
            "",
            f"llama_cpp_repo={LLAMA_CPP_REPO_URL}",
            f"llama_cpp_commit={LLAMA_CPP_COMMIT}",
            f"llama_target={LLAMA_TARGET}",
            f"llama_binary={llama_binary}",
            "",
            f"prompt={prompt}",
            f"command={command_line(result.args)}",
            f"exit_code={result.returncode}",
            "",
            "validation=PASS: model hash matched, llama-simple-chat exited 0, "
            "assistant response was non-empty",
            "",
            "assistant_response:",
            response,
            "",
            "stdout_clean_tail:",
            tail(clean_stdout, 2000),
            "",
            "stderr_tail:",
            tail(result.stderr.strip(), 2000),
            "",
        ]
    )
    report_path.write_text(report, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run a real local small open-source model smoke for CPU Volume 3. "
            "The script caches llama.cpp and the GGUF model outside the Git repository."
        )
    )
    parser.add_argument("--cache-dir", type=Path, default=default_cache_dir())
    parser.add_argument("--model-path", type=Path, default=None)
    parser.add_argument("--llama-bin", type=Path, default=None)
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--prompt", default=DEFAULT_PROMPT)
    parser.add_argument("--ctx", type=int, default=DEFAULT_CONTEXT_TOKENS)
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--jobs", type=int, default=DEFAULT_BUILD_JOBS)
    parser.add_argument("--force-download", action="store_true")
    parser.add_argument("--no-download", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cache_dir = args.cache_dir.expanduser().resolve()
    model_path = (
        args.model_path.expanduser().resolve()
        if args.model_path
        else cache_dir / "models" / MODEL_FILE_NAME
    )
    report_path = args.report.expanduser().resolve()

    try:
        download_model(
            model_path,
            force_download=args.force_download,
            no_download=args.no_download,
        )
        if args.llama_bin:
            llama_binary = args.llama_bin.expanduser().resolve()
            if not llama_binary.exists() or not os.access(llama_binary, os.X_OK):
                raise RuntimeError(f"--llama-bin is not executable: {llama_binary}")
        else:
            llama_binary = ensure_llama_simple_chat(
                cache_dir,
                jobs=args.jobs,
                skip_build=args.skip_build,
            )

        result, response = run_model_smoke(
            llama_binary,
            model_path,
            prompt=args.prompt,
            context_tokens=args.ctx,
            timeout_seconds=args.timeout,
        )
        write_report(
            report_path,
            cache_dir=cache_dir,
            llama_binary=llama_binary,
            model_path=model_path,
            prompt=args.prompt,
            result=result,
            response=response,
        )
    except RuntimeError as exc:
        print(f"[lcqi-smoke] ERROR: {exc}", file=sys.stderr)
        return 1

    print(f"report={report_path}")
    print(f"model_sha256={MODEL_EXPECTED_SHA256}")
    print(f"assistant_response={response}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
