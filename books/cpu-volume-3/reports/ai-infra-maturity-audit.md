# 第三册 AI Infra 成熟度审计

## 当前判断

第三册现在能证明系统学习过 AI 推理、量化、KV cache、运行时、CPU/GPU 后端和工程验收，但还不能证明具备生产级 AI Infra 能力。原因不是正文页数不够，而是工程证据链还没有打穿。

当前最硬的资产是：

- 书稿已经建立本地推理引擎的完整知识地图。
- `linux_cpu_inference` 已有 int8 MLP 教学切片。
- 新增 tiny reference decoder，覆盖 RMSNorm、float linear、RoPE、GQA KV cache、decode attention、SwiGLU、lm head。
- 新增 int8 scalar baseline、packed layout 路径、AVX2/NEON 路径和 shape sweep benchmark。

当前最大短板是：

- 只有第一版 SIMD int8 GEMV 路径，还没有反汇编、perf counter 和系统对标。
- packed int8 标量路径只证明 layout 正确，性能收益主要依赖 SIMD 路径和 shape。
- 没有 perf counter、反汇编、IPC、cache miss、branch miss、CPU frequency、NUMA 报告。
- 没有与 oneDNN、OpenBLAS、ONNX Runtime、llama.cpp、ggml 等真实系统对照。
- 没有完整 operator registry、graph executor、memory planner、thread pool 的可运行实现。
- 没有 sanitizer、fuzz、随机 shape、边界 shape 的系统化 CI。
- 没有开源贡献记录。

## 大师级门禁

第三册要成为大师级 AI Infra 求职资产，必须补齐以下证据。

### 1. 手写 int8 GEMM/GEMV kernel

必须交付：

- scalar reference。
- packed int8 layout。
- AVX2、AVX-512 或 NEON kernel，至少一个必须在当前机器上实测。
- tiling、packing、cache blocking 说明。
- tail path：K tail、N tail、非对齐输入。
- 反汇编截图或摘要。
- perf counter：cycles、instructions、IPC、cache miss、branch miss。

通过标准：

- correctness 对齐 scalar reference。
- shape sweep 覆盖规整和非规整尺寸。
- Release 构建下至少在部分 decode 形状优于 scalar baseline。

### 2. 小型 CPU 推理 runtime

必须交付：

- tensor descriptor。
- operator registry。
- graph executor。
- memory planner。
- thread pool。
- benchmark harness。
- run trace。

通过标准：

- 能运行 tiny decoder graph。
- 能输出每个 operator 的耗时、内存和 correctness 状态。
- memory planner 能证明临时 buffer 不重叠，KV cache 不被误复用。

### 3. 真实系统对照

至少选两个：

- oneDNN。
- OpenBLAS。
- ONNX Runtime。
- llama.cpp。
- ggml。

通过标准：

- 同 shape、同 dtype 或明确 dtype 差异。
- correctness 和 perf 同时报告。
- 报告能解释差距来自 layout、kernel、线程、量化、KV cache 或 runtime 调度。

### 4. 报告体系

必须输出 CSV/JSON：

- throughput。
- latency p50/p95。
- cache miss。
- branch miss。
- IPC。
- CPU frequency。
- NUMA policy。
- batch size。
- shape sweep。

通过标准：

- 每次 benchmark 可复现。
- 报告记录 commit、compiler、flags、CPU model、thread count。

### 5. 测试和 CI

必须交付：

- golden output。
- random shape。
- boundary shape。
- fuzz。
- sanitizer。
- Debug/Release CI。

通过标准：

- correctness 测试在 CI 中稳定运行。
- sanitizer 通过。
- fuzz 至少覆盖 descriptor、shape、packing、tail。

### 6. 开源贡献

目标项目：

- llama.cpp。
- ggml。
- ONNX Runtime。
- TVM。
- MLIR。
- oneDNN 或 OpenBLAS 相关文档/benchmark。

通过标准：

- 至少一次合并的文档、benchmark、测试或小 bug 修复。

## 当前仓库下一步

优先级顺序：

1. 把 `lcqi_bench` 扩成 Release benchmark，并固定 CSV 输出。
2. 加 Linux `perf stat` wrapper，收集 cycles、instructions、cache misses、branches、branch misses。
3. 给 AVX2/NEON kernel 增加反汇编摘要和 shape 分档解释。
4. 加 OpenBLAS 或 oneDNN 对照脚本。
5. 加 random/boundary shape tests。
6. 加 sanitizer CMake preset。

这份审计的结论很直接：书稿是基础，生产级证据必须靠代码、benchmark、报告和对标来证明。
