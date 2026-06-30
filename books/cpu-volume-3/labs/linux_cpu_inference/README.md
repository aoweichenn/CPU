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
- GPT-2 F32 dot kernel：cached generation 热路径使用可替换 dot 边界，x86-64/GNU/Clang 构建接入 AVX2/FMA 实现，其他平台保留标量 fallback
- tiny reference decoder：RMSNorm、float linear、RoPE、GQA KV cache、decode attention、SwiGLU、lm head
- GPT-2 reference path：byte-level BPE tokenizer、`config.json`、F32/F16/BF16 safetensors 张量读取、HuggingFace GPT-2 name mapping、LayerNorm、绝对位置嵌入、packed QKV、causal attention、GELU、tied lm head、full-prefix greedy generation、KV cache greedy generation 和分阶段 benchmark
- reference trace CSV：从 prompt/tokenizer 合同到 logits/sampler/token，逐 checkpoint 输出 shape、stride、dtype、layout、checksum、max_abs、values 摘要
- 7B ledger CSV：输出典型 7B shape 的 FLOPs、KV 容量、权重/KV 字节和 bandwidth-only tokens/s 上限
- serving SLO probe CSV：输出 admission、batch state、KV 预算、queue wait、TTFT、TPOT、取消回收和拒绝率
- 自研 GPT-2 冒烟：加载 `openai-community/gpt2` 的 `config.json`、`vocab.json`、`merges.txt`、`model.safetensors`，在 LCQI C++ reference path 上生成 1 个 token，并把文件 hash、prompt ids、generated ids 和文本输出写入报告
- 自研 GPT-2 benchmark 对比：同一个 GPT-2 F32 safetensors 模型分别跑 full-prefix 和 cached-KV，并在可用时附带 llama.cpp/SmolLM2 Q4_K_M 作为成熟引擎参考
- 自研 GPT-2 优化 A/B：从指定 baseline commit 建临时 worktree，当前实现和 baseline 都通过 `books/cpu-volume-3-practice` 的 Release CMake 入口构建，再用同一个 GPT-2 F32 模型多轮测量 full-prefix 与 cached-KV
- 真实 small 开源模型冒烟：通过 llama.cpp 加载 SmolLM2-135M-Instruct 的 GGUF 量化文件，在本地 CPU 上生成一段 assistant 文本，并把模型 hash、命令、输出和性能行写入报告
- 自研 SmolLM2/GGUF reference decode：读取同一个 GGUF 内的 tokenizer tokens/merges 和 `F32/Q8_0/Q5_0/Q6_K/Q4_K` 混合权重，加载 30 层 SmolLM2，执行 RMSNorm、normal RoPE、GQA KV cache、SwiGLU 和 tied lm head；默认把 shape 兼容的 `Q4_K` linear tensor 留在 GGUF block bytes 中直接执行，其余格式走 f32 fallback；`LCQI_LLAMA_GGML_DIRECT=1` 可打开 `Q5_0/Q6_K/Q8_0` 实验直算路径用于分析
- CLI 推理和简单 benchmark
- CTest 正确性测试

后续扩展方向：

- 真实 7B 级模型 metadata、tokenizer 和量化权重导入
- SentencePiece tokenizer、safetensors 分片加载和更多模型方言 name mapping
- GPT-2 reference path 的 Transformers/PyTorch golden logits 对齐报告
- KV cache 分页、内存规划和可选量化
- CPU packed weight、SIMD、NUMA 和线程亲和优化
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

GPT-2 自研 reference smoke：

```bash
books/cpu-volume-3/build/lcqi-debug/labs/linux_cpu_inference/lcqi_gpt2
make cpu3-gpt2-smoke
make cpu3-gpt2-benchmark-compare
make cpu3-gpt2-hotspot-profile
python3 books/cpu-volume-3/tools/run_gpt2_optimization_ab.py --rounds 3
python3 books/cpu-volume-3/tools/run_gpt2_hotspot_profile.py --token-counts 4,16 --rounds 3
```

第一条命令不需要下载模型，直接运行仓库内 tiny GPT-2 fixture，输出固定 prompt ids、logits、predicted token 和 greedy generated ids。`lcqi_tests` 会覆盖 tiny GPT-2 forward/generation、byte-level BPE 编码解码、F32/F16/BF16 safetensors 读取、HuggingFace 风格 tiny GPT-2 checkpoint 加载和坏 shape 拒绝。

第二条命令显式依赖网络，只下载 `openai-community/gpt2` 的 `config.json`、`vocab.json`、`merges.txt`、`model.safetensors` 到 `~/.cache/lcqi-gpt2-smoke/openai-community--gpt2`，模型文件不进入 Git。脚本会校验四个文件的 bytes 和 SHA256，构建 `lcqi_gpt2`，运行：

