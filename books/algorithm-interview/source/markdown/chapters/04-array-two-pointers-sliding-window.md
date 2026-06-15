# 第 4 章：数组、双指针和滑动窗口

数组题是刷题的地基。双指针和滑动窗口不是两个孤立技巧，而是从暴力枚举子结构中优化出来的两类方法。双指针常用于有序数组、两端收缩、左右边界协作；滑动窗口常用于连续子数组或子串，并且窗口状态能被增量维护。

## 盛最多水的容器：从暴力到双指针

题目：给定数组 `height`，第 `i` 条线高度为 `height[i]`，选择两条线和 x 轴组成容器，求最大面积。

暴力解枚举左右边界：

```cpp
int max_area_bruteforce(const std::vector<int>& height)
{
    int best = 0;
    for (int left = 0; left < static_cast<int>(height.size()); ++left) {
        for (int right = left + 1; right < static_cast<int>(height.size()); ++right) {
            const int width = right - left;
            const int bounded_height = std::min(height[left], height[right]);
            best = std::max(best, width * bounded_height);
        }
    }
    return best;
}
```

复杂度是 `O(n^2)`。慢点在于枚举了所有边界对。能不能少枚举？面积由宽度和较短边决定。初始取最宽的左右边界，如果移动较高的一边，宽度变小，而较短边没有变高，面积不可能变大。因此每次应该移动较短边，因为只有这样才有机会提高瓶颈高度。

优化解：

```cpp
int max_area_two_pointers(const std::vector<int>& height)
{
    int left = 0;
    int right = static_cast<int>(height.size()) - 1;
    int best = 0;

    while (left < right) {
        const int width = right - left;
        const int bounded_height = std::min(height[left], height[right]);
        best = std::max(best, width * bounded_height);

        if (height[left] <= height[right]) {
            ++left;
        } else {
            --right;
        }
    }
    return best;
}
```

不变量：每一步排除的那条短边，与当前另一边或更内侧任意边组成的面积都不会超过当前已考虑面积中的上界，因为宽度只会变小，短边高度仍受它限制。

力扣案例：11 盛最多水的容器。变形：接雨水、三数之和、有序数组两数之和。

## 长度最小的子数组：从枚举子数组到滑动窗口

题目：给定正整数数组 `nums` 和目标 `target`，求和至少为 `target` 的最短连续子数组长度。

暴力枚举起点和终点：

```cpp
int min_subarray_len_bruteforce(int target, const std::vector<int>& nums)
{
    int best = std::numeric_limits<int>::max();
    for (int left = 0; left < static_cast<int>(nums.size()); ++left) {
        int sum = 0;
        for (int right = left; right < static_cast<int>(nums.size()); ++right) {
            sum += nums[right];
            if (sum >= target) {
                best = std::min(best, right - left + 1);
                break;
            }
        }
    }
    return best == std::numeric_limits<int>::max() ? 0 : best;
}
```

优化的关键是“正整数”。右边界向右移动时，窗口和只会增加；左边界向右移动时，窗口和只会减少。因此当窗口和已经达到目标，就可以不断收缩左边界，直到不满足为止。

```cpp
int min_subarray_len_sliding_window(int target, const std::vector<int>& nums)
{
    int best = std::numeric_limits<int>::max();
    int left = 0;
    int sum = 0;

    for (int right = 0; right < static_cast<int>(nums.size()); ++right) {
        sum += nums[right];
        while (sum >= target) {
            best = std::min(best, right - left + 1);
            sum -= nums[left];
            ++left;
        }
    }
    return best == std::numeric_limits<int>::max() ? 0 : best;
}
```

为什么这个方法在有负数时会失效？因为右移右边界不一定让和变大，左移左边界不一定让和变小，窗口的单调性消失。此时要考虑前缀和、单调队列或哈希。

力扣案例：209 长度最小的子数组。变形：862 和至少为 K 的最短子数组、560 和为 K 的子数组。

## 无重复字符的最长子串：窗口状态如何维护

暴力法枚举每个起点，并用集合检查直到重复：

```cpp
int longest_substring_without_repeat_bruteforce(const std::string& s)
{
    int best = 0;
    for (int left = 0; left < static_cast<int>(s.size()); ++left) {
        std::unordered_set<char> seen;
        for (int right = left; right < static_cast<int>(s.size()); ++right) {
            if (seen.contains(s[right])) {
                break;
            }
            seen.insert(s[right]);
            best = std::max(best, right - left + 1);
        }
    }
    return best;
}
```

优化时，窗口不变量是：`[left, right]` 内没有重复字符。处理 `s[right]` 时，如果它上次出现位置在窗口内，就把 `left` 跳到上次出现位置后一格。

```cpp
int longest_substring_without_repeat_window(const std::string& s)
{
    constexpr int ASCII_RANGE = 256;
    std::array<int, ASCII_RANGE> last_seen{};
    last_seen.fill(-1);

    int left = 0;
    int best = 0;
    for (int right = 0; right < static_cast<int>(s.size()); ++right) {
        const auto ch = static_cast<unsigned char>(s[right]);
        if (last_seen[ch] >= left) {
            left = last_seen[ch] + 1;
        }
        last_seen[ch] = right;
        best = std::max(best, right - left + 1);
    }
    return best;
}
```

这里用 `std::array<int, 256>` 是因为题目通常按 ASCII 字符处理。如果是完整 Unicode 字符，需要换成哈希表或先定义字符单元。

## 本章题单

基础题：移动零、删除有序数组重复项、合并两个有序数组、有序数组的平方。

双指针：盛最多水的容器、三数之和、四数之和、接雨水。

滑动窗口：长度最小的子数组、无重复字符的最长子串、找到字符串中所有字母异位词、最小覆盖子串、替换后的最长重复字符。

复盘要求：每题都写出窗口不变量，说明窗口扩大和收缩条件。
