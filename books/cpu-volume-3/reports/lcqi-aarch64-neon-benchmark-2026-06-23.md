# LCQI aarch64 NEON benchmark

## Scope

This report records the first reproducible SIMD evidence for `linux_cpu_inference` on the current aarch64 machine. It does not claim x86 AVX2 performance; AVX2 is compiled only on x86 targets.

## Environment

- Commit before this change: `6e63f0a`.
- OS: `Linux localhost 6.17.0-PRoot-Distro ... aarch64 GNU/Linux`.
- Compiler: `c++ (GCC) 16.1.1 20260515`.
- CMake: `4.3.0`.
- Build: Release.
- Tests: Debug and Release `ctest` passed.
- Perf counters: unavailable in this environment.

## Result

CSV artifact:

```text
books/cpu-volume-3/results/lcqi-aarch64-neon-2026-06-23.csv
```

Summary:

| shape | scalar us | packed scalar us | NEON us | max diff |
| --- | ---: | ---: | ---: | ---: |
| 128 x 128 | 15.312 | 33.731 | 5.30755 | 0 |
| 256 x 256 | 63.6661 | 47.7086 | 14.5154 | 0 |
| 512 x 512 | 280.195 | 173.169 | 36.6846 | 0 |
| 513 x 257 | 136.218 | 93.2565 | 20.1917 | 0 |
| 1024 x 4096 | 4757.07 | 3014.18 | 702.123 | 0 |

The packed scalar backend proves the packed layout and tail handling; it is not guaranteed to beat the row-major scalar loop on every shape. The NEON backend is the first local SIMD kernel with measured speedup and scalar-equivalent output.

## Remaining Gaps

- No perf counter, IPC, cache miss, branch miss, or frequency data yet.
- No disassembly annotation yet.
- No x86 AVX2 validation on this aarch64 host.
- No OpenBLAS, oneDNN, ONNX Runtime, llama.cpp, or ggml comparison yet.
- No sanitizer/fuzz CI yet.
