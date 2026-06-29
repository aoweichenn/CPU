# LCQI GPT-2 F32 Row-Range Optimization Summary

Command shape on `caw`:

```bash
cmake -S books/cpu-volume-3-practice -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target lcqi_tests lcqi_gpt2 -j 8
build/release/linux_cpu_inference/lcqi_tests
build/release/linux_cpu_inference/lcqi_gpt2 --benchmark --engine cached \
  --threads 0 /home/aoweichen/.cache/lcqi-gpt2-smoke "Hello, my name is" 4
```

Environment:

- Host: `caw`
- CPU: Intel Core Ultra 7 265KF
- Compiler: GCC 15.2.1
- Build mode: Release through `books/cpu-volume-3-practice`
- Model: GPT-2 F32 safetensors
- Prompt: `Hello, my name is`
- Baseline: `ab72ccb` (`Add AVX2 F32 dot kernel for LCQI GPT-2`)
- Current cached worker mode: `--threads 0`, auto-selected `16` workers
- Raw A/B report: `books/cpu-volume-3/reports/lcqi-gpt2-f32-rowrange-ab-caw.txt`
- Hotspot report: `books/cpu-volume-3/reports/lcqi-gpt2-f32-rowrange-hotspot-caw.txt`

Implementation accepted:

- Added row-range F32 kernel APIs for linear output chunks and logits-free row max chunks.
- `linear_f32_rows_unchecked` and `max_dot_f32_rows_unchecked` dispatch once per row range instead of once per output row.
- The AVX2 implementation includes a 4-row row-range path that reuses each loaded input vector across four output rows.
- `Gpt2ParallelWorkerPool::parallel_for_rows` is now a templated call boundary, removing `std::function` construction and indirect calls from each hot linear dispatch.
- GPT-2 cached linear layers, full logits, and logits-free greedy `lm_head` now call the row-range kernels.
- `lcqi_tests` compares scalar, dispatched, and explicit AVX2 row-range outputs and row-max results.

Baseline to current A/B median:

| max new tokens | baseline generate ms | current generate ms | speedup | reduction | baseline decode ms | current decode ms |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 60.6291 | 61.1045 | 0.99x | -0.78% | 22.3931 | 22.4494 |
| 16 | 153.354 | 152.574 | 1.01x | 0.51% | 115.202 | 113.670 |

This baseline-to-current run shows the optimization is small relative to normal endpoint noise for 4 generated tokens, and slightly positive for 16 generated tokens. It should not be sold as a large speedup.

Direct row-range to 4-row A/B median:

| max new tokens | row-range generate ms | row-range + 4-row generate ms | speedup | reduction | row-range decode ms | row-range + 4-row decode ms |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 62.0243 | 60.3965 | 1.03x | 2.62% | 22.8999 | 22.1052 |
| 16 | 154.087 | 151.222 | 1.02x | 1.86% | 113.587 | 112.565 |

This direct comparison is the reason the 4-row AVX2 row-range code was kept: when isolated against the row-range version, it consistently reduces median generate/decode time. The effect is still small because LCQI remains F32 row-major matvec.

Hotspot note:

The final hotspot profile still shows the same structural bottleneck. For 16 tokens, median `lm_head` is `22.3075%`, MLP `c_fc` is `22.4147%`, MLP projection is `22.448%`, QKV projection is `18.6294%`, attention projection is `11.1687%`, and cached attention itself is only `0.653274%`.

Remaining gap:

This pass reduced dispatch overhead and improved AVX2 reuse within row-major F32 matvec, but it did not introduce a llama.cpp-style packed or quantized weight format. The next material step is GGUF/block-quantized execution or at least packed F32/F16 matvec, not more small scheduling changes around the same row-major F32 weights.
