# Linux CPU Quantized Inference

这是第三册贯穿项目的第一阶段切片：Linux 本地 CPU 量化线性层和最小推理流程。第三册的贯穿目标不是这个 MLP 本身，而是一台能推理开源 7B 左右 decoder-only 大模型的本地引擎，并逐步覆盖 tokenizer、模型加载、Transformer 层、KV cache、CPU/GPU 后端、正确性报告和性能对标。

目标平台是 x86-64 Linux 实验环境。完整性能报告应记录 CPU 型号、内核版本、编译器版本、是否能使用 PMU、NUMA 和线程亲和能力。若运行环境缺少 perf、NUMA、线程亲和性或硬件性能计数器权限，报告必须把相关结论降级为“未验证”，不能把源码路径存在当作实测性能证据。后续 GPU 后端会作为独立 backend 接入，不改变当前切片的语义合同。

当前版本支持：

- 教学文本模型格式 `LCQI_MODEL_V1`
- 最小 tokenizer 合同格式 `LCQI_TOKENIZER_V1`
- safetensors header manifest 解析：metadata、tensor name、dtype、shape、data offsets、offset 边界和 dtype/shape 字节数校验
- 两层 MLP 教学切片
- int8 权重加 per-layer scale
- float 输入和激活
- 量化线性层、ReLU、分类 argmax
- int8 linear scalar baseline、packed layout、AVX2 路径和 shape sweep benchmark
- tiny reference decoder：RMSNorm、float linear、RoPE、GQA KV cache、decode attention、SwiGLU、lm head
- reference trace CSV：从 prompt/tokenizer 合同到 logits/sampler/token，逐 checkpoint 输出 shape、stride、dtype、layout、checksum、max_abs、values 摘要
- 7B ledger CSV：输出典型 7B shape 的 FLOPs、KV 容量、权重/KV 字节和 bandwidth-only tokens/s 上限
- serving SLO probe CSV：输出 admission、batch state、KV 预算、queue wait、TTFT、TPOT、取消回收和拒绝率
- 真实 small 开源模型冒烟：通过 llama.cpp 加载 SmolLM2-135M-Instruct 的 GGUF 量化文件，在本地 CPU 上生成一段 assistant 文本，并把模型 hash、命令、输出和性能行写入报告
- CLI 推理和简单 benchmark
- CTest 正确性测试

后续扩展方向：

- 真实 7B 级模型 metadata 和 tokenizer
- 完整 BPE/SentencePiece tokenizer、GGUF/safetensors 权重导入、分片加载和 name mapping
- 多层 decoder-only Transformer reference path
- KV cache 分页、内存规划和可选量化
- CPU packed weight、SIMD、线程和 NUMA 优化
- GPU backend adapter 和 device kernel
- 与现有框架的 prefill/decode/内存/误差对标报告

构建：

```bash
cmake -S books/cpu-volume-3 -B books/cpu-volume-3/build/lcqi-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build books/cpu-volume-3/build/lcqi-debug
ctest --test-dir books/cpu-volume-3/build/lcqi-debug --output-on-failure
```

运行：

```bash
books/cpu-volume-3/build/lcqi-debug/labs/linux_cpu_inference/lcqi_cli \
  books/cpu-volume-3/labs/linux_cpu_inference/models/tiny_mlp_i8.txt \
  1.0 2.0 -1.0 0.5 1000
```

kernel benchmark：

```bash
books/cpu-volume-3/build/lcqi-debug/labs/linux_cpu_inference/lcqi_bench 200
```

输出 CSV 字段：

```text
target_arch,input_size,output_size,output_block,repeat,backend,available,average_us,max_abs_diff,checksum
```

当前 benchmark 按 backend 逐行输出：`scalar`、`packed_scalar`、`avx2`。x86-64 构建会编译 AVX2 路径；不支持 AVX2/FMA 的机器会把该 backend 标为 `available=0`，避免把源码存在误报成当前环境实测性能。

这还不是生产级高性能 kernel。当前 benchmark 建立 scalar baseline、packed layout、AVX2 基线正确性和 shape sweep 口径。要达到完整 AI Infra 证据，还必须继续补反汇编分析、AVX-512、perf counter、oneDNN/OpenBLAS/llama.cpp/ggml 对照、sanitizer、fuzz 和 CI。

safetensors manifest gate：

`lcqi_tests` 会运行时生成一个最小 safetensors fixture，验证 header 长度、`__metadata__`、tensor name、dtype、shape、data offsets、payload 边界和 dtype/shape 推导出的字节数。它还会生成 offset 超出 payload、dtype/shape 字节数不匹配的坏文件，确认加载器会拒绝。这个 gate 只覆盖模型资产目录表，不等于已经支持完整 LLaMA/Qwen 权重映射、分片合并或 mmap 执行；后续真实模型加载必须在这个 manifest 上继续接 name mapping、shape verifier、quant descriptor 和 first token trace。

