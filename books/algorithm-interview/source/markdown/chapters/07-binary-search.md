# 第 7 章：二分查找和答案空间

二分查找的本质不是“在数组里找数”，而是在单调结构中找边界。只要能定义一个判断函数 `check(x)`，并且它在答案空间上单调，就可以二分。

## 基础二分

在升序数组中找目标值：

```cpp
int binary_search_index(const std::vector<int>& nums, int target)
{
    int left = 0;
    int right = static_cast<int>(nums.size()) - 1;
    while (left <= right) {
        const int mid = left + (right - left) / 2;
        if (nums[mid] == target) {
            return mid;
        }
        if (nums[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}
```

`mid` 用 `left + (right - left) / 2` 是为了避免 `left + right` 溢出。

## 找左边界

很多题不是找任意一个目标，而是找第一个满足条件的位置。C++ 标准库的 `lower_bound` 就是这个模型：找第一个不小于目标的位置。

```cpp
const auto it = std::lower_bound(nums.begin(), nums.end(), target);
```

手写时推荐使用左闭右开区间 `[left, right)`，因为循环和边界更统一。

## 答案二分

爱吃香蕉的珂珂不是在数组里找数，而是在速度范围里找最小可行速度。定义：

```text
check(speed) = 用 speed 能否在 h 小时内吃完
```

速度越大越容易完成，所以 `check` 从 false 变成 true。我们要找第一个 true。

答案二分的三步：

1. 明确答案范围。
2. 写 `check(x)`。
3. 判断找最小可行还是最大可行。

## 常见坑

- 死循环：`left` 和 `right` 更新没有排除 `mid`。
- 边界错：不知道返回 `left` 还是 `right`。
- 单调性错：`check` 并不单调，却强行二分。
- 溢出：`mid` 或 `check` 内部乘法溢出，需要 `long long`。

## 本章题单

基础：二分查找、搜索插入位置、在排序数组中查找元素的第一个和最后一个位置。

变形：搜索旋转排序数组、寻找峰值、寻找旋转排序数组中的最小值。

答案二分：爱吃香蕉的珂珂、在 D 天内送达包裹的能力、分割数组的最大值、制作 m 束花所需的最少天数。

复盘要求：每道答案二分题必须写出 `check(x)` 的单调性证明。
