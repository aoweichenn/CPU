# 第 5 章：哈希表和前缀和

哈希表解决“快速查历史信息”，前缀和解决“快速计算区间信息”。两者结合后，可以把很多子数组问题从 `O(n^2)` 降到 `O(n)`。

## 哈希表的优化模型

两数之和是哈希表入门，但真正要记住的不是题，而是模型：

```text
当前元素 + 历史某个元素 = 目标
```

如果能把“历史某个元素”放入哈希表，当前只需要一次查询。这个模型可以扩展到四数相加、最长连续序列、异位词分组、重复元素检测。

## 前缀和

定义 `prefix[i]` 为前 `i` 个元素之和，则子数组 `[left, right]` 的和是：

```text
prefix[right + 1] - prefix[left]
```

如果题目问“和为 K 的子数组个数”，等价于对每个当前位置 `right`，寻找之前有多少个 `prefix[left] == prefix[right + 1] - K`。

哈希表保存前缀和出现次数：

```cpp
int subarray_sum(const std::vector<int>& nums, int k)
{
    std::unordered_map<int, int> prefix_count;
    prefix_count.reserve(nums.size() + 1);
    prefix_count[0] = 1;

    int prefix = 0;
    int answer = 0;
    for (int x : nums) {
        prefix += x;
        const int need = prefix - k;
        if (const auto found = prefix_count.find(need); found != prefix_count.end()) {
            answer += found->second;
        }
        ++prefix_count[prefix];
    }
    return answer;
}
```

为什么先查再插入？因为当前前缀只能和之前的前缀组成非空子数组。如果先插入，在 `k == 0` 时可能把当前位置和自己配对。

## 常见题型

异位词：把字符串映射成频次签名。小写字母可以用长度 26 的数组；字符集大时用哈希表。

最长连续序列：把所有数放入 `unordered_set`，只从“没有前驱”的数开始向后扩展。这样每个数最多被访问常数次，平均 `O(n)`。

子数组计数：前缀和 + 哈希计数。注意负数存在时，滑动窗口通常不能替代前缀和。

同余问题：如果 `(prefix[i] - prefix[j]) % k == 0`，说明两个前缀和模 `k` 相同。保存每个余数第一次出现位置或出现次数。

## C++ 注意点

`unordered_map` 的 `operator[]` 会插入默认值。计数时这很方便，查询时要小心：

```cpp
if (const auto it = count.find(key); it != count.end()) {
    answer += it->second;
}
```

如果 key 是数组或 pair，需要自定义 hash。面试中能不用自定义 hash 就先不用；比如异位词可以把 26 个计数拼成字符串 key，或对排序后的字符串分组。

## 本章题单

基础：两数之和、有效的字母异位词、两个数组的交集、快乐数。

哈希分组：字母异位词分组、四数相加 II、最长连续序列。

前缀和：和为 K 的子数组、连续数组、乘积小于 K 的子数组、路径总和 III。

复盘要求：说明哈希表里保存的是“值、下标、次数、最早位置”中的哪一种。
