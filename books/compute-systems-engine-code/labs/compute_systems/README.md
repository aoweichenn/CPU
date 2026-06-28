# Compute Systems Models And Runtime Labs

This lab is the standalone code-practice version of the CPU volume 2 compute
systems experiments. It gives each chapter a runnable model, then adds the
concurrency, wait-channel, task-runtime, and futex probes that later stages use.

## What This Lab Covers

- Per-chapter runnable models in `chapter_models.hpp/.cpp`.
- Sequential and parallel reduction.
- Mutex and atomic counters.
- Closable bounded MPMC queue with wait and close reports.
- Semaphore, barrier, future/promise, and atomic wait contracts.
- `eventfd`/`epoll` readiness and drain contracts.
- Futex wait/wake probes.
- Minimal task runtime, same-pool future wait probe, continuation probe, and CSV report.

## Per-Chapter Model Map

| Chapter | Function | What the model fixes |
| --- | --- | --- |
| 1 | `model_ch01_input_pipeline` | Accepted/rejected records and checksum |
| 2 | `model_ch02_branch_predictor` | Branch history and misprediction count |
| 3 | `model_ch03_cache_tlb` | Cache line, page, and TLB miss accounting |
| 4 | `model_ch04_numa_coherence` | Local/remote access and coherence transfers |
| 5 | `model_ch05_roofline` | FLOPs, bytes, bandwidth, and attainable GFLOPS |
| 5b | `model_ch05b_pmu_simd_evidence` | PMU/SIMD evidence completeness |
| 6 | `model_ch06_layout_locality` | AoS/SoA bytes touched |
| 7 | `model_ch07_simd_ilp` | Vector iterations, scalar tail, and lane utilization |
| 8 | `model_ch08_map_filter_reduce` | Filtered records and reduced sum |
| 9 | `model_ch09_thread_scheduling` | Worker load and imbalance |
| 9b | `model_ch09b_linux_wait_wake` | Spurious wakeups versus real completion |
| 10 | `model_ch10_sync_backpressure` | Capacity, production, consumption, and backpressure |
| 11 | `model_ch11_atomic_publication` | Stale reads and monotonic version observations |
| 12 | `model_ch12_spsc_ring` | Push/pop/full/empty ring-buffer states |
| 12b | `model_ch12b_epoch_reclamation` | Reclaimed versus deferred retired nodes |
| 13 | `model_ch13_reduce_scan_partition` | Reduce, scan, and partition outputs |
| 14 | `model_ch14_work_stealing` | Stolen and completed tasks |
| 15 | `model_ch15_async_io_backpressure` | Submitted/completed I/O and in-flight pressure |
| 16 | `model_ch16_distributed_attempts` | Logical tasks, duplicates, and stale generations |
| 17 | `model_ch17_quorum_commit` | Quorum size and commit decision |
| 18 | `model_ch18_checkpoint_recovery` | Checkpoint count and replay range |

## Build And Run

```bash
cmake -S books/compute-systems-engine-code -B books/compute-systems-engine-code/build/reference-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build books/compute-systems-engine-code/build/reference-debug
ctest --test-dir books/compute-systems-engine-code/build/reference-debug --output-on-failure
```

Useful executables:

```bash
books/compute-systems-engine-code/build/reference-debug/labs/compute_systems/cse_compsys_demo
books/compute-systems-engine-code/build/reference-debug/labs/compute_systems/cse_compsys_chapter_models_probe
books/compute-systems-engine-code/build/reference-debug/labs/compute_systems/cse_compsys_wait_channels_probe
books/compute-systems-engine-code/build/reference-debug/labs/compute_systems/cse_compsys_task_runtime_probe
books/compute-systems-engine-code/build/reference-debug/labs/compute_systems/cse_compsys_futex_lab_probe
```
