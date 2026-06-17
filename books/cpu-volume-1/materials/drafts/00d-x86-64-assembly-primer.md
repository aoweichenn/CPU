# 第 -1 章：x86-64 汇编语法入门

## 本章目标

本章从零开始讲如何读 x86-64 汇编。它不是完整 ISA 手册，而是让你具备阅读后续章节汇编的最低硬基础。

完成本章后，你应该能：

- 区分 Intel 语法和 AT&T 语法。
- 解释一条汇编指令的 opcode、destination、source。
- 读懂寄存器别名：`rax/eax/ax/al`。
- 解释立即数、寄存器操作数、内存操作数。
- 读懂 x86-64 addressing mode：`[base + index*scale + displacement]`。
- 解释 `mov`、`lea`、`add`、`sub`、`imul`、`cmp`、`test`、`jmp`、`call`、`ret`。
- 理解 flags 和条件跳转。
- 读懂简单函数的 prologue/epilogue。

本章重点是“如何读”。真正写手写汇编和 ABI 细节在第 3 章继续。

## 为什么先学语法

如果你看到：

```asm
mov eax, dword ptr [rdi + rsi*4 + 12]
```

却不知道：

- `eax` 是什么。
- `dword ptr` 是什么。
- `[]` 是什么。
- `rdi + rsi*4 + 12` 是地址还是值。
- 这条指令读什么、写什么。

那么后面的性能分析都会变成死记硬背。

汇编不是神秘语言。它只是更接近机器的一种表示。  
你的目标不是背所有指令，而是能逐条拆解热路径代码。

## Intel 语法和 AT&T 语法

x86 汇编常见两种语法。

### Intel 语法

本书主要使用 Intel 语法：

```asm
mov eax, edi
add eax, esi
mov eax, dword ptr [rdi]
```

特点：

- destination 在前，source 在后。
- 内存地址使用 `[]`。
- 寄存器没有 `%` 前缀。
- 立即数没有 `$` 前缀。

### AT&T 语法

GCC 默认输出常是 AT&T 语法：

```asm
movl %edi, %eax
addl %esi, %eax
movl (%rdi), %eax
```

特点：

- source 在前，destination 在后。
- 寄存器有 `%` 前缀。
- 立即数有 `$` 前缀。
- 指令后缀表示大小，例如 `movl`。

本书使用：

```bash
-masm=intel
```

让 GCC/Clang 输出 Intel 语法。

## 一条指令的结构

Intel 语法：

```asm
add eax, esi
```

拆解：

```text
opcode: add
destination: eax
source: esi
```

语义：

```text
eax = eax + esi
```

读：

```text
eax, esi
```

写：

```text
eax, EFLAGS
```

注意：destination 既是输入又是输出。这和 C++ 中：

```cpp
x += y;
```

类似。

## 操作数类型

x86 指令操作数常见三类：

```text
immediate
register
memory
```

### immediate

立即数直接写在指令中：

```asm
mov eax, 42
add rdi, 8
```

### register

寄存器操作数：

```asm
mov eax, edi
add rax, rcx
```

### memory

内存操作数：

```asm
mov eax, dword ptr [rdi]
mov qword ptr [rsp + 8], rax
```

`[]` 表示括号内表达式是地址，要访问该地址处的内存。

## 重要限制：不能任意 memory-to-memory

大多数 x86 普通指令不能两个操作数都直接是内存。

通常不允许：

```asm
mov [rdi], [rsi]
```

需要中转寄存器：

```asm
mov eax, dword ptr [rsi]
mov dword ptr [rdi], eax
```

原因很底层：CPU 指令编码和执行单元通常围绕寄存器和有限内存操作设计。寄存器是计算的中心。

## 寄存器宽度和别名

`rax` 是 64 位寄存器。它的低位部分有不同名字：

```text
rax  64-bit
eax  low 32-bit
ax   low 16-bit
al   low 8-bit
ah   high 8-bit of low 16-bit
```

图：

```text
rax: [63...............................0]
eax:                         [31.......0]
 ax:                                 [15..0]
 al:                                    [7..0]
 ah:                                [15..8]
```

非常重要的规则：

