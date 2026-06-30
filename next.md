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

七、操作系统第一册必须补的内容

当前 OS 原理卷只有 117 页 PDF / 5947 行 LaTeX / ~24 个独立主题，而成熟的第二册是 675 页 / 24670 行。OS 书大约只完成了 15-20% 的内容量。

（一）全书级结构问题

问题    当前情况    修改建议
deep-dive 章节拆分过细  7 对”基础章 + deep-dive 章”，基础章偏薄(平均 140 行)，deep-dive 章偏厚(平均 310 行)  合并为一章，把 deep-dive 内容作为节插入
ch05 和 ch06 调度重复  ch05-scheduling-deep-dive(309行) 与 ch06-scheduler-deep-dive(157行) 主题重叠  合并为”进程、线程与调度”一章
11 个章节过薄  ch20 安全隔离仅 85 行，ch14 设备驱动仅 110 行  每章至少扩充到 250-400 行
实践卷几乎为空  OS-Practice 仅 39 行 / 3 章 / Lab 351 行代码  实践卷应达到 500+ 行正文 + 完整教学 OS 实现
没有进入正式导出  无 EPUB，PDF 在 build/ 下，不在 book-exports/ 中  完成正文后接入 books-export 流程
缺少 frontmatter  无 abbreviations.tex，无 capability-checklist 之外的 backmatter  补前言、缩写表、术语表

（二）ch00-ch01 导言：当前 144+128=272 行，应扩充到 400+ 行

模块    必补内容
硬件基础  x86-64 启动序列：RESET → 实模式 → 保护模式 → 长模式，UEFI/BIOS 差异
执行模型  流水线、乱序、分支预测、 speculative execution 对 OS 的影响
特权级  Ring 0-3、syscall/sysret vs int 0x80、MSR 寄存器
中断体系  APIC、IOAPIC、MSI、中断向量表、IDT
内存体系  NUMA 拓扑、内存映射、PCI 配置空间
OS 契约  POSIX 标准、Linux syscall ABI、Windows NT API 对照
演进路线  batch → multiprogramming → time-sharing → microkernel → exkernel → unikernel
状态机模板  全书统一分析框架：谁拥有状态、谁能改变、如何同步、失败如何回滚、证据如何保存

（三）ch02 裸机 MCU：当前 157 行，应扩充到 300+ 行

模块    必补内容
UART 驱动  真实寄存器操作：TX/RX FIFO、波特率 divisor、TXE/RXNE 位轮询
GPIO/LED  内存映射寄存器、位操作、方向控制
定时器中断  周期性中断配置、tick 源选择
启动汇编  _start、向量表、栈指针初始化、BSS 清零、data 复制
链接脚本  MEMORY 命令、SECTION 布局、VMA/LMA 差异
轮询 vs 中断  两种模式的代码对比、延迟分析、CPU 占用率
状态机模板  从 super loop 到 event-driven 的迁移路径
真实芯片对照  STM32F4/RISC-V GD32 启动代码阅读

（四）ch03 中断定时器：当前 191 行，应扩充到 300+ 行

模块    必补内容
中断生命周期  push error code → save context → IDT lookup → handler → EOI → iret
trap frame  完整寄存器保存结构：rax/rcx/rdx/rbx/rsp/rbp/rsi/rdi/r8-r15/rip/cs/rflags
中断嵌套  重入问题、中断屏蔽、sti/cli、irqsave/irqrestore
bottom half  软中断、tasklet、workqueue、threaded irq 四种机制对比
定时器子系统  hrtimer、timer wheel、tickless、jiffies vs ktime
调度时钟  scheduler tick、update_process_times、calc_global_load
真实代码  Linux do_IRQ、handle_irq_event、timer interrupt path

（五）ch04 boot/linker/init：当前 181 行，应扩充到 300+ 行

