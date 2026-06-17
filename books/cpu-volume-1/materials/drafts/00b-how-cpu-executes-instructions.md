# 第 -3 章：CPU 如何执行指令

## 本章目标

本章回答一个最基础但最重要的问题：

```text
CPU 到底如何把一串机器码变成程序行为？
```

完成本章后，你应该能：

- 解释指令、寄存器、内存、程序计数器、ALU 的关系。
- 画出一个极简 CPU 的 fetch-decode-execute 循环。
- 解释一条 `add`、`mov`、`cmp`、`jmp` 指令大致如何执行。
- 区分架构状态和微架构实现。
- 解释为什么现代 CPU 需要 pipeline、cache、branch prediction、out-of-order。
- 理解“CPU 执行的是机器字节，不是 C++ 语句”这句话的实际含义。

这一章不要求你掌握芯片设计，但要求你建立足够清晰的执行模型。后面学习汇编、性能、SIMD、GEMM，都要基于这个模型。

## 从一个问题开始：`x + y` 是怎么被执行的

C++：

```cpp
int add(int x, int y)
{
    return x + y;
}
```

在 x86-64 System V ABI 下，优化后可能得到：

```asm
lea eax, [rdi + rsi]
ret
```

或者：

```asm
mov eax, edi
add eax, esi
ret
```

你在 C++ 里写的是：

```text
return x + y
```

CPU 看到的不是变量名 `x` 和 `y`，而是：

```text
从某些寄存器读位模式
执行整数加法电路
把结果写到返回值寄存器
跳回调用者
```

本章就从这件事讲起。

## 程序员模型：CPU 提供了什么抽象

对程序员和编译器来说，一个 CPU 架构通常提供以下抽象：

```text
registers
memory
instructions
program counter
flags
```

### registers

寄存器是 CPU 内部可以被指令直接读写的高速存储位置。

x86-64 通用寄存器包括：

```text
rax rbx rcx rdx
rsi rdi rbp rsp
r8  r9  r10 r11
r12 r13 r14 r15
```

它们不是 C++ 变量，但 C++ 变量在编译后可能被放进寄存器。

### memory

内存是字节数组的抽象。

程序可以通过地址读写内存：

```text
load:  memory[address] -> register
store: register -> memory[address]
```

CPU 通常不能直接对“C++ 对象”操作。对象最终要落到内存字节或寄存器位模式。

### instructions

指令告诉 CPU 做什么，例如：

```text
add
sub
mov
cmp
jmp
call
ret
load/store via mov
```

指令有两种表示：

- 汇编文本：人类可读。
- 机器码字节：CPU 实际读取。

例如某条 `ret` 指令在 x86 上常编码为字节：

```text
c3
```

### program counter

程序计数器保存下一条将要执行的指令地址。

x86-64 中对应的架构寄存器叫：

```text
rip
```

一般指令执行后，`rip` 前进到下一条指令。  
跳转、调用、返回会改变 `rip`。

### flags

x86 有状态标志寄存器，常见标志：

```text
ZF: zero flag
SF: sign flag
CF: carry flag
OF: overflow flag
```

例如：

```asm
cmp eax, edx
je equal
```

`cmp` 设置 flags。  
`je` 根据 ZF 决定是否跳转。

## 架构状态和微架构实现

这是学习 CPU 时最重要的区分之一。

### 架构状态

架构状态是软件可见的状态，包括：

- 通用寄存器。
- SIMD 寄存器。
- `rip`。
- flags。
- 内存中可见的字节。
- 特权寄存器和系统状态。

ISA 手册规定：每条指令执行后，架构状态如何变化。

例如：

```asm
add eax, edx
```

架构语义：

```text
eax = eax + edx
更新 flags
```

### 微架构实现

微架构是 CPU 内部如何实现这个语义。

它可以包括：

- pipeline。
- decode。
- uops。
- rename。
- scheduler。
- execution ports。
- reorder buffer。
- cache。
- TLB。
- branch predictor。

同样是 x86-64，Intel、AMD、不同代 CPU 微架构可以完全不同，但必须给软件呈现同一套 ISA 语义。

这就是为什么：

```text
同一段机器码在不同 CPU 上都能运行，但性能不同。
```

## 极简 CPU 模型

先想象一台非常简单的 CPU：

