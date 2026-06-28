#include <algobook/backtracking_patterns.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace algobook {
namespace {

constexpr double POINT_24_TARGET = 24.0;
constexpr double POINT_24_EPSILON = 1e-6;

struct SearchState {
    std::vector<double> values;
};

void push_next_state(std::vector<SearchState>& stack, std::vector<double> values)
{
    stack.push_back(SearchState{std::move(values)});
}

std::vector<double> make_remaining_values(const std::vector<double>& values, int first, int second)
{
    std::vector<double> remaining;
    remaining.reserve(values.size() - 1);
    for (int index = 0; index < static_cast<int>(values.size()); ++index) {
        if (index != first && index != second) {
            remaining.push_back(values[index]);
        }
    }
    return remaining;
}

void push_operation_results(
    std::vector<SearchState>& stack,
    const std::vector<double>& values,
    int first,
    int second)
{
    const double left = values[first];
    const double right = values[second];
    const std::vector<double> remaining = make_remaining_values(values, first, second);

    auto with_result = [&remaining](double result) {
        std::vector<double> next = remaining;
        next.push_back(result);
        return next;
    };

    push_next_state(stack, with_result(left + right));
    push_next_state(stack, with_result(left * right));
    push_next_state(stack, with_result(left - right));
    push_next_state(stack, with_result(right - left));

    if (std::abs(right) > POINT_24_EPSILON) {
        push_next_state(stack, with_result(left / right));
    }
    if (std::abs(left) > POINT_24_EPSILON) {
        push_next_state(stack, with_result(right / left));
    }
}

}  // namespace

bool judge_point_24_iterative(const std::array<int, POINT_24_CARD_COUNT>& cards)
{
    std::vector<SearchState> stack;
    stack.push_back(SearchState{std::vector<double>{
        static_cast<double>(cards[0]),
        static_cast<double>(cards[1]),
        static_cast<double>(cards[2]),
        static_cast<double>(cards[3]),
    }});

    while (!stack.empty()) {
        SearchState state = std::move(stack.back());
        stack.pop_back();

        if (state.values.size() == 1) {
            if (std::abs(state.values.front() - POINT_24_TARGET) < POINT_24_EPSILON) {
                return true;
            }
            continue;
        }

        for (int first = 0; first < static_cast<int>(state.values.size()); ++first) {
            for (int second = first + 1; second < static_cast<int>(state.values.size()); ++second) {
                push_operation_results(stack, state.values, first, second);
            }
        }
    }

    return false;
}

}  // namespace algobook
