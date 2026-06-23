# 第三册 LaTeX 正式书稿工程

第三册主题是 AI 计算怎样落成本地大模型推理引擎：张量表示、算子语义、内存布局、量化、矩阵乘、attention、KV cache、推理图、CPU/GPU 后端、调度、验证和性能对标。

构建命令：

```bash
make check
make pdf
make epub
```

生成文件：

- `main.pdf`
- `main.epub`

写作约束：

- 从第二册的机器账本继续推导，不直接堆 AI 名词。
- 每个概念先说明它解决的底层问题，再给术语。
- 正文以原理、伪代码、关键 C++ 片段、汇编形态和测量边界为主。
- 当前 `books/cpu-volume-3/labs/linux_cpu_inference/` 是最小 MLP/int8 权重切片；完整工程路线会继续扩展到 7B 级 decoder-only 模型加载、tokenizer、Transformer 层、KV cache、CPU/GPU 后端和 benchmark harness。
- EPUB 不包含图片文件，保持微信读书兼容导出。
