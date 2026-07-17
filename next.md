下面这份按“你下一版应该怎么改”给。结论先说：不是继续盲目加页数，而是把“地图式说明、口号式验收、重复方法论”压缩掉，换成源码、实验、证据、失败案例。

一、全书级必须先改

问题    当前情况    修改建议

目录太粗    三册 tocdepth=0，读者看不到内部知识点   改到 tocdepth=1 或 2
有内容没进 PDF  第一册 4 个文件未收录，第二册 3 个，第三册 7 个 要么收进 main.tex，要么删除，不能悬空
方法论重复  “账本”出现非常多，“证据链/验收线/不变量”反复出现   每章只保留一次方法论，后面必须落到代码/命令/输出
实验不足    第二册代码实验很薄，第三册实验和文字野心不匹配  每一大章必须有可运行实验
工业对标偏概述  提到了工业系统，但源码级拆解不足    每个工业对标都要有源码路径、核心结构、关键函数、设计取舍
“大师”路径不清  有知识，但缺阶段性能力验收  每册末尾加“大师级能力清单”：能实现什么、能诊断什么、能解释什么


二、第一册必须补的内容

第一册主题是“C++ 到进程、二进制、ABI、CPU 执行”。还要补这些：

模块    必补内容

编译器前端  预处理、宏展开、模板实例化、重载决议、名称查找、AST 结构
IR/优化 LLVM IR/GIMPLE 真实 dump、SSA、CFG、dominance、mem2reg、GVN、DCE、LICM、inline、vectorize
后端    指令选择、寄存器分配、spill/reload、calling convention lowering
二进制  ELF header、section/segment、symbol table、relocation、GOT/PLT、动态链接器
运行时启动  _start、libc init、constructor/destructor、main 前后发生什么
ABI 深水区  struct 返回、浮点参数、vector 参数、varargs、red zone、stack alignment
异常/调试   DWARF、CFI、unwind table、C++ exception、stack trace 原理
内存分配    malloc/free、arena、tcache、mmap/brk、碎片、对齐、false sharing
syscall 边界    syscall ABI、vDSO、errno、信号、文件描述符生命周期
TLS thread-local storage 模型、访问代码、动态库里的 TLS
跨平台对照  Windows x64 ABI、PE/COFF、MSVC calling convention
安全/未定义行为 strict aliasing、lifetime、alignment、overflow、use-after-free、UB 到汇编的证据链


第一册最该加的不是更多概念，而是完整案例：
cpp -> clang AST -> LLVM IR -> asm -> object -> ELF relocation -> loader -> process map -> gdb/perf 证据。

三、第二册必须补的内容

第二册这一版同步原语补得明显，但还要补“工程级实证”。

模块    必补内容

mutex   fast path、futex slow path、owner、priority inversion、robust mutex
condition_variable  虚假唤醒、谓词循环、lost wakeup、notify 顺序、生命周期错误
semaphore   permit 模型、bounded queue、限流器、连接池、生产消费模型
latch/barrier   一次性同步、多阶段同步、phase reuse、异常退出边界
future/promise  shared state、broken promise、same-pool deadlock、continuation
coroutine   coroutine frame、awaiter、scheduler、blocking await 的灾难
atomic wait atomic::wait/notify 和 condvar/futex 的边界
lock-free   Treiber stack、Michael-Scott queue、ABA、hazard pointer、epoch、RCU
memory order    release/acquire、seq_cst、relaxed 正确用法、litmus test
scheduler   CFS/EEVDF、runqueue、wake up、preemption、CPU affinity、cgroup
memory manager  VMA、PTE、page fault、THP、NUMA、reclaim、OOM、page cache
PMU perf stat/record 真实输出、Top-Down、IPC、uop、cache miss、TLB miss
SIMD    AVX2/AVX512/AMX、port pressure、alignment、tail handling、downclock
I/O epoll、io_uring、eventfd、timerfd、zero-copy、backpressure、cancellation
网络    TCP backlog、Nagle、拥塞、TLS、RPC、连接池、超时/重试/幂等
分布式  Raft、lease、quorum、split brain、日志压缩、读写一致性、故障注入


