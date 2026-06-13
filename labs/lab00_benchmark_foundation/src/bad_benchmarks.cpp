#include <lab00/benchmark.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::size_t LAB00_BAD_INPUT_SIZE = 1'048'576;
constexpr std::size_t LAB00_BAD_REPEAT_COUNT = 20;
constexpr std::uint64_t LAB00_BAD_RANDOM_SEED = 12'345;
constexpr std::uint64_t LAB00_BAD_VECTOR_FILL = 7;

template <typename Function>
[[nodiscard]] std::int64_t measure_ns(Function function)
{
    auto const begin = std::chrono::steady_clock::now();
    function();
    auto const end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
}

[[nodiscard]] std::vector<std::uint64_t> make_input()
{
    return std::vector<std::uint64_t>(LAB00_BAD_INPUT_SIZE, LAB00_BAD_VECTOR_FILL);
}

void print_result(std::string_view name, std::int64_t elapsed_ns)
{
    std::cout << name << ": " << elapsed_ns << " ns\n";
}

void demonstrate_dead_code_elimination()
{
    auto const input = make_input();

    std::int64_t const bad_elapsed = measure_ns([&input] {
        std::uint64_t sum = 0;
        for (std::uint64_t value : input) {
            sum += value;
        }
        static_cast<void>(sum);
    });

    std::int64_t const controlled_elapsed = measure_ns([&input] {
        std::uint64_t sum = 0;
        for (std::uint64_t value : input) {
            sum += value;
        }
        lab00::do_not_optimize(sum);
    });

    print_result("bad_dead_result_not_observed", bad_elapsed);
    print_result("controlled_dead_result_observed", controlled_elapsed);
}

void demonstrate_allocation_in_timed_region()
{
    std::int64_t const bad_elapsed = measure_ns([] {
        std::uint64_t checksum = 0;
        for (std::size_t repeat = 0; repeat < LAB00_BAD_REPEAT_COUNT; ++repeat) {
            std::vector<std::uint64_t> values(LAB00_BAD_INPUT_SIZE, LAB00_BAD_VECTOR_FILL);
            checksum += std::accumulate(values.begin(), values.end(), std::uint64_t{0});
        }
        lab00::do_not_optimize(checksum);
    });

    auto values = make_input();
    std::int64_t const controlled_elapsed = measure_ns([&values] {
        std::uint64_t checksum = 0;
        for (std::size_t repeat = 0; repeat < LAB00_BAD_REPEAT_COUNT; ++repeat) {
            checksum += std::accumulate(values.begin(), values.end(), std::uint64_t{0});
        }
        lab00::do_not_optimize(checksum);
    });

    print_result("bad_allocation_inside_timing", bad_elapsed);
    print_result("controlled_allocation_outside_timing", controlled_elapsed);
}

void demonstrate_rng_in_timed_region()
{
    std::int64_t const bad_elapsed = measure_ns([] {
        std::mt19937_64 rng(LAB00_BAD_RANDOM_SEED);
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < LAB00_BAD_INPUT_SIZE; ++index) {
            checksum += rng();
        }
        lab00::do_not_optimize(checksum);
    });

    std::vector<std::uint64_t> values(LAB00_BAD_INPUT_SIZE);
    std::mt19937_64 rng(LAB00_BAD_RANDOM_SEED);
    for (auto& value : values) {
        value = rng();
    }

    std::int64_t const controlled_elapsed = measure_ns([&values] {
        std::uint64_t checksum = 0;
        for (std::uint64_t value : values) {
            checksum += value;
        }
        lab00::do_not_optimize(checksum);
    });

    print_result("bad_rng_inside_timing", bad_elapsed);
    print_result("controlled_rng_precomputed", controlled_elapsed);
}

void demonstrate_formatting_in_timed_region()
{
    std::int64_t const bad_elapsed = measure_ns([] {
        std::uint64_t checksum = 0;
        for (std::size_t repeat = 0; repeat < LAB00_BAD_REPEAT_COUNT; ++repeat) {
            std::ostringstream stream;
            stream << "repeat=" << repeat << ",value=" << LAB00_BAD_VECTOR_FILL;
            checksum += stream.str().size();
        }
        lab00::do_not_optimize(checksum);
    });

    std::int64_t const controlled_elapsed = measure_ns([] {
        std::uint64_t checksum = 0;
        for (std::size_t repeat = 0; repeat < LAB00_BAD_REPEAT_COUNT; ++repeat) {
            checksum += repeat + LAB00_BAD_VECTOR_FILL;
        }
        lab00::do_not_optimize(checksum);
    });

    print_result("bad_formatting_inside_timing", bad_elapsed);
    print_result("controlled_no_formatting_inside_timing", controlled_elapsed);
}

} // namespace

int main()
{
#ifndef NDEBUG
    std::cout << "Build mode note: NDEBUG is not defined. This looks like a Debug build.\n";
    std::cout << "For timing experiments, rebuild with -DCMAKE_BUILD_TYPE=Release.\n";
#endif

    std::cout << "These examples are intentionally flawed. Use them to learn what not to trust.\n";
    demonstrate_dead_code_elimination();
    demonstrate_allocation_in_timed_region();
    demonstrate_rng_in_timed_region();
    demonstrate_formatting_in_timed_region();
    return 0;
}
