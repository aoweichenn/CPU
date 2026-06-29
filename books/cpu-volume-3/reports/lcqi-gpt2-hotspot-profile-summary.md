# LCQI GPT-2 Pre-Threading Hotspot Profile Summary

Command on `caw`:

```bash
python3 books/cpu-volume-3/tools/run_gpt2_hotspot_profile.py \
  --model-dir /home/aoweichen/.cache/lcqi-gpt2-smoke \
  --build-dir /home/aoweichen/lcqi-cpu-hotspot/build/lcqi-hotspot-profile \
  --report /home/aoweichen/lcqi-gpt2-hotspot-profile-caw.txt \
  --token-counts 4,16 --rounds 3 --jobs 8 --timeout-seconds 900
```

Environment:

- Host: `caw`
- OS: Linux `6.6.87.2-microsoft-standard-WSL2`
- CPU: Intel Core Ultra 7 265KF, 20 cores, AVX2/AVX-VNNI available
- Compiler: GCC 15.2.1
- CMake: 3.31.11
- Build mode: Release through `books/cpu-volume-3-practice`
- Model: GPT-2 F32 safetensors, prompt `Hello, my name is`

Generation-stage hotspot median:

| max new tokens | generate median ms | generated token/s median | lm head % | MLP fc % | MLP projection % | QKV projection % | attention projection % | attention % |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 399.484 | 10.0129 | 30.2423 | 23.0116 | 23.4112 | 17.2127 | 5.79324 | 0.0558254 |
| 16 | 985.792 | 16.2306 | 30.2911 | 22.8772 | 23.3552 | 17.1256 | 5.93439 | 0.111537 |

Interpretation:

- The decode hot path is dominated by scalar F32 matrix/vector work, not causal attention.
- `lm_head` alone is about 30% of generation-stage time because every generated token scans `50257 x 768` F32 weights to find the greedy token.
- The two MLP linear projections together are about 46% of generation-stage time.
- QKV projection is about 17%; attention projection is about 6%.
- Cached attention itself is below 0.2% for this short-context GPT-2 workload, so optimizing attention first would not explain the current slowdown.

A coarse `perf record -F 99 -g` run on `caw` collected 70 task-clock samples for one cached 4-token run. With model loading included, samples were roughly split as `run_gpt2_cached_step_unchecked` 55.71%, `transpose_hf_conv1d` 28.57%, `read_safetensor_f32_tensor` 7.14%, and `__memset_avx2_unaligned_erms` 2.86%. That is useful for separating startup/loading cost from generation cost, but it is too coarse for deciding which decode kernel to optimize. The generation-stage profile above isolates the runtime decode loop and is the correct evidence for the next inference-kernel optimization.

This is the pre-threading profile used to choose the next optimization target. The
accepted threaded implementation and after-profile are summarized in
`books/cpu-volume-3/reports/lcqi-gpt2-threaded-optimization-summary.md`.

Next optimization priority from this profile:

1. Optimize `lm_head` greedy argmax first: it is one independent dot product per vocab row and is a clean target for row parallelism, SIMD dot products, packed layout, or quantized weights.
2. Optimize the GPT-2 F32 linear kernel used by MLP and QKV: current code is scalar row-major matvec with no threading and no platform-specific vector kernel.
3. Keep attention changes lower priority until context length grows enough for attention work to become visible in the profile.
