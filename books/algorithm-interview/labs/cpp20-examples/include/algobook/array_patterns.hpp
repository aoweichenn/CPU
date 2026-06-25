#pragma once

#include <string>
#include <vector>

namespace algobook {

std::vector<int> two_sum_bruteforce(const std::vector<int>& nums, int target);
std::vector<int> two_sum_hash_table(const std::vector<int>& nums, int target);

int max_area_bruteforce(const std::vector<int>& height);
int max_area_two_pointers(const std::vector<int>& height);

int min_subarray_len_bruteforce(int target, const std::vector<int>& nums);
int min_subarray_len_sliding_window(int target, const std::vector<int>& nums);

int subarray_sum_equals_k_bruteforce(const std::vector<int>& nums, int target);
int subarray_sum_equals_k_prefix_hash(const std::vector<int>& nums, int target);

int longest_substring_without_repeat_bruteforce(const std::string& s);
int longest_substring_without_repeat_window(const std::string& s);

}  // namespace algobook