```text
Hello, my name is
```

当前验证报告写到：

```text
books/cpu-volume-3/results/lcqi-gpt2-smoke.txt
```

最近一次本机报告的关键行是：

```text
prompt_ids 15496 11 616 1438 318
generated_ids 15496 11 616 1438 318 1757
generated_text Hello, my name is John
validation=PASS
```

这说明 LCQI 自研 C++ reference path 已经能跑标准 GPT-2 safetensors 的 tokenizer、权重导入、forward 和 greedy generation。默认真实模型路径现在使用 `cached_kv`，也可以用 `--engine full` 复现旧的 full-prefix 重算路径：

```bash
books/cpu-volume-3/build/lcqi-release/labs/linux_cpu_inference/lcqi_gpt2 \
  --benchmark --engine cached --threads 0 \
  ~/.cache/lcqi-gpt2-smoke/openai-community--gpt2 \
  "Hello, my name is" 4
```

`--threads 0` 表示自动选择 cached decoder worker 数；`--threads 1` 强制串行；`--threads N` 固定 worker 数。当前 auto 上限是 16，避免在短 decode 上把线程调度开销放大到超过收益。

`make cpu3-gpt2-benchmark-compare` 会生成：

```text
books/cpu-volume-3/results/lcqi-gpt2-benchmark-compare.txt
```

`run_gpt2_optimization_ab.py` 会生成：

```text
books/cpu-volume-3/results/lcqi-gpt2-optimization-ab.txt
```

这个 A/B 报告专门回答“当前 LCQI 代码相对指定 baseline 改快了吗”。脚本默认 baseline 是 `4febf3f`，会临时创建 baseline worktree，分别构建 baseline 和当前工作区的 Release `lcqi_gpt2`，再多轮运行同一条 prompt。上一轮工作区复用优化的 2 轮 smoke 摘要保存在 `books/cpu-volume-3/reports/lcqi-gpt2-optimization-ab-summary.md`；本轮线程优化的正式 raw 报告保存在 `books/cpu-volume-3/reports/lcqi-gpt2-threaded-manual-ab-caw.txt`，汇总保存在 `books/cpu-volume-3/reports/lcqi-gpt2-threaded-optimization-summary.md`。以 `73f40c7` 作为 baseline，在 `caw` 上 5 轮中位数显示：4 token cached-KV 生成阶段从 `395.397 ms` 降到 `76.4141 ms`，16 token 从 `989.060 ms` 降到 `185.479 ms`，生成吞吐分别提升到 `52.3464 token/s` 和 `86.2633 token/s`。随后的 F32 dot kernel 优化以 `cb4d89f` 为 baseline，raw 报告保存在 `books/cpu-volume-3/reports/lcqi-gpt2-f32-dot-ab-caw.txt`，汇总保存在 `books/cpu-volume-3/reports/lcqi-gpt2-f32-dot-optimization-summary.md`。在同一台 `caw`、同一 GPT-2 F32 模型、同样 `--threads 0` 自动 16 workers 下，4 token 生成阶段中位数从 `74.4247 ms` 到 `62.3628 ms`，16 token 从 `181.257 ms` 到 `157.230 ms`；decode token/s 分别提升到 `130.073` 和 `127.405`。再往后，row-range kernel 和去 `std::function` 调度开销的 raw 报告保存在 `books/cpu-volume-3/reports/lcqi-gpt2-f32-rowrange-ab-caw.txt`，汇总保存在 `books/cpu-volume-3/reports/lcqi-gpt2-f32-rowrange-optimization-summary.md`。这一轮相对 `ab72ccb` 的端到端提升很小，16 token 生成中位数约 `153.354 ms` 到 `152.574 ms`；但直接比较 row-range 与 4-row AVX2 row-range 时，4 token 从 `62.0243 ms` 到 `60.3965 ms`，16 token 从 `154.087 ms` 到 `151.222 ms`。本轮 prefill-skip 优化的 raw 报告保存在 `books/cpu-volume-3/reports/lcqi-gpt2-prefill-skip-ab-caw.txt`，汇总保存在 `books/cpu-volume-3/reports/lcqi-gpt2-prefill-skip-optimization-summary.md`。它跳过 prompt 前缀 token 上不会被使用的 `lm_head` 预测：4 token 生成中位数从 `60.5154 ms` 到 `52.1949 ms`，16 token 从 `153.574 ms` 到 `147.424 ms`，生成文本和 token id 全部一致。端到端 GPT-2 数字受调度、温度和共享机器状态影响很大，因此正式结论必须看多轮中位数、min/max 和生成文本一致性，不能只挑一次最快结果。

