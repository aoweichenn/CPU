# Compute Systems Engine 代码实践卷

本书是《从 C++ 到计算系统：第二册》的配套代码实践卷。第二册正文作为原理卷，负责讲清硬件、并发、运行时、I/O 和分布式计算的因果模型；本卷负责把同一条主线落成完整 C++20 工程。

贯穿项目统一命名为 `Compute Systems Engine`。本卷按阶段实现：

- 输入生成器、输入合同、顺序 reference 和诊断系统
- `InputSplit`、`RecordId`、稳定输出和 checksum
- `HotBatch`、冷热字段分离、benchmark harness 和 `HotKernelReport`
- stable filter、thread-local histogram、partition manifest、top-k 和 kernel suite
- `ThreadPoolPlan`、`WorkerSnapshot`、`ThreadTimeline`、有界队列和 `QueueContract`
- `AtomicProtocolNote`、SPSC/MPMC 候选、`LockFreeLifecycleReport`
- 任务图、work stealing 指标、取消、关闭和错误聚合
- 可靠写、短写处理、buffer pool、completion、manifest commit 和 checkpoint
- `MessageBus`、`MessageEnvelope`、attempt、deadline、retry budget、epoch、分片和故障注入
- `RunReport`、恢复审计、端到端毕业项目

## 目录结构

```text
source/latex/      # 代码实践卷正式书稿
labs/              # Compute Systems Engine C++20 工程
materials/         # 设计草稿和阶段计划
reports/           # 实验报告模板
results/           # 示例输出和精选结果
tools/             # 本卷专用脚本
```

本卷会逐章给出完整代码、测试、构建命令、实验命令和报告样例。第二册原理卷中的伪代码和状态表，应在本卷中找到对应的 C++ 实现和验证路径。