模块    必补内容
UEFI 启动  EFI_SYSTEM_TABLE、BootServices、RuntimeServices、ExitBootServices
内核加载  PE/COFF 内核格式、relocation、内核命令行解析
早期内存  early_ioremap、fixmap、identity mapping 的建立
初始化调用  initcall 机制：early_initcall、core_initcall、arch_initcall、fs_initcall、device_initcall、late_initcall
多核启动  AP startup、IPI、smp_init、CPU hotplug
内核命令行  __setup、early_param、cmdline 解析
真实代码  Linux start_kernel、setup_arch、mm_init、sched_init 调用序列

（六）ch05-ch06 进程调度：当前 167+309+157=633 行(3章)，应合并为 1 章 400+ 行

模块    必补内容
task_struct  PID、state、flags、mm、files、signal、sighand、ptrace、cgroup、sched_entity
进程创建  fork → clone → vfork 差异、COW 实现、copy_process
进程终止  do_exit、exit_notify、wait、僵尸/孤儿处理
线程实现  NPTL、TLS、set_tid_address、CLONE_ 标志
上下文切换  __switch_to、switch_mm、TLB 刷新、fpu 保存
调度类  stop_sched_class、dl_sched_class、rt_sched_class、fair_sched_class、idle_sched_class
CFS 调度  vruntime、红黑树、sched_latency、min_granularity、wakeup_preempt
实时调度  SCHED_FIFO、SCHED_RR、priority inheritance、sched_setscheduler
负载均衡  load_balance、sched_domain、NUMA balancing、CPU affinity
cgroup 调度  cpu.shares、cpu.cfs_quota_us、cpu.cfs_period_us
能源调度  EAS、schedutil、CPU frequency 与调度联动
真实代码  Linux __schedule、pick_next_task_fair、enqueue_task_fair、check_preempt_wakeup

（七）ch07 并发 IPC 信号：当前 137 行，应扩充到 350+ 行

模块    必补内容
管道  pipefs、pipe_buffer、pipe_wait、FIFO 语义、pipe capacity(65536)
信号量  System V semaphore、POSIX semaphore、named vs unnamed
消息队列  System V msgqueue、POSIX mq、mq_notify
共享内存  shmget/shmat、tmpfs、hugetlbfs、futex 在 shm 中的应用
信号  signal delivery、sigaction、sigprocmask、pending signal、real-time signal
signalfd  signalfd、siginfo_t、SA_SIGINFO
eventfd  eventfd、EFD_NONBLOCK、EFD_SEMAPHORE、与 epoll 配合
timerfd  timerfd_create、timerfd_settime、TFD_NONBLOCK、TFD_TIMER_ABSTIME
memfd  memfd_create、sealing、用于共享内存
IPC 性能  延迟对比、吞吐量对比、适用场景决策树

（八）ch08 内核锁死锁：当前 186 行，应扩充到 350+ 行

模块    必补内容
自旋锁  raw_spinlock_t、spin_lock_irqsave、ticket lock、queued spinlock(MCS)
rwlock  rwlock_t、read_lock、write_lock、reader-writer 公平性
mutex  struct mutex、mutex_lock、mutex_lock_interruptible、mutex_trylock、priority inheritance
rwsem  struct rwsem、down_read、down_write、乐观自旋
seqlock  seqlock_t、read_seqbegin、read_seqretry、写者优势场景
percpu   percpu 变量、get_cpu_ptr、put_cpu_ptr、为什么能避免锁
preempt  preempt_disable、preempt_enable、preempt_count、migrate_disable
内存屏障  smp_mb、smp_rmb、smp_wmb、smp_store_release、smp_load_acquire
lockdep  CONFIG_DEBUG_LOCK_ALLOC、lockdep_map、死锁检测、lockdep 报告解读
真实代码  Linux mutex slow path、rwsem optimistic spin、qspinlock MCS chain

（九）ch09 系统调用权限：当前 160 行，应扩充到 300+ 行

