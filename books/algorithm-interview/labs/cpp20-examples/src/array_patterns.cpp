#include <algobook/array_patterns.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace algobook {

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

int subarray_sum_equals_k_bruteforce(const std::vector<int>& nums, int target)
{
    int answer_count = 0;

    for (int left = 0; left < static_cast<int>(nums.size()); ++left) {
        std::int64_t sum = 0;
        for (int right = left; right < static_cast<int>(nums.size()); ++right) {
            sum += nums[right];
            if (sum == target) {
                ++answer_count;
            }
        }
    }

    return answer_count;
}

int subarray_sum_equals_k_prefix_hash(const std::vector<int>& nums, int target)
{
    std::unordered_map<std::int64_t, int> prefix_count;
    prefix_count.reserve(nums.size() + 1);
    prefix_count.emplace(0, 1);

    std::int64_t prefix_sum = 0;
    int answer_count = 0;

    for (const int value : nums) {
        prefix_sum += value;
        const auto found =
            prefix_count.find(prefix_sum - static_cast<std::int64_t>(target));
        if (found != prefix_count.end()) {
            answer_count += found->second;
        }
        ++prefix_count[prefix_sum];
    }

    return answer_count;
}

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

int max_subarray_bruteforce(const std::vector<int>& nums)
{
    if (nums.empty()) {
        throw std::invalid_argument("max_subarray requires a non-empty input");
    }

    int best = std::numeric_limits<int>::min();

    for (int left = 0; left < static_cast<int>(nums.size()); ++left) {
        int sum = 0;
        for (int right = left; right < static_cast<int>(nums.size()); ++right) {
            sum += nums[right];
            best = std::max(best, sum);
        }
    }

    return best;
}

int max_subarray_kadane(const std::vector<int>& nums)
{
    if (nums.empty()) {
        throw std::invalid_argument("max_subarray requires a non-empty input");
    }

    int best_end_here = nums.front();
    int best_overall = nums.front();

    for (int index = 1; index < static_cast<int>(nums.size()); ++index) {
        const int value = nums[index];
        best_end_here = std::max(value, best_end_here + value);
        best_overall = std::max(best_overall, best_end_here);
    }

    return best_overall;
}

}  // namespace algobook
