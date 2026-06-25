# x86-64, CPU, C++ Performance, HPC and AI Operator Detailed Curriculum

这是一份扩展参考手册，作用是把每个知识点讲得更细。它不是当前排期主线。

当前如果只有 3 到 4 个月，请优先执行压缩主线：

- `books/cpu-volume-1/materials/x86_64_hpc_ai_16_week_bootcamp.md`

当前只按 16 周主线推进。本文档在你卡住、需要补基础、或者 16 周结束后继续深入时再查。

核心目标：

1. 从基础不稳开始，逐步建立 C++、Linux、二进制、汇编、CPU、OS、性能测量、HPC kernel、AI 算子的完整知识链。
2. 每个知识点都通过小实验验证，每个实验都有正确性测试、性能数据、汇编证据和报告。
3. 最终达到可以阅读生产级 CPU 算子库、设计高性能 kernel、解释性能瓶颈、指导工程优化的水平。

重要提醒：

- 不要跳过基础。高性能优化的难点不是记住几个 AVX intrinsic，而是把语言语义、编译器、微架构、OS、测量方法和算法结构连起来。
- 不要只追求“跑得快”。真正的能力是能证明为什么快、为什么慢、什么时候会失效。
- 不要把 benchmark 当作计时器。benchmark 是一个实验系统，需要控制变量、正确性校验、统计方法和反例。
- 不要迷信手写汇编。现代 C++、编译器、intrinsics、profile-guided 优化和库实现都必须学会取舍。

## 如何使用这份扩展手册

当前执行计划只有一条：`books/cpu-volume-1/materials/x86_64_hpc_ai_16_week_bootcamp.md`。

这份扩展手册只在三种情况下使用：

- 16 周计划里的某个概念看不懂，需要更细解释。
- 某个 lab 做不出来，需要更多分层任务。
- 16 周结束后，想继续深入某个方向。

如果每周时间少于 8 小时，把周期乘以 1.5。  
如果每周可以稳定投入 20 小时以上，可以把基础阶段压缩，但不能跳过实验报告。

## 学习方法

每个主题都按 7 步走：

1. 先问问题：这节课要解释哪个现象？
2. 建立最小模型：用语言或公式描述预期性能。
3. 写最小代码：先正确，再优化。
4. 看编译产物：`.s`、`objdump`、编译器优化报告。
5. 做测量：多组输入、重复测量、记录环境。
6. 解释差异：理论、汇编、运行数据哪里不一致？
7. 写报告：把本次学到的规则和反例固定下来。

每周固定产出：

- 一份学习笔记。
- 一个可运行实验。
- 一份短报告。
- 一个“我原来理解错了什么”的复盘。

## 仓库建议结构

建议把仓库整理成以下结构：

```text
CPU/
  CMakeLists.txt
  docs/
    x86_64_hpc_ai_master_plan.md
    x86_64_hpc_ai_detailed_curriculum.md
    reports/
      lab00.md
      lab01.md
  labs/
    lab00_benchmark_foundation/
    lab01_binary_pipeline/
    lab02_abi_assembly/
  tools/
    env_report.sh
    run_bench.py
    plot_csv.py
  third_party/
  results/
    lab00/
  notes/
    reading/
    mistakes/
```

每个 lab 建议结构：

```text
labXX_name/
  README.md
  CMakeLists.txt
  src/
  include/
  tests/
  bench/
  scripts/
  results/
  report.md
```

## 基础桥接阶段

这个阶段是为基础不稳准备的。不要觉得它简单。后面所有性能优化都依赖这些基础。

### Bridge 00：Linux 命令行和文件系统

目标：

- 能在 Linux 下独立创建、编译、运行、调试 C++ 程序。
- 熟悉性能实验常用命令。

必须掌握：

- `pwd`、`ls`、`cd`、`mkdir`、`rm`、`cp`、`mv`。
- `find`、`rg`、`sed`、`awk` 的基础用法。
- `time`、`taskset`、`lscpu`、`uname`、`cat /proc/cpuinfo`。
- stdout、stderr、pipe、redirect。

练习：

- 用命令行创建一个 `hello.cpp`，编译并运行。
- 用 `rg` 搜索一个函数名。
- 用 `find` 列出所有 `.cpp` 文件。
- 用 `time` 粗略测一个程序运行时间。

作业：

- 写一页“我的机器环境报告”，包含 OS、kernel、CPU、编译器、shell、工作目录。
- 解释 WSL2、虚拟机、裸机 Linux 对性能实验的影响。

验收：

- 不依赖 IDE，也能完成编译运行。
- 能解释相对路径和绝对路径。

### Bridge 01：C++ 编译和 CMake 入门

目标：

- 理解 C++ 文件如何变成可执行程序。
- 会写最小 CMake 项目。

必须掌握：

- `.cpp`、`.hpp`、translation unit。
- declaration、definition、linking。
- `g++ main.cpp -o main`。
- `cmake -S . -B build`、`cmake --build build`。
- Debug 和 Release 的区别。

练习：

- 写 `add.hpp`、`add.cpp`、`main.cpp`。
- 故意制造 undefined reference，理解链接错误。
- 分别用 Debug 和 Release 编译。

作业：

- 写一个最小 CMake 工程，包含一个库、一个可执行程序、一个测试程序。
- 报告中解释编译错误、链接错误、运行时错误的区别。

验收：

- 能独立解决 include path 和 link target 问题。
- 能解释为什么头文件里随便放非 inline 函数定义会出问题。

### Bridge 02：C++ 内存和对象基础

目标：

- 为后续 cache、SIMD、ABI 打基础。

必须掌握：

- stack、heap、static storage。
- object lifetime。
- pointer、reference、array、`std::vector`。
- `sizeof`、`alignof`、padding。
- RAII。

练习：

- 打印不同 struct 的 `sizeof` 和字段偏移。
- 比较 `std::vector<int>` 连续访问和 `std::list<int>` 遍历。
- 故意写 use-after-free，用 AddressSanitizer 检出。

作业：

- 写 5 个 struct 布局实验，解释 padding 为什么出现。
- 写一页“C++ 对象模型对性能的影响”。

验收：

- 能画出一个 vector 的内存布局。
- 能解释为什么连续内存通常更快。

### Bridge 03：数制、整数、浮点和误差

目标：

- 理解二进制表示，为位运算、SIMD、量化打基础。

必须掌握：

- binary、hex、two's complement。
- signed/unsigned conversion。
- IEEE-754 float/double。
- NaN、Inf、subnormal。
- 浮点加法不满足严格结合律。

练习：

- 打印整数和浮点的 bit pattern。
- 比较 `(a + b) + c` 和 `a + (b + c)`。
- 构造 float sum 顺序不同导致结果不同的例子。

作业：