reference decoder trace：

```bash
books/cpu-volume-3/build/lcqi-debug/labs/linux_cpu_inference/lcqi_trace
```

输出 CSV 字段：

```text
name,token_position,layer_id,shape,stride,dtype,layout,checksum,max_abs,values
```

这条 trace 固定 prompt fixture `tok1 tok2 tok3`，tokenizer 输出完整 `tokens=[0,1,2,3,4]`，其中 `0/4` 是 BOS/EOS；decoder trace 会剥离 BOS/EOS 后进入 tiny decoder。这样报告能同时固定 tokenizer 合同和 decoder 输入合同，避免把二者混成一个不可解释的 token 数组。随后 trace 把 embedding、RMSNorm、Q/K/V、RoPE、KV cache slot、attention、SwiGLU、layer output、final norm、logits、argmax sampler 和 generated token 串成可回归检查的硬链路。`lcqi_tests` 会检查 tokenizer contract hash、关键 checkpoint 的 shape、stride、dtype、layout 和 logits golden，CTest 也会直接运行 `lcqi_trace`。

当前 tokenizer fixture 的关键输出：

```text
tokenizer,0,-1,5,1,i32,contiguous,10,4,0;1;2;3;4
tokenizer_contract,0,-1,scalar,scalar,u64,fnv1a,13452902845388333734,0,tiny-chat-template-v1
```

7B ledger：

```bash
books/cpu-volume-3/build/lcqi-debug/labs/linux_cpu_inference/lcqi_ledger \
  > books/cpu-volume-3/results/lcqi-sevenb-ledger-2026-06-24.csv
```

输出 CSV 字段：

```text
section,item,value,unit,notes
```

这份账本固定 `L=32,H=4096,n_q=32,n_kv=8,head_dim=128,context=4096`，报告 decode FLOPs、KV 容量、fp16/int8/int4 每 token 字节和不同有效带宽下的 tokens/s 上限。CTest 会检查 `int4_at_100_gbps=23.602 tokens/s` 这一行，防止账本公式漂移。

serving SLO probe：

```bash
books/cpu-volume-3/build/lcqi-debug/labs/linux_cpu_inference/lcqi_serving_probe \
  > books/cpu-volume-3/results/lcqi-serving-slo-probe-2026-06-24.csv
```

输出 CSV 字段：

```text
row_type,request_id,accepted,rejection_reason,prompt_tokens,reserved_tokens,kv_reserved_mib,queue_wait_ms,prefill_ms,ttft_ms,decode_step_p95_ms,tpot_ms,completion_tokens,finish_reason,cancel_reclaim_ms,metric_name,metric_value
```

probe 固定四个请求：短请求、RAG 请求、取消请求和超长请求。它演示 admission 先按 `prompt_tokens + max_new_tokens` 预留 KV，取消请求释放预算，超长请求返回 `memory_budget_exceeded`，并输出 `ttft_p95_ms`、`queue_wait_p95_ms`、`rejection_rate` 等服务化指标。

真实 small 开源模型 smoke：

```bash
make cpu3-smollm2-smoke
```

这条命令显式依赖网络和外部构建，因此不放进默认 CTest。脚本会把 llama.cpp 和模型文件缓存到 `~/.cache/lcqi-smollm2-small-smoke`，模型不进入 Git。固定模型是 `HuggingFaceTB/SmolLM2-135M-Instruct` 的 GGUF 量化版本，来源仓库为 `bartowski/SmolLM2-135M-Instruct-GGUF`，文件为 `SmolLM2-135M-Instruct-Q4_K_M.gguf`。脚本会校验文件大小 `105454432` 字节和 SHA256：

```text
2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d
```

脚本构建固定 commit 的 `llama-simple-chat`，用 `-ngl 0` 强制 CPU 路径，输入短 prompt，检查 assistant response 非空，并生成：

```text
books/cpu-volume-3/results/lcqi-smollm2-small-model-smoke.txt
```

这份报告只能证明真实 GGUF small 模型已经完成加载、tokenizer/chat template、prefill、decode 和文本输出。它不能证明 LCQI 自研 C++ 引擎已经支持完整 GGUF、不能证明回答质量正确，也不能替代 tiny fixture 的逐层 golden trace。tiny fixture 继续负责可手算语义和回归定位；SmolLM2 smoke 负责把“真实开源模型能在本机跑起来”变成可复查证据。
