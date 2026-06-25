# Compute Systems Engine 早期实验素材

这是第二册早期计算系统实验素材，保留顺序归约、并行归约、计数器和有界队列等基础代码。第二册正式正文现在作为原理卷维护；完整 C++20 工程实现、测试、benchmark、故障注入和报告会进入配套代码实践卷：

```text
books/compute-systems-engine-code/
```

当前实验覆盖：

- 顺序归约和并行归约
- 线程切分和局部累加
- mutex 计数器和 atomic 计数器
- 可关闭 bounded MPMC 队列：阻塞 `push`、阻塞 `pop`、`close`、drain 和等待报告
- 同步原语语义切片：`counting_semaphore` 许可限制、`barrier` 多阶段同步、`future/promise` 值/异常/broken-promise、`atomic::wait` 版本发布
- `eventfd`/`epoll` 唤醒合同：用户态队列、单次唤醒、drain 和 semaphore 模式
- futex 合同探针：用户态锁字、`FUTEX_WAIT_PRIVATE` 返回码、`FUTEX_WAKE_PRIVATE` 唤醒数量和 ready 状态重读
- 最小 task runtime：same-pool future 等待风险、ready 子任务饥饿和 continuation 修复路径
- CTest 正确性测试

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