```text
while true:
    instruction = memory[PC]
    PC = PC + instruction_length
    decode instruction
    read input registers
    execute operation
    write output register or memory
```

画成数据流：

```text
        +----------------+
        | instruction    |
        | memory         |
        +--------+-------+
                 |
                 v
PC/RIP ---> fetch instruction bytes
                 |
                 v
             decode
                 |
                 v
        read registers / memory
                 |
                 v
              execute
                 |
                 v
       write registers / memory
                 |
                 v
         update next PC/RIP
```

这叫 fetch-decode-execute 模型。

它不等于现代 CPU 真实细节，但它是所有复杂模型的地基。

## 指令执行例子 1：寄存器加法

汇编：

```asm
mov eax, edi
add eax, esi
ret
```

假设进入函数时：

```text
edi = 10
esi = 32
```

逐条执行：

### `mov eax, edi`

语义：

```text
eax = edi
```

读：

```text
edi
```

写：

```text
eax
```

执行后：

```text
eax = 10
edi = 10
esi = 32
```

### `add eax, esi`

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
eax, flags
```

执行后：

```text
eax = 42
```

### `ret`

语义：

```text
从栈顶取返回地址
rsp = rsp + 8
rip = 返回地址
```

`ret` 是控制流指令。它让 CPU 回到调用者。

## 指令执行例子 2：内存 load

汇编：

```asm
mov eax, dword ptr [rdi]
ret
```

假设：

```text
rdi = 0x1000
memory[0x1000..0x1003] = 2a 00 00 00
```

这条 `mov` 的意思：

```text
从地址 rdi 指向的内存读 4 字节
把它作为 32 位整数放入 eax
```

数据流：

```text
rdi
 |
 v
address generation
 |
 v
load memory at address
 |
 v
eax
```

注意：`mov eax, [rdi]` 和 `mov eax, edi` 完全不同。

```asm
mov eax, edi      ; 复制寄存器值
mov eax, [rdi]    ; 把 rdi 当地址，从内存读取
```

这是初学汇编最常见的分界线。

## 指令执行例子 3：内存 store

汇编：

```asm
mov dword ptr [rdi], esi
ret
```

语义：

```text
memory[rdi..rdi+3] = low 32 bits of esi
```

读：

```text
rdi, esi
```

写：

```text
memory
```

store 不只是“写变量”。它涉及地址计算、权限检查、cache、store buffer 等机制。后面第 7 章会深入。

## 指令执行例子 4：条件分支

C++：

```cpp
int abs_i32(int x)
{
    if (x < 0) {
        return -x;
    }
    return x;
}
```

可能汇编：

```asm
mov eax, edi
neg eax
cmovs eax, edi
ret
```

也可能是：

```asm
test edi, edi
jns .non_negative
mov eax, edi
neg eax
ret
.non_negative:
mov eax, edi
ret
```

第二种用分支：

```text
test 设置 flags
jns 根据 flags 决定是否改变 rip
```

控制流指令最重要的效果是改变下一条执行的指令地址。

## 机器码：CPU 读的是字节

汇编只是人类表示。CPU 取指时读的是内存中的字节。

命令：

```bash
mkdir -p scratch/ch00b
cat > scratch/ch00b/add.cpp <<'EOF'
extern "C" int add_i32(int a, int b)
{
    return a + b;
}
EOF

g++ -O3 -c scratch/ch00b/add.cpp -o scratch/ch00b/add.o
objdump -drwC -Mintel scratch/ch00b/add.o
```

你可能看到类似：

```text
0000000000000000 <add_i32>:
   0:  8d 04 37              lea    eax,[rdi+rsi*1]
   3:  c3                    ret