```asm
mov eax, 1
```

写 32 位寄存器会把对应 64 位寄存器高 32 位清零。

也就是：

```text
rax = 0x0000000000000001
```

这也是为什么编译器经常用 `xor eax, eax` 清零。

## 常见通用寄存器

```text
rax accumulator / return value
rbx callee-saved general register
rcx counter / argument 4 in SysV
rdx data / argument 3 in SysV
rsi source index / argument 2 in SysV
rdi destination index / argument 1 in SysV
rbp base pointer
rsp stack pointer
r8-r15 additional registers
rip instruction pointer
```

不要把这些传统含义理解得太死。现代编译器会根据 ABI 和寄存器分配自由使用很多寄存器。  
但 `rsp` 和 `rip` 特殊，不能当普通变量寄存器随便用。

## 内存操作数大小

看：

```asm
mov [rdi], 1
```

这有歧义：写 1 字节、4 字节还是 8 字节？

所以 Intel 语法常写：

```asm
mov byte ptr [rdi], 1
mov dword ptr [rdi], 1
mov qword ptr [rdi], 1
```

大小：

```text
byte  = 1 byte
word  = 2 bytes
dword = 4 bytes
qword = 8 bytes
```

SIMD 中还会看到：

```text
xmmword = 16 bytes
ymmword = 32 bytes
zmmword = 64 bytes
```

## x86-64 addressing mode

最常见地址形式：

```asm
[base + index*scale + displacement]
```

各部分：

```text
base:         一个 64-bit 寄存器
index:        一个 64-bit 寄存器
scale:        1, 2, 4, 8
displacement: 常量偏移
```

例子：

```asm
mov eax, dword ptr [rdi + rsi*4 + 12]
```

语义：

```text
address = rdi + rsi * 4 + 12
eax = load 4 bytes from address
```

C++ 可能是：

```cpp
return a[i + 3];
```

因为：

```text
(i + 3) * 4 = i*4 + 12
```

## `mov`：复制位模式

`mov` 复制数据。

寄存器到寄存器：

```asm
mov eax, edi
```

内存到寄存器：

```asm
mov eax, dword ptr [rdi]
```

寄存器到内存：

```asm
mov dword ptr [rdi], esi
```

立即数到寄存器：

```asm
mov eax, 42
```

`mov` 名字叫 move，但源操作数通常不会被清空。它更像 copy。

## `lea`：地址计算，也常用于整数计算

`lea` 是 Load Effective Address。

例子：

```asm
lea rax, [rdi + rsi*4 + 12]
```

它不访问内存。  
它只计算地址表达式，把结果放进 `rax`：

```text
rax = rdi + rsi*4 + 12
```

编译器常用 `lea` 做整数加法或乘小常数：

```asm
lea eax, [rdi + rdi*2]
```

等价：

```text
eax = rdi * 3
```

初学者常误解 `lea` 一定 load 内存。不是。`lea` 不读 `[地址]` 里的内容。

## 算术指令

### `add`

```asm
add eax, esi
```

```text
eax = eax + esi
update flags
```

### `sub`

```asm
sub eax, esi
```

```text
eax = eax - esi
update flags
```

### `imul`

```asm
imul eax, esi
```

```text
eax = eax * esi
update flags
```

x86 的 `imul` 有多种形式。初期先掌握两个操作数形式。

### `inc` 和 `dec`

```asm
inc eax
dec ecx
```

加 1、减 1。  
现代编译器不一定偏爱它们，因为 flags 行为和编码/性能有细节。

## `cmp` 和 `test`

### `cmp`

```asm
cmp eax, esi
```

它计算：

```text
eax - esi
```

但不保存结果，只更新 flags。

后面通常跟条件跳转：

```asm
cmp eax, esi
jg greater
```

### `test`

```asm
test eax, eax
```

它计算：

```text
eax & eax
```

但不保存结果，只更新 flags。

常用于判断是否为 0：

```asm
test eax, eax
je zero
```

因为 `x & x` 为 0 等价于 `x == 0`。

## flags 和条件跳转

`cmp` 或 `test` 设置 flags，条件跳转读取 flags。

常见跳转：

