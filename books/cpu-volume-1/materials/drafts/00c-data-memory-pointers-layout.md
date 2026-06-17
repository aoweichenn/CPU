# 第 -2 章：数据表示、内存、指针和对象布局

## 本章目标

本章把 C/C++ 中的值、对象、指针、数组和结构体映射到机器字节。

完成本章后，你应该能：

- 解释内存为什么可以看成按地址编号的字节数组。
- 解释整数补码、无符号溢出、有符号溢出的区别。
- 解释小端字节序如何影响内存中的字节排列。
- 解释 `int* p`、`p + 1` 和地址计算的关系。
- 解释数组访问如何变成 base + index * scale。
- 解释 struct padding、alignment 和 `sizeof`。
- 解释栈对象、堆对象、全局对象的基本生命周期。
- 理解这些知识为什么是 ABI、汇编、SIMD、cache 优化的前置条件。

本章不是 C++ 语法复习，而是从机器角度重新理解你已经学过的 C/C++。

## 从一个问题开始：`a[i]` 到底访问了什么

C++：

```cpp
int get(int const* a, int i)
{
    return a[i];
}
```

可能汇编：

```asm
movsxd rsi, esi
mov eax, dword ptr [rdi + rsi*4]
ret
```

这里的关键是：

```asm
[rdi + rsi*4]
```

如果你只从 C++ 看，`a[i]` 是“数组第 i 个元素”。  
如果从机器看：

```text
base address = rdi
index = rsi
element size = 4 bytes
effective address = rdi + rsi * 4
load 4 bytes from memory
put them into eax
```

本章就是为了让你从这两种视角自由切换。

## 内存是字节数组

机器层面，用户程序看到的内存可以简化为：

```text
address -> byte
```

每个地址指向一个字节。一个字节通常是 8 bit。

例如：

```text
address   byte
0x1000    0x78
0x1001    0x56
0x1002    0x34
0x1003    0x12
```

这 4 个字节可以解释成：

- 一个 32 位整数。
- 四个字符。
- 一个浮点数的位模式。
- 一个结构体的一部分。
- 机器指令的一部分。

内存里的字节本身没有类型。类型是编译器和程序解释字节的规则。

## 对象和值

C++ 标准里，对象是有存储的实体。  
机器层面，对象最终占据一段字节。

例如：

```cpp
int x = 0x12345678;
```

如果 `int` 是 4 字节，那么 `x` 占用 4 个字节。  
这些字节如何排列，取决于字节序。

## 小端字节序

x86-64 使用 little-endian，小端字节序。

整数：

```text
0x12345678
```

在内存中从低地址到高地址通常是：

```text
0x78 0x56 0x34 0x12
```

低有效字节放在低地址。

实验：

```bash
mkdir -p scratch/ch00c
cat > scratch/ch00c/endian.cpp <<'EOF'
#include <cstdint>
#include <iomanip>
#include <iostream>

int main()
{
    std::uint32_t x = 0x12345678U;
    auto const* p = reinterpret_cast<unsigned char const*>(&x);
    for (std::size_t i = 0; i < sizeof(x); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(p[i]) << '\n';
    }
}
EOF

g++ -O2 scratch/ch00c/endian.cpp -o scratch/ch00c/endian
./scratch/ch00c/endian
```

如果输出：

```text
78
56
34
12
```

说明你观察到了小端存储。

## 二进制和十六进制

计算机硬件处理 bit。  
人类通常用十六进制表示 bit 模式，因为 1 个十六进制数字正好表示 4 bit。

```text
0xf = 1111b
0x10 = 0001 0000b
0xff = 1111 1111b
```

常见宽度：

```text
uint8_t   1 byte  = 8 bits
uint16_t  2 bytes = 16 bits
uint32_t  4 bytes = 32 bits
uint64_t  8 bytes = 64 bits
```

看汇编、内存 dump、寄存器时，十六进制是必备语言。

## 无符号整数

`std::uint32_t` 表示 32 位无符号整数，范围：

```text
0 .. 2^32 - 1
```

无符号运算按模 `2^N` 回绕。

例子：

```cpp
std::uint8_t x = 255;
x += 1;
```

结果是：

```text
0
```

因为：

```text
255 + 1 = 256
256 mod 256 = 0
```

