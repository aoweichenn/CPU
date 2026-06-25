# 从 C++ 到 AI 计算：第三册

第三册用于承接 AI、AI 编译器、算子开发和本地大模型推理引擎。第二册只讨论硬件原理、多核、高性能计算、并发、锁、无锁数据结构、并行算法和分布式计算，不再直接进入 AI。

本册贯穿项目是一台能推理开源 7B 左右 decoder-only 大模型的本地引擎，并把 AI 编译器作为引擎内部主线：模型图导入、typed tensor IR、算子融合、内存规划、CPU/GPU lowering、kernel selection 和 runtime dispatch 都要能落回编译原理与底层机器账本。工程路线从 CPU 上可解释的 reference 和量化切片开始，逐步扩展到 tokenizer、模型加载、Transformer 层、KV cache、CPU SIMD/多线程后端、GPU 后端、正确性报告和性能对标。

正式书稿工程位于：

```text
source/latex/
```

构建命令：

```bash
make -C source/latex check
make -C source/latex pdf
make -C source/latex epub
```

生成文件：

```text
source/latex/main.pdf
source/latex/main.epub
```

当前保留的第一阶段切片：

```text
labs/linux_cpu_inference/
```

该目录目前包含最小 MLP/int8 权重教学切片、tiny reference decoder、int8 scalar baseline、packed layout、AVX2 SIMD 路径和 shape sweep benchmark，用来验证张量、线性层、量化权重、KV cache、oracle 和 benchmark 的基本边界。当前 x86-64 AVX2 结果见：

```text
results/lcqi-x86-avx2-benchmark-2026-06-25.csv
results/lcqi-x86-avx2-decode4096-2026-06-25.csv
```

它仍然不是生产级 AI Infra。生产级门禁见：

```text
reports/ai-infra-maturity-audit.md
```

第三册后续必须把 lab 推进到“小型 CPU 推理 runtime + 手写高性能算子 + 严格 benchmark 报告 + 真实系统对标”，否则只能证明系统学习，不能证明生产级能力。
