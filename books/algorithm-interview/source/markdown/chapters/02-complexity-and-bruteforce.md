# 第 2 章：复杂度、暴力法和优化思维

复杂度不是写在题解最后的装饰，而是决定你能不能通过的第一约束。输入规模如果是 `n <= 100`，`O(n^3)` 可能可以接受；如果是 `n <= 2 * 10^5`，通常要接近 `O(n log n)` 或 `O(n)`；如果是网格 `m * n <= 10^5`，BFS/DFS 扫一遍通常合理；如果状态空间是指数级，就必须问能不能剪枝、记忆化或动态规划。

## 暴力法不是错误答案

暴力法的作用是把问题完整展开。以两数之和为例，题目要找两个下标 `i, j`，满足 `nums[i] + nums[j] == target`。最直接的暴力法就是枚举所有下标对：

```cpp
std::vector<int> two_sum_bruteforce(const std::vector<int>& nums, int target)
{
    for (int left = 0; left < static_cast<int>(nums.size()); ++left) {
        for (int right = left + 1; right < static_cast<int>(nums.size()); ++right) {
            if (nums[left] + nums[right] == target) {
                return {left, right};
            }
        }
    }
    return {};
}
```

这段代码的枚举对象是下标对，数量大约是 `n * (n - 1) / 2`，所以时间复杂度是 `O(n^2)`。空间复杂度是 `O(1)`，不算返回结果。

暴力法慢在哪里？慢在每固定一个 `left`，都要从右侧重新寻找一个值。我们真正需要的不是“扫描右侧所有数”，而是“快速知道之前是否出现过 target - nums[i]”。这就把问题转化成了历史查询，哈希表自然出现。

```cpp
std::vector<int> two_sum_hash_table(const std::vector<int>& nums, int target)
{
    std::unordered_map<int, int> value_to_index;
    value_to_index.reserve(nums.size());

    for (int index = 0; index < static_cast<int>(nums.size()); ++index) {
        const int need = target - nums[index];
        const auto found = value_to_index.find(need);
        if (found != value_to_index.end()) {
            return {found->second, index};
        }
        value_to_index.emplace(nums[index], index);
    }
    return {};
}
```

优化后的复杂度平均是 `O(n)`。为什么是平均？因为 `unordered_map` 依赖哈希分布，极端冲突时可能退化。本书刷题代码会用它，但必须知道这个前提。

## 优化的常见来源

第一类是缓存重复计算。递归斐波那契、网格路径、背包、字符串匹配都可能有大量重复子问题。缓存可以是哈希表、数组、二维表，也可以是前缀和。

第二类是利用单调性。二分查找、答案二分、双指针很多时候都来自单调性。单调性不是“数组有序”这么简单，而是某个判断函数从 false 到 true 或从 true 到 false 只变化一次。

第三类是维护局部状态。滑动窗口之所以能把枚举所有子数组降到线性，是因为窗口移动时状态可以增量更新。例如字符计数、窗口和、不同元素个数、最大值候选。

第四类是淘汰不可能成为答案的候选。单调栈、单调队列、堆、双指针都常用这种思想。盛水容器每次移动短板，就是因为固定短板时，向内移动长板不可能得到更大面积。

第五类是改变问题表示。图题经常要从题面抽象出节点和边；区间题可能要排序；字符串题可能要转成频次或前缀状态；树题可能要把递归信息改成后序汇总。

## 从暴力到优化的提问清单

写完暴力法后问：

- 我枚举的对象是什么？元素、下标对、子数组、路径、状态，还是排列？
- 是否重复计算了同一段信息？
- 是否只需要历史信息的一小部分？
- 是否存在有序、单调、边界收缩或候选淘汰？
- 是否可以把区间查询变成前缀差？
- 是否可以用哈希把查询从线性变成平均常数？
- 是否可以把指数搜索改成状态 DP？
- 优化后需要额外空间吗？这个空间是否接受？

## 面试表达

不要直接说“这题用哈希表”。更好的表达是：

“暴力会枚举所有下标对，时间是 `O(n^2)`。慢点在于对每个数都重复查找另一个数是否存在。我们可以从左到右扫描，并用哈希表保存已经看到的值到下标的映射。处理当前 `nums[i]` 时，只要查 `target - nums[i]` 是否在表里。这样每个元素只进入表一次、查询一次，平均时间 `O(n)`，空间 `O(n)`。”

这段话同时交代了暴力、瓶颈、优化结构、正确性和复杂度，比只背“哈希表秒了”可靠得多。