第二册最缺的是配套代码。建议新增这些实验：

实验    要求

sync_lab    mutex/condvar/semaphore/barrier/future 全部实现案例
futex_lab   用户态锁 + futex wait/wake + strace/perf
threadpool_lab  work stealing、future deadlock、continuation 修复
lockfree_lab    stack/queue + hazard pointer + sanitizer
pmu_lab cache/TLB/branch/SIMD 反例和 perf 报告
io_lab  epoll/io_uring echo server + backpressure
raft_lab    最小 Raft + 故障注入


四、第三册必须补的内容

第三册最大问题是：文字覆盖到工业 AI Infra，但代码还停在教学推理器。

模块    必补内容

tokenizer   BPE、SentencePiece、special token、chat template、detokenize
模型格式    GGUF、safetensors、metadata、tensor name mapping、shard loading
真实模型加载    mmap、lazy load、dtype 校验、shape 校验、错误报告
Transformer RMSNorm、RoPE、QKV、GQA/MQA、MLP、SiLU、residual、layer stack
Attention   prefill/decode 区别、mask、paged attention、FlashAttention 原理
KV Cache    block table、paged KV、prefix cache、eviction、量化 KV
batching    continuous batching、request state machine、token scheduler
decoding    top-k、top-p、temperature、repetition penalty、speculative decoding
quantization    int8/int4、group quant、scale/zero-point、GGML/GPTQ/AWQ 细节
CPU kernel  packing、microkernel、cache blocking、NUMA、AVX2/AVX512/AMX
GPU kernel  CUDA thread/block、warp、shared memory、Tensor Core、stream
多 GPU  tensor parallel、pipeline parallel、NCCL、KV 通信、带宽账本
LoRA    adapter 加载、merge/unmerge、多租户 adapter cache
MoE router、top-k experts、expert parallel、负载均衡
Serving admission control、SLO、队列、取消、超时、metrics、trace
RAG/Agent   只保留和推理服务相关的接口契约，不要喧宾夺主


第三册代码最低应补到这个程度：

1. 能加载一个真实小型 safetensors/GGUF 模型。


2. 有真实 tokenizer。


3. 有多层 decoder-only forward。


4. 有 KV cache。


5. 有 continuous batching。


6. 有 int8/int4 至少一种真实量化路径。


7. 有 CPU AVX2 baseline。


8. 有可选 CUDA 最小 kernel。


9. 有 serving probe，不只是单次推理。


10. 有与 llama.cpp/vLLM/ONNX Runtime 的源码级对照。



五、现在重复/低价值内容怎么处理

类型    问题    处理

“本章不是为了……”    出现太多，像模板开头    每册最多保留 2-3 次，其余删
“账本”  概念有用，但过度使用    只在性能、内存、服务容量处使用
“证据链”    反复宣言    改成真实命令：objdump/perf/gdb/readelf/strace
“本节验收线”    第三册过多  改成每章末尾一个“硬验收”
“复查清单”  第二册模板感重  合并到每一 part 末尾
“工业设计参照”  如果没有源码级拆解就是空话  要么补源码路径，要么删
“工程边界”  说法偏抽象  改成失败案例、错误日志、恢复策略
“大师级/工业级” 容易变宣传语    用具体能力替代，比如“实现 hazard pointer queue”
导读/路线图文件 有些没进 PDF    收录或删除，不能留半成品
第三册 RAG/Agent    容易偏应用层    压缩，只保留和推理引擎接口相关内容


六、最应该立刻改的 10 件事

1. 把三册未收录但有价值的导读/路线图重新接进 main.tex。


2. tocdepth 改到 1 或 2。


3. 第一册补完整 C++ -> IR -> ASM -> ELF -> Process 案例。


4. 第一册补 DWARF/unwind/TLS/malloc/syscall。


5. 第二册补同步原语实验，不只讲概念。


6. 第二册补 futex、PMU、scheduler、memory manager 的真实 trace。


7. 第二册补完整 thread pool / work stealing / future dead锁实验。


8. 第三册补真实 tokenizer 和模型格式加载。