模块    必补内容
syscall 入口  entry_SYSCALL_64、swapgs、per-cpu 变量、syscall table 分发
参数验证  access_ok、copy_from_user、copy_to_user、get_user、put_user
capability  CAP_SYS_ADMIN、CAP_NET_ADMIN、capable、ns_capable、user_namespace 嵌套
LSM  hook  security_* hook 点、SELinux、AppArmor、BPF LSM
seccomp  seccomp filter、BPF program、SECCOMP_RET_*、seccomp_unotify
vdso  vdso 机制、__vdso_clock_gettime、为什么能加速 gettimeofday
syscall 性能  syscall overhead、vDSO 优化、io_uring 绕过 syscall
系统调用表  arch/x86/entry/syscalls/syscall_64.tbl、SYSCALL_DEFINE 宏
真实代码  Linux do_syscall_64、__x64_sys_write、SYSCALL_DEFINE3

（十）ch10 exec ELF 加载：当前 145 行，应扩充到 300+ 行

模块    必补内容
ELF 结构  Elf64_Ehdr、Elf64_Phdr、Elf64_Shdr、program header 类型(PT_LOAD、PT_INTERP、PT_PHDR)
加载流程  load_elf_binary、elf_map、set_brk、padzero、create_elf_tables
动态链接  PT_INTERP、ld-linux.so、auxv(AT_PHDR、AT_ENTRY、AT_PHENT、AT_PHNUM、AT_BASE、AT_NULL)
解释器脚本  #!/bin/sh、binfmt_script、递归 binfmt
binfmt 机制  register_binfmt、binfmt_elf、binfmt_misc、自定义二进制格式
用户栈构造  argv、envp、auxv 布局、stack randomization、stack guard
COW 与 exec  exec 丢弃旧 mm、建立新 mm、为什么 vfork + exec 高效
真实代码  Linux load_elf_binary、setup_arg_pages、create_elf_tables

（十一）ch11 虚拟内存：当前 195+316=511 行(2章)，应合并为 1 章 400+ 行

模块    必补内容
页表结构  PGD/P4D/PUD/PMD/PTE、PTE 标志(_PRESENT、_RW、_USER、_PWT、_PCD、_ACCESSED、_DIRTY)
地址翻译  虚拟地址拆分、物理地址计算、page offset、TLB 查找
VMA 结构  vm_area_struct、vm_start、vm_end、vm_flags、vm_ops、红黑树 + 链表
缺页处理  do_page_fault、handle_mm_fault、do_anonymous_page、do_fault、do_swap_page
COW 实现  pte_wrprotect、page_add_anon_rmap、_wp_page
页面回收  LRU 链表(active/inactive)、kswapd、shrink_lruvec、shrink_slab
swap  swap_info_struct、swap slot、swap cache、swappiness
HugeTLB  PMD 级大页、PUD 级大页、透明大页(THP)、khugepaged
KASAN  Kernel Address Sanitizer、shadow memory、检测 use-after-free
内存压力  PSI(Pressure Stall Information)、psi_memstall_enter
真实代码  Linux handle_mm_fault、do_anonymous_page、shrink_lruvec

（十二）ch12 mmap page cache：当前 149 行，应扩充到 300+ 行

模块    必补内容
mmap 系统调用  sys_mmap、mmap_region、vm_get_page_prot、file backed vs anonymous
文件映射  file->f_ops->mmap、fault 回调、page cache 预读
匿名映射  MAP_ANONYMOUS、MAP_PRIVATE vs MAP_SHARED、COW 语义
madvise  MADV_NORMAL、MADV_RANDOM、MADV_SEQUENTIAL、MADV_DONTNEED、MADV_HUGEPAGE
mlock  mlock、mlockall、VMA 锁定、不参与回收
page cache  address_space、radix tree、XArray、find_get_page、add_to_page_cache
回写  writeback_control、wb_workfn、balance_dirty_pages_ratelimited
预读  readahead、page_cache_sync_readahead、page_cache_async_readahead
fsync  filemap_fdatawrite、filemap_fdatawait、error range 跟踪
O_DIRECT  绕过 page cache、对齐要求、DMA 直接访问
DAX  Direct Access、持久内存、跳过 page cache
真实代码  Linux do_mmap、filemap_fault、writeback_single_inode