```text
je / jz     equal / zero
jne / jnz   not equal / not zero
jg / jnle   signed greater
jge         signed greater or equal
jl          signed less
jle         signed less or equal
ja          unsigned above
jae         unsigned above or equal
jb          unsigned below
jbe         unsigned below or equal
```

为什么 signed 和 unsigned 有不同跳转？

同一组 bit pattern 可以解释成有符号或无符号。  
例如 8 位：

```text
0xff
```

无符号是 255。  
有符号补码是 -1。  
比较规则不同，所以条件跳转也不同。

## 无条件跳转

```asm
jmp label
```

直接改变 `rip` 到目标地址。

C++ 中循环、`goto`、switch、if/else 都可能生成跳转。

## `call` 和 `ret`

`call target` 大致做：

```text
push return_address
rip = target
```

`ret` 大致做：

```text
rip = memory[rsp]
rsp = rsp + 8
```

所以函数调用依赖栈保存返回地址。

这也是为什么栈损坏会导致程序跳到奇怪地址甚至崩溃。

## 栈和 `rsp`

`rsp` 是栈指针。x86-64 栈通常向低地址增长。

`push rax` 大致等价：

```text
rsp = rsp - 8
memory[rsp] = rax
```

`pop rax` 大致等价：

```text
rax = memory[rsp]
rsp = rsp + 8
```

函数 prologue 常见：

```asm
push rbp
mov rbp, rsp
sub rsp, 32
```

函数 epilogue 常见：

```asm
leave
ret
```

或：

```asm
add rsp, 32
pop rbp
ret
```

优化后编译器可能省略 `rbp`，直接用 `rsp` 或寄存器。

## 一个完整函数走读

C++：

```cpp
extern "C" int sum3(int a, int b, int c)
{
    int x = a + b;
    return x + c;
}
```

编译：

```bash
mkdir -p scratch/ch00d
cat > scratch/ch00d/sum3.cpp <<'EOF'
extern "C" int sum3(int a, int b, int c)
{
    int x = a + b;
    return x + c;
}
EOF

g++ -O3 -S -masm=intel scratch/ch00d/sum3.cpp -o scratch/ch00d/sum3.s
sed -n '/sum3:/,/\\.size/p' scratch/ch00d/sum3.s
```

可能看到：

```asm
sum3:
    add edi, esi
    lea eax, [rdi+rdx]
    ret
```

System V ABI 下：

```text
a -> edi
b -> esi
c -> edx
return -> eax
```

逐条：

```asm
add edi, esi
```

`edi = edi + esi`，得到 `a + b`。

```asm
lea eax, [rdi+rdx]
```

`eax = edi + edx`，得到 `(a+b)+c`。

```asm
ret
```

返回调用者，`eax` 中是返回值。

注意：源码里的局部变量 `x` 没有内存位置。它只存在于寄存器计算中。

## `-O0` 汇编为什么很啰嗦

同样的函数用 `-O0`：

```bash
g++ -O0 -S -masm=intel scratch/ch00d/sum3.cpp -o scratch/ch00d/sum3_O0.s
sed -n '/sum3:/,/\\.size/p' scratch/ch00d/sum3_O0.s
```

你可能看到：

```asm
push rbp
mov rbp, rsp
mov DWORD PTR -20[rbp], edi
mov DWORD PTR -24[rbp], esi
mov DWORD PTR -28[rbp], edx
...
pop rbp
ret
```

`-O0` 适合调试。它把变量放到栈上，保留清晰映射。  
`-O3` 适合性能。它会删除不必要的栈操作。

这就是为什么不要用 Debug 汇编推断性能。

## 实验 A：Intel vs AT&T

命令：

```bash
g++ -O3 -S scratch/ch00d/sum3.cpp -o scratch/ch00d/sum3_att.s
g++ -O3 -S -masm=intel scratch/ch00d/sum3.cpp -o scratch/ch00d/sum3_intel.s

sed -n '/sum3:/,/\\.size/p' scratch/ch00d/sum3_att.s
sed -n '/sum3:/,/\\.size/p' scratch/ch00d/sum3_intel.s
```

任务：

- 找出 source/destination 顺序差异。
- 找出寄存器写法差异。
- 以后统一用 Intel 语法阅读本书。

