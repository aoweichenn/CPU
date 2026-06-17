#include <lab00/benchmark.hpp>
#include <lab00/kernels.hpp>

#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view LAB00_OPTION_HELP = "--help";
constexpr std::string_view LAB00_OPTION_LIST = "--list";
constexpr std::string_view LAB00_OPTION_CSV = "--csv";
constexpr std::string_view LAB00_OPTION_SIZES = "--sizes";
constexpr std::string_view LAB00_OPTION_WARMUP = "--warmup";
constexpr std::string_view LAB00_OPTION_ITERATIONS = "--iterations";
constexpr std::string_view LAB00_OPTION_ONLY = "--only";

struct CliOptions {
    lab00::BenchmarkConfig config;
    std::string csv_path;
    bool list_only = false;
};

void print_usage(std::ostream& output)
{
    output << "Usage: lab00_bench [options]\n"
           << "\n"
           << "Options:\n"
           << "  --help                 Show this help text.\n"
           << "  --list                 List available benchmarks.\n"
           << "  --csv <path>           Write CSV results to a file. Defaults to stdout.\n"
           << "  --sizes <a,b,c>        Input sizes. Example: 1024,16384,1048576.\n"
           << "  --warmup <count>       Warmup iterations per benchmark and size.\n"
           << "  --iterations <count>   Measured iterations per benchmark and size.\n"
           << "  --only <name>          Run one benchmark name from --list.\n";
}

[[nodiscard]] std::string_view require_value(
    std::span<char const* const> arguments,
    std::size_t& index,
    std::string_view option)
{
    if (index + 1 >= arguments.size()) {
        throw std::invalid_argument(std::string(option) + " requires a value");
    }

    ++index;
    return arguments[index];
}

[[nodiscard]] std::optional<CliOptions> parse_arguments(std::span<char const* const> arguments)
{
    CliOptions options;

    for (std::size_t index = 1; index < arguments.size(); ++index) {
        std::string_view const argument = arguments[index];

        if (argument == LAB00_OPTION_HELP) {
            print_usage(std::cout);
            return std::nullopt;
        }

        if (argument == LAB00_OPTION_LIST) {
            options.list_only = true;
            continue;
        }

        if (argument == LAB00_OPTION_CSV) {
            options.csv_path = require_value(arguments, index, LAB00_OPTION_CSV);
            continue;
        }

        if (argument == LAB00_OPTION_SIZES) {
            options.config.input_sizes = lab00::parse_size_list(
                require_value(arguments, index, LAB00_OPTION_SIZES));
            continue;
        }

        if (argument == LAB00_OPTION_WARMUP) {
            options.config.warmup_iterations = lab00::parse_positive_count(
                require_value(arguments, index, LAB00_OPTION_WARMUP),
                "warmup");
            continue;
        }

        if (argument == LAB00_OPTION_ITERATIONS) {
            options.config.measured_iterations = lab00::parse_positive_count(
                require_value(arguments, index, LAB00_OPTION_ITERATIONS),
                "iterations");
            continue;
        }

        if (argument == LAB00_OPTION_ONLY) {
            options.config.only_name = require_value(arguments, index, LAB00_OPTION_ONLY);
            continue;
        }

        throw std::invalid_argument("unknown option: " + std::string(argument));
    }

    return options;
}

void add_foundation_benchmarks(lab00::BenchmarkRunner& runner)
{
    for (auto benchmark_case : lab00::make_foundation_benchmarks()) {
        runner.add(std::move(benchmark_case));
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        auto const arguments = std::span<char const* const>(argv, static_cast<std::size_t>(argc));
        std::optional<CliOptions> const options = parse_arguments(arguments);
        if (!options.has_value()) {
            return 0;
        }

        lab00::BenchmarkRunner runner(options->config);
        add_foundation_benchmarks(runner);

        if (options->list_only) {
            for (auto const& benchmark_case : runner.cases()) {
                std::cout << benchmark_case.name << '\n';
            }
            return 0;
        }

        std::vector<lab00::BenchmarkResult> const results = runner.run();

        if (options->csv_path.empty()) {
            lab00::BenchmarkRunner::write_csv(std::cout, results);
            return 0;
        }

        std::ofstream output(options->csv_path);
        if (!output) {
            std::cerr << "failed to open CSV path: " << options->csv_path << '\n';
            return 1;
        }

        lab00::BenchmarkRunner::write_csv(output, results);
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "lab00_bench: " << error.what() << '\n';
        return 1;
    }
}