（十三）ch13 内核内存分配：当前 150 行，应扩充到 300+ 行

模块    必补内容
buddy 系统  free_area、order、__rmqueue、__free_pages、fragmentation
slab 缓存  kmem_cache、kmem_cache_alloc、kmem_cache_free、partial/full/empty 链表
slub  allocator  SLUB vs SLAB vs SLOB、debug 模式、red zone、poison
kmalloc  大小映射(32/64/96/128/192/256/512/1024/2048/4096)、size 向上取整
vmalloc  vmalloc、__vmalloc、非连续物理内存、vm_struct、vmap/vunmap
GFP  flags  GFP_KERNEL、GFP_ATOMIC、GFP_NOIO、GFP_NOFS、GFP_HIGHUSER、GFP_DMA
内存碎片  外部碎片 vs 内部碎片、compaction、__GFP_COMPACT
Kmemleak  内存泄漏检测、kmemleak_alloc、kmemleak_free、scan 机制
percpu 内存  alloc_percpu、free_percpu、per_cpu_ptr、为什么避免 cache line bouncing
真实代码  Linux __alloc_pages_nodemask、kmem_cache_alloc_trace

（十四）ch14 设备驱动 DMA：当前 110 行，应扩充到 350+ 行

模块    必补内容
设备模型  bus_type、device_driver、device、class、kobject、kset
PCI 驱动  pci_driver、pci_device_id、probe/remove、pci_config_block
字符设备  cdev、cdev_add、register_chrdev_region、file_operations
块设备  gendisk、request_queue、blk_mq_ops、submit_bio
网络设备  net_device、net_device_ops、NAPI、ndo_start_xmit
platform 设备  platform_driver、platform_device、device tree 匹配
IOMMU  iommu_map、iommu_unmap、SVA(Shared Virtual Addressing)
中断注册  request_irq、request_threaded_irq、IRQF_SHARED、free_irq
DMA 映射  dma_map_single、dma_map_sg、dma_alloc_coherent、streaming DMA
真实代码  Linux e1000 驱动、nvme 驱动、virtio 驱动核心结构

（十五）ch15 块层 IO 调度：当前 165 行，应扩充到 300+ 行

模块    必补内容
bio 结构  bio、bio_vec、bvec_iter、bio_end_io_t
请求队列  request_queue、request、blk_mq_tag_set、hw queue
IO 调度器  noop、deadline、cfq、bfq、mq-deadline、kyber、bfq weight
合并逻辑  front merge、back merge、last merge、no merge
多队列  blk-mq、nr_hw_queues、CPU 映射、hctx
IO 统计  /proc/diskstats、/proc/[pid]/io、iostat 数据来源
IO 优先级  ionice、IOPRIO_CLASS_RT、IOPRIO_CLASS_BE、IOPRIO_CLASS_IDLE
IO_URING  io_uring_setup、io_uring_enter、io_uring_register、SQE/CQE、fixed buffer/file
真实代码  Linux blk_mq_submit_bio、deadline_dispatch_requests

（十六）ch16 文件 IO：当前 145+325=470 行(2章)，应合并为 1 章 400+ 行

模块    必补内容
fd 管理  files_struct、fdtable、fdt、expand_files、next_fd
open  path_openat、do_last、lookup、permission、may_open
read/write  vfs_read、vfs_write、rw_verify_area、iterate、iterate_shared
sync  sync、syncfs、fsync、fdatasync、filemap_write_and_wait_range
aio  io_submit、io_getevents、struct iocb、IOCB_CMD_PREAD
splice  splice、vmsplice、tee、zero-copy 管道传输
fcntl  F_GETFL、F_SETFL、F_SETLK、F_SETLKW、F_OFD_GETLK
flock  flock、lockf、POSIX lock vs BSD lock
select/poll  do_select、poll_wqueues、poll_table、pollfd
epoll  do_epoll_create、do_epoll_ctl、do_epoll_wait、ep_poll_callback、EPOLLET、EPOLLONESHOT
真实代码  Linux vfs_read、path_openat、ep_poll_callback

