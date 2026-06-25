# 第 -4 章：程序如何从源码变成正在运行的进程

## 本章目标

本章是所有底层学习的入口。你刚学完 C/C++ 后，通常知道“写代码、编译、运行”，但不清楚中间发生了什么。本章把这个过程拆开。

完成本章后，你应该能解释：

- 源码、头文件、目标文件、静态库、动态库、可执行文件分别是什么。
- 编译器、汇编器、链接器、加载器分别做什么。
- 为什么 C++ 程序不是从 `main` 的第一行直接开始执行。
- 进程是什么，虚拟地址空间是什么。
- 栈、堆、全局区、代码区大致放在哪里。
- `./program` 运行时 OS 和 CPU 如何协作。
- 为什么理解这个流程是学习汇编、ABI、性能优化的前置条件。

你暂时不需要记住所有 ELF 细节，但必须形成一张完整地图。

## 一个最小程序背后有什么

写一个最小 C++ 程序：

```cpp
#include <iostream>

int main()
{
    int x = 1;
    int y = 2;
    std::cout << x + y << '\n';
    return 0;
}
```

从人的角度看，它做了三件事：

```text
定义两个整数
相加
打印结果
```

从机器角度看，它涉及更多层：

```text
source file
  -> preprocessor
  -> compiler frontend
  -> optimizer
  -> backend
  -> assembly
  -> assembler
  -> object file
  -> linker
  -> executable ELF
  -> loader
  -> process address space
  -> CPU executes instructions
```

后续所有章节都在研究这条链路中的某一段。

## 源文件和头文件

C/C++ 源文件通常是：

```text
.c
.cc
.cpp
.cxx
```

头文件通常是：

```text
.h
.hpp
```

源文件是编译单元的主要输入。头文件不是单独“链接进去”的东西，而是在预处理阶段被文本式包含进源文件。

例如：

```cpp
#include "math.hpp"
```

预处理器会把 `math.hpp` 的内容放到当前源文件中。  
所以一个 `.cpp` 加上它包含的所有头文件，经过预处理后会变成一个很大的翻译单元。

术语：

```text
translation unit = 一个源文件经过预处理后的结果
```

这是 C++ 编译模型的基本单位。

## 预处理器做什么

预处理器处理：

- `#include`
- `#define`
- `#if`
- `#ifdef`
- `#pragma`
- 删除注释。

例子：

```cpp
#define SCALE 4

int f(int x)
{
    return x * SCALE;
}
```

预处理后大致变成：

```cpp
int f(int x)
{
    return x * 4;
}
```

命令：

```bash
mkdir -p scratch/ch00a
cat > scratch/ch00a/main.cpp <<'EOF'
#include <iostream>

#define SCALE 4

int f(int x)
{
    return x * SCALE;
}

int main()
{
    std::cout << f(3) << '\n';
}
EOF

g++ -E scratch/ch00a/main.cpp -o scratch/ch00a/main.i
wc -l scratch/ch00a/main.cpp scratch/ch00a/main.i
```

你会发现 `main.i` 行数远远超过原文件，因为 `<iostream>` 展开了大量标准库声明。

### 为什么性能学习要理解预处理

原因：

- 宏可能改变你以为的代码。
- 头文件 inline 函数会影响编译器优化。
- 模板代码通常在头文件里实例化，可能导致代码膨胀。
- 条件编译可能让不同平台编译出完全不同路径。

你看源码时看到的是一个版本，编译器看到的可能是另一个版本。

## 编译器前端做什么

编译器前端负责把预处理后的 C++ 变成编译器能理解的结构。

主要步骤：

```text
字符流
  -> tokens
  -> parser
  -> AST
  -> semantic analysis
  -> IR
```

### token

代码：

```cpp
int x = a + 3;
```

会被词法分析成类似：

```text
int
x
=
a
+
3
;
```

### AST

AST 是 Abstract Syntax Tree，抽象语法树。

上面的表达式可以理解成：

```text
declaration x
  type: int
  initializer:
    binary +
      left: a
      right: 3
```

### semantic analysis

语义分析会检查：

