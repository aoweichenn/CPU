# LCQI SmolLM2 F32 Row8 Kernel Analysis

Date: 2026-06-30
Host: caw
Raw report: `books/cpu-volume-3/reports/lcqi-smollm2-f32-row8-compare-caw.txt`

## Question

After Q4_K row-range and GGML direct row-range, the default SmolLM2 path still spent most of its time in F32 fallback projections. The previous caw report showed:

- default prefill median: `162.492 ms`
- default decode step median: `6.41487 ms`
- F32 fallback hotspot median: `148.132 ms`
- Q4_K direct hotspot median: `5.39028 ms`

That means the next safe optimization target was not attention or KV cache. It was the default F32 row-major GEMV kernel used by the remaining 256 materialized tensors.

## Change

`src/f32_kernels_avx2.cpp` now has an 8-row fast path for `linear_f32_rows_avx2_unchecked` and `max_dot_f32_rows_avx2_unchecked`.

The old fast path reused each loaded input vector across 4 output rows. The new path reuses it across 8 output rows, then falls back to the existing 4-row and single-row tail paths. The public API and model math are unchanged.

The test suite now checks:

- full row range, which exercises the 8-row fast path on x86 AVX2 machines;
- a 6-row subrange, which forces the 4-row path plus scalar tail;
- scalar oracle agreement for row outputs and row-max selection.

## Same-Session A/B

Before replacing the remote source file, the old caw binary was measured for 5 rounds on the same model and prompt. Then the row8 candidate was built and measured for 5 rounds with the same command.

| Path | Prefill median | Decode step median | F32 fallback median |
| --- | ---: | ---: | ---: |
| old 4-row AVX2 kernel | `169.902 ms` | `6.88524 ms` | `155.152 ms` |
| row8 AVX2 candidate | `158.790 ms` | `6.44826 ms` | `144.172 ms` |
| speedup | `1.06998x` | `1.06777x` | `1.07616x` |

This is a real but bounded improvement. It hits the measured F32 fallback hotspot, but it does not change the larger execution model: prompt tokens are still processed one at a time and most fallback tensors are still row-major F32 GEMV.

## Full Same-Input Report

The full same-input script then ran LCQI Q4-direct-off, serial default, threaded default, opt-in GGML direct, llama-simple, llama-tokenize, and llama-bench.

Key medians from `lcqi-smollm2-f32-row8-compare-caw.txt`:

- default threaded LCQI prefill: `158.634 ms`
- default threaded LCQI decode step: `6.54156 ms`
- default threaded F32 fallback hotspot: `144.161 ms`
- default Q4_K direct hotspot: `4.98158 ms`
- serial default prefill: `346.083 ms`
- serial default decode step: `14.2514 ms`
- llama.cpp prompt eval: `22.430 ms`
- llama.cpp eval step: `2.960 ms`
- LCQI / llama.cpp prefill ratio: `7.072403x`
- LCQI / llama.cpp decode ratio: `2.209986x`

The prompt token count and token ids still match exactly across LCQI and llama.cpp, so the remaining gap is not a prompt or tokenizer mismatch.

## Decision

Keep the 8-row F32 AVX2 path. It improves the dominant default hotspot, preserves scalar-oracle tests, and passed both local and caw CTest.

Do not claim that this closes the llama.cpp gap. The remaining gap is still structural:

- no batched prompt prefill;
- no ggml-style graph scheduling;
- many tensors still execute as materialized F32 row-major GEMV;
- GGML direct coverage is still opt-in because current Q5_0/Q6_K/Q8_0 kernels are slower than the default mix;
- no packed multi-row low-bit layout, tuned prefetch, or mature thread scheduling comparable to llama.cpp.

Next high-leverage work should be batched prefill or redesigned packed low-bit multi-row kernels, not another narrow scalar cleanup.
