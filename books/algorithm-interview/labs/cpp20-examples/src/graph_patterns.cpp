#include <algobook/graph_patterns.hpp>

#include <array>
#include <queue>
#include <utility>

namespace algobook {

namespace {

constexpr int FRESH = 1;
constexpr int ROTTEN = 2;
constexpr char WATER_OR_VISITED_LAND = '0';

using Cell = std::pair<int, int>;

constexpr std::array<Cell, 4> DIRECTIONS = {
    Cell{1, 0},
    Cell{-1, 0},
    Cell{0, 1},
    Cell{0, -1},
};

bool in_bounds(int row, int col, int rows, int cols)
{
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

}  // namespace

int oranges_rotting_bfs(std::vector<std::vector<int>> grid)
{
    if (grid.empty() || grid.front().empty()) {
        return 0;
    }

    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid.front().size());
    std::queue<Cell> queue;
    int fresh_count = 0;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (grid[row][col] == FRESH) {
                ++fresh_count;
            } else if (grid[row][col] == ROTTEN) {
                queue.emplace(row, col);
            }
        }
    }

    int minutes = 0;
    while (!queue.empty() && fresh_count > 0) {
        const int level_size = static_cast<int>(queue.size());
        for (int step = 0; step < level_size; ++step) {
            const auto [row, col] = queue.front();
            queue.pop();

            for (const auto [dr, dc] : DIRECTIONS) {
                const int next_row = row + dr;
                const int next_col = col + dc;
                if (!in_bounds(next_row, next_col, rows, cols) || grid[next_row][next_col] != FRESH) {
                    continue;
                }
                grid[next_row][next_col] = ROTTEN;
                --fresh_count;
                queue.emplace(next_row, next_col);
            }
        }
        ++minutes;
    }

    return fresh_count == 0 ? minutes : -1;
}

int number_of_islands_bfs(std::vector<std::vector<char>> grid)
{
    if (grid.empty() || grid.front().empty()) {
        return 0;
    }

    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid.front().size());
    int island_count = 0;

    for (int start_row = 0; start_row < rows; ++start_row) {
        for (int start_col = 0; start_col < cols; ++start_col) {
            if (grid[start_row][start_col] != '1') {
                continue;
            }

            ++island_count;
            std::queue<Cell> queue;
            queue.emplace(start_row, start_col);
            grid[start_row][start_col] = WATER_OR_VISITED_LAND;

            while (!queue.empty()) {
                const auto [row, col] = queue.front();
                queue.pop();

                for (const auto [dr, dc] : DIRECTIONS) {
                    const int next_row = row + dr;
                    const int next_col = col + dc;
                    if (!in_bounds(next_row, next_col, rows, cols) || grid[next_row][next_col] != '1') {
                        continue;
                    }
                    grid[next_row][next_col] = WATER_OR_VISITED_LAND;
                    queue.emplace(next_row, next_col);
                }
            }
        }
    }

    return island_count;
}

}  // namespace algobook