- 变量是否声明。
- 类型是否匹配。
- 函数调用参数是否正确。
- 重载解析选择哪个函数。
- 模板实例化是否合法。
- `const`、引用、生命周期等规则。

如果你写：

```cpp
int* p = 3;
```

前端会报错，因为整数不能直接当成 `int*`。

## IR 是什么

IR 是 Intermediate Representation，中间表示。  
它比 C++ 简单，比汇编抽象，是编译器做优化的主要工作对象。

例如：

```cpp
int add(int a, int b)
{
    return a + b;
}
```

可能变成类似：

```llvm
define i32 @add(i32 %a, i32 %b) {
entry:
  %sum = add i32 %a, %b
  ret i32 %sum
}
```

这不是 x86 汇编，而是 LLVM IR。  
第 4 章会系统学习 IR。现在你只需要知道：编译器并不是直接从 C++ 跳到机器码，中间有一层适合优化的表示。

## 优化器做什么

优化器会在保持程序可观察行为不变的前提下改写 IR。

常见优化：

- 删除无用代码。
- 常量折叠。
- 函数内联。
- 循环展开。
- 循环不变量外提。
- 自动向量化。
- 公共子表达式消除。

比如：

```cpp
int f()
{
    return 2 * 3 + 4;
}
```

优化后可能直接变成：

```cpp
return 10;
```

对应汇编可能是：

```asm
mov eax, 10
ret
```

这就是为什么 benchmark 里如果结果没有被使用，整个计算可能被删掉。

## 后端做什么

编译器后端把优化后的 IR 变成目标机器的汇编或机器码。

后端需要做：

- 指令选择：用哪些 x86 指令表达这个操作。
- 寄存器分配：哪些变量放在哪些寄存器。
- 指令调度：指令顺序如何安排。
- 调用约定：参数和返回值放哪里。
- 生成汇编。

同一段 C++，不同目标机器会生成不同汇编：

```text
x86-64
other-isa
RISC-V
```

即使都是 x86-64，不同 flags 也会不同：

```text
-O0
-O3
-march=native
-mavx2
-mavx512f
```

## 汇编器做什么

汇编器把汇编文本变成目标文件。

汇编：

```asm
mov eax, 10
ret
```

会被编码成机器字节。  
CPU 最终执行的是字节，不是文本。

目标文件通常是 `.o`：

```bash
g++ -c scratch/ch00a/main.cpp -o scratch/ch00a/main.o
```

目标文件里有：

- 机器码。
- 符号表。
- section。
- relocation。
- debug 信息。

但它通常还不能直接运行，因为外部函数和库还没链接好。

## 链接器做什么

链接器把多个目标文件和库合成一个可执行文件或共享库。

假设：

```cpp
// add.cpp
int add(int a, int b)
{
    return a + b;
}
```

```cpp
// main.cpp
int add(int, int);

int main()
{
    return add(1, 2);
}
```

命令：

```bash
g++ -c add.cpp -o add.o
g++ -c main.cpp -o main.o
g++ add.o main.o -o app
```

`main.o` 里有对 `add` 的引用，但不知道 `add` 的最终地址。  
链接器把这个引用解析到 `add.o` 中的函数地址。

链接器还会处理：

- 标准库。
- 启动代码。
- 动态库引用。
- relocation。
- 符号可见性。

## 可执行文件是什么

在 Linux x86-64 上，可执行文件通常是 ELF 格式。

ELF 里包含：

- 程序头：告诉加载器如何映射到内存。
- section：代码、数据、只读数据、符号、调试信息等。
- 入口点地址。
- 动态链接信息。

查看：

```bash
g++ scratch/ch00a/main.cpp -o scratch/ch00a/app
file scratch/ch00a/app
readelf -h scratch/ch00a/app
readelf -l scratch/ch00a/app
readelf -S scratch/ch00a/app | sed -n '1,80p'
```

你不需要立刻看懂所有字段。现在只要知道：可执行文件不是“纯代码”，而是一个带元数据的二进制容器。

## 操作系统如何运行程序

当你输入：

```bash
./scratch/ch00a/app
```

shell 会请求操作系统创建新进程。Linux 内核大致做：