`run_gpt2_hotspot_profile.py` 专门回答“现在热点到底在哪”。它通过 `--profile-hotspots` 打开 cached decode 内部计时，字段包括 `hotspot_lm_head_pct`、`hotspot_mlp_fc_pct`、`hotspot_mlp_projection_pct`、`hotspot_qkv_projection_pct` 和 `hotspot_attention_pct`。`caw` 上的 3 轮 Release profile 摘要保存在 `books/cpu-volume-3/reports/lcqi-gpt2-hotspot-profile-summary.md`，原始报告保存在 `books/cpu-volume-3/reports/lcqi-gpt2-hotspot-profile-caw.txt`。关键结论是：4 token 与 16 token 两个口径下，`lm_head` 中位数约 `30.2%`，MLP 两个线性投影合计约 `46%`，QKV 投影约 `17%`，cached attention 本体只有 `0.06%` 到 `0.11%`。所以当前短上下文 GPT-2 的慢点不是 KV attention，而是标量 F32 matvec 和 `50257 x 768` vocab 扫描。

当前优化点集中在 cached-KV generation：`Gpt2CachedGreedyDecoder` 把模型级 shape validation、KV cache、临时 workspace 和 worker pool 的生命周期提升到整段生成；`Gpt2ForwardWorkspace` 复用 hidden、normed、Q/K/V、packed QKV、attention、MLP、scores 和 logits 缓冲区；生成路径只需要 greedy token 时走 logits-free argmax，避免把完整 logits vector 作为 API 结果反复分配；prompt prefill 前缀通过 `advance_without_prediction()` 只更新 KV cache，不跑 final norm 和 `lm_head`，因为这些预测结果不会被用作输出；QKV、attention output projection、MLP 两个 projection 和 tied `lm_head` 都在 cached session 内按输出行并行；每个线程 chunk 再通过 `linear_f32_rows_unchecked` 或 `max_dot_f32_rows_unchecked` 进入 F32 row-range kernel，x86-64 可用时使用 AVX2/FMA，其他平台走标量 fallback。KV cache 的 checked public API 仍保留，内部 cached attention 在一次边界校验后使用私有 unchecked span 扫描热路径。它不改变 GPT-2 数学语义，只改变会话生命周期、任务分片、无用工作剔除和热循环调度。

`make cpu3-gpt2-benchmark-compare` 仍用于和外部成熟引擎放在同一报告里看工程差距。同机 llama.cpp/SmolLM2 Q4_K_M 是成熟引擎参照，不是同模型质量排名：LCQI 跑的是 GPT-2 F32 reference path，llama.cpp 跑的是 GGUF 量化 small instruct 模型。它揭示的是工程差距：LCQI 已经消除了“每步重算前缀”的主要算法错误，补上 cached F32 row parallelism，并把热路径接到 AVX2/FMA row-range kernel；但还缺 packed/quantized weight、GGUF 量化 block、mmap/lazy load、采样器、分页 KV cache、NUMA/亲和和成熟调度。

它仍然不是生产级推理引擎：当前 GPT-2 路径还没有把 logits 和 Transformers/PyTorch 逐数值对齐成外部 golden 报告，也没有 GGUF 自研导入、分页 KV cache、量化权重执行和高性能 SIMD/packed kernel。

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

这份报告只能证明 llama.cpp 成熟外部引擎上的真实 GGUF small 模型已经完成加载、tokenizer/chat template、prefill、decode 和文本输出。它不能证明 LCQI 自研 C++ 引擎已经达到 llama.cpp 的性能，也不能替代 tiny fixture 的逐层 golden trace。

LCQI 自研 SmolLM2/GGUF reference decode：

```bash
books/cpu-volume-3/build/lcqi-release/labs/linux_cpu_inference/lcqi_llama_gguf \
  ~/.cache/lcqi-smollm2-small-smoke/models/SmolLM2-135M-Instruct-Q4_K_M.gguf \
  --prompt "Hello" --max-new 2 --benchmark --decode-text
```

caw 上的最新报告写到：

```text
books/cpu-volume-3/results/lcqi-smollm2-gguf-reference-decode-caw.txt
```

关键行是：

