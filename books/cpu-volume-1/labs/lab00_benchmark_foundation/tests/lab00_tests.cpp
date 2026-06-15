#include <lab00/benchmark.hpp>
#include <lab00/kernels.hpp>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void test_parse_size_list()
{
    std::vector<std::size_t> const sizes = lab00::parse_size_list("16, 32,64");
    require(sizes.size() == 3, "parse_size_list should parse three values");
    require(sizes[0] == 16, "parse_size_list first value mismatch");
    require(sizes[1] == 32, "parse_size_list second value mismatch");
    require(sizes[2] == 64, "parse_size_list third value mismatch");
}

void test_parse_size_list_rejects_zero()
{
    bool rejected = false;
    try {
        static_cast<void>(lab00::parse_size_list("128,0"));
    } catch (std::invalid_argument const&) {
        rejected = true;
    }

    require(rejected, "parse_size_list should reject zero");
}

void test_sum_i32()
{
    std::vector<std::int32_t> const values{1, -2, 3, 4};
    require(lab00::sum_i32(values) == 6, "sum_i32 returned the wrong sum");
}

void test_sum_f32()
{
    std::vector<float> const values{1.0F, 2.0F, 3.5F};
    require(lab00::sum_f32(values) == 6.5F, "sum_f32 returned the wrong sum");
}

void test_dot_f32()
{
    std::vector<float> const left{1.0F, 2.0F, 3.0F};
    std::vector<float> const right{4.0F, 5.0F, 6.0F};
    require(lab00::dot_f32(left, right) == 32.0F, "dot_f32 returned the wrong dot product");
}

void test_dot_f32_rejects_size_mismatch()
{
    bool rejected = false;
    try {
        std::vector<float> const left{1.0F};
        std::vector<float> const right{1.0F, 2.0F};
        static_cast<void>(lab00::dot_f32(left, right));
    } catch (std::invalid_argument const&) {
        rejected = true;
    }

    require(rejected, "dot_f32 should reject mismatched input sizes");
}

void test_fill_and_copy_u8()
{
    std::vector<std::uint8_t> input{1, 2, 3, 4};
    std::vector<std::uint8_t> output(input.size());

    static_cast<void>(lab00::fill_u8(output, 9));
    for (std::uint8_t value : output) {
        require(value == 9, "fill_u8 did not fill every byte");
    }

    static_cast<void>(lab00::copy_u8(input, output));
    require(output == input, "copy_u8 did not copy the input");
}

void test_foundation_benchmarks_are_registered()
{
    std::vector<lab00::BenchmarkCase> const benchmarks = lab00::make_foundation_benchmarks();
    require(benchmarks.size() == 5, "make_foundation_benchmarks should register five cases");
}

void test_benchmark_runner_records_measured_iterations_only()
{
    lab00::BenchmarkConfig config;
    config.warmup_iterations = 1;
    config.measured_iterations = 2;
    config.input_sizes = {8};

    std::size_t calls = 0;
    lab00::BenchmarkRunner runner(config);
    runner.add(lab00::BenchmarkCase{
        .name = "test_case",
        .make_workload = [&calls](std::size_t input_size) {
            return [&calls, input_size] {
                ++calls;
                return static_cast<std::uint64_t>(input_size + calls);
            };
        },
    });

    std::vector<lab00::BenchmarkResult> const results = runner.run();
    require(results.size() == 2, "runner should record measured iterations only");
    require(calls == 3, "runner should execute warmup plus measured iterations");
    require(results[0].name == "test_case", "runner should preserve benchmark name");
    require(results[0].input_size == 8, "runner should preserve input size");
}

void test_benchmark_runner_only_filter()
{
    lab00::BenchmarkConfig config;
    config.input_sizes = {4};
    config.measured_iterations = 1;
    config.only_name = "selected";

    lab00::BenchmarkRunner runner(config);
    runner.add(lab00::BenchmarkCase{
        .name = "skipped",
        .make_workload = [](std::size_t) {
            return [] {
                return std::uint64_t{1};
            };
        },
    });
    runner.add(lab00::BenchmarkCase{
        .name = "selected",
        .make_workload = [](std::size_t) {
            return [] {
                return std::uint64_t{2};
            };
        },
    });

    std::vector<lab00::BenchmarkResult> const results = runner.run();
    require(results.size() == 1, "only filter should keep one benchmark");
    require(results[0].name == "selected", "only filter should keep the selected benchmark");
}

void test_write_csv()
{
    std::vector<lab00::BenchmarkResult> const results{
        lab00::BenchmarkResult{
            .name = "case",
            .input_size = 16,
            .iteration = 1,
            .elapsed_ns = 25,
            .checksum = 99,
        },
    };

    std::ostringstream output;
    lab00::BenchmarkRunner::write_csv(output, results);
    require(
        output.str() == "name,input_size,iteration,elapsed_ns,checksum\ncase,16,1,25,99\n",
        "write_csv should emit the expected CSV");
}

} // namespace

int main()
{
    try {
        test_parse_size_list();
        test_parse_size_list_rejects_zero();
        test_sum_i32();
        test_sum_f32();
        test_dot_f32();
        test_dot_f32_rejects_size_mismatch();
        test_fill_and_copy_u8();
        test_foundation_benchmarks_are_registered();
        test_benchmark_runner_records_measured_iterations_only();
        test_benchmark_runner_only_filter();
        test_write_csv();
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "lab00_tests: " << error.what() << '\n';
        return 1;
    }
}
