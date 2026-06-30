# LCQI SmolLM2 GGML Direct Analysis

Date: 2026-06-30
Host: caw
Raw report: `books/cpu-volume-3/reports/lcqi-smollm2-ggml-direct-compare-caw.txt`

## Question

LCQI already had a verified default `Q4_K x Q8_K` direct path in the real SmolLM2/GGUF decoder. The next question was whether more GGUF quantized formats should also run directly instead of materializing f32 fallback weights.

This change adds an explicit experimental path for shape-compatible `Q5_0`, `Q6_K`, and `Q8_0` linear tensors:

- `matvec_ggml_quantized_f32`: exact single-operator oracle from GGUF block bytes and f32 input.
- `matvec_ggml_quantized_q8_0`: experimental direct path using temporary `Q8_0` activation blocks.
- AVX2 block dot support for `Q8_0 x Q8_0` and `Q5_0 x Q8_0`.
- `LCQI_LLAMA_GGML_DIRECT=1`: opt-in decoder integration for the experimental path.

## Same-Input Evidence

All measurements use the same model file, same expanded SmolLM2 chat prompt, same 31 prompt token ids, and `max-new=2`.

Model:

- `SmolLM2-135M-Instruct-Q4_K_M.gguf`
- bytes: `105454432`
- sha256: `2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`
- llama.cpp commit: `b3fed31b99f9bd37725833674252bccb429bb183`

Default LCQI `Q4_K` direct:

- prefill median: `370.847 ms`
- decode step median: `15.4182 ms`
- direct quantized weight byte share: `0.076809`
- `Q4_K` direct tensors: `16`
- GGML experimental direct tensors: `0`
- f32 fallback tensors: `256`
- `Q4_K` direct calls: `512`
- f32 fallback calls: `6208`
- `w_down` hotspot median: `63.5371 ms`

LCQI with `LCQI_LLAMA_Q4_DIRECT=0`:

- prefill median: `393.992 ms`
- decode step median: `15.8782 ms`
- f32 fallback calls: `6720`
- `w_down` hotspot median: `91.6812 ms`

Default `Q4_K` direct A/B:

- prefill speedup: `1.062411x`
- decode step speedup: `1.029835x`
- `w_down` speedup: `1.442955x`
- f32 materialized weight bytes saved: `56623104`

Experimental `LCQI_LLAMA_GGML_DIRECT=1`:

- prefill median: `1211.710 ms`
- decode step median: `42.4420 ms`
- direct quantized weight byte share: `0.708479`
- GGML experimental direct tensors: `194`
- GGML experimental direct calls: `6208`
- GGML direct hotspot median: `1207.130 ms`
- default-over-experimental prefill speedup: `0.306053x`
- default-over-experimental decode step speedup: `0.363277x`

llama.cpp same-input reference:

- prompt eval median: `22.840 ms`
- eval step median: `2.880 ms`
- LCQI default prefill ratio over llama.cpp: `16.236734x`
- LCQI default decode step ratio over llama.cpp: `5.353542x`

## Decision

Keep the default path as `gguf_mixed_q4_k_direct`: only shape-compatible `Q4_K` tensors run through direct quantized matvec, while the rest use f32 fallback.

Do not enable `Q5_0/Q6_K/Q8_0` direct by default. The experiment increases quantized byte coverage from `7.68%` to `70.85%`, but it makes end-to-end execution much slower. The bottleneck moves into `benchmark_hotspot_ggml_direct_ms`, which reaches `1207.13 ms`.

The negative result is useful because it separates three facts that are easy to confuse:

- Lower memory footprint does not automatically mean faster execution.
- More direct-format coverage is not a performance result unless the direct kernels are packed, vectorized, and scheduled well.
- A small AVX2 dot improvement inside a 32-element block cannot compensate for row-by-row dispatch, repeated block unpacking, scalar `Q6_K`, and missing threaded/batched execution.

## Next Optimization Target

The best next work is not to simply add more scalar quantized formats. The evidence points to two higher-leverage directions:

1. Batched prefill: LCQI currently processes prompt tokens as repeated decode steps, while llama.cpp evaluates the prompt through batched graph execution.
2. Packed multi-row low-bit kernels: redesign GGML direct execution around multi-row layout, SIMD coverage for the dominant formats, prefetch, and thread partitioning before trying to make it a default path.

Until those are implemented and measured, `LCQI_LLAMA_GGML_DIRECT=1` remains an opt-in experiment and a source-reading case study, not a production default.
