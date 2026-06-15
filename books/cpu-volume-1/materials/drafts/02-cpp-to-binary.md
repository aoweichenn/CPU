# 第 2 章：C++ 到目标文件、汇编和 ELF

## 本章目标

本章建立第一条核心链路：

```text
C++ source
  -> preprocessed source
  -> assembly
  -> object file
  -> executable ELF
  -> disassembly
```

完成本章后，你应该能：

- 分阶段编译 C++。
- 找到函数在汇编和二进制中的位置。
- 使用 `objdump`、`readelf`、`nm`。
- 理解 symbol、section、relocation 的基本意义。
- 比较 `-O0`、`-O2`、`-O3`、`-march=native` 对汇编的影响。

## 为什么要学二进制路径

性能优化不能只看源码。

源码表达的是语义，CPU 执行的是机器码。中间经过编译器优化、寄存器分配、指令选择、链接、重定位，最终机器码可能和源码长得完全不同。

比如：

```cpp
int add(int a, int b)
{
    return a + b;
}
```

在 `-O0` 下，编译器可能生成栈读写，便于调试。  
在 `-O3` 下，它可能只是一条 `lea` 或 `add`。  
如果被 inline，最终二进制里可能没有独立的 `add` 函数。

所以你必须学会从源码追到最终二进制。

## 编译的四个阶段

### 1. Preprocessing

命令：

```bash
g++ -E main.cpp -o main.i
```

发生什么：

- 展开 `#include`。
- 展开宏。
- 处理条件编译。

输出 `.i` 通常很大，因为标准库头文件被展开进来了。

### 2. Compilation

命令：

```bash
g++ -S -O3 -masm=intel main.cpp -o main.s
```

发生什么：

- C++ 被解析、语义检查。
- 生成中间表示。
- 执行优化。
- 选择目标指令。
- 输出汇编。

### 3. Assembly

命令：

```bash
g++ -c main.cpp -o main.o
```

发生什么：

- 汇编变成机器码。
- 生成目标文件。
- 未解析的外部引用保留为 relocation。

### 4. Linking

命令：

```bash
g++ main.o -o main
```

发生什么：

- 合并多个目标文件。
- 解析符号。
- 处理 relocation。
- 生成 ELF 可执行文件。

## 第一个观察实验

写：

```cpp
extern "C" int add_i32(int a, int b)
{
    return a + b;
}
```

编译：

```bash
g++ -O0 -S -masm=intel add.cpp -o add_O0.s
g++ -O3 -S -masm=intel add.cpp -o add_O3.s
g++ -O3 add.cpp -c -o add.o
objdump -drwC -Mintel add.o
```

观察：

- `-O0` 是否使用栈？
- `-O3` 是否只保留寄存器操作？
- 参数在哪些寄存器？
- 返回值在哪个寄存器？

在 System V AMD64 ABI 中，前两个 `int` 参数通常在 `edi` 和 `esi`，返回值在 `eax`。

## `.s` 和最终反汇编的区别

`.s` 文件是编译器生成的汇编文本。  
`objdump` 看到的是目标文件或可执行文件里的机器码反汇编。

为什么最终反汇编更可靠？

- 链接可能改变地址。
- LTO 可能改变函数边界。
- inline 可能让函数消失。
- PLT/GOT 和动态链接只在链接后完整体现。
- 汇编器可能对部分指令编码做选择。

所以学习时两者都看：

- `.s` 适合看编译器输出。
- `objdump` 适合看最终二进制。

## symbol

symbol 是链接器用来识别函数和对象的名字。

查看：

```bash
nm -C main.o
```

常见符号类型：

- `T`：text section 中的全局符号。
- `t`：text section 中的局部符号。
- `U`：undefined symbol，需要链接时解析。
- `B`：BSS。
- `D`：data。

C++ 有 name mangling：

```cpp
int add(int, int);
```

可能变成：

```text
_Z3addii
```

使用 `extern "C"` 可以避免 C++ name mangling，便于和汇编互调。

