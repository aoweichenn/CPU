# LCQI SmolLM2 Same-Input Gap Analysis

Date: 2026-06-30

Host: `caw`, Linux WSL2 x86-64, `nproc=20`.

Model: `SmolLM2-135M-Instruct-Q4_K_M.gguf`

Model SHA256: `2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`

llama.cpp commit: `b3fed31b99f9bd37725833674252bccb429bb183`

Raw artifact: `books/cpu-volume-3/reports/lcqi-smollm2-same-input-compare-caw.txt`

## Measurement Contract

The comparison used the same model file, the same user prompt `Hello`, the same expanded SmolLM2 chat prompt, and `max_new=2`.

Expanded prompt:

```text
<|im_start|>system
You are a helpful AI assistant named SmolLM, trained by Hugging Face<|im_end|>
<|im_start|>user
Hello<|im_end|>
<|im_start|>assistant
```

LCQI prompt ids and llama.cpp `llama-tokenize` ids matched exactly:

```text
1 9690 198 2683 359 253 5356 5646 11173 3365 3511 308 34519 28 7018 411 407 19712 8182 2 198 1 4093 198 19556 2 198 1 520 9531 198
```

So the large gap is not a prompt/template/tokenizer mismatch.

## caw Median Results

| Path | Prefill | Decode step | Notes |
| --- | ---: | ---: | --- |
| LCQI `lcqi_llama_gguf` | `1278.47 ms` for 31 prompt tokens, `24.25 tok/s` | `44.64 ms`, `22.40 tok/s` | `f32_dequantized_reference` |
| llama.cpp `llama-simple` | `22.92 ms` for 31 prompt tokens, `1352.47 tok/s` | `2.84 ms`, `351.62 tok/s` | pinned llama.cpp CPU path |
| Ratio LCQI / llama.cpp | `55.78x` slower | `15.72x` slower | same token ids |

The synthetic same-token-count `llama-bench -p 31 -n 2` reference reported `pp31=1463.87 tok/s` and `tg2=508.91 tok/s`.

## Why The Gap Is Still Large

LCQI still runs the SmolLM2 GGUF path as a correctness-first reference engine:

- `llama_gguf.cpp` loads each GGUF tensor and immediately dequantizes it into `float`. The report shows `103,668,480` quantized bytes becoming `538,060,032` f32 bytes, a `5.19x` weight-memory expansion.
- `llama_gguf_generate_greedy` processes the 31-token prompt one token at a time. There is no batched prompt graph, so prefill repeats scalar decode work 31 times instead of using prompt batching.
- `forward_llama_layer` calls generic `linear_f32` for Q, K, V, output, gate, up, and down projections. The generic path is scalar row-by-row f32 matvec, not packed quantized matmul.
- `attention_decode` allocates and fills temporary score/probability vectors per step and uses f32 KV cache. llama.cpp uses a planned graph/backend path and compact KV storage.
- llama.cpp keeps GGUF quantized block weights in the hot path, uses ggml graph scheduling, tuned CPU kernels, prompt batching, thread coordination, and compact f16 KV cache.

The important split is prefill versus decode. Decode is already much closer than prefill because both sides are token-by-token after KV cache exists, but LCQI is still `15.72x` slower from f32 dequantized weights and scalar kernels. Prefill is `55.78x` slower because LCQI also lacks batched prompt evaluation.

## Next Optimization Priority

1. Integrate quantized GGUF block matvec into the full decoder path instead of only the isolated `lcqi_gguf_q4_bench` microbenchmark.
2. Add batched prompt prefill for the same decoder block so 31 prompt tokens do not execute as 31 independent decode steps.
3. Add a row-parallel or tile-parallel execution policy for the LLaMA linear projections, separate from the GPT-2-only parallel path.
4. Store KV cache in f16 or quantized-compatible compact form and remove per-step attention temporary allocation.
5. Add a benchmark mode that reports per-layer projection timing for SmolLM2, so the next optimization is chosen from measured hotspots rather than broad engine-level ratios.
