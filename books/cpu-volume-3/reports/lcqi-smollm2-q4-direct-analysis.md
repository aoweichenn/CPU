# LCQI SmolLM2 Q4_K Direct Decoder Integration

Status: superseded for latest numbers by `books/cpu-volume-3/reports/lcqi-smollm2-ggml-direct-analysis.md`. This file remains the historical Q4_K-direct integration note.

Date: 2026-06-30

Host: `caw`, Linux WSL2 x86-64, `nproc=20`.

Model: `SmolLM2-135M-Instruct-Q4_K_M.gguf`

Model SHA256: `2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`

Raw artifact: `books/cpu-volume-3/reports/lcqi-smollm2-q4-direct-compare-caw.txt`

## What Changed

LCQI no longer has only an isolated Q4_K microbenchmark. The real SmolLM2 GGUF decoder now stores shape-compatible Q4_K linear tensors as raw GGUF block bytes and dispatches those matrices through `matvec_q4_k_q8`. Tensors that are not Q4_K, or whose input dimension is not a QK_K block multiple, still fall back to f32 materialization.

The CLI also reports per-op hotspots for RMSNorm, RoPE, attention, every linear projection, the lm head, direct Q4_K time, fallback f32 time, and direct/fallback call counts.

## caw Same-Input Results

| Run | Prefill median | Decode-step median | `w_down` hotspot | f32 bytes | direct Q4_K bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Q4 direct off | `423.56 ms` | `17.21 ms` | `98.10 ms` | `538,060,032` | `0` |
| Q4 direct on | `396.93 ms` | `16.44 ms` | `67.50 ms` | `481,436,928` | `7,962,624` |

A/B effect from the same binary and same input:

- Prefill speedup: `1.067x`.
- Decode-step speedup: `1.047x`.
- `w_down` hotspot speedup: `1.453x`.
- f32 materialized weight bytes saved: `56,623,104` bytes.
- Direct quantized byte share: `7.68%` of the model bytes loaded by LCQI.
- Direct Q4_K calls: `512`; f32 fallback calls: `6208`.

Compared with the previous caw report before this work:

- LCQI prefill median moved from `1278.47 ms` to `396.93 ms`.
- LCQI decode-step median moved from `44.64 ms` to `16.44 ms`.
- LCQI/llama.cpp prefill ratio moved from `55.78x` slower to `16.23x` slower.
- LCQI/llama.cpp decode-step ratio moved from `15.72x` slower to `5.25x` slower.

The larger before/after improvement includes both this code path and the cleaner Release rebuild/hotspot-instrumented path on caw. The strict same-binary A/B number is the conservative evidence for the Q4_K direct feature itself.

## Why The Remaining Gap Is Still Large

The direct Q4_K path currently reaches only shape-compatible Q4_K matrices. In this SmolLM2 Q4_K_M file, the biggest remaining hotspots are still f32 fallback projections: `w_gate` around `107.59 ms`, `w_up` around `106.59 ms`, and attention projections around `40.11 ms`/`40.21 ms`. `w_down` improved because the compatible Q4_K tensors are now executed directly, but most linear calls still use dequantized f32 weights.

The next useful work is not another isolated microbenchmark. It is either support for additional GGUF quantized tensor layouts in the real decoder, especially the formats used by `w_gate` and `w_up`, or a batched prefill path. Batched prefill attacks the `16.23x` prefill gap directly; broader quantized tensor coverage attacks both prefill and decode.
