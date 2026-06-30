# LCQI SmolLM2 GGML Direct Analysis

Date: 2026-06-30
Host: caw
Raw report: `books/cpu-volume-3/reports/lcqi-smollm2-ggml-direct-compare-caw.txt`
Follow-up report: `books/cpu-volume-3/reports/lcqi-smollm2-threaded-q4-compare-caw.txt`
GGML row-range follow-up report: `books/cpu-volume-3/reports/lcqi-smollm2-ggml-threaded-compare-caw.txt`

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

The best next work was not to simply add more scalar quantized formats. The next change connected the existing f32 fallback and default Q4_K direct path to a persistent row worker pool, then added a Q4_K row-range unchecked entry so the default direct path was not left single-threaded after f32 fallback became parallel.

Follow-up caw evidence, same model and same 31-token prompt:

- serial default Q4_K direct prefill median: `363.759 ms`
- serial default Q4_K direct decode step median: `15.1652 ms`
- threaded default worker count median: `8`
- threaded default Q4_K direct prefill median: `161.436 ms`
- threaded default Q4_K direct decode step median: `7.26678 ms`
- threaded f32 fallback hotspot median: `146.731 ms`, down from serial `338.173 ms`
- threaded Q4_K direct hotspot median: `4.98589 ms`, down from serial `19.6195 ms`
- threaded speedup over serial: prefill `2.253271x`, decode step `2.086922x`, f32 hotspot `2.304714x`
- Q4_K direct, compared with Q4 direct off in the same threaded binary: prefill `1.087948x`, decode step `0.995660x`, `w_down` `1.645349x`
- LCQI threaded default over llama.cpp same-input: prefill `7.381619x` slower, decode step `2.471694x` slower

The first threaded attempt exposed an important interaction: after f32 fallback became row-parallel, the single-thread Q4_K direct path could lose to the parallel f32 fallback in some decode measurements. The fix was not to disable Q4_K direct, but to make Q4_K direct obey the same output-row partitioning. This is why the final report must be `lcqi-smollm2-threaded-q4-compare-caw.txt`, not the intermediate `lcqi-smollm2-threaded-compare-caw.txt`.

## GGML Direct Row-Range Follow-Up

The next change applied the same row partitioning idea to the opt-in GGML direct path. `matvec_ggml_quantized_q8_0_rows_unchecked` computes only `[row_begin,row_end)` after the caller has already validated the tensor shape and byte span. `linear_ggml_quantized_direct_parallel` then sends those row ranges through `LlamaParallelWorkerPool`.

Final caw evidence, same model and same 31-token prompt:

- serial default Q4_K direct prefill median: `364.282 ms`
- serial default Q4_K direct decode step median: `14.9794 ms`
- threaded default worker count median: `8`
- threaded default Q4_K direct prefill median: `162.492 ms`
- threaded default Q4_K direct decode step median: `6.41487 ms`
- threaded f32 fallback hotspot median: `148.132 ms`, down from serial `338.779 ms`
- threaded Q4_K direct hotspot median: `5.39028 ms`, down from serial `19.6660 ms`
- threaded speedup over serial: prefill `2.241846x`, decode step `2.335106x`, f32 hotspot `2.287008x`
- Q4_K direct, compared with Q4 direct off in the same threaded binary: prefill `1.073148x`, decode step `1.057014x`, `w_down` `1.642115x`
- LCQI threaded default over llama.cpp same-input: prefill `7.089529x` slower, decode step `2.250832x` slower
- experimental GGML direct row-range prefill median: `242.149 ms`
- experimental GGML direct row-range decode step median: `8.98937 ms`
- experimental GGML direct hotspot median: `228.300 ms`
- experimental GGML direct tensors: `194`
- experimental GGML direct calls: `6208`
- experimental direct quantized weight byte share: `0.708479`

This is a real improvement over the first opt-in GGML direct experiment: prefill dropped from `1211.710 ms` to `242.149 ms`, and decode step dropped from `42.4420 ms` to `8.98937 ms`. The improvement confirms that the first failure was not just "low-bit direct is bad"; one major bottleneck was that the direct path did not participate in row-level parallel scheduling.

It is still not the default path. The default Q4_K plus f32 fallback mix is `162.492 ms` prefill and `6.41487 ms` decode step. The remaining GGML direct hotspot is `228.300 ms`, with slower `wk`, `wv`, and `w_down` regions than the default mix. That points to the next real work: packed multi-row layout, full SIMD coverage for the dominant formats including `Q6_K`, less per-block scale overhead, and prefetch. Simply increasing direct-format coverage is no longer the bottleneck statement.

The final report also records `llama_cpp_source_git_commit=unavailable`, `llama_cpp_source_commit_verified=false`, and `llama_cpp_skip_build=true` because caw reused an existing offline llama.cpp binary cache without `.git`. The model identity was still verified by bytes and SHA-256, but the binary provenance is explicitly weaker than a fresh source checkout.

The remaining evidence points to two higher-leverage directions:

1. Batched prefill: LCQI currently processes prompt tokens as repeated decode steps, while llama.cpp evaluates the prompt through batched graph execution.
2. Packed multi-row low-bit kernels: redesign GGML direct execution around multi-row layout, SIMD coverage for the dominant formats, prefetch, and thread partitioning before trying to make it a default path.

Until those are implemented and measured, `LCQI_LLAMA_GGML_DIRECT=1` remains an opt-in experiment and a source-reading case study, not a production default.