```text
读取 ELF header
创建进程地址空间
把代码和数据段映射到虚拟内存
准备栈
设置 argc/argv/envp
加载动态链接器
跳到程序入口点
```

如果程序使用动态链接，动态链接器还会：

- 加载共享库。
- 解析符号。
- 设置 GOT/PLT。
- 运行全局构造。
- 最后调用 C/C++ runtime，再进入 `main`。

所以 `main` 不是整个程序真正的第一条指令。  
它是 C/C++ runtime 调用你的入口函数。

## 进程是什么

进程可以理解为：

```text
正在运行的程序实例 + 独立虚拟地址空间 + OS 管理的资源
```

资源包括：

- 内存映射。
- 文件描述符。
- 当前工作目录。
- 信号处理。
- 线程。
- 权限。

同一个可执行文件可以运行多个进程，每个进程有自己的虚拟地址空间。

## 虚拟地址空间

程序里看到的指针是虚拟地址，不是物理内存地址。

一个典型进程地址空间包含：

```text
high address
  stack
  ...
  shared libraries / mmap regions
  ...
  heap
  BSS
  data
  rodata
  text
low address
```

这只是简化图，真实布局受 ASLR、动态链接、系统配置影响。

### text

代码段，存放机器指令。

### rodata

只读数据，例如字符串字面量、常量表。

### data

已初始化的全局变量。

### BSS

未显式初始化或初始化为 0 的全局/静态变量。

### heap

动态分配内存，例如 `new`、`malloc`。

### stack

函数调用栈，存放返回地址、局部变量、保存的寄存器等。

## CPU 从哪里开始执行

ELF header 里有入口点地址。加载器准备好进程后，会把控制权交给入口点。

入口点通常不是 `main`，而是类似 `_start` 的运行时启动函数。

你可以查看：

```bash
readelf -h scratch/ch00a/app | rg "Entry point"
objdump -d -Mintel scratch/ch00a/app | sed -n '/<_start>:/,+40p'
```

`_start` 最终会调用 C runtime，然后调用你的 `main`。

这一点重要，因为：

- benchmark 程序启动成本不等于被测函数成本。
- 全局构造可能在 `main` 前运行。
- 动态链接和初始化会影响第一次运行。
- 函数调用栈和 ABI 从 runtime 开始就必须正确。

## 数据如何进入 CPU

当 CPU 执行：

```asm
mov eax, dword ptr [rbp - 4]
```

它不是“读变量 x”，而是：

```text
计算地址 rbp - 4
通过 load/store 单元发起内存读
虚拟地址翻译
查 L1/L2/L3/内存
把 4 字节放入 eax
```

C++ 的变量名在编译后通常不存在。  
CPU 只看到寄存器、内存地址和指令。

## 为什么这章对性能重要

如果你不理解程序运行流程，会出现很多错误判断：

- 把编译时间、启动时间、动态链接时间当成 kernel 性能。
- 不知道为什么 Debug 和 Release 汇编完全不同。
- 不知道为什么函数可能被 inline 后从符号表消失。
- 不知道为什么全局对象构造影响第一次测量。
- 不知道为什么共享库调用有 PLT/GOT。
- 不知道为什么指针地址每次运行可能变化。
- 不知道为什么同一个源码在不同 flags 下完全不是同一段机器码。

性能优化不是“改几行 C++”，而是控制从源码到执行的一条长链路。

## 实验 A：拆开编译流程

写：

```bash
mkdir -p scratch/ch00a
cat > scratch/ch00a/add.cpp <<'EOF'
extern "C" int add_i32(int a, int b)
{
    return a + b;
}
EOF
```

逐步编译：

```bash
g++ -E scratch/ch00a/add.cpp -o scratch/ch00a/add.i
g++ -S -O0 -masm=intel scratch/ch00a/add.cpp -o scratch/ch00a/add_O0.s
g++ -S -O3 -masm=intel scratch/ch00a/add.cpp -o scratch/ch00a/add_O3.s
g++ -c -O3 scratch/ch00a/add.cpp -o scratch/ch00a/add.o
objdump -drwC -Mintel scratch/ch00a/add.o
```