- 写一个 float bit inspector。
- 写一份“为什么优化 reduction 需要考虑数值误差”报告。

验收：

- 能解释 `0.1 + 0.2 != 0.3`。
- 能解释 int8 量化为什么会产生误差。

### Bridge 04：调试、测试和 sanitizer

目标：

- 优化之前先保证程序正确。

必须掌握：

- `gdb` 基础。
- `assert`。
- AddressSanitizer、UndefinedBehaviorSanitizer。
- 随机测试和 reference implementation。

练习：

- 写一个错误数组越界程序，用 ASan 检出。
- 写一个 signed overflow 程序，用 UBSan 检出。
- 给 `dot` 写 reference 和随机测试。

作业：

- 为一个小型 vector math 库写 20 个测试。
- 故意注入 5 个 bug，证明测试能抓住。

验收：

- 所有性能 lab 都必须先有 correctness test。
- 能解释 sanitizer 对性能测量的影响。

## Lab 00：建立可信性能实验室

### 为什么学

性能优化第一课不是 AVX，而是测量。没有可信测量，就无法判断优化是否真实。

### 前置知识

- 会编译运行 C++。
- 知道 Debug/Release 区别。
- 知道循环可能被编译器优化掉。

### 教学讲解顺序

1. 什么是性能实验。
2. 为什么 `time ./a.out` 只能做粗略观察。
3. 为什么 benchmark 必须有 warmup。
4. 为什么要防止 dead-code elimination。
5. 为什么要重复测量并取 median。
6. 为什么要记录环境。
7. WSL2、虚拟机、裸机的测量差异。

### 课堂演示

演示 1：错误 benchmark。

```cpp
for (int i = 0; i < n; ++i) {
    x += i;
}
```

如果 `x` 没有被使用，编译器可能删除整个循环。

演示 2：加入 checksum。

```cpp
volatile int sink = x;
```

这能防止部分优化，但不是所有场景都适合。后续会学习更好的 benchmark guard。

演示 3：不同优化级别。

```bash
g++ -O0 bench.cpp -o bench_O0
g++ -O3 -march=native bench.cpp -o bench_O3
```

观察运行时间和汇编差异。

### 实验任务

任务 A：环境记录器。

- 记录 `uname -a`。
- 记录 `lscpu`。
- 记录 GCC 和 Clang 版本。
- 记录是否运行在 WSL2。
- 记录 `/proc/sys/kernel/perf_event_paranoid`。

任务 B：benchmark harness。

- 支持 warmup iterations。
- 支持 measured iterations。
- 支持输入规模列表。
- 输出 CSV：`name,size,iteration,ns,checksum`。
- 每个 benchmark 必须返回 checksum。

任务 C：基础 benchmark。

- `sum_i32`。
- `sum_f32`。
- `dot_f32`。
- `memset_like`。
- `copy_like`。

任务 D：错误 benchmark 博物馆。

至少写 5 个错误 benchmark：

- 结果未使用导致循环删除。
- 在热路径里打印。
- 每次测量都重新分配大内存。
- 没有 warmup。
- 把随机数生成放进被测循环。
- Debug build 当成优化结果。

### 分层作业

基础作业：

- 完成 benchmark harness。
- 对 3 个输入规模测 `sum_i32`。
- 写 1 页报告。

进阶作业：

- 对 5 个 benchmark 扫描 10 个输入规模。
- 输出 CSV 并画图。
- 分析 cache 容量附近的性能变化。

挑战作业：

- 在 WSL2 和裸机 Linux 或另一台机器上分别测量。
- 解释差异来自哪里。

### 报告必须回答

- 本机 CPU 是什么？
- 有哪些指令集？
- 是否能使用硬件 PMU？
- benchmark 如何防止被优化掉？
- 为什么选择 median？
- 哪个输入规模波动最大？为什么？

### 常见错误

- 忘记 Release 编译。
- 用 sanitizer build 做性能结论。
- 没有固定输入数据。
- 只测一次。
- 把内存分配时间混入 kernel 时间。

### 验收标准

- 一条命令可以跑完整个 lab。
- 结果可复现。
- 报告能说明测量限制。

## Lab 01：C++ 到目标文件、汇编和二进制

### 为什么学

性能优化不能停留在源码层。你必须知道编译器实际生成了什么。

### 前置知识

- C++ 函数、头文件、编译链接。
- 基本 Linux 命令。

### 教学讲解顺序

1. preprocessing：宏和 include 展开。
2. compilation：C++ 到汇编。
3. assembly：汇编到目标文件。
4. linking：目标文件到可执行程序。
5. ELF 文件包含什么。
6. symbol、section、relocation。
7. `objdump`、`readelf`、`nm` 如何使用。

### 课堂演示

编译分阶段执行：

```bash
g++ -E main.cpp -o main.i
g++ -S -O3 -masm=intel main.cpp -o main.s
g++ -c main.cpp -o main.o
g++ main.o -o main
```

观察：

- `.i` 文件为什么巨大。
- `.s` 文件里的函数标签。
- `.o` 文件不能直接运行。
- 可执行文件中符号名如何变化。

### 实验任务

写 12 个函数：

- `add_i32`
- `add_f32`
- `loop_sum`
- `if_else`
- `switch_case`
- `call_function`
- `inline_candidate`
- `template_add`
- `lambda_call`
- `virtual_call`
- `vector_sum`
- `throw_exception`

对每个函数：

- 保存 `-O0` 汇编。
- 保存 `-O2` 汇编。
- 保存 `-O3 -march=native` 汇编。
- 记录函数是否被 inline。
- 记录关键指令。

### 分层作业

基础作业：

- 用 `objdump -drwC -Mintel` 找到 5 个函数。
- 解释 `add_i32` 和 `add_f32` 用的寄存器不同。

进阶作业：

- 对比 GCC 和 Clang 生成的汇编。
- 找出至少 5 个差异，并解释可能原因。

挑战作业：

- 分析一个标准库调用，例如 `std::vector<int>::push_back` 的调用路径。
- 解释哪些部分被 inline，哪些没有。

### 报告必须回答

- `-O0` 为什么汇编很长？
- inline 后函数为什么在符号表里看不到？
- 虚函数调用和普通调用的汇编差异是什么？
- 异常支持对二进制带来哪些 section 或符号？

### 常见错误

- 只看 `.s`，不看最终二进制反汇编。
- 忘记 name demangling。
- 把源码行数和汇编行数直接对应。
- 认为 C++ 语句一定对应一段连续汇编。

### 验收标准

- 能从二进制里找到指定函数。
- 能解释一个函数从源码到机器码的路径。

## Lab 02：System V AMD64 ABI 和手写汇编

### 为什么学

ABI 是 C++、汇编、操作系统、动态库之间的契约。性能工程里经常需要理解函数调用成本、寄存器保存、栈布局。

