#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lab00 {

inline constexpr std::size_t LAB00_DEFAULT_WARMUP_ITERATIONS = 3;
inline constexpr std::size_t LAB00_DEFAULT_MEASURED_ITERATIONS = 10;
inline constexpr std::size_t LAB00_SMALL_INPUT_SIZE = 1'024;
inline constexpr std::size_t LAB00_MEDIUM_INPUT_SIZE = 16'384;
inline constexpr std::size_t LAB00_LARGE_INPUT_SIZE = 1'048'576;
inline constexpr char LAB00_SIZE_LIST_DELIMITER = ',';

struct BenchmarkConfig {
    std::size_t warmup_iterations = LAB00_DEFAULT_WARMUP_ITERATIONS;
    std::size_t measured_iterations = LAB00_DEFAULT_MEASURED_ITERATIONS;
    std::vector<std::size_t> input_sizes;
    std::string only_name;
};

struct BenchmarkResult {
    std::string name;
    std::size_t input_size = 0;
    std::size_t iteration = 0;
    std::int64_t elapsed_ns = 0;
    std::uint64_t checksum = 0;
};

using BenchmarkWorkload = std::function<std::uint64_t()>;
using BenchmarkFactory = std::function<BenchmarkWorkload(std::size_t input_size)>;

struct BenchmarkCase {
    std::string name;
    BenchmarkFactory make_workload;
};

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(BenchmarkConfig config);

    void add(BenchmarkCase benchmark_case);
    [[nodiscard]] std::span<const BenchmarkCase> cases() const noexcept;
    [[nodiscard]] std::vector<BenchmarkResult> run() const;

    static void write_csv(std::ostream& output, std::span<const BenchmarkResult> results);

private:
    [[nodiscard]] bool should_run(std::string_view name) const;

    BenchmarkConfig config_;
    std::vector<BenchmarkCase> cases_;
};

[[nodiscard]] std::vector<std::size_t> default_input_sizes();
[[nodiscard]] std::vector<std::size_t> parse_size_list(std::string_view text);
[[nodiscard]] std::size_t parse_positive_count(std::string_view text, std::string_view field_name);

template <typename T>
inline void do_not_optimize(T const& value) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(value) : "memory");
#else
    auto const volatile* volatile_value = &value;
    static_cast<void>(volatile_value);
#endif
}

inline void clobber_memory() noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : : "memory");
#endif
}

} // namespace lab00