报告：

- `.cpp`、`.i`、`.s`、`.o` 分别是什么。
- `-O0` 和 `-O3` 汇编差异。
- `objdump` 输出是否和 `.s` 完全一样。

## 实验 B：观察进程地址空间

写：

```bash
cat > scratch/ch00a/address.cpp <<'EOF'
#include <iostream>
#include <vector>

int global_initialized = 42;
int global_zero;
char const* message = "hello";

int main()
{
    int stack_value = 1;
    auto* heap_value = new int(2);
    std::vector<int> vec(16, 3);

    std::cout << "main              " << reinterpret_cast<void*>(&main) << '\n';
    std::cout << "global_initialized" << &global_initialized << '\n';
    std::cout << "global_zero       " << &global_zero << '\n';
    std::cout << "message variable  " << &message << '\n';
    std::cout << "message literal   " << static_cast<void const*>(message) << '\n';
    std::cout << "stack_value       " << &stack_value << '\n';
    std::cout << "heap_value        " << heap_value << '\n';
    std::cout << "vector data       " << vec.data() << '\n';

    delete heap_value;
}
EOF

g++ -O0 -g scratch/ch00a/address.cpp -o scratch/ch00a/address
./scratch/ch00a/address
./scratch/ch00a/address
```

任务：

- 比较两次运行地址是否一样。
- 推测哪些属于 text、data、BSS、rodata、stack、heap。
- 解释为什么地址可能每次变化。

## 实验 C：查看入口点

命令：

```bash
readelf -h scratch/ch00a/address | rg "Entry point"
objdump -d -Mintel scratch/ch00a/address | sed -n '/<_start>:/,+50p'
nm -C scratch/ch00a/address | rg " main$|_start"
```

任务：

- 找到 `_start`。
- 找到 `main`。
- 解释为什么入口点不是 `main`。

## 作业 -4.1：画出程序运行全流程

手画或用文本画出：

```text
hello.cpp
  -> hello.i
  -> hello.s
  -> hello.o
  -> hello
  -> process
  -> instructions executed by CPU
```

每一层写：

- 输入是什么。
- 输出是什么。
- 哪个工具负责。
- 性能学习中为什么要关心。

## 作业 -4.2：符号和函数消失实验

写三个函数：

- `add_inline_candidate`
- `add_noinline`
- `unused_function`

要求：

- 一个允许 inline。
- 一个用 `[[gnu::noinline]]` 阻止 inline。
- 一个从不调用。

分别用 `-O0` 和 `-O3` 编译。

命令：

```bash
nm -C your_binary
objdump -drwC -Mintel your_binary
```

报告：

- 哪些符号存在。
- 哪些函数被 inline 或删除。
- 为什么不能只靠源码判断最终执行了什么。

## 作业 -4.3：启动成本和 kernel 成本

写一个程序：

- `main` 里调用一个很小函数 1 次。
- 再调用同一个函数 1 亿次。

分别测：

- 整个程序运行时间。
- 函数循环运行时间。

报告：

- 为什么启动成本不能代表函数成本。
- 为什么 microbenchmark 要排除初始化。
- 为什么 warmup 有意义。

## 常见误区

### 误区 1：C++ 程序从 `main` 的第一行开始

不准确。`main` 之前有启动代码、动态链接、全局构造等。

### 误区 2：头文件会被单独编译

通常不对。头文件被包含进源文件，形成翻译单元。

### 误区 3：目标文件就是可执行文件

不对。目标文件还需要链接。

### 误区 4：指针就是物理地址

用户态程序看到的是虚拟地址。

### 误区 5：源码里有函数，二进制里一定有这个函数

不对。优化后函数可能被 inline、合并或删除。

## 验收标准

本章完成标准：

- 你能解释预处理、编译、汇编、链接、加载的区别。
- 你能生成 `.i`、`.s`、`.o`、可执行文件。
- 你能用 `readelf` 找入口点。
- 你能解释为什么 `main` 不是真正入口点。
- 你能画出一个进程的简化虚拟地址空间。
- 你能解释为什么性能测量要排除启动和初始化成本。
