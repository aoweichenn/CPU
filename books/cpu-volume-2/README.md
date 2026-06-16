# CPU 高性能与 AI 计算教材第二册

本目录是第二册的自包含工程。第一册讲程序、二进制、x86-64、ABI、Linux 工具链和可信性能测量；第二册先讲 Linux 上的现代 CPU 性能工程、多核心并发、锁、原子操作、缓存、TLB、SIMD 和软件优化，再从零讲 AI 模型、张量、推理图和算子开发，最终完成一个本地 CPU 量化推理引擎。

## 目录结构

```text
source/latex/      # 正式 LaTeX 书稿，生成 main.pdf 和 main.epub
materials/         # 草稿、素材、参考资料和扩写计划
labs/              # 与本书绑定的代码实验
reports/           # 实验报告模板
results/           # 实验输出
tools/             # 本书专用脚本
```

## 生成书籍

```bash
make check
make pdf
make epub
make text-count
```

生成文件位置：

- `source/latex/main.pdf`
- `source/latex/main.epub`

## 本书维护规则

- 正式成书以 `source/latex/` 为准。
- 本书只维护一个正式版本，不再并行维护 Markdown 版本。
- 排版、字体、代码样式、目录层级和 EPUB 约束沿用第一册。
- 章节写作顺序保持“原理先行、模型约束、代码证据、测量验证、源码入口”。
- C++ 示例统一使用位宽明确的整数类型，例如 `std::int32_t`、`std::int64_t` 和 `std::uint64_t`。
- 所有性能结论必须说明输入规模、数据布局、缓存行为、指令约束和测量边界。

## 贯穿项目

贯穿项目位于：

```text
labs/linux_cpu_inference/
```

当前第一阶段实现了 Linux 本地 CPU 量化 MLP 推理：

- 文本模型格式 `LCQI_MODEL_V1`
- int8 权重和 per-layer scale
- float 输入和激活
- 量化线性层、ReLU、argmax
- CLI 推理和 benchmark
- CTest 正确性测试