## 实验 B：寻址模式训练

代码：

```bash
cat > scratch/ch00d/addressing.cpp <<'EOF'
extern "C" int f1(int const* a, int i)
{
    return a[i];
}

extern "C" int f2(int const* a, int i)
{
    return a[i + 7];
}

extern "C" long f3(long const* a, int i)
{
    return a[i * 2 + 1];
}
EOF

g++ -O3 -S -masm=intel scratch/ch00d/addressing.cpp -o scratch/ch00d/addressing.s
rg -n "f1:|f2:|f3:|\\[" scratch/ch00d/addressing.s
```

任务：

- 对每个 `[]` 写出地址公式。
- 判断元素大小如何反映到 scale。
- 判断常量偏移如何反映到 displacement。

## 实验 C：flags 和分支

代码：

```bash
cat > scratch/ch00d/compare.cpp <<'EOF'
extern "C" int cmp_signed(int a, int b)
{
    return a > b ? 1 : 0;
}

extern "C" int cmp_unsigned(unsigned a, unsigned b)
{
    return a > b ? 1 : 0;
}
EOF

g++ -O3 -S -masm=intel scratch/ch00d/compare.cpp -o scratch/ch00d/compare.s
rg -n "cmp_signed|cmp_unsigned|cmp|set|j" scratch/ch00d/compare.s
```

任务：

- signed 和 unsigned 使用的条件是否不同？
- 是否生成 `setg`、`seta` 等指令？
- 解释 signed/unsigned 比较为什么不同。

## 作业 -1.1：逐条注释汇编

选择下面函数：

```cpp
extern "C" int max3(int a, int b, int c)
{
    int m = a;
    if (b > m) {
        m = b;
    }
    if (c > m) {
        m = c;
    }
    return m;
}
```

用 `-O0` 和 `-O3` 生成汇编。

要求：

- 对每条指令写注释。
- 标出读寄存器、写寄存器。
- 标出内存访问。
- 标出控制流。
- 解释 `-O0` 和 `-O3` 的差别。

## 作业 -1.2：寻址模式 20 题

为以下表达式写出可能的地址公式：

```cpp
a[i]
a[i + 1]
a[2 * i]
a[2 * i + 3]
p->x
p[i].x
p[i].arr[j]
```

要求：

- 给出假设的类型定义。
- 写 `sizeof` 和 offset。
- 写 x86-64 addressing mode。
- 编译验证至少 8 个例子。

## 作业 -1.3：写一个最小汇编函数

写 `scratch/ch00d/add_asm.S`：

```asm
.intel_syntax noprefix
.text
.globl add_asm
.type add_asm, @function
add_asm:
    lea eax, [edi + esi]
    ret
```

写 C++ 调用：

```cpp
extern "C" int add_asm(int, int);
```

要求：

- 编译链接运行。
- 写 20 组测试。
- 用 `objdump` 确认函数机器码。
- 解释参数在哪些寄存器。

## 常见误区

### 误区 1：Intel 和 AT&T 语法只差一点点，可以混着看

不建议。source/destination 顺序相反，初学时混着看很容易错。

### 误区 2：`lea` 会访问内存

错误。`lea` 只计算有效地址，不读取内存内容。

### 误区 3：`mov` 会清空源操作数

错误。`mov` 是复制，不是移动所有权。

### 误区 4：`cmp a, b` 保存了 `a-b`

错误。`cmp` 只更新 flags，不保存结果。

### 误区 5：`[]` 表示数组

不准确。汇编里的 `[]` 表示内存地址解引用。它可以来自数组、结构体字段、栈变量、全局变量等。

## 验收标准

本章完成标准：

- 你能稳定阅读 Intel 语法。
- 你能解释常见寄存器宽度别名。
- 你能区分立即数、寄存器、内存操作数。
- 你能读懂 `[base + index*scale + displacement]`。
- 你能解释 `mov`、`lea`、`add`、`cmp`、`test`、`jmp`、`call`、`ret`。
- 你能对一个 10 行以内的函数逐条写汇编注释。
- 你能写并链接一个最小 `.S` 函数。