无符号溢出在 C++ 中是定义良好的。

## 有符号整数和补码

现代机器几乎都用 two's complement，补码，表示有符号整数。

8 位例子：

```text
0000 0000 = 0
0000 0001 = 1
0111 1111 = 127
1000 0000 = -128
1111 1111 = -1
```

为什么 `1111 1111` 是 `-1`？

在 8 位补码中：

```text
0xff + 1 = 0x00  modulo 256
```

所以 `0xff` 表示加 1 后变成 0 的数，也就是 `-1`。

### C++ 有符号溢出

注意：C++ 中有符号整数溢出是 undefined behavior。

```cpp
int x = INT_MAX;
int y = x + 1; // undefined behavior
```

即使硬件上通常会回绕，编译器也可以基于“有符号溢出不会发生”做优化。

这对性能学习非常重要，因为优化器可能因为 UB 改写你的代码。

## 符号扩展和零扩展

把小整数放大到大整数时，有两种扩展。

### zero extension

无符号扩展，高位补 0：

```text
uint8_t  0xff -> uint32_t 0x000000ff
```

### sign extension

有符号扩展，复制符号位：

```text
int8_t -1 = 0xff -> int32_t 0xffffffff
```

汇编里你会看到：

```asm
movzx
movsx
movsxd
```

它们分别用于零扩展或符号扩展。

## 浮点数是另一套解释规则

`float` 通常是 IEEE 754 binary32：

```text
1 sign bit
8 exponent bits
23 fraction bits
```

同样的 32 位 bit，可以被解释成整数，也可以被解释成浮点。

例如：

```cpp
float f = 1.0f;
```

它的 bit pattern 通常是：

```text
0x3f800000
```

实验：

```bash
cat > scratch/ch00c/float_bits.cpp <<'EOF'
#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>

int main()
{
    float f = 1.0f;
    std::uint32_t bits = std::bit_cast<std::uint32_t>(f);
    std::cout << std::hex << bits << '\n';
}
EOF

g++ -std=c++20 -O2 scratch/ch00c/float_bits.cpp -o scratch/ch00c/float_bits
./scratch/ch00c/float_bits
```

浮点学习后续会在 reduction、FMA、softmax、layernorm 中反复出现。

现在你只需要记住：

- 浮点不是实数。
- 浮点加法不满足严格结合律。
- 改变累加顺序会改变结果。
- 编译器优化浮点时受语义限制。

## 指针是地址，但不是普通整数

C++ 指针保存地址。

```cpp
int x = 42;
int* p = &x;
```

机器层面，`p` 的值是某个虚拟地址。  
但 C++ 类型系统知道它是 `int*`，所以 `p + 1` 不是地址加 1，而是加 `sizeof(int)`。

```cpp
int a[4];
int* p = &a[0];
int* q = p + 1;
```

如果 `p` 是 `0x1000`，`q` 通常是：

```text
0x1004
```

因为 `int` 是 4 字节。

## 数组访问的地址计算

C++：

```cpp
return a[i];
```

等价地址：

```text
address = base(a) + i * sizeof(a[0])
```

如果 `a` 是 `int*`：

```text
address = base + i * 4
```

如果 `a` 是 `double*`：

```text
address = base + i * 8
```

x86-64 支持常见的 addressing mode：

```text
[base + index * scale + displacement]
```

其中 scale 可以是：

```text
1, 2, 4, 8
```

这正好适合常见元素大小。

## 指针和数组不是一回事

数组对象：

```cpp
int a[4];
```

包含 4 个连续 `int` 对象。

指针：

```cpp
int* p = a;
```

保存第一个元素地址。

很多表达式中数组会 decay 成指向首元素的指针，但数组本身不是指针。

实验：

```bash
cat > scratch/ch00c/array_pointer.cpp <<'EOF'
#include <iostream>

int main()
{
    int a[4] = {1, 2, 3, 4};
    int* p = a;

    std::cout << sizeof(a) << '\n';
    std::cout << sizeof(p) << '\n';
    std::cout << static_cast<void*>(a) << '\n';
    std::cout << static_cast<void*>(&a[0]) << '\n';
}
EOF

g++ -O2 scratch/ch00c/array_pointer.cpp -o scratch/ch00c/array_pointer
./scratch/ch00c/array_pointer
```

