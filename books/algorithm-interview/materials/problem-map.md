# 题单地图

本题单按能力分层，不按力扣编号排序。

## 使用规则

每天按“一道新题、一道同模型变形、一道回访题”选题。新题用于扩展模型，变形题用于验证迁移，回访题用于检查是否真的能重建推理链。每道题都要留下六个字段：暴力枚举对象、重复工作、优化依赖的性质、核心数据结构、不变量、失败样例。

回访间隔建议为 `1 天 -> 7 天 -> 21 天`。若回访时不能在不看题解的情况下写出暴力解、优化动机和边界测试，这道题回到当前层继续训练，不进入下一层。

## 第一层：必须秒懂的基础题

数组：LeetCode 1 两数之和、LeetCode 283 移动零、LeetCode 26 删除有序数组重复项、LeetCode 88 合并两个有序数组。

字符串：LeetCode 125 有效回文、LeetCode 14 最长公共前缀、LeetCode 344 反转字符串、LeetCode 28 实现 strStr。

哈希：LeetCode 242 有效的字母异位词、LeetCode 349 两个数组的交集、LeetCode 202 快乐数。

栈队列：LeetCode 20 有效括号、LeetCode 225 用队列实现栈、LeetCode 232 用栈实现队列。

树：LeetCode 104 二叉树最大深度、LeetCode 100 相同的树、LeetCode 226 翻转二叉树。

## 第二层：面试高频中等题

双指针：LeetCode 15 三数之和、LeetCode 11 盛最多水的容器、LeetCode 42 接雨水。

滑动窗口：LeetCode 3 无重复字符的最长子串、LeetCode 209 长度最小的子数组、LeetCode 438 找到字符串中所有字母异位词、LeetCode 76 最小覆盖子串。

前缀和：LeetCode 560 和为 K 的子数组、LeetCode 525 连续数组、LeetCode 523 连续的子数组和。

前缀和训练链：LeetCode 303 区域和检索 -> LeetCode 560 和为 K 的子数组 -> LeetCode 525 连续数组 -> LeetCode 974 和可被 K 整除的子数组 -> LeetCode 1074 元素和为目标值的子矩阵数量。重点不是背公式，而是判断哈希表保存的是前缀和、差值、余数还是压缩后的行状态。

单调结构：LeetCode 739 每日温度、LeetCode 239 滑动窗口最大值、LeetCode 84 柱状图最大矩形。

二分：LeetCode 33 搜索旋转排序数组、LeetCode 162 寻找峰值、LeetCode 875 爱吃香蕉的珂珂、LeetCode 410 分割数组的最大值。

图：LeetCode 200 岛屿数量、LeetCode 994 腐烂的橘子、LeetCode 207 课程表、LeetCode 127 单词接龙。

动态规划：LeetCode 322 零钱兑换、LeetCode 300 最长递增子序列、LeetCode 139 单词拆分、LeetCode 1143 最长公共子序列。

## 第三层：拉开差距的综合题

LeetCode 295 数据流中位数、LeetCode 224 基本计算器、LeetCode 72 编辑距离、LeetCode 51 N 皇后、LeetCode 10 正则表达式匹配、LeetCode 632 最小区间覆盖 K 个列表、LeetCode 23 合并 K 个升序链表、LeetCode 721 账户合并、LeetCode 934 最短桥。

## 题目复盘标签

给每道题打标签：

- `bruteforce`：暴力枚举对象。
- `invariant`：关键不变量。
- `container`：核心 C++ 容器。
- `boundary`：边界和空输入。
- `overflow`：是否需要 `long long`。
- `revisit`：需要一周后回访。
