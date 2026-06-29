# LCQI GPT-2 Threaded Optimization Summary

Command shape on `caw`:

```bash
# Baseline source: git archive of 73f40c7
# Current source: current working tree with threaded cached F32 row kernels
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
- Baseline: `73f40c7` (`Profile LCQI GPT-2 hotspots on caw`)
- Current cached worker mode: `--threads 0`, auto-selected `16` workers
- Raw A/B report: `books/cpu-volume-3/reports/lcqi-gpt2-threaded-manual-ab-caw.txt`
- Raw hotspot report: `books/cpu-volume-3/reports/lcqi-gpt2-threaded-hotspot-caw.txt`

Generation A/B median:

| max new tokens | baseline generate ms | current generate ms | speedup | reduction | baseline gen tok/s | current gen tok/s |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 395.397 | 76.4141 | 5.17x | 80.67% | 10.1164 | 52.3464 |
| 16 | 989.060 | 185.479 | 5.33x | 81.25% | 16.1770 | 86.2633 |

Decode-stage median:

| max new tokens | baseline decode ms | current decode ms | baseline decode tok/s | current decode tok/s |
| ---: | ---: | ---: | ---: | ---: |
| 4 | 147.928 | 27.5817 | 20.2801 | 108.768 |
| 16 | 741.359 | 136.685 | 20.2331 | 109.741 |

Optimized hotspot median:

| max new tokens | generate median ms | workers | lm head % | MLP fc % | MLP projection % | QKV projection % | attention projection % | attention % |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 75.9807 | 16 | 20.4188 | 22.9213 | 22.9965 | 19.2513 | 12.2371 | 0.272274 |
| 16 | 184.780 | 16 | 19.8852 | 22.8183 | 22.7715 | 19.5133 | 12.6247 | 0.566485 |

Implementation accepted:

- `Gpt2CachedGreedyDecoder` owns a session-scoped `Gpt2ParallelWorkerPool`.
- `--threads 0` uses `std::thread::hardware_concurrency()` capped at 16 workers; `--threads 1` disables the pool; `--threads N` fixes the worker count.
- Cached GPT-2 QKV projection, attention output projection, MLP `c_fc`, MLP `c_proj`, full logits, and logits-free greedy `lm_head` argmax use row-parallel F32 dot products when shape thresholds justify dispatch.
- The public checked full-prefix path remains scalar and easy to read; the optimized path is the cached generation session.
- Tiny GPT-2 tests now include a forced two-worker decoder with `parallel_min_rows=1`, and compare its logits and predicted token against the full-prefix golden result.

Threshold note:

The first auto threshold only parallelized shapes with at least one million multiply-adds. That improved `lm_head`, MLP, and QKV, but it left the `768 x 768` attention output projection serial. The follow-up hotspot showed attention projection rising to about 25% of total generation time, so the final threshold also parallelizes row-major projections when both row count and column count are at least 512. The final hotspot brings attention projection back to about 12% while cached attention itself remains below 1%.

Remaining gap:

This change adds coarse row parallelism around scalar F32 dot products. It does not yet add SIMD dot kernels, packed weights, quantized weights, mmap/lazy load, NUMA placement, thread affinity, batching, paged KV cache, or a GGUF loader. The next kernel-level step is to replace the scalar inner dot loop with a measured SIMD or packed/quantized matvec path and compare it against llama.cpp/ggml-style kernels.
