# 从 Cpp 到计算系统第一册实践卷

本目录是第一册的独立代码实践卷。第一册正文讲程序、C++、二进制、x86-64、ABI、Linux 工具链和可信测量；本卷把这些内容落到一个贯穿项目：Executable Evidence Kit。

项目目标不是写零散 demo，而是训练一条可复查链路：

- 读取可执行文件字节；
- 解析 ELF64 小端 header、section table、string table 和 symbol table；
- 输出稳定 report 和 CSV；
- 用 GTest/GMock 固定边界；
- 用 Google Benchmark 做解析 smoke；
- 在研究项目中把 objdump、readelf、perf、编译选项和 ABI 观察串起来。

## 构建和测试

```bash
cmake -S books/cpu-volume-1-practice \
      -B books/cpu-volume-1-practice/build/executable-evidence-debug \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build books/cpu-volume-1-practice/build/executable-evidence-debug
ctest --test-dir books/cpu-volume-1-practice/build/executable-evidence-debug \
      --output-on-failure
```

## 构建书籍

```bash
make -C books/cpu-volume-1-practice pdf
make -C books/cpu-volume-1-practice epub
```

## 运行 CLI

```bash
books/cpu-volume-1-practice/build/executable-evidence-debug/labs/executable_evidence/cpu1ee_cli \
  books/cpu-volume-1-practice/build/executable-evidence-debug/labs/executable_evidence/cpu1ee_cli
```