```

左边：

```text
8d 04 37
c3
```

是机器码字节。  
右边：

```asm
lea eax, [rdi+rsi*1]
ret
```

是反汇编器把字节翻译回来的文本。

CPU 不知道变量名，也不知道函数名。符号名是工具和调试信息给人看的。

## 为什么需要 pipeline

极简 CPU 每次完整执行一条指令：

```text
fetch -> decode -> execute -> writeback
```

如果每条指令都等完整结束再开始下一条，硬件利用率很低。  
当第一条指令在 execute 时，fetch 单元可能闲着；当第一条在 decode 时，ALU 闲着。

pipeline 的想法是把执行拆成阶段，让不同指令同时处于不同阶段：

```text
cycle 1: instr1 fetch
cycle 2: instr1 decode, instr2 fetch
cycle 3: instr1 execute, instr2 decode, instr3 fetch
cycle 4: instr1 writeback, instr2 execute, instr3 decode, instr4 fetch
```

像工厂流水线一样提高吞吐。

重要区别：

```text
latency: 单条指令从开始到结果 ready 要多久
throughput: 稳态下每个周期能完成多少工作
```

第 5 章会深入。

## pipeline 遇到的问题

### 数据依赖

```asm
add eax, edx
imul eax, ecx
```

第二条指令需要第一条的 `eax` 结果。  
如果结果还没 ready，第二条必须等待。

### 控制依赖

```asm
cmp eax, 0
je target
```

CPU 在条件算出来前不知道下一条该取哪里。  
现代 CPU 会预测，这就是分支预测。

### 内存延迟

```asm
mov eax, [rdi]
add ecx, eax
```

如果 `[rdi]` 不在 cache，需要等很久。  
后续依赖 `eax` 的指令也会等待。

## 为什么需要 cache

CPU 核心执行很快，DRAM 相对很慢。  
如果每次 load 都去 DRAM，CPU 大部分时间会等待。

cache 的基本思想：

```text
把最近使用或附近可能使用的数据放在离 CPU 更近的小容量高速存储中。
```

简化层级：

```text
registers
  -> L1 cache
  -> L2 cache
  -> L3 cache
  -> DRAM
```

连续数组访问快，链表随机访问慢，根源之一就是 cache 和预取器是否能发挥作用。

## 为什么需要 branch prediction

pipeline 想提前取后续指令。  
遇到分支时，如果等条件算完再取，流水线会停。

所以 CPU 猜：

```text
这个分支会跳还是不跳？
跳到哪里？
```

猜对：继续高速执行。  
猜错：丢弃错误路径上的工作，重新取正确路径。

这就是为什么随机分支会很慢。

## 为什么需要 out-of-order execution

顺序执行 CPU 遇到 cache miss 可能停住：

```asm
mov eax, [rdi]    ; cache miss
add r8d, r9d      ; 和 eax 无关
add r10d, r11d    ; 和 eax 无关
```

后两条指令不依赖 `eax`，理论上可以先执行。  
out-of-order execution 就是让 CPU 在不改变单线程可见语义的前提下，先执行已经 ready 的指令。

简化流程：

```text
decode instructions
track dependencies
when operands ready, issue to execution units
execute possibly out of order
retire in program order
```

它需要：

- 寄存器重命名。
- 调度队列。
- reorder buffer。
- load/store 队列。

第 5 章会更深入。本章只要知道：现代 CPU 不一定按汇编文本顺序“执行完成”，但必须按程序顺序呈现正确结果。

## 为什么需要寄存器重命名

看：

```asm
mov eax, [rdi]
add eax, 1
mov eax, [rsi]
add eax, 2
```

架构上都写 `eax`。  
但第二组 `eax` 不一定真的依赖第一组最终结果。

现代 CPU 会把架构寄存器映射到更多物理寄存器，消除假依赖：

```text
architectural eax -> physical register P1
later eax          -> physical register P2
```

这叫 register renaming。  
它是乱序执行的关键。

## CPU 执行和 C++ 抽象的关系

C++ 提供高级抽象：

- 变量。
- 类型。
- 函数。
- 对象。
- 引用。
- 模板。
- 异常。
- RAII。

CPU 看到低级状态：

- 寄存器位模式。
- 内存字节。
- 指令。
- 地址。
- flags。

编译器负责把前者映射到后者。

例如：

```cpp
int x = a + b;
```

可能映射为：

```asm
lea eax, [rdi + rsi]
```

也可能完全没有独立的 `x`，因为它被优化掉或保存在寄存器中。

## 实验 A：观察机器码字节

代码：

```bash
mkdir -p scratch/ch00b
cat > scratch/ch00b/simple.cpp <<'EOF'
extern "C" int add_i32(int a, int b)
{
    return a + b;
}

extern "C" int load_i32(int const* p)
{
    return *p;
}

extern "C" void store_i32(int* p, int value)
{
    *p = value;
}
EOF