### 前置知识

- 会看简单汇编。
- 知道寄存器是什么。

### 教学讲解顺序

1. 通用寄存器和 SIMD 寄存器。
2. 参数如何传递。
3. 返回值如何传递。
4. caller-saved 和 callee-saved。
5. 栈 16 字节对齐。
6. leaf function 和 red zone。
7. struct return。
8. C++ name mangling 和 `extern "C"`。

### 实验任务

任务 A：C++ 调汇编。

- 写 `extern "C" int64_t add_i64(int64_t, int64_t);`
- 用 `.S` 文件实现。
- C++ 测试随机输入。

任务 B：汇编调 C++。

- C++ 提供 callback。
- 汇编函数调用 callback。
- 检查寄存器保存是否正确。

任务 C：浮点 ABI。

- 实现 `float add_f32(float, float)`。
- 实现 `double add_f64(double, double)`。
- 观察 `xmm0/xmm1`。

任务 D：破坏 ABI。

- 故意修改 `rbx` 不恢复。
- 观察调用者错误。
- 写报告解释为什么错。

### 分层作业

基础作业：

- 手写 `add_i64`、`max_i64`、`sum_i64`。
- 全部通过随机测试。

进阶作业：

- 手写 `dot_i32`。
- 对比 C++ `-O3` 版本汇编。

挑战作业：

- 写一个小型 context dump，函数入口保存所有通用寄存器到结构体。
- 在 C++ 中打印。

### 报告必须回答

- 前 6 个整数参数在哪些寄存器？
- 第 7 个参数在哪里？
- 哪些寄存器由 callee 保存？
- 为什么调用函数前栈要 16 字节对齐？
- `extern "C"` 解决了什么问题？

### 常见错误

- 忘记恢复 `rbx/rbp/r12-r15`。
- 栈没有对齐就调用 C++ 函数。
- Intel 语法和 AT&T 语法混用。
- 忘记 `.globl`。

### 验收标准

- 能画出函数调用时寄存器和栈的变化。
- 能定位一个 ABI 错误。

## Lab 03：数据表示、位运算和 branchless 基础

### 为什么学

很多性能优化来自减少分支、利用位运算、避免未定义行为。但这些技巧如果不理解语义，很容易写错。

### 前置知识

- 整数和浮点基础。
- 简单汇编。

### 教学讲解顺序

1. two's complement。
2. sign extension 和 zero extension。
3. shift 的语义。
4. `test`、`cmp`、flags。
5. `setcc` 和 `cmov`。
6. branchless 的收益和代价。
7. signed overflow UB。

### 实验任务

实现并测试：

- `abs_i32`。
- `min_i32`。
- `max_i32`。
- `clamp_i32`。
- `is_power_of_two`。
- `round_up_to_power_of_two`。
- `popcount`。

每个函数写：

- straightforward branch version。
- branchless version。
- standard library 或 compiler builtin version。

数据分布：

- 全正。
- 全负。
- 正负随机。
- 有序。
- 极端值。

### 分层作业

基础作业：

- 实现所有函数并通过边界测试。
- 生成汇编并标注 `cmp/test/cmov/setcc`。

进阶作业：

- 对不同数据分布 benchmark。
- 解释 branch version 何时更快。

挑战作业：

- 找 3 个 UB 相关例子，证明优化后结果和直觉不同。

### 报告必须回答

- branchless 为什么不是永远更快？
- `cmov` 解决了什么问题，又带来什么代价？
- `std::popcount` 生成了什么指令？
- signed overflow 为什么危险？

### 验收标准

- 能从汇编识别条件分支和条件移动。
- 能解释数据分布如何影响分支预测。

## Lab 04：二进制阅读和控制流逆向

### 为什么学

你必须能读懂没有源码或源码被高度优化后的程序行为。这是性能排障、库阅读和调试的基础。

### 前置知识

- `objdump`。
- 基本控制流汇编。

### 教学讲解顺序

1. basic block。
2. control-flow graph。
3. if/else lowering。
4. loop lowering。
5. switch jump table。
6. tail call。
7. vtable call。

### 实验任务

任务 A：源码到 CFG。

- 给 5 个 C++ 函数画 CFG。
- 再看汇编 CFG。
- 比较差异。

任务 B：汇编到伪代码。

- 给 5 段反汇编，恢复伪代码。
- 写等价 C++，让输出一致。

任务 C：真实函数阅读。

- 从一个小型开源库选一个 50 到 100 行函数。
- 先看汇编猜功能，再看源码验证。

### 分层作业

基础作业：

- 恢复 3 个简单函数伪代码。

进阶作业：

- 恢复包含 switch 和 loop 的函数。

挑战作业：

- 设计一个小型 binary puzzle，让别人通过反汇编找输入。

### 报告必须回答

- 如何从 `cmp` 和 `jcc` 判断条件？
- 如何识别数组访问？
- 如何识别结构体字段？
- 如何识别虚函数调用？

### 验收标准

- 能画 CFG。
- 能把简单汇编还原为 C++ 伪代码。

## Lab 05：编译器优化开关和优化报告

### 为什么学

很多优化不是手写汇编完成的，而是通过让编译器看懂代码完成的。

### 前置知识

- 会看汇编。
- 会写 loop。

### 教学讲解顺序

1. `-O0/-O1/-O2/-O3/-Ofast`。
2. `-march` 和 `-mtune`。
3. inlining。
4. loop invariant code motion。
5. unrolling。
6. vectorization。
7. GCC/Clang optimization reports。
8. strict aliasing 和 `restrict`。

### 实验任务

准备 30 个 loop case：

- 简单 map。
- zip map。
- reduction。
- prefix sum。
- histogram。
- stencil。
- pointer alias。
- function call in loop。
- branch in loop。
- indirect access。

对每个 case：

- GCC `-fopt-info-vec-optimized -fopt-info-vec-missed`。
- Clang `-Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize`。
- 记录是否向量化。
- 记录失败原因。

### 分层作业

基础作业：

- 完成 10 个 case。
- 解释 3 个 missed reason。

进阶作业：

- 完成 30 个 case。
- 修复 10 个 missed vectorization。

挑战作业：

- 写一份编译器优化案例库，后续每次遇到类似问题都能查。

### 报告必须回答

- `-O3` 相比 `-O2` 多做了什么？
- `-Ofast` 为什么可能改变数值语义？
- `restrict` 为什么能帮助向量化？
- 函数调用为什么可能阻碍优化？

### 验收标准

- 能用优化报告解释向量化成败。
- 能通过源码改写帮助编译器。

## Lab 06：latency、throughput、uops 和 llvm-mca

### 为什么学

现代 CPU 不是“一条指令执行完再执行下一条”。必须理解乱序执行和吞吐模型。

### 前置知识