观察：

- `sizeof(a)` 是整个数组大小。
- `sizeof(p)` 是指针大小。

## struct 布局和 padding

结构体：

```cpp
struct A {
    char c;
    int x;
};
```

你可能以为大小是：

```text
1 + 4 = 5
```

实际通常是 8。

原因是 alignment，对齐。

`int` 通常要求 4 字节对齐，所以编译器在 `c` 后面插入 padding：

```text
offset 0: c
offset 1: padding
offset 2: padding
offset 3: padding
offset 4: x byte 0
offset 5: x byte 1
offset 6: x byte 2
offset 7: x byte 3
```

实验：

```bash
cat > scratch/ch00c/layout.cpp <<'EOF'
#include <cstddef>
#include <iostream>

struct A {
    char c;
    int x;
};

struct B {
    int x;
    char c;
};

struct C {
    char c1;
    int x;
    char c2;
};

int main()
{
    std::cout << "sizeof(A) " << sizeof(A) << '\n';
    std::cout << "offsetof(A,c) " << offsetof(A, c) << '\n';
    std::cout << "offsetof(A,x) " << offsetof(A, x) << '\n';

    std::cout << "sizeof(B) " << sizeof(B) << '\n';
    std::cout << "sizeof(C) " << sizeof(C) << '\n';
}
EOF

g++ -O2 scratch/ch00c/layout.cpp -o scratch/ch00c/layout
./scratch/ch00c/layout
```

## alignment 为什么存在

硬件访问内存不是按 C++ 对象粒度，而是按字节地址、cache line、总线事务等机制。

对齐有几个目的：

- 让多字节对象访问更高效。
- 避免一次访问跨越多个自然边界。
- 满足某些指令或 ABI 要求。
- 让数组中每个元素都按自身要求对齐。

例如结构体数组：

```cpp
A arr[2];
```

如果 `sizeof(A)` 不是对齐的，`arr[1].x` 可能不满足 `int` 对齐要求。  
所以结构体尾部也可能有 padding。

## 栈对象、堆对象、全局对象

### 栈对象

```cpp
void f()
{
    int x = 1;
}
```

`x` 通常是自动存储期对象。未被优化掉时，它可能放在栈上，也可能放在寄存器中。

### 堆对象

```cpp
int* p = new int(1);
delete p;
```

对象由 allocator 管理，生命周期由程序控制。

### 全局对象

```cpp
int g = 1;
```

在程序启动前分配，通常位于 data 或 BSS。

性能学习里要小心：

- 栈对象可能被优化到寄存器。
- 堆分配很贵，不要放在 hot loop benchmark 中。
- 全局对象构造可能影响程序启动。

## strict aliasing 的前置理解

C++ 类型系统不仅用于检查，还影响优化。  
编译器会利用“不同类型指针通常不指向同一对象”的规则做优化。

例子：

```cpp
void f(int* p, float* q)
{
    *p = 1;
    *q = 2.0f;
    *p = *p + 1;
}
```

编译器可能假设 `int*` 和 `float*` 不别名，从而优化 load/store。

违反 aliasing 规则可能导致未定义行为。  
高性能代码经常关注 alias，因为它决定编译器能否重排、向量化、缓存 load。

第 4 章会在自动向量化中继续讲。

## 实验 A：数组访问汇编

代码：

```bash
cat > scratch/ch00c/access.cpp <<'EOF'
#include <cstddef>

extern "C" int get_i32(int const* a, int i)
{
    return a[i];
}

extern "C" double get_f64(double const* a, int i)
{
    return a[i];
}

extern "C" int get_i32_offset(int const* a, int i)
{
    return a[i + 3];
}
EOF

g++ -O3 -S -masm=intel scratch/ch00c/access.cpp -o scratch/ch00c/access.s
rg -n "get_i32|get_f64|get_i32_offset|\\[" scratch/ch00c/access.s
```

任务：

- 找到 `*4` 和 `*8`。
- 解释 displacement 从哪里来。
- 说明返回值寄存器分别是什么。

## 实验 B：struct 布局影响访问

代码：