g++ -O3 -c scratch/ch00b/simple.cpp -o scratch/ch00b/simple.o
objdump -drwC -Mintel scratch/ch00b/simple.o
```

任务：

- 找到每个函数的机器码字节。
- 找到对应汇编。
- 标出哪些指令读寄存器、写寄存器、访问内存。

## 实验 B：用 GDB 单步观察寄存器

代码：

```bash
cat > scratch/ch00b/debug.cpp <<'EOF'
#include <iostream>

extern "C" int add_i32(int a, int b)
{
    int c = a + b;
    return c;
}

int main()
{
    std::cout << add_i32(10, 32) << '\n';
}
EOF

g++ -O0 -g scratch/ch00b/debug.cpp -o scratch/ch00b/debug
gdb -q scratch/ch00b/debug
```

GDB 中执行：

```gdb
break add_i32
run
layout asm
info registers rax rdi rsi rip rsp rbp
stepi
info registers rax rdi rsi rip rsp rbp
stepi
info registers rax rdi rsi rip rsp rbp
quit
```

任务：

- 记录进入函数时 `rdi`、`rsi` 的值。
- 每单步一条指令，记录哪些寄存器变了。
- 解释 `rip` 如何变化。

如果 `layout asm` 在你的终端不好用，可以用：

```gdb
disassemble /m add_i32
x/10i $rip
```

## 实验 C：分支如何改变 RIP

代码：

```bash
cat > scratch/ch00b/branch.cpp <<'EOF'
extern "C" int choose(int x)
{
    if (x > 0) {
        return 1;
    }
    return -1;
}
EOF

g++ -O0 -g -c scratch/ch00b/branch.cpp -o scratch/ch00b/branch.o
objdump -drwC -Mintel scratch/ch00b/branch.o
```

任务：

- 找到 `cmp` 或 `test`。
- 找到条件跳转指令。
- 画出控制流图：

```text
entry
  -> positive path
  -> non-positive path
```

## 作业 -3.1：手工执行 5 条指令

给定初始状态：

```text
rax = 0
rdi = 10
rsi = 20
memory[0x1000] = 7
rdx = 0x1000
```

指令：

```asm
mov eax, edi
add eax, esi
mov ecx, dword ptr [rdx]
add eax, ecx
ret
```

写出每条指令后：

- `eax`。
- `ecx`。
- `rdx`。
- 是否访问内存。
- 读了哪些输入。
- 写了哪些输出。

## 作业 -3.2：画 CPU 执行数据流

画出以下指令的数据流：

```asm
mov eax, dword ptr [rdi + rsi*4]
```

必须包含：

- base register。
- index register。
- scale。
- effective address。
- memory load。
- destination register。

再用 C++ 写出它可能对应的表达式。

## 作业 -3.3：解释现代 CPU 为什么复杂

写一篇不少于 800 字的小报告，回答：

```text
如果 CPU 只是按 fetch-decode-execute 顺序执行，性能会被哪些因素限制？
pipeline、cache、branch prediction、out-of-order execution 分别解决什么问题？
它们又分别引入了什么新复杂性？
```

要求：

- 至少举 3 个汇编或 C++ 例子。
- 至少画 1 张文本图。
- 不允许只写概念定义。

## 常见误区

### 误区 1：CPU 执行 C++ 语句

错误。CPU 执行机器码指令。C++ 语句经过编译后可能变成多条指令、一条指令，甚至没有指令。

### 误区 2：汇编文本就是机器码

不准确。汇编是人类可读文本，机器码是字节。汇编器负责编码。

### 误区 3：现代 CPU 完全按汇编顺序执行

不准确。现代 CPU 可以乱序执行，但按程序顺序提交可见结果。

### 误区 4：寄存器就是变量

不准确。寄存器是硬件状态。变量可能在寄存器、内存、立即数中，或者被优化掉。

### 误区 5：内存访问就是一次简单读取

不准确。内存访问涉及地址计算、虚拟地址翻译、cache、可能的 miss、权限检查等。

## 验收标准

本章完成标准：

- 你能画出 fetch-decode-execute 循环。
- 你能解释 `rip` 的作用。
- 你能逐条解释 `mov`、`add`、`cmp`、条件跳转、`ret` 的基本效果。
- 你能区分架构状态和微架构实现。
- 你能用 `objdump` 找机器码字节和反汇编。
- 你能用 GDB 单步观察寄存器变化。
- 你能解释 pipeline、cache、branch prediction、out-of-order 分别为什么存在。
