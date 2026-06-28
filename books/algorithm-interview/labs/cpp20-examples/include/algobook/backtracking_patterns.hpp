#pragma once

#include <array>
#include <cstddef>

namespace algobook {

inline constexpr std::size_t POINT_24_CARD_COUNT = 4;

bool judge_point_24_iterative(const std::array<int, POINT_24_CARD_COUNT>& cards);

}  // namespace algobook
