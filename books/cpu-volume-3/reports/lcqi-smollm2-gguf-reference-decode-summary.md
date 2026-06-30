# LCQI SmolLM2 GGUF Reference Decode Summary

Status: historical baseline. The latest same-input default/experimental direct-path evidence is in `books/cpu-volume-3/reports/lcqi-smollm2-ggml-direct-analysis.md`.

Artifact: `books/cpu-volume-3/results/lcqi-smollm2-gguf-reference-decode-caw.txt`

Remote validation: `caw`

Model: `SmolLM2-135M-Instruct-Q4_K_M.gguf`

Implementation path: LCQI self-hosted `lcqi_llama_gguf`, not llama.cpp.

Key result:

- Loaded `272` GGUF tensors from the same SmolLM2 Q4_K_M file used by the llama.cpp smoke.
- Tensor formats covered by the implementation: `F32`, `Q8_0`, `Q5_0`, `Q6_K`, `Q4_K`.
- Weight execution mode: `f32_dequantized_reference`.
- Quantized GGUF tensor bytes: `103668480`.
- Decoded f32 weight bytes: `538060032`.
- Text prompt smoke generated `Hello!` after the chat prompt.
- Prompt tokens: `31`.
- Generated tokens: `2`.
- First-token throughput in this short prompt run: `0.78061 tok/s`.
- Continuing decode throughput in this short prompt run: `22.1886 tok/s`.

Interpretation:

This is a correctness and integration baseline, not a claim that LCQI has llama.cpp-level performance. LCQI now owns the full GGUF loading and LLaMA-style decode path for this small model, but it currently expands quantized weights to f32 and then uses generic f32 matvec. llama.cpp remains much faster because it keeps quantized blocks in the hot path and uses mature packed kernels, threading, prefetching, scheduling, and serving/runtime machinery.

Pre-Q4-direct same-input comparison:

- Raw artifact: `books/cpu-volume-3/reports/lcqi-smollm2-same-input-compare-caw.txt`.
- Same model, same expanded SmolLM2 chat prompt, same `max-new=2`.
- LCQI prompt ids and llama.cpp `llama-tokenize` ids matched exactly: `31` tokens.
- caw median prefill: LCQI `1278.47 ms` vs llama.cpp `22.92 ms`, LCQI `55.78x` slower.
- caw median decode step: LCQI `44.64 ms` vs llama.cpp `2.84 ms`, LCQI `15.72x` slower.
- Pre-Q4-direct root cause: LCQI was still `f32_dequantized_reference`, one-token-at-a-time prefill; llama.cpp used quantized block execution, prompt batching, ggml graph scheduling, compact KV cache, and tuned CPU kernels.

Follow-up Q4_K direct decoder integration:

- Raw artifact: `books/cpu-volume-3/reports/lcqi-smollm2-q4-direct-compare-caw.txt`.
- Analysis: `books/cpu-volume-3/reports/lcqi-smollm2-q4-direct-analysis.md`.
- LCQI now keeps shape-compatible `Q4_K` linear tensors as raw GGUF block bytes and dispatches them through `matvec_q4_k_q8` in the real decoder.
- Same-binary caw A/B: Q4 direct off prefill `423.56 ms`, decode step `17.21 ms`, `w_down` hotspot `98.10 ms`; Q4 direct on prefill `396.93 ms`, decode step `16.44 ms`, `w_down` hotspot `67.50 ms`.
- Conservative feature speedup: prefill `1.067x`, decode step `1.047x`, `w_down` `1.453x`; f32 materialized weight bytes saved `56,623,104`.
- Remaining gap: only `7.68%` of LCQI-loaded weight bytes enter the direct Q4_K path; `6208` linear calls still use f32 fallback, and prefill remains one token at a time.