（十七）ch17 文件系统：当前 158 行，应扩充到 350+ 行

模块    必补内容
VFS 层  super_block、inode、dentry、file、file_operations、inode_operations、super_operations
ext4  extent  extent tree、ext4_extent、ext4_extent_idx、多级 extent 树
日志  jbd2、journal_t、transaction_t、commit、checkpoint、recovery
日志模式  journal、ordered、writeback 三种模式对比
目录操作  ext4_dir_entry_2、hash tree、线性目录、lookup、create
空间管理  ext4_group_desc、block bitmap、inode bitmap、mballoc
xattr  ext4_xattr_entry、security.selinux、system.posix_acl_access
quota  dquot、dqblk、quotaon、quotaoff
fsck  e2fsck、superblock check、inode scan、directory check、block bitmap check
FUSE  fuse_conn、fuse_file、fuse_req、用户态文件系统实现
真实代码  Linux ext4_readdir、ext4_map_blocks、jbd2_journal_commit_transaction

（十八）ch18 网络：当前 182+316=498 行(2章)，应合并为 1 章 400+ 行

模块    必补内容
网络协议栈  net_device、sk_buff、sock、inet_sock、tcp_sock、udp_sock
skb 管理  alloc_skb、kfree_skb、skb_clone、skb_copy、skb_shared_info
TCP 状态机  TCP_ESTABLISHED、TCP_SYN_SENT、TCP_FIN_WAIT1/2、TCP_TIME_WAIT、TCP_CLOSE_WAIT
TCP 连接  tcp_v4_connect、tcp_v4_do_rcv、三次握手、四次挥手
拥塞控制  tcp_congestion_ops、cubic、bbr、slow start、congestion avoidance
NAPI  napi_struct、netif_napi_add、napi_schedule、poll 函数
Netfilter  nf_hook_ops、NF_INET_PRE_ROUTING、NF_INET_LOCAL_IN、NF_INET_FORWARD、NF_INET_LOCAL_OUT、NF_INET_POST_ROUTING
Socket 层  inet_create、inet_sendmsg、inet_recvmsg、tcp_sendmsg、tcp_recvmsg
epoll 与网络  ep_poll_callback 在 socket 唤醒中的应用
真实代码  Linux tcp_v4_rcv、tcp_sendmsg、ep_poll_callback

（十九）ch19 持久化恢复：当前 142 行，应扩充到 300+ 行

模块    必补内容
崩溃模型  transaction crash、system crash、media failure
WAL  write-ahead log、log record、LSN、checkpoint
journaling  metadata journaling、data journaling、ordered mode
COW 文件系统  btrfs、zfs、snapshot、clone、send/receive
RAID  RAID 0/1/5/6/10、md 驱动、write hole、bitmap
LVM  dm-linear、dm-snapshot、dm-thin、thin provisioning
fsck 一致性  superblock、inode bitmap、block bitmap、orphan inode
幂等重试  request ID、dedup、at-least-once vs exactly-once
真实代码  Linux jbd2 commit、ext4_journal_start

（二十）ch20 安全隔离：当前 85+318=403 行(2章)，应合并为 1 章 350+ 行