9. 第三册补 KV cache、continuous batching、真实 decoder-only 模型。


10. 全书删掉 60% 的模板化“验收/账本/证据链”表述，换成代码、输出、图、源码路径。



最终标准很简单：每个重要原理都必须有源码、实验、证据、反例、工业对照。没有这五个之一，就不要写成”大师级”。

七、操作系统第一册评价与待办（2026-06-30 更新）

（零）总体评价

commit fb7114e 新增 9409 行，OS 原理卷从 5947 → 11,798 行。
从"骨架阶段"一步跨入"准出版级"。最大变化不是字数，而是结构重组和密度提升。

当前体量对比：
- OS 原理卷：11,798 行 / 53 文件 / 7 个大章
- cpu-volume-2（成熟）：24,670 行 / 675 页
- OS 在"教学理念"和"结构创新"上已超越成熟书，但体量还差约 50%

核心优点：
1. 结构重组为 7 个大章故事线，从硬件到系统演进，不再是平铺章节
2. T16 教学计算机（423 行，19 步从零搭 CPU）—— 全书最大亮点，所有中文 OS 教材未见
3. 组成原理主线（430 行）—— 从晶体管到 OS，每个硬件机制指向"修复的 OS 失败"
4. 统一分析框架：失败场景 → 原理 → 不变量 → 实践 → 测试
5. 源码阅读索引（33 个主题 → Linux 路径）+ 能力清单（24 条）
6. 写作质量高：每章有 keyidea/longtable/lstlisting/rubricbox，信息密度均匀

（一）仍 <200 行的薄章节 📌待扩充

章节    当前行数    缺什么
ch00-hardware-foundations  144  与 primer/deep-dive 重叠，应合并
ch01-os-map-and-contracts  128  内容好但篇幅短，缺 POSIX/Windows 对照
ch05-process-thread-scheduling  167  缺 task_struct 字段、fork/exec/wait 完整路径
ch06-scheduler-deep-dive  157  与 ch05-deep-dive(408) 重叠，应合并
ch16-files-devices-io  145  缺 fd 管理、open/read/write 完整路径
ch20-security-isolation  171  缺 Landlock 实践、BPF LSM 细节
ch23-practice-and-acceptance  118  评分矩阵和验收标准未展开

（二）deep-dive 章节合并 📌待做
7 对"基础章(薄)+deep-dive章(厚)"仍分离，基础章平均 150 行，deep-dive 平均 340 行。
建议合并为一章（350-450 行），deep-dive 内容作为 \section{深入} 插入：
ch00: foundations(144) + deep-dive(282) + primer(430) → 合并为 1 章
ch05: scheduling(167) + ch06(157) + deep-dive(408) → 合并为 1 章
ch11: vm(280) + deep-dive(316) → 合并
ch16: io(145) + deep-dive(325) → 合并
ch18: net(277) + deep-dive(316) → 合并
ch20: security(171) + deep-dive(318) → 合并
ch23: acceptance(118) + casebook(601) → 合并

（三）ch00 三章节重叠 📌待合并
ch00 有 11 个子节共 3289 行，其中：
- computer-organization-primer(430) ← 最完整
- minimal-computer-from-scratch(423) ← 最独特（T16）
- hardware-foundations(144) + hardware-deep-dive(282) ← 与 primer 重叠
建议：foundations 和 deep-dive 合并到 primer 中，保留 T16 独立

（四）frontmatter/backmatter 📌部分完成
✅ abbreviations.tex、how-to-read.tex、preface.tex、title.tex、source-reading-index.tex
📌 glossary.tex（术语表，OS 缩写多，必须有）
📌 全书总结/回顾章节

（五）导出流程 📌待做
- Makefile 加 os-check / os-test 目标
- 只生成 PDF，旧 EPUB 构建入口已移除
- 接入 books-export 和 phone-books-export
- PDF 从 build/ 移到 source/latex/

（六）未跟踪文件 📌需 git add
- CMakeLists.txt、labs/(527 LOC)、materials/reading-map.md
- reports/acceptance-template.md、source/markdown/(539 行, 9 章)