- 会生成汇编。
- 知道基本 SIMD 指令。

### 教学讲解顺序

1. latency。
2. throughput。
3. dependency chain。
4. independent chains。
5. uop。
6. execution port。
7. scheduler、ROB、register renaming。
8. `llvm-mca` 的输入和输出。

### 实验任务

任务 A：latency chain。

- 构造 `x = x + a`。
- 构造 `x = x * a`。
- 构造 FMA dependency chain。

任务 B：throughput。

- 构造 4 到 8 个独立 accumulator。
- 比较运行时间。

任务 C：`llvm-mca`。

- 提取循环汇编。
- 用 `llvm-mca -mcpu=arrowlake-s` 分析。
- 记录 IPC、block throughput、resource pressure。

### 分层作业

基础作业：

- 分析 `add`、`imul`、`vaddps`。

进阶作业：

- 分析 `vfmadd231ps`、`vpermps`、`vgather`。

挑战作业：

- 对比 uops.info、Intel Intrinsics Guide、`llvm-mca` 和实测。

### 报告必须回答

- latency 和 throughput 有什么区别？
- 为什么多个 accumulator 会更快？
- `llvm-mca` 不能预测哪些东西？
- 为什么 WSL2 下实测可能和模型差异大？

### 验收标准

- 能判断一个 loop 是依赖链瓶颈还是吞吐瓶颈。
- 能读懂 `llvm-mca` 的 resource pressure。

## Lab 07：分支预测、条件移动和前端瓶颈

### 为什么学

分支错误预测和前端供给不足是高性能代码的常见瓶颈。

### 教学讲解顺序

1. pipeline 为什么需要预测。
2. predictable branch。
3. unpredictable branch。
4. branchless rewrite。
5. code size。
6. instruction cache 和 uop cache。
7. loop unroll 的收益和风险。

### 实验任务

实现 threshold sum：

- branch version。
- `cmov` version。
- arithmetic mask version。
- SIMD mask version。

数据分布：

- 全部小于阈值。
- 全部大于阈值。
- 90/10。
- 50/50 随机。
- 周期模式。

### 分层作业

基础作业：

- 对比 branch 和 branchless。

进阶作业：

- 加入 unroll factor 1、2、4、8、16。

挑战作业：

- 构造一个 unroll 过大导致变慢的例子，并用汇编解释。

### 报告必须回答

- 随机分支为什么慢？
- branchless 为什么可能浪费执行端口？
- code size 如何影响前端？

### 验收标准

- 能基于数据分布选择 branch 或 branchless。

## Lab 08：cache、bandwidth、TLB 和数据布局

### 为什么学

很多程序慢不是因为算得慢，而是因为等内存。

### 教学讲解顺序

1. cache line。
2. L1/L2/L3/DRAM。
3. spatial locality。
4. temporal locality。
5. hardware prefetch。
6. TLB。
7. page walk。
8. AoS、SoA、AoSoA。

### 实验任务

任务 A：latency ladder。

- pointer chasing。
- 工作集从 KB 到 GB。
- 画访问延迟曲线。

任务 B：bandwidth。

- sequential read。
- sequential write。
- copy。
- triad。

任务 C：stride。

- stride 从 1 到 4096 bytes。
- 观察 cache line 和 page 影响。

任务 D：layout。

- 粒子结构 AoS。
- 粒子结构 SoA。
- 粒子结构 AoSoA。

### 分层作业

基础作业：

- 完成 sequential read/write。

进阶作业：

- 画 cache/TLB 拐点图。

挑战作业：

- 给一个真实结构体做 layout 优化，至少提升 1.5x。

### 报告必须回答

- L1/L2/L3 拐点在哪里？
- 什么是 cache line waste？
- AoS 和 SoA 各自适合什么场景？
- TLB miss 为什么会很贵？

### 验收标准

- 能从访问模式判断 memory-bound。
- 能用数据布局优化内存访问。

## Lab 09：load/store、alignment、prefetch 和 streaming store

### 为什么学

高性能 kernel 经常受 load/store 限制。理解写入路径和预取是必要能力。

### 教学讲解顺序

1. load queue 和 store queue。
2. store forwarding。
3. alignment。
4. split load/store。
5. write allocate。
6. software prefetch。
7. non-temporal store。

### 实验任务

- aligned vs unaligned load。
- store forwarding penalty。
- copy benchmark。
- streaming store benchmark。
- software prefetch 正例和反例。

### 分层作业

基础作业：

- 实现 aligned/unaligned benchmark。

进阶作业：

- 对大数组 copy 比较普通 store 和 non-temporal store。

挑战作业：

- 写一个 prefetch 有收益和一个 prefetch 负收益的实验。

### 报告必须回答

- 为什么 unaligned 不总是慢？
- write allocate 会带来什么隐藏流量？
- prefetch 为什么可能变慢？

### 验收标准

- 能判断什么时候使用 streaming store。

## Lab 10：自动向量化训练营

### 为什么学

生产代码里大量 SIMD 来自编译器自动向量化。你需要让源码对编译器友好。

### 教学讲解顺序

1. vectorization legality。
2. vectorization profitability。
3. alias analysis。
4. reduction recognition。
5. tail handling。
6. alignment hints。
7. loop canonicalization。

### 实验任务

20 个 loop：

- `y[i] = a[i] + b[i]`。
- `y[i] = alpha * x[i] + y[i]`。
- sum reduction。
- dot reduction。
- min/max reduction。
- conditional map。
- stencil。
- histogram。
- gather。
- scatter。

每个 loop：

- scalar baseline。
- compiler-friendly rewrite。
- 编译器报告。
- 汇编验证。

### 作业分层

基础作业：

- 让 8 个 loop 自动向量化。

进阶作业：

- 让 12 个 loop 自动向量化，并解释 5 个无法向量化的 case。

挑战作业：

- 把一个图像处理 kernel 改写到自动向量化。

### 报告必须回答

- 编译器为什么担心 alias？
- reduction 如何被识别？
- tail loop 是什么？
- 为什么 histogram 很难自动向量化？

### 验收标准

- 能读懂 vectorization report。
- 能通过源码结构影响 SIMD。

## Lab 11：AVX2/FMA intrinsics 基础

### 为什么学

当自动向量化不够时，需要用 intrinsics 明确表达 SIMD。

### 教学讲解顺序

1. SIMD register。
2. `__m256`。
3. aligned/unaligned load。
4. broadcast。
5. FMA。
6. multiple accumulators。
7. horizontal reduction。
8. tail handling。

### 实验任务

实现：

- `saxpy_f32`。
- `dot_f32`。
- `sum_f32`。
- `l2_norm_f32`。
- `relu_f32`。
- `clamp_f32`。

每个函数：

- scalar。
- auto-vectorized。
- AVX2 intrinsics。

