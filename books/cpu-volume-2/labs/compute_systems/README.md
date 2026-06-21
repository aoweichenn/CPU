# Compute Systems Engine 早期实验素材

这是第二册早期计算系统实验素材，保留顺序归约、并行归约、计数器和有界队列等基础代码。第二册正式正文现在作为原理卷维护；完整 C++20 工程实现、测试、benchmark、故障注入和报告会进入配套代码实践卷：

```text
books/compute-systems-engine-code/
```

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