```bash
cat > scratch/ch00c/struct_access.cpp <<'EOF'
struct Particle {
    float x;
    float y;
    float z;
    int id;
};

extern "C" float get_y(Particle const* p)
{
    return p->y;
}

extern "C" int get_id(Particle const* p)
{
    return p->id;
}

extern "C" float get_y_at(Particle const* p, int i)
{
    return p[i].y;
}
EOF

g++ -O3 -S -masm=intel scratch/ch00c/struct_access.cpp -o scratch/ch00c/struct_access.s
rg -n "get_y|get_id|get_y_at|\\[" scratch/ch00c/struct_access.s
```

任务：

- `p->y` 的 offset 是多少？
- `p->id` 的 offset 是多少？
- `p[i].y` 的地址如何计算？
- `sizeof(Particle)` 如何影响 scale 或地址计算？

## 实验 C：有符号溢出和优化

代码：

```bash
cat > scratch/ch00c/overflow.cpp <<'EOF'
#include <climits>

extern "C" bool greater_after_add_signed(int x)
{
    return x + 1 > x;
}

extern "C" bool greater_after_add_unsigned(unsigned x)
{
    return x + 1 > x;
}
EOF

g++ -O3 -S -masm=intel scratch/ch00c/overflow.cpp -o scratch/ch00c/overflow.s
rg -n "greater|cmp|set|mov" scratch/ch00c/overflow.s
```

任务：

- 比较 signed 和 unsigned 版本。
- 解释为什么 signed 版本可能恒为 true。
- 解释 unsigned 版本为什么要考虑 wraparound。

## 作业 -2.1：内存字节解释报告

写一个程序，打印以下对象的字节：

- `uint32_t 0x12345678`
- `int32_t -1`
- `float 1.0f`
- `double 1.0`
- 一个包含 `char` 和 `int` 的结构体

要求：

- 使用 `unsigned char const*` 观察字节。
- 使用 `std::bit_cast` 观察浮点 bit。
- 写报告解释每个字节序列。

## 作业 -2.2：结构体布局优化

设计三个结构体：

```cpp
struct BadLayout;
struct BetterLayout;
struct PackedLayout;
```

要求：

- `BadLayout` 故意制造 padding。
- `BetterLayout` 通过字段重排减少 padding。
- `PackedLayout` 使用编译器扩展强行 packed。

报告：

- `sizeof` 和 `offsetof`。
- 每个字段地址。
- 为什么 packed 不一定更快。
- 如果这是数组，cache line 利用率如何变化。

## 作业 -2.3：从 C++ 表达式推地址

给定：

```cpp
struct S {
    int a;
    double b;
    char c;
};

S* p;
int i;
auto value = p[i].b;
```

你要写出：

- `sizeof(S)` 可能是多少。
- `b` 的 offset。
- `p[i].b` 的地址公式。
- 可能的 x86-64 addressing mode。
- 编译后可能出现哪些寄存器。

然后写代码编译验证。

## 作业 -2.4：alias 和 vectorization 预习

写两个函数：

```cpp
void add_plain(float* y, float const* x, int n);
void add_restrict(float* __restrict y, float const* __restrict x, int n);
```

都做：

```cpp
y[i] += x[i];
```

用 Clang/GCC 输出向量化报告。

报告：

- 是否有 runtime alias check。
- `restrict` 是否改变汇编。
- 为什么 alias 信息会影响性能。

## 常见误区

### 误区 1：内存里有类型

不准确。内存里是字节。类型是程序解释字节的规则。

### 误区 2：`p + 1` 是地址加 1

错误。指针加法按指向类型大小缩放。

### 误区 3：有符号溢出和无符号溢出一样

错误。无符号溢出定义为模回绕；有符号溢出在 C++ 中是未定义行为。

### 误区 4：结构体大小等于字段大小相加

不一定。padding 和 alignment 会改变大小。

### 误区 5：packed 结构体一定省内存又更快

不一定。它可能导致未对齐访问，反而变慢。

## 验收标准

本章完成标准：

- 你能解释小端字节序。
- 你能把 `a[i]` 转换成地址公式。
- 你能看懂 `[base + index*scale + displacement]`。
- 你能解释 `sizeof`、`alignof`、`offsetof` 的意义。
- 你能预测简单 struct 的布局。
- 你能解释有符号溢出为什么影响编译器优化。
- 你能用汇编验证数组和结构体访问。
