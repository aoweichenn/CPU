# Executable Evidence Kit

这是第一册实践卷的贯穿项目。它把“C++ 源码如何成为进程”落成一个可测试工具：读取 ELF64 小端可执行文件，解析 header、section table、string table 和 symbol table，输出稳定文本报告和 section CSV。

## 阶段路线

1. 字节读取：用 `read_binary_file` 把文件内容读成 `std::vector<std::uint8_t>`，并用 checksum 固定输入身份。
2. ELF header：检查 magic、class、endianness、machine 和 entry address，区分“不是 ELF”“不是 ELF64”“不是小端”。
3. Section table：解析 `.text`、`.shstrtab`、`.symtab` 和 `.strtab`，把 section name、offset、size 和 flags 输出成 CSV。
4. Symbol table：解析符号名、绑定、类型、所在 section、地址和大小，把 ABI/链接视角接回源码函数。
5. CLI 证据：让工具可以检查自己或其他二进制，输出 report 或 section CSV。
6. 测试与 benchmark：用 GTest/GMock 固定边界，用 Google Benchmark 对解析路径做 smoke。

## 构建

```bash
cmake -S books/cpu-volume-1-practice \
      -B books/cpu-volume-1-practice/build/executable-evidence-debug \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build books/cpu-volume-1-practice/build/executable-evidence-debug
ctest --test-dir books/cpu-volume-1-practice/build/executable-evidence-debug \
      --output-on-failure
```

## 运行

```bash
books/cpu-volume-1-practice/build/executable-evidence-debug/labs/executable_evidence/cpu1ee_cli \
  books/cpu-volume-1-practice/build/executable-evidence-debug/labs/executable_evidence/cpu1ee_cli

books/cpu-volume-1-practice/build/executable-evidence-debug/labs/executable_evidence/cpu1ee_cli \
  books/cpu-volume-1-practice/build/executable-evidence-debug/labs/executable_evidence/cpu1ee_cli \
  --sections-csv
```

## 研究项目

最终项目不是“看懂 ELF 文件格式”这么窄，而是提交一份二进制证据报告：

- 选一个自己编译的小程序；
- 用 `cpu1ee_cli` 记录 header、sections、symbols 和 checksum；
- 用 `readelf` 或 `objdump` 交叉验证至少三个字段；
- 对一个 C++ 函数解释它的符号、section、入口地址或机器码位置；
- 说明 Debug/Release、优化等级或 strip 对报告有什么影响；
- 把不可验证的结论写成限制，不写成事实。