## section

查看 section：

```bash
readelf -S main.o
```

常见 section：

- `.text`：代码。
- `.rodata`：只读数据。
- `.data`：已初始化全局数据。
- `.bss`：未初始化全局数据。
- `.rela.text`：text relocation。
- `.eh_frame`：异常和栈展开信息。

性能分析里 `.text` 和 `.rodata` 很常见。  
异常、RTTI、虚函数会让二进制出现更多辅助结构。

## relocation

目标文件还不知道外部函数或全局对象的最终地址，所以会留下 relocation。

查看：

```bash
readelf -r main.o
```

比如调用 `printf`，目标文件里可能只有一个待链接的符号引用。链接器会在最终可执行文件里修正它。

理解 relocation 对后续阅读动态链接、PLT/GOT、共享库很重要。

## 优化级别如何改变汇编

### `-O0`

特点：

- 易调试。
- 保留更多变量和栈操作。
- 很少 inline。
- 不适合性能推理。

### `-O2`

特点：

- 大多数通用优化。
- 适合很多生产代码。
- 通常比较保守。

### `-O3`

特点：

- 更激进的 loop 优化、vectorization、unrolling。
- 可能增加代码尺寸。
- 不一定总比 `-O2` 快。

### `-Ofast`

特点：

- 包含违反严格标准语义的优化。
- 可能改变浮点结果。
- 性能实验中必须单独说明。

### `-march=native`

特点：

- 针对当前 CPU 启用 ISA。
- 可能生成 AVX2、FMA、AVX-VNNI 等指令。
- 不适合直接发布给未知机器。

## 必会命令

生成 Intel syntax 汇编：

```bash
clang++ -O3 -march=native -masm=intel -S file.cpp -o file.s
```

反汇编：

```bash
objdump -drwC -Mintel ./binary | less
```

查看符号：

```bash
nm -C file.o
```

查看 section：

```bash
readelf -S file.o
```

查看 relocation：

```bash
readelf -r file.o
```

查看动态依赖：

```bash
ldd ./binary
```

## 实验

### 实验 1：12 个函数

写 12 个函数：

- `add_i32`
- `add_f32`
- `sum_i32`
- `sum_f32`
- `dot_f32`
- `if_else`
- `switch_dense`
- `switch_sparse`
- `template_add`
- `lambda_call`
- `virtual_call`
- `vector_sum`

对每个函数：

- 生成 `-O0` 汇编。
- 生成 `-O3 -march=native` 汇编。
- 生成目标文件。
- 用 `objdump` 反汇编。
- 记录关键指令。

### 实验 2：inline 消失

写：

```cpp
inline int square(int x)
{
    return x * x;
}
```

在调用点反汇编。观察是否还有 `square` symbol。

### 实验 3：虚函数调用

写：

```cpp
struct Base {
    virtual int value() const = 0;
    virtual ~Base() = default;
};
```

观察：

- vtable 在哪里。
- indirect call 如何出现。
- `final` 或静态类型是否让编译器 devirtualize。

## 作业

1. 完成 12 个函数的汇编对比表。
2. 解释 `add_i32` 和 `add_f32` 的参数寄存器差异。
3. 解释 `switch_dense` 和 `switch_sparse` lowering 差异。
4. 找一个函数被 inline 的例子。
5. 找一个 virtual call 的反汇编例子。
6. 写 2 页报告：为什么源码行和汇编行不能一一对应。

## 常见误区

- 只看 `.s`，不看最终二进制。
- 不使用 `-C` demangle C++ 名字。
- 把 `-O0` 汇编当成性能参考。
- 以为 inline 函数一定有独立 symbol。
- 以为每句 C++ 一定对应连续汇编。

## 验收标准

- 能独立完成 C++ 分阶段编译。
- 能解释 ELF section、symbol、relocation 的基本作用。
- 能从 `objdump` 找到函数。
- 能比较 `-O0` 和 `-O3` 的关键差异。

