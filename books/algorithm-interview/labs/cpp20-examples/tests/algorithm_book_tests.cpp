#include <algobook/array_patterns.hpp>
#include <algobook/graph_patterns.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void test_two_sum()
{
    const std::vector<int> nums = {2, 7, 11, 15};
    require(algobook::two_sum_bruteforce(nums, 9) == std::vector<int>({0, 1}), "two_sum_bruteforce basic");
    require(algobook::two_sum_hash_table(nums, 9) == std::vector<int>({0, 1}), "two_sum_hash_table basic");

    const std::vector<int> duplicates = {3, 3};
    require(algobook::two_sum_hash_table(duplicates, 6) == std::vector<int>({0, 1}), "two_sum duplicates");
}

void test_container_with_most_water()
{
    const std::vector<int> height = {1, 8, 6, 2, 5, 4, 8, 3, 7};
    require(algobook::max_area_bruteforce(height) == 49, "max_area_bruteforce sample");
    require(algobook::max_area_two_pointers(height) == 49, "max_area_two_pointers sample");
}

void test_min_subarray_len()
{
    const std::vector<int> nums = {2, 3, 1, 2, 4, 3};
    require(algobook::min_subarray_len_bruteforce(7, nums) == 2, "min_subarray_len_bruteforce sample");
    require(algobook::min_subarray_len_sliding_window(7, nums) == 2, "min_subarray_len_sliding_window sample");
    require(algobook::min_subarray_len_sliding_window(100, nums) == 0, "min_subarray_len no answer");
}

void test_subarray_sum_equals_k()
{
    const std::vector<int> sample = {1, 1, 1};
    require(algobook::subarray_sum_equals_k_bruteforce(sample, 2) == 2, "subarray_sum brute sample");
    require(algobook::subarray_sum_equals_k_prefix_hash(sample, 2) == 2, "subarray_sum prefix sample");

    const std::vector<int> with_negative = {1, -1, 0, 2, -2, 2};
    require(algobook::subarray_sum_equals_k_bruteforce(with_negative, 0) == 7, "subarray_sum brute negatives");
    require(algobook::subarray_sum_equals_k_prefix_hash(with_negative, 0) == 7, "subarray_sum prefix negatives");

    const std::vector<int> zeroes = {0, 0, 0};
    require(algobook::subarray_sum_equals_k_prefix_hash(zeroes, 0) == 6, "subarray_sum prefix zeroes");
}

void test_longest_substring()
{
    require(algobook::longest_substring_without_repeat_bruteforce("abcabcbb") == 3, "substring bruteforce sample");
    require(algobook::longest_substring_without_repeat_window("abcabcbb") == 3, "substring window sample");
    require(algobook::longest_substring_without_repeat_window("bbbbb") == 1, "substring repeated");
    require(algobook::longest_substring_without_repeat_window("") == 0, "substring empty");
}

void test_oranges_rotting()
{
    const std::vector<std::vector<int>> grid = {
        {2, 1, 1},
        {1, 1, 0},
        {0, 1, 1},
    };
    require(algobook::oranges_rotting_bfs(grid) == 4, "oranges rotting sample");

    const std::vector<std::vector<int>> blocked = {
        {2, 1, 1},
        {0, 1, 1},
        {1, 0, 1},
    };
    require(algobook::oranges_rotting_bfs(blocked) == -1, "oranges rotting unreachable");
}

void test_number_of_islands()
{
    const std::vector<std::vector<char>> grid = {
        {'1', '1', '1', '1', '0'},
        {'1', '1', '0', '1', '0'},
        {'1', '1', '0', '0', '0'},
        {'0', '0', '0', '0', '0'},
    };
    require(algobook::number_of_islands_bfs(grid) == 1, "number of islands sample one");

    const std::vector<std::vector<char>> many = {
        {'1', '1', '0', '0', '0'},
        {'1', '1', '0', '0', '0'},
        {'0', '0', '1', '0', '0'},
        {'0', '0', '0', '1', '1'},
    };
    require(algobook::number_of_islands_bfs(many) == 3, "number of islands sample three");
}

}  // namespace

int main()
{
    test_two_sum();
    test_container_with_most_water();
    test_min_subarray_len();
    test_subarray_sum_equals_k();
    test_longest_substring();
    test_oranges_rotting();
    test_number_of_islands();
    std::cout << "algorithm book tests passed\n";
    return 0;
}