### 作业分层

基础作业：

- 写 `saxpy` 和 `sum`。

进阶作业：

- `dot` 写 1、2、4、8 accumulator。

挑战作业：

- 用 `llvm-mca` 分析不同 accumulator 数量的吞吐。

### 报告必须回答

- 为什么 FMA 能减少指令数？
- 为什么 horizontal reduction 麻烦？
- accumulator 数量如何隐藏 latency？
- AVX2 和 SSE 混用有什么风险？

### 验收标准

- 能写正确的 AVX2 kernel。
- 能解释核心指令。

## Lab 12：SIMD shuffle、gather、transpose 和 mask

### 为什么学

SIMD 难点往往不是加减乘，而是数据排列。

### 教学讲解顺序

1. lane。
2. shuffle。
3. permute。
4. blend。
5. gather。
6. mask load/store。
7. SIMD transpose。

### 实验任务

- 4x4 float transpose。
- 8x8 float transpose。
- AoS to SoA。
- gather lookup。
- mask tail。

### 作业分层

基础作业：

- 完成 4x4 transpose。

进阶作业：

- 完成 AoS to SoA，并对比后续计算性能。

挑战作业：

- 设计一个通过 layout 改写消除 gather 的实验。

### 报告必须回答

- shuffle 为什么贵？
- gather 为什么通常不如连续 load？
- AoSoA 解决了什么问题？

### 验收标准

- 能看出 kernel 是否被数据重排限制。

## Lab 13：AVX-VNNI、int8 dot 和量化基础

### 为什么学

AI 推理大量使用低精度计算。int8 性能优化需要同时理解数值和指令。

### 教学讲解顺序

1. int8/uint8。
2. scale 和 zero point。
3. per-tensor 和 per-channel。
4. int32 accumulation。
5. requantization。
6. AVX-VNNI。
7. saturation。

### 实验任务

- fp32 vector quantize to int8。
- int8 dot reference。
- AVX2 int8 dot。
- AVX-VNNI int8 dot。
- int8 GEMV。

### 作业分层

基础作业：

- 完成 quantize/dequantize，并给误差统计。

进阶作业：

- 完成 int8 dot 和 GEMV。

挑战作业：

- per-channel quantization，并和 per-tensor 对比精度。

### 报告必须回答

- int8 为什么可能更快？
- int32 accumulator 什么时候可能溢出？
- zero point 如何影响公式？
- 性能提升是否被量化/反量化抵消？

### 验收标准

- 正确性和误差都可解释。
- 能使用本机 AVX-VNNI 能力。

## Lab 14：GEMM baseline、roofline 和循环顺序

### 为什么学

GEMM 是 HPC 和 AI 算子的核心。理解 GEMM 是进入高性能计算的关键门槛。

### 教学讲解顺序

1. 矩阵乘法定义。
2. row-major。
3. `ijk/ikj/jik/kij/jki/kji`。
4. FLOPs 计算。
5. bytes 估计。
6. arithmetic intensity。
7. roofline。

### 实验任务

- 实现 6 种循环顺序。
- 支持任意 M/N/K。
- 支持 alpha/beta 简化版。
- 统计 GFLOP/s。
- 和 BLAS 对比。

### 作业分层

基础作业：

- 实现 naive `ijk`。

进阶作业：

- 实现 6 种顺序并画性能图。

挑战作业：

- 找出每种 loop order 的最佳和最差 shape。

### 报告必须回答

- GEMM 的 FLOPs 是多少？
- 为什么 `ijk` 通常差？
- 哪个矩阵访问不连续？
- roofline 给出的上限是什么？

### 验收标准

- 能从访问模式解释性能。

## Lab 15：GEMM blocking、packing 和 AVX2 microkernel

### 为什么学

这是从普通优化进入 HPC kernel 的核心 lab。

### 教学讲解顺序

1. 为什么 naive GEMM 重复读内存。
2. cache blocking。
3. register blocking。
4. microkernel。
5. packing A/B。
6. K blocking。
7. prefetch。
8. BLIS 思想。

### 实验任务

阶段 1：blocked GEMM。

- 实现 `mc/nc/kc` 三层 blocking。
- 暂不 packing。

阶段 2：packing。

- pack B panel。
- pack A panel。
- 对比 packing 成本。

阶段 3：AVX2 microkernel。

- 实现 4x8 或 6x8 microkernel。
- 用 FMA。
- 多 accumulator。

阶段 4：对比生产库。

- OpenBLAS 或 BLIS 单线程。
- 至少 10 个 shape。

### 作业分层

基础作业：

- blocked GEMM 比 naive 有明显提升。

进阶作业：

- packed GEMM + AVX2 microkernel 达到参考 BLAS 50%。

挑战作业：

- 达到参考 BLAS 70%，并解释剩余差距。

### 报告必须回答

- blocking 尺寸如何选择？
- packing 为什么能提升连续访问？
- microkernel 中寄存器如何分配？
- load/FMA 比例是多少？
- 为什么生产库仍然更快？

### 验收标准

- 能写出结构清晰的 GEMM kernel。
- 能用数据和汇编解释性能。

## Lab 16：卷积、im2col、direct convolution 和融合

### 为什么学

CNN 算子体现了“算法转换”和“内存膨胀”的性能权衡。

### 教学讲解顺序

1. NCHW 和 NHWC。
2. convolution 定义。
3. 1x1 convolution。
4. im2col。
5. direct convolution。
6. depthwise convolution。
7. fusion。

### 实验任务

- naive 2D convolution。
- im2col + GEMM。
- direct 3x3 convolution。
- conv + bias + relu fusion。

### 作业分层

基础作业：

- 实现 naive convolution。

进阶作业：

- im2col + GEMM 比 naive 快。

挑战作业：

- 对 1x1、3x3、depthwise 分别选择实现策略。

### 报告必须回答

- im2col 为什么会增加内存？
- 什么时候 im2col 仍然值得？
- fusion 减少了哪些内存流量？

### 验收标准

- 能根据 shape 选择 conv 实现。

## Lab 17：softmax、layernorm、GELU 和 transformer 常用算子

### 为什么学

AI 模型不只有 matmul。很多端到端性能瓶颈在 memory-bound elementwise/reduction 算子。

### 教学讲解顺序

1. stable softmax。
2. max reduction。
3. exp。
4. sum reduction。
5. layernorm。
6. RMSNorm。
7. GELU approximation。
8. fusion。

### 实验任务

- softmax reference。
- softmax SIMD。
- layernorm。
- RMSNorm。
- GELU exact 和 approximate。

### 作业分层

基础作业：

- 正确实现 stable softmax。

进阶作业：

- 优化 layernorm，支持 hidden size 扫描。

挑战作业：

- 融合 bias + GELU 或 residual + layernorm。

