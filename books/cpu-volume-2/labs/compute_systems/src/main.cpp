#include <compsys/parallel_reduce.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr std::int32_t COMPSYS_DEMO_SIZE = 1024;
constexpr std::int32_t COMPSYS_DEMO_WORKERS = 4;

std::vector<std::int32_t> make_demo_values() {
    std::vector<std::int32_t> values(static_cast<std::size_t>(COMPSYS_DEMO_SIZE));
    for (std::int32_t i = 0; i < COMPSYS_DEMO_SIZE; ++i) {
        values[static_cast<std::size_t>(i)] = i % 17;
    }
    return values;
}

}  // namespace

int main() {
    const std::vector<std::int32_t> values = make_demo_values();
    const std::int64_t sequential = compsys::sequential_sum(values);
    const std::int64_t parallel = compsys::parallel_sum(values, COMPSYS_DEMO_WORKERS);

    compsys::AtomicCounter counter;
    counter.add(sequential);
    counter.add(parallel);

    std::cout << "sequential_sum " << sequential << "\n";
    std::cout << "parallel_sum " << parallel << "\n";
    std::cout << "counter " << counter.value() << "\n";
    return EXIT_SUCCESS;
}