模块    必补内容
访问控制  DAC、MAC、RBAC、ABAC 模型
capability  CAP_CHOWN、CAP_NET_RAW、CAP_SYS_PTRACE、bounding set、ambient set
namespace  PID、Mount、Network、IPC、UTS、User、CGroup、Time namespace
cgroup v2  cgroup.controllers、cgroup.subtree_control、cgroup.max.depth
LSM  SELinux context、AppArmor profile、BPF LSM program
seccomp  seccomp filter、SECCOMP_RET_TRAP、SECCOMP_RET_ERRNO、SECCOMP_RET_TRACE
Landlock  LSM based unprivileged sandboxing、ruleset、access bit
TPM  measured boot、remote attestation、sealed storage
ASLR  stack randomization、heap randomization、mmap randomization、PIE
stack canary  __stack_chk_fail、-fstack-protector
CFI  -fsanitize=control-flow-integrity、shadow call stack
KASLR  kernel address space layout randomization
真实代码  Linux security_file_alloc、selinux_file_permission

（二十一）ch21 panic 错误恢复：当前 178 行，应扩充到 300+ 行

模块    必补内容
错误传播  errno、goto rollback、cleanup attribute(__attribute__((cleanup)))
内核异常  BUG()、BUG_ON()、WARN()、WARN_ON()、panic()
oops  printk、oops message、ksymoops、kdump
KASAN  use-after-free、out-of-bounds、stack-buffer-overflow 检测
KCSAN  data race 检测、atomic vs non-atomic access
KFENCE  slab heap overflow、use-after-free 低开销检测
Kfence vs KASAN  精度 vs 性能权衡
fault injection  fail_make_request、fail_page_alloc、should_failslab
kdump  kexec、crashkernel、/proc/vmcore、crash 工具
watchdog  softlockup、hardlockup、watchdog_thresh、touch_nmi_watchdog
真实代码  Linux do_error_trap、oops_exit、kasan_report

（二十二）ch22 可观测性调试：当前 149 行，应扩充到 350+ 行

模块    必补内容
printk  loglevel、console_printk、dmesg、rate limiting
ftrace  function tracer、function_graph tracer、tracepoint、trace_event
perf  perf stat、perf record、perf report、perf top、PMU event
eBPF  BPF program type、BPF map、BPF helper、bpf_trace_printk
bpftrace  one-liner、kprobe、uprobe、tracepoint、profile
strace  ptrace、PTRACE_SYSCALL、syscall enter/exit
ltrace  _mcount、PLT hook、library call trace
gdb  kgdb、gdbserver、remote debugging、symbol table
/proc  /proc/meminfo、/proc/cpuinfo、/proc/interrupts、/proc/slabinfo
/sys  sysfs、kobject、device tree、/sys/fs/cgroup
sysctl  /proc/sys、ctl_table、sysctl(2)
systemd-journald  structured logging、journalctl、sd-id128
PSI  Pressure Stall Information、memory/CPU/IO stall
真实代码  Linux trace_output_call、perf_event_read

（二十三）ch23 综合案例：当前 118+402=520 行(2章)，应合并为 1 章 400+ 行

当前案例质量较好，但缺少：
- 案例九：内核模块加载与 device mapper 创建
- 案例十：容器从 clone 到 exec 的完整路径
- 案例十一：TCP 服务端从 socket 到 epoll 的完整路径
- 案例十二：文件系统从 mount 到 fsck 的完整路径
- 案例十三：从按键到字符设备到 read 的系统调用链
- 案例十四：内存压力下的页面回收与 OOM 决策
- 案例十五：系统调用被信号中断后的重启路径

（二十四）实践卷必须补的内容

当前 OS-Practice 仅 39 行正文 + 351 行 Lab 代码，远不够。

模块    必补内容
正文扩写  每章 200+ 行，覆盖原理、实现、测试、边界
Lab 扩展  从仅支持 write/exit 扩展到：
  - 进程创建(fork)、调度(round-robin)、终止(exit/wait)
  - 内存管理(页表、COW、缺页)
  - 文件系统(open/read/write/close)、VFS 层
  - 管道(pipe)、信号(signal delivery)
  - 用户态程序加载(ELF 简化格式)
验收报告  完整验收