### 报告必须回答

- softmax 为什么需要减 max？
- reduction 为什么难以达到高 FLOP/s？
- 近似 exp/GELU 的误差如何评估？

### 验收标准

- 能同时分析性能和数值稳定性。

## Lab 18：attention block CPU mini-kernel

### 为什么学

attention 综合了 matmul、softmax、layout、cache blocking 和中间张量流量。

### 教学讲解顺序

1. Q/K/V。
2. QK^T。
3. scale 和 mask。
4. softmax。
5. PV。
6. KV cache。
7. streaming softmax。
8. FlashAttention 思想简化。

### 实验任务

- reference attention。
- causal mask。
- blocked attention。
- streaming softmax 简化版。
- 内存占用统计。

### 作业分层

基础作业：

- 实现单头 attention reference。

进阶作业：

- blocked attention 减少 cache miss。

挑战作业：

- 简化 CPU FlashAttention，不写回完整 score matrix。

### 报告必须回答

- attention 哪些部分 compute-bound？
- 哪些部分 memory-bound？
- 中间矩阵写回成本是多少？
- KV cache 如何改变访问模式？

### 验收标准

- 能解释 attention 优化不是简单调用两次 GEMM。

## Lab 19：Linux perf、VTune 和 PMU

### 为什么学

到这个阶段，不能只靠时间猜瓶颈。需要用硬件计数器验证。

### 前置要求

- 最好使用裸机 Linux。
- WSL2 只能做部分软件事件和计时实验。

### 教学讲解顺序

1. PMU 是什么。
2. `perf stat`。
3. `perf record`。
4. `perf report`。
5. `perf annotate`。
6. cycles 和 instructions。
7. IPC。
8. cache miss、branch miss。
9. Top-Down 方法。
10. VTune hotspot。

### 实验任务

- 在裸机重跑 branch lab。
- 在裸机重跑 cache lab。
- 在裸机重跑 GEMM lab。
- 用 `perf annotate` 标注热点汇编。

### 作业分层

基础作业：

- 对一个程序收集 cycles、instructions、branches。

进阶作业：

- 对三个不同瓶颈程序做 PMU 对比。

挑战作业：

- 找出一个原本判断错误的瓶颈，并用 PMU 修正。

### 报告必须回答

- IPC 高一定好吗？
- cache-misses 高一定是瓶颈吗？
- PMU multiplexing 是什么？
- 为什么虚拟机下 PMU 不可靠？

### 验收标准

- 能把 PMU 数据和汇编热点对应起来。

## Lab 20：线程、亲和性、false sharing 和 C++ 内存模型

### 为什么学

单核优化之后，多核扩展会遇到调度、同步和缓存一致性问题。

### 教学讲解顺序

1. thread。
2. core。
3. hardware thread。
4. CPU affinity。
5. cache coherence。
6. false sharing。
7. mutex。
8. atomic。
9. memory order。
10. work partition。

### 实验任务

- parallel sum。
- blocked parallel sum。
- false sharing benchmark。
- atomic counter。
- mutex counter。
- thread pinning。

### 作业分层

基础作业：

- 实现 parallel reduction。

进阶作业：

- 用 padding 消除 false sharing。

挑战作业：

- 对 relaxed/acquire/release/seq_cst 写可解释实验。

### 报告必须回答

- 为什么线程数越多不一定越快？
- false sharing 如何发生？
- atomic 慢在哪里？
- affinity 为什么影响结果？

### 验收标准

- 能设计可扩展的多线程 benchmark。

## Lab 21：NUMA、huge page、page fault 和内存分配

### 为什么学

服务器性能经常被 OS 内存策略影响。AI/HPC 大数组尤其明显。

### 教学讲解顺序

1. virtual memory。
2. page table。
3. page fault。
4. TLB。
5. huge page。
6. NUMA。
7. first touch。
8. memory binding。
9. allocator。

### 实验任务

- page fault cold start。
- huge page TLB 实验。
- NUMA local vs remote。
- first-touch 初始化。
- allocator 对齐和大块分配。

### 作业分层

基础作业：

- 解释 page fault 和 TLB。

进阶作业：

- 在支持 NUMA 的机器上完成 local/remote benchmark。

挑战作业：

- 写一个 NUMA 误用导致性能大幅下降的复现实验。

### 报告必须回答

- 虚拟地址如何影响 TLB？
- huge page 解决什么问题？
- first-touch 为什么重要？
- allocator 如何影响 SIMD alignment？

### 验收标准

- 能写生产部署性能 checklist。

## Lab 22：ISA dispatch、CPUID、模板 kernel 和 JIT 思路

### 为什么学

生产库必须在不同 CPU 上运行，不能只针对自己的机器 `-march=native`。

### 教学讲解顺序

1. CPUID。
2. feature flags。
3. function multiversioning。
4. dispatch table。
5. ifunc。
6. template specialization。
7. JIT kernel。
8. primitive cache。

### 实验任务

- 写 scalar dot。
- 写 AVX2 dot。
- 写 AVX-VNNI int8 dot。
- runtime dispatch。
- verbose log。
- benchmark 不同实现。

### 作业分层

基础作业：

- 基于编译宏选择实现。

进阶作业：

- 运行时 CPUID dispatch。

挑战作业：

- mini primitive cache，缓存 shape 到 kernel 的选择。

### 报告必须回答

- 为什么库不能只发一个 AVX2 binary？
- runtime dispatch 的开销如何控制？
- JIT 和 template 的取舍是什么？

### 验收标准

- 能设计跨机器可运行的 kernel 入口。

## Lab 23：AVX-512、AMX、AVX10 和前沿 CPU ISA

### 为什么学

服务器 AI 算子正在大量使用更宽向量和矩阵扩展。即使本机不支持，也要理解方向。

### 教学讲解顺序

1. AVX-512 mask。
2. AVX-512 BF16/FP16/INT8。
3. downclock 风险。
4. AMX tile。
5. tile configure。
6. AMX BF16/INT8 dot。
7. OS 对扩展状态的支持。
8. AVX10 和未来兼容性。

### 实验任务

本机可做：

- 读文档。
- 写伪代码。
- 用编译器生成 AVX-512 汇编但不运行。
- 对比 AVX2 kernel 结构。

服务器可做：

- AVX-512 masked softmax。
- AMX BF16 matmul。
- AMX INT8 matmul。
- 对比 oneDNN。

### 作业分层

基础作业：

- 写 AVX-512/AMX 学习报告。

进阶作业：

- 在服务器上跑一个 AVX-512 kernel。

挑战作业：

- 实现简化 AMX matmul 并解释 tile blocking。

### 报告必须回答

- AVX-512 mask 比 AVX2 tail handling 好在哪里？
- AMX 为什么适合 matmul？
- 宽向量为什么可能降频？
- OS 为什么需要管理扩展寄存器状态？

