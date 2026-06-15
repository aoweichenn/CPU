# 第 3 章：C++ 容器用法和底层原理

算法面试里的 C++ 容器不是语法细节，而是复杂度和正确性的组成部分。选错容器，算法复杂度会变；不了解迭代器失效，代码可能隐蔽崩溃；不知道拷贝成本，可能在大输入下超时。

## vector

`std::vector<T>` 是连续动态数组。它的核心特点是：

- 下标访问 `O(1)`。
- 尾部 `push_back` 均摊 `O(1)`。
- 中间插入和删除 `O(n)`，因为要移动后续元素。
- 扩容时会重新分配内存，旧指针、引用、迭代器可能失效。

刷题里，`vector` 是数组、动态规划表、邻接表、排序输入的默认选择。

常用写法：

```cpp
std::vector<int> nums;
nums.reserve(n);
nums.push_back(10);
nums.emplace_back(20);

std::sort(nums.begin(), nums.end());
const auto it = std::lower_bound(nums.begin(), nums.end(), 15);
```

`reserve` 和 `resize` 不一样。`reserve(n)` 只预留容量，不改变 `size()`；`resize(n)` 会把大小改成 `n`，并构造元素。写 DP 数组时通常用 `resize` 或构造函数：

```cpp
std::vector<int> dp(n + 1, 0);
```

## string

`std::string` 可以看成字符版 `vector`，但它有字符串相关 API。滑动窗口题常用 `s.size()`、`s[i]`、`substr`。要注意 `substr` 会创建新字符串，频繁调用可能把复杂度从线性拖成平方。

在最长无重复子串里，不要每次 `substr` 后再判断重复；应该维护窗口边界和字符最后出现位置。

## deque

`std::deque<T>` 支持两端 `push_front`、`push_back`、`pop_front`、`pop_back`，复杂度都是 `O(1)`。它不是连续数组，随机访问虽然是 `O(1)`，但局部性不如 `vector`。单调队列和 BFS 队列底层经常用它。

`std::queue` 默认就是基于 `deque` 的容器适配器。适配器的意思是它限制了接口，只暴露队列操作：

```cpp
std::queue<int> q;
q.push(1);
q.push(2);
int x = q.front();
q.pop();
```

`pop()` 不返回元素，这是 C++ 标准库的设计。需要先 `front()` 再 `pop()`。

## stack 和 queue

`std::stack` 用于最近未匹配结构：括号匹配、表达式、路径简化、单调栈。`std::queue` 用于 BFS 层序扩展。

栈题的关键问题通常是：栈里存值，还是存下标？单调栈多数时候存下标，因为需要计算距离、宽度或回到原数组取值。

队列题的关键问题通常是：如何区分层？可以记录 `level_size`，每轮处理当前队列长度；也可以把距离和节点一起入队。腐烂橘子用 `level_size` 最自然，因为每一层代表一分钟。

## unordered_map 和 unordered_set

`std::unordered_map<K, V>` 是哈希表，平均查询、插入、删除 `O(1)`。常用于：

- 值到下标：两数之和。
- 前缀和到次数：和为 K 的子数组。
- 字符到计数：异位词、窗口覆盖。
- 节点到访问状态：图搜索。

常见坑：

- `operator[]` 如果 key 不存在，会插入默认值。只想查询时用 `find` 或 `contains`。
- 插入大量元素前可以 `reserve`，减少 rehash。
- 遍历顺序不稳定，不要依赖输出顺序。
- 自定义 key 需要哈希函数。

示例：

```cpp
std::unordered_map<int, int> freq;
freq.reserve(nums.size());
for (int x : nums) {
    ++freq[x];
}

if (freq.contains(10)) {
    // C++20 contains，只判断是否存在。
}
```

## map 和 set

`std::map` 和 `std::set` 通常基于红黑树，查询、插入、删除是 `O(log n)`，但保持有序。需要有序遍历、前驱后继、区间边界时使用。

如果只是查存在性，优先 `unordered_set`。如果需要“比 x 大的最小元素”，用 `set.lower_bound(x)`。

## priority_queue

`std::priority_queue<T>` 是堆适配器，默认大顶堆。它适合动态 Top K、合并 K 个有序链表、任务调度。

小顶堆写法：

```cpp
std::priority_queue<int, std::vector<int>, std::greater<int>> min_heap;
```

堆只能访问堆顶，不能高效删除任意元素。滑动窗口中如果需要删除过期元素，可以堆里存下标并在堆顶懒删除；如果需要严格维护窗口最大值，单调队列通常更合适。

## 容器选择表

| 需求 | 首选容器 | 原因 |
|---|---|---|
| 连续数组、DP、排序 | `vector` | 连续内存，下标快 |
| 字符串窗口 | `string` + 数组/哈希 | 字符访问和计数 |
| BFS | `queue` | 先进先出 |
| 最近未匹配 | `stack` | 后进先出 |
| 两端维护候选 | `deque` | 两端 O(1) |
| 平均 O(1) 查询 | `unordered_map/set` | 哈希 |
| 有序查询 | `map/set` | 平衡树 |
| 动态最大/最小 | `priority_queue` | 堆 |

## 面试注意

讲容器时要顺带讲复杂度。例如“我用 `unordered_map` 保存前缀和出现次数，每次查询平均 `O(1)`，所以总时间 `O(n)`，额外空间 `O(n)`”。如果题目要求稳定顺序或最坏复杂度，再考虑 `map`。
