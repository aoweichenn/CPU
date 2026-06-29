# LCQI GPT-2 Spin/Yield Worker-Pool Optimization Summary

This is the accepted optimization after the packed F32 and 8-row AVX2 candidates were rejected by `caw` data.

Environment:

- Host: `caw`
- Compiler: GCC 15.2.1
- Build: Release through `books/cpu-volume-3-practice`
- Model: GPT-2 F32 safetensors at `/home/aoweichen/.cache/lcqi-gpt2-smoke`
- Prompt: `Hello, my name is`
- Rounds: 9

Accepted change:

- Cap automatic GPT-2 workers at 8.
- Replace the cached-decoder worker-pool condition-variable hand-off with an atomic generation hand-off and bounded spin/yield waiting.
- Keep the pool session-local to `Gpt2CachedGreedyDecoder`.

Median generation-stage results:

| comparison | 4 tokens | 16 tokens | 64 tokens |
| --- | ---: | ---: | ---: |
| baseline auto16 -> spinpool auto8 | 53.7887 ms -> 37.5403 ms, 30.21% | 146.930 ms -> 104.107 ms, 29.15% | 510.650 ms -> 377.233 ms, 26.13% |
| auto8 condvar -> spinpool auto8 | 50.5068 ms -> 37.5403 ms, 25.67% | 147.796 ms -> 104.107 ms, 29.56% | 474.141 ms -> 377.233 ms, 20.44% |
| auto8 fixed8 -> spinpool fixed8 | 47.5870 ms -> 38.9927 ms, 18.06% | 135.136 ms -> 103.683 ms, 23.28% | 463.330 ms -> 373.601 ms, 19.37% |

Interpretation:

- The gain is not primarily from changing the mathematical kernel. It comes from reducing the cost of hundreds of small parallel-region hand-offs in cached GPT-2 decode.
- The largest hotspot reductions appear in QKV, attention projection, and both MLP projections because those paths invoke `parallel_for_rows` at every layer and token.
- `lm_head` still remains a large hotspot; it improves only modestly because it is one larger vocab scan per predicted token and is less dominated by worker wake-up overhead.
- The result does not close the gap to llama.cpp. LCQI still lacks GGUF/block quantization, mature packed quantized matvec, mmap-oriented loading, NUMA/affinity handling, and production scheduler tuning.

Rejected candidates:

- Packed F32: rejected by `books/cpu-volume-3/reports/lcqi-gpt2-packed-f32-ab-caw.txt`.
- 8-row AVX2 row-range: rejected by `books/cpu-volume-3/reports/lcqi-gpt2-spinpool-ab-caw.txt` because the measured result was noise-level or worse.