### 验收标准

- 能理解现代 CPU AI ISA 的设计动机。

## Lab 24：生产库阅读和性能复现

### 为什么学

真正的高性能工程藏在生产库里：dispatch、packing、线程、边界处理、测试、平台兼容。

### 教学讲解顺序

1. 如何读大型代码库。
2. 从 public API 找到 primitive。
3. verbose log。
4. ISA dispatch。
5. microkernel。
6. packing。
7. threading。
8. benchmark suite。

### 实验任务

选择一个库：

- oneDNN。
- BLIS。
- OpenBLAS。

完成：

- 编译库。
- 跑官方 benchmark 或简单 benchmark。
- 打开 verbose log。
- 追踪一个 matmul 或 GEMM 调用路径。
- 画调用图。
- 找到 microkernel 或 kernel dispatch 入口。

### 作业分层

基础作业：

- 编译并跑通库 benchmark。

进阶作业：

- 写 8 到 12 页代码阅读报告。

挑战作业：

- 修改一处日志或添加一个简单 shape 路径观察点，证明你理解调用路径。

### 报告必须回答

- 生产库比教学 kernel 多哪些工程层？
- dispatch 在哪里发生？
- packing buffer 如何管理？
- threading 如何切分工作？
- 自己的 GEMM 和生产库最大差距是什么？

### 验收标准

- 能独立阅读一个生产 kernel 路径。
- 能把教学实现和工业实现对比。

## 超长期专题补全

下面这些专题不一定每个都做成完整 lab，但要在 2 到 3 年内逐步覆盖。

### 专题 A：操作系统深水区

学习点：

- process、thread、context switch。
- scheduler。
- syscall 成本。
- futex。
- signal。
- file cache。
- `mmap`。
- copy-on-write。
- page reclaim。
- transparent huge page。
- cgroup。

建议实验：

- syscall microbenchmark。
- context switch benchmark。
- futex wait/wake。
- `mmap` vs `read`。
- fork + copy-on-write。

高质量产出：

- 一份“OS 现象如何污染性能测试”的报告。

### 专题 B：编译器和 IR

学习点：

- Clang AST。
- LLVM IR。
- SSA。
- mem2reg。
- loop pass。
- vectorizer。
- alias analysis。
- instruction selection。
- register allocation。

建议实验：

- 用 `clang -emit-llvm -S` 看 IR。
- 写 10 个 C++ case，对比 LLVM IR 和最终汇编。
- 观察 `restrict` 如何改变 IR metadata。
- 用 `opt` 跑简单 pass。

高质量产出：

- 一份“C++ -> LLVM IR -> x86-64 汇编”的 pipeline 图谱。

### 专题 C：数学库和近似计算

学习点：

- `exp/log/sin/cos` 成本。
- polynomial approximation。
- table lookup。
- error bound。
- fast math。
- denormal flush。

建议实验：

- softmax 中替换 exp。
- GELU 近似。
- 比较 `-ffast-math` 前后误差和性能。

高质量产出：

- 一份“数值误差预算”报告。

### 专题 D：稀疏计算和不规则访存

学习点：

- CSR/CSC/COO。
- block sparse。
- gather/scatter。
- load imbalance。
- cache miss。
- sparse attention。

建议实验：

- sparse matrix-vector multiply。
- block sparse matmul。
- 稀疏度和 block size 扫描。

高质量产出：

- 对比 dense、sparse、block sparse 的性能边界。

### 专题 E：JIT 和代码生成

学习点：

- runtime code generation。
- Xbyak 或 asmjit 思路。
- shape-specialized kernel。
- register allocation 简化。
- code cache。

建议实验：

- 为固定 M/N/K 生成专用 microkernel。
- 对比 template 和 JIT。
- 实现 code cache key。

高质量产出：

- 一个极简 JIT matmul kernel 设计文档。

### 专题 F：跨架构视野

学习点：

- 其他 CPU 向量扩展的设计差异。
- RISC-V Vector。
- GPU SIMT。
- CPU SIMD 和 GPU SIMT 的差异。
- 内存层级对比。

建议实验：

- 不要求深入实现，但要写架构对比笔记。
- 选择一个 CPU kernel，讨论迁移到 GPU 的瓶颈变化。

高质量产出：

- 一份“CPU AI 算子和 GPU AI 算子的优化思维差异”报告。

## 大作业详细要求

### Capstone A：可信性能实验平台

阶段 1：最小可用。

- 支持注册 benchmark。
- 支持 warmup。
- 支持 repeat。
- 输出 CSV。
- 防止 DCE。

阶段 2：严谨测量。

- 支持 CPU pinning。
- 支持输入规模扫描。
- 支持 median/p95/min。
- 自动记录环境。
- 自动记录 compiler flags。

阶段 3：报告自动化。

- CSV 转图表。
- 自动生成 Markdown 报告骨架。
- 保存汇编。
- 保存编译器优化报告。

优秀标准：

- 后续 10 个以上 lab 都能复用。
- 在不同机器上结果目录清晰隔离。
- 能解释每个字段的意义。

### Capstone B：MiniBLAS

功能：

- `sscal`。
- `saxpy`。
- `sdot`。
- `sgemv`。
- `sgemm`。

版本：

- scalar。
- auto-vectorized。
- AVX2/FMA。
- runtime dispatch。

验收：

- correctness against reference。
- 支持随机 shape。
- 支持边界 shape。
- 单线程 SGEMM 达到参考 BLAS 50% 为及格，70% 为优秀。

报告：

- kernel design。
- roofline。
- cache blocking。
- packing。
- microkernel。
- 与 BLIS/OpenBLAS 差距。

### Capstone C：MiniDNN CPU Operator Pack

功能：

- matmul。
- softmax。
- layernorm/RMSNorm。
- GELU/SiLU。
- int8 GEMV 或 int8 matmul。
- attention block。

版本：

- fp32 reference。
- optimized fp32。
- int8 或 bf16 路线之一。
- 至少一个 fused operator。

验收：

- 数值误差有阈值。
- 性能有 baseline。
- shape 覆盖 transformer 常见规模。
- 与 oneDNN 或 PyTorch CPU 对比。

报告：

- 哪些算子 compute-bound。
- 哪些算子 memory-bound。
- fusion 节省多少内存流量。
- int8 的误差和性能 tradeoff。

### Capstone D：生产性能事故复盘

要求：

- 自己制造一个复杂性能问题，或者选择真实开源项目热点。
- 写出初始假设。
- 证明至少一个假设是错的。
- 使用汇编、benchmark、PMU 或替代证据修正判断。
- 给出最终优化和失败优化。

高质量标准：

- 像工程 postmortem 一样写。
- 不只给结果，还给决策过程。
- 能让另一个工程师复现实验。

