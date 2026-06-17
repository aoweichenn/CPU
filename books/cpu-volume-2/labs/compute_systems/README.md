# Compute Systems Labs

这是第二册的计算系统贯穿实验。第二册不再放 AI 推理内容，重点是现代 CPU 硬件原理、多核并行、同步、无锁数据结构、并行算法和分布式计算基础。

当前实验覆盖：

- 顺序归约和并行归约
- 线程切分和局部累加
- mutex 计数器和 atomic 计数器
- bounded MPMC 队列
- CTest 正确性测试

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
