# LCQI GPT-2 F32 Dot Optimization Summary

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
- Baseline: `cb4d89f` (`Optimize LCQI GPT-2 cached row kernels`)
- Current cached worker mode: `--threads 0`, auto-selected `16` workers
- Raw A/B report: `books/cpu-volume-3/reports/lcqi-gpt2-f32-dot-ab-caw.txt`
- Hotspot report: `books/cpu-volume-3/reports/lcqi-gpt2-f32-dot-hotspot-caw.txt`

Generation A/B median:

| max new tokens | baseline generate ms | current generate ms | speedup | reduction | baseline gen tok/s | current gen tok/s |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 74.4247 | 62.3628 | 1.19x | 16.21% | 53.7456 | 64.1408 |
| 16 | 181.257 | 157.230 | 1.15x | 13.26% | 88.2723 | 101.762 |

Decode-stage median:

| max new tokens | baseline decode ms | current decode ms | speedup | reduction | baseline decode tok/s | current decode tok/s |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 26.1887 | 23.0640 | 1.14x | 11.93% | 114.553 | 130.073 |
| 16 | 132.201 | 117.734 | 1.12x | 10.94% | 113.463 | 127.405 |

Implementation accepted:

- Added `include/lcqi/f32_kernels.hpp`, `src/f32_kernels.cpp`, and `src/f32_kernels_avx2.cpp`.
- `dot_f32_unchecked` dispatches to AVX2/FMA when the x86 build enables `LCQI_ENABLE_AVX2`, the CPU reports AVX2/FMA support, and the vector is large enough to amortize dispatch.
- GPT-2 cached unchecked linear rows, full logits, and logits-free greedy `lm_head` argmax now use the F32 dot kernel boundary.
- The full-prefix checked reference path still keeps the simple scalar `linear_f32` loop for readability and golden comparison.
- `lcqi_tests` compares scalar, dispatch, and explicit AVX2 F32 dot output when AVX2 is available, and rejects mismatched input spans.

Hotspot note:

Single profile samples after the change show the same structural bottleneck: `lm_head`, MLP `c_fc`, MLP `c_proj`, QKV projection, and attention output projection still dominate; cached attention itself remains below 1%. The optimization shortens each row dot, but it does not change the higher-level work distribution.

Remaining gap:

This is still F32 row-major matvec. It does not introduce packed weights, quantized weights, GGUF block formats, mmap/lazy loading, NUMA placement, thread affinity, batching, paged KV cache, or a model loader compatible with llama.cpp assets. The next large step toward llama.cpp/ggml is packed or quantized weight execution, not more scalar F32 structure.
