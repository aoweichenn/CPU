#include <lcqi/int8_kernels.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::int32_t LCQI_DEFAULT_REPEAT = 200;
constexpr int LCQI_REPEAT_ARG_INDEX = 1;

std::int32_t parse_repeat(int argc, char** argv) {
    if (argc <= LCQI_REPEAT_ARG_INDEX) {
        return LCQI_DEFAULT_REPEAT;
    }
    const std::int32_t repeat = static_cast<std::int32_t>(std::stoi(argv[LCQI_REPEAT_ARG_INDEX]));
    if (repeat <= 0) {
        throw std::runtime_error("repeat must be positive");
    }
    return repeat;
}

std::vector<lcqi::KernelBenchmarkCase> make_cases(std::int32_t repeat) {
    return {
        {128, 128, 16, repeat},
        {256, 256, 16, repeat},
        {512, 512, 16, repeat},
        {513, 257, 16, repeat},
        {1024, 4096, 16, std::max(1, repeat / 4)},
    };
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::int32_t repeat = parse_repeat(argc, argv);
        std::cout
            << "input_size,output_size,output_block,repeat,scalar_us,packed_us,max_abs_diff,"
               "scalar_checksum,packed_checksum\n";
        for (const lcqi::KernelBenchmarkCase& benchmark_case : make_cases(repeat)) {
            const lcqi::KernelBenchmarkResult result =
                lcqi::benchmark_linear_i8_case(benchmark_case);
            std::cout << result.benchmark_case.input_size << ','
                      << result.benchmark_case.output_size << ','
                      << result.benchmark_case.output_block_size << ','
                      << result.benchmark_case.repeat << ','
                      << result.scalar_average_us << ','
                      << result.packed_average_us << ','
                      << result.max_abs_diff << ','
                      << result.scalar_checksum << ','
                      << result.packed_checksum << '\n';
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
