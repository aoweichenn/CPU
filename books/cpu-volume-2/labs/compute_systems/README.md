# Compute Systems Engine 早期实验素材

这是第二册早期计算系统实验素材，保留顺序归约、并行归约、计数器和有界队列等基础代码。第二册正式正文现在作为原理卷维护；完整 C++20 工程实现、测试、benchmark、故障注入和报告会进入配套代码实践卷：

```text
books/compute-systems-engine-code/
```

当前实验覆盖：

- 逐章可运行模型：`chapter_models.hpp/.cpp` 为第二册每个主章和深入章给出最小完整模型函数
- 顺序归约和并行归约
- 线程切分和局部累加
- mutex 计数器和 atomic 计数器
- 可关闭 bounded MPMC 队列：阻塞 `push`、阻塞 `pop`、`close`、drain 和等待报告
- 同步原语语义切片：`counting_semaphore` 许可限制、`barrier` 多阶段同步、`future/promise` 值/异常/broken-promise、`atomic::wait` 版本发布
- `eventfd`/`epoll` 唤醒合同：用户态队列、单次唤醒、drain 和 semaphore 模式
- futex 合同探针：用户态锁字、`FUTEX_WAIT_PRIVATE` 返回码、`FUTEX_WAKE_PRIVATE` 唤醒数量和 ready 状态重读
- 最小 task runtime：same-pool future 等待风险、ready 子任务饥饿和 continuation 修复路径
- 章节模型探针：`compsys_chapter_models_probe` 输出每章模型的稳定摘要
- CTest 正确性测试

逐章模型不是性能 benchmark，而是把每章的抽象名词落到一个能编译、能断言、能打印报告的最小 C++20 函数。当前映射如下：

| 章节 | 模型函数 | 固定下来的问题 |
| --- | --- | --- |
| 第 1 章 | `model_ch01_input_pipeline` | 输入记录怎样被接受、拒绝并进入 checksum |
| 第 2 章 | `model_ch02_branch_predictor` | 分支历史预测为什么会在交替输入上失误 |
| 第 3 章 | `model_ch03_cache_tlb` | 字节地址怎样映射到 cache line、page 和 TLB miss |
| 第 4 章 | `model_ch04_numa_coherence` | 本地访问、远端访问和写者迁移怎样分开计数 |
| 第 5 章 | `model_ch05_roofline` | FLOP、字节数、带宽和峰值算力怎样推出 attainable GFLOPS |
| 第 5b 章 | `model_ch05b_pmu_simd_evidence` | counter 可用性和 SIMD 指令比例怎样进入证据 |
| 第 6 章 | `model_ch06_layout_locality` | AoS 与 SoA 在只读热字段时触碰多少字节 |
| 第 7 章 | `model_ch07_simd_ilp` | 向量 lane、完整向量轮次和 scalar tail 怎样计算 |
| 第 8 章 | `model_ch08_map_filter_reduce` | map/filter/reduce 的输入、过滤数量和归约值怎样对齐 |
| 第 9 章 | `model_ch09_thread_scheduling` | 任务成本如何分配到 worker 并暴露负载不均 |
| 第 9b 章 | `model_ch09b_linux_wait_wake` | 等待循环为什么要区分真实完成和伪唤醒 |
| 第 10 章 | `model_ch10_sync_backpressure` | 容量、生产速率和消费速率怎样产生背压事件 |
| 第 11 章 | `model_ch11_atomic_publication` | 版本发布怎样区分 stale read 和乱序观察 |
| 第 12 章 | `model_ch12_spsc_ring` | ring buffer 怎样区分成功 push、满拒绝、成功 pop 和空 pop |
| 第 12b 章 | `model_ch12b_epoch_reclamation` | 被保护节点为什么必须延迟回收 |
| 第 13 章 | `model_ch13_reduce_scan_partition` | reduce、scan 和 partition 三种结果如何来自同一输入 |
| 第 14 章 | `model_ch14_work_stealing` | 本地队列不均衡时哪些任务需要被窃取 |
| 第 15 章 | `model_ch15_async_io_backpressure` | in-flight 深度如何限制提交并产生背压 |
| 第 16 章 | `model_ch16_distributed_attempts` | attempt、duplicate 和 stale generation 怎样分开 |
| 第 17 章 | `model_ch17_quorum_commit` | 多数派 quorum 怎样决定 commit |
| 第 18 章 | `model_ch18_checkpoint_recovery` | checkpoint 后失败时从哪里 replay |

队列实验对应正文第 10 章的同步原语合同。它故意使用容量为 1 的压力用例，覆盖：

- 空队列关闭唤醒消费者
- 满队列关闭唤醒生产者
- 多 producer/multiple consumer drain，不丢、不重
- `QueueReport` 中的 `push_success`、`push_closed`、`push_wait_count`、`push_wait_ns`、`pop_success`、`pop_closed`、`pop_wait_count`、`pop_wait_ns`、`max_size`

`sync_primitives` 实验对应正文第 10 章的同步原语族谱，目标不是测哪个 API 更快，而是固定每类原语表达的状态：

- semaphore 只限制 permit，不保护多字段对象不变量
- barrier 表达固定参与者的重复 phase，所有 worker 每轮都必须到达
- future/promise 表达一次结果或异常，broken promise 是独立失败路径
- atomic wait 等待单个原子版本变化，不适合跨字段队列谓词
- eventfd/epoll 表达唤醒和 ready，不表达业务 payload 或 completion
- futex 表达底层睡眠/唤醒路径，不表达 C++ 对象生命周期或业务谓词
- task runtime 实验把 worker、ready queue、unfinished task、same-pool 等待和 continuation 分开，避免把 `future.get()` 当作安全的调度原语

构建：

```bash
cmake -S books/cpu-volume-2 -B books/cpu-volume-2/build/compsys-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build books/cpu-volume-2/build/compsys-debug
ctest --test-dir books/cpu-volume-2/build/compsys-debug --output-on-failure
```

运行：

```bash
books/cpu-volume-2/build/compsys-debug/labs/compute_systems/compsys_demo
```

eventfd/epoll 合同探针：

```bash
books/cpu-volume-2/build/compsys-debug/labs/compute_systems/compsys_wait_channels_probe
```

futex 合同探针：

```bash
books/cpu-volume-2/build/compsys-debug/labs/compute_systems/compsys_futex_lab_probe
```

task runtime 合同探针：

```bash
books/cpu-volume-2/build/compsys-debug/labs/compute_systems/compsys_task_runtime_probe
```

逐章模型探针：

```bash
books/cpu-volume-2/build/compsys-debug/labs/compute_systems/compsys_chapter_models_probe
```