### Capstone E：前沿简化项目

任选：

- AMX BF16/INT8 matmul。
- AVX-512 masked softmax。
- AVX10/APX 调研和示例。
- CPU FlashAttention-style streaming attention。
- sparse/block-sparse matmul。
- JIT generated microkernel。
- LLM CPU inference path with KV cache。

要求：

- 明确前沿技术解决的瓶颈。
- 实现一个简化版。
- 有严谨对照。
- 说明为什么教学版和生产版仍有差距。

## 每月复盘模板

每个月写一次复盘：

```text
本月主题：
完成的 lab：
最重要的 3 个概念：
最重要的 3 个实验结果：
我原来误解的地方：
还不能解释的现象：
下个月要补的基础：
代码质量问题：
测量质量问题：
```

## 每季度能力考试

### Q1：基础和二进制

- 给定 C++ 函数，预测关键汇编。
- 给定汇编，恢复伪代码。
- 解释一个链接错误。
- 解释一个 ABI bug。

### Q2：编译器和微架构

- 给定 loop，判断能否自动向量化。
- 给定 `llvm-mca` 输出，判断瓶颈。
- 给定 benchmark 数据，指出测量问题。

### Q3：内存和 SIMD

- 给定访问模式，判断 cache/TLB 问题。
- 写一个 AVX2 reduction。
- 优化一个 AoS/SoA layout。

### Q4：HPC kernel

- 从 naive GEMM 优化到 blocked GEMM。
- 写 roofline 分析。
- 与 BLAS 对比。

### Q5：OS 和并行

- 解释 false sharing。
- 设计 thread partition。
- 分析 NUMA first-touch 问题。

### Q6：AI 算子和生产库

- 优化 softmax/layernorm。
- 阅读 oneDNN 或 BLIS 路径。
- 完成一个 capstone 报告。

## 读书和资料顺序

第一层：入门必须。

- CS:APP 中 machine-level programming、memory hierarchy、linking、exceptional control flow。
- Compiler Explorer 日常使用。
- cppreference 中 object lifetime、undefined behavior、atomics。

第二层：性能核心。

- Agner Fog: Optimizing software in C++。
- Agner Fog: The microarchitecture of Intel, AMD and VIA CPUs。
- Intel Optimization Reference Manual。
- Intel Intrinsics Guide。
- uops.info。

第三层：系统和工具。

- Linux perf wiki 和 man page。
- Brendan Gregg 的性能分析方法。
- VTune documentation。
- Linux kernel 文档中 scheduler、perf events、NUMA 相关部分。

第四层：生产库。

- oneDNN documentation。
- BLIS paper 和文档。
- OpenBLAS kernel 目录。
- MLIR Linalg/Vector dialect 基础。

第五层：前沿。

- AMX、AVX-512、AVX10 资料。
- FlashAttention 思想。
- quantization papers 和 production inference notes。
- sparse/block sparse transformer 优化资料。

## 学习过程中的硬性规则

1. 任何优化前先写 reference。
2. 任何性能数字前先记录环境。
3. 任何 benchmark 前先防止 DCE。
4. 任何结论至少要有两个证据来源。
5. 任何 SIMD 代码必须有 scalar fallback。
6. 任何浮点优化必须说明误差。
7. 任何多线程优化必须说明线程绑定和分工。
8. 任何生产库对比必须说明版本、编译参数和线程数。
9. 任何“更快”必须说明在哪些 shape 更快，在哪些 shape 不一定。
10. 每个阶段至少保留一个失败实验。

## 从今天开始的 30 天详细执行表

### 第 1 周：环境和 C++ 基础

Day 1：

- 记录系统环境。
- 创建 `books/cpu-volume-1/labs/lab00_benchmark_foundation`。
- 写 `hello_bench.cpp`。

Day 2：

- 写 `sum_i32` benchmark。
- 分别用 `-O0` 和 `-O3` 编译。
- 记录运行时间。

Day 3：

- 加 checksum。
- 故意移除 checksum，观察循环是否消失。
- 保存汇编。

Day 4：

- 加 warmup 和 repeat。
- 输出 CSV。

Day 5：

- 写 3 个错误 benchmark。
- 写错误解释。

Day 6：

- 学 CMake 最小项目。
- 把 lab00 接入 CMake。

Day 7：

- 写 Lab 00 短报告。
- 列出本机测量限制。

### 第 2 周：编译链和汇编观察

Day 8：

- 创建 Lab 01。
- 写 6 个 C++ 小函数。
- 生成 `.s`。

Day 9：

- 学 `objdump/readelf/nm`。
- 找到函数符号。

Day 10：

- 对比 `-O0/-O2/-O3`。
- 写差异表。

Day 11：

- 加 template、lambda、virtual。
- 观察汇编。

Day 12：

- GCC 和 Clang 对比。

Day 13：

- 写源码到汇编映射。

Day 14：

- 写 Lab 01 报告。

### 第 3 周：ABI 和手写汇编

Day 15：

- 创建 Lab 02。
- 写第一个 `.S` 函数。

Day 16：

- C++ 调汇编。
- 随机测试。

Day 17：

- 学 caller/callee saved。
- 故意破坏 `rbx`。

Day 18：

- 实现 `sum_i64` 汇编。

Day 19：

- GDB 单步看寄存器。

Day 20：

- 实现浮点参数函数。

Day 21：

- 写 ABI 报告。

### 第 4 周：数据表示和 branchless

Day 22：

- 创建 Lab 03。
- 写 bit inspector。

Day 23：

- 实现 `min/max/abs` branch 版。

Day 24：

- 实现 branchless 版。

Day 25：

- 边界测试和 UB sanitizer。

Day 26：

- benchmark 不同数据分布。

Day 27：

- 生成汇编并标注 `cmov/setcc`。

Day 28：

- 写 Lab 03 报告。

Day 29：

- 复盘前 4 周。
- 列出还不懂的概念。

Day 30：

- 修补代码和报告质量。
- 准备进入 Lab 04。

## 最终目标画像

完成这套训练后，你应该能做到：

- 看到 C++ 热点代码，能预测大概汇编结构。
- 看到汇编，能还原性能相关的数据流和控制流。
- 看到 benchmark 数据，能质疑它是否可信。
- 看到一个 loop，能判断可能是分支、内存、前端、后端、依赖链还是吞吐瓶颈。
- 能写 AVX2/FMA/AVX-VNNI kernel。
- 能设计 GEMM blocking 和 packing。
- 能优化 transformer 常用 CPU 算子。
- 能使用 perf/VTune/llvm-mca/objdump/编译器报告形成证据链。
- 能阅读 oneDNN/BLIS/OpenBLAS 的核心路径。
- 能把高性能优化写成可维护、可复现、可迁移的工程系统。