```text
weight_execution gguf_mixed_q4_k_direct
generated_text ... <|im_start|>assistant
Hello!
benchmark_prompt_tokens 31
benchmark_generated_tokens 2
benchmark_decode_tokens_per_second 64.8586
benchmark_quantized_weight_bytes 103668480
benchmark_f32_weight_bytes 481436928
benchmark_direct_quantized_weight_bytes 7962624
benchmark_fallback_dequantized_weight_bytes 481436928
benchmark_q4_k_direct_tensors 16
benchmark_ggml_direct_tensors 0
benchmark_f32_fallback_tensors 256
benchmark_hotspot_w_down_ms 63.5371
benchmark_hotspot_q4_k_direct_calls 512
benchmark_hotspot_ggml_direct_calls 0
benchmark_hotspot_f32_fallback_calls 6208
```

这条路径证明 LCQI 自研代码已经能读取同一个 SmolLM2 GGUF、解析 tokenizer arrays、处理混合量化权重并端到端生成文本。默认执行路径不再只是“加载时全部解成 f32”：shape 兼容的 `Q4_K` linear tensor 会保留 GGUF block bytes，并在真实 decoder 里通过 `matvec_q4_k_q8` 直接执行；`Q5_0/Q6_K/Q8_0/F32` tensor 仍然 materialize 成 f32 fallback。当前只有约 `7.68%` 的加载权重字节进入默认 direct Q4_K 路径，prefill 仍逐 token 执行；与 llama.cpp 的差距主要来自更完整的低比特权重覆盖、prompt batching、packed kernel、线程调度、prefetch、KV cache 和整图执行。

实验路径 `LCQI_LLAMA_GGML_DIRECT=1` 会尝试让 shape 兼容的 `Q5_0/Q6_K/Q8_0` linear tensor 也保留 GGUF block bytes，并通过 `matvec_ggml_quantized_q8_0` 执行。caw 同输入报告显示它把 direct 量化字节覆盖率从 `0.076809` 提高到 `0.708479`，但 prefill 中位数从默认 `370.847 ms` 退化到 `1211.710 ms`，decode step 从 `15.4182 ms` 退化到 `42.4420 ms`。原因是这些 GGML direct kernel 仍按 block/row 标量组织，热点 `benchmark_hotspot_ggml_direct_ms` 达到 `1207.13 ms`，慢于当前 f32 AVX2 fallback。因此它保留为实验路径，不默认启用。

LCQI 与 llama.cpp 同输入对比：

```bash
python3 books/cpu-volume-3/tools/run_smollm2_same_input_compare.py \
  --cache-dir ~/.cache/lcqi-smollm2-same-input-compare \
  --model-path ~/.cache/lcqi-smollm2-small-smoke/models/SmolLM2-135M-Instruct-Q4_K_M.gguf \
  --build-dir books/cpu-volume-3/build/lcqi-release \
  --rounds 5 --max-new 2
```

这个脚本会构建固定 commit 的 llama.cpp，运行 `llama-tokenize`、`llama-simple` 和 `llama-bench`，并确认 LCQI 和 llama.cpp 看到的 prompt token ids 完全一致。caw 上的同输入报告保存在：

```text
books/cpu-volume-3/reports/lcqi-smollm2-threaded-q4-compare-caw.txt
```

关键结论：同一个模型、同一个展开 chat prompt、同样 31 个 token id、同样 `max-new=2` 下，当前默认 LCQI 使用 `8` 个 worker，对大行数 f32 fallback 和 Q4_K direct row-range 都做输出行切分。默认 Q4_K direct prefill 中位数从串行 `363.759 ms` 降到 `161.436 ms`，decode step 从串行 `15.1652 ms` 降到 `7.26678 ms`；f32 fallback hotspot 从 `338.173 ms` 降到 `146.731 ms`。相对同输入 llama.cpp，LCQI prefill 仍慢 `7.381619x`，decode step 仍慢 `2.471694x`。同一二进制里关闭 `LCQI_LLAMA_Q4_DIRECT=0` 后，LCQI prefill 为 `175.634 ms`，decode step 为 `7.23524 ms`，`w_down` 热点为 `36.7329 ms`；默认 Q4_K direct 后分别为 `161.436 ms`、`7.26678 ms`、`22.3253 ms`。所以这轮默认优化的 A/B 证据是：线程池带来 prefill `2.253271x`、decode step `2.086922x`、f32 hotspot `2.304714x`；Q4_K direct 在并行后带来 prefill `1.087948x`、`w_down` `1.645349x`，decode 中位数 `0.995660x` 基本持平，并减少 `56,623,104` 字节 f32 materialized 权重。实验 GGML direct 仍只保留为 opt-in：之前的 caw 负结果已经证明，单纯提高 `Q5_0/Q6_K/Q8_0` 覆盖率而没有 packed multi-row/SIMD/threaded kernel，会让端到端倒退。
