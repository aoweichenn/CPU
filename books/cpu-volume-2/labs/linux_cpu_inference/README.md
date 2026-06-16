# Linux CPU Quantized Inference

这是第二册的贯穿大作业第一阶段：Linux 本地 CPU 量化推理引擎。

目标平台是本地 Linux 机器。普通 x86-64 Linux 台式机、笔记本或服务器是完整实验环境；Termux 可以用于阅读、构建小模型和跑基础 CTest，但 perf、NUMA、线程亲和性和硬件性能计数器通常会受系统权限与内核能力限制。

当前版本支持：

- 教学文本模型格式 `LCQI_MODEL_V1`
- 两层 MLP
- int8 权重加 per-layer scale
- float 输入和激活
- 量化线性层、ReLU、分类 argmax
- CLI 推理和简单 benchmark
- CTest 正确性测试

构建：

```bash
cmake -S books/cpu-volume-2 -B books/cpu-volume-2/build/lcqi-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build books/cpu-volume-2/build/lcqi-debug
ctest --test-dir books/cpu-volume-2/build/lcqi-debug --output-on-failure
```

运行：

```bash
books/cpu-volume-2/build/lcqi-debug/labs/linux_cpu_inference/lcqi_cli \
  books/cpu-volume-2/labs/linux_cpu_inference/models/tiny_mlp_i8.txt \
  1.0 2.0 -1.0 0.5 1000
```
