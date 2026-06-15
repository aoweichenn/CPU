#include <lab00/benchmark.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <ostream>
#include <stdexcept>
#include <string>

namespace lab00 {
namespace {

[[nodiscard]] std::string_view trim_ascii_space(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }

    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }

    return text;
}

[[nodiscard]] std::size_t parse_size_token(std::string_view token, std::string_view field_name)
{
    token = trim_ascii_space(token);
    if (token.empty()) {
        throw std::invalid_argument(std::string(field_name) + " contains an empty value");
    }

    std::size_t value = 0;
    auto const* const begin = token.data();
    auto const* const end = token.data() + token.size();
    auto const [position, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || position != end || value == 0) {
        throw std::invalid_argument(std::string(field_name) + " must contain positive integer values");
    }

    return value;
}

[[nodiscard]] std::int64_t elapsed_nanoseconds(
    std::chrono::steady_clock::time_point begin,
    std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
}

} // namespace

BenchmarkRunner::BenchmarkRunner(BenchmarkConfig config)
    : config_(std::move(config))
{
    if (this->config_.input_sizes.empty()) {
        this->config_.input_sizes = default_input_sizes();
    }
}

void BenchmarkRunner::add(BenchmarkCase benchmark_case)
{
    this->cases_.push_back(std::move(benchmark_case));
}

std::span<const BenchmarkCase> BenchmarkRunner::cases() const noexcept
{
    return this->cases_;
}

std::vector<BenchmarkResult> BenchmarkRunner::run() const
{
    std::vector<BenchmarkResult> results;

    for (auto const& benchmark_case : this->cases_) {
        if (!this->should_run(benchmark_case.name)) {
            continue;
        }

        for (std::size_t input_size : this->config_.input_sizes) {
            BenchmarkWorkload workload = benchmark_case.make_workload(input_size);

            for (std::size_t warmup = 0; warmup < this->config_.warmup_iterations; ++warmup) {
                std::uint64_t const checksum = workload();
                do_not_optimize(checksum);
            }

            for (std::size_t iteration = 0; iteration < this->config_.measured_iterations; ++iteration) {
                auto const begin = std::chrono::steady_clock::now();
                std::uint64_t const checksum = workload();
                auto const end = std::chrono::steady_clock::now();

                do_not_optimize(checksum);

                results.push_back(BenchmarkResult{
                    .name = benchmark_case.name,
                    .input_size = input_size,
                    .iteration = iteration,
                    .elapsed_ns = elapsed_nanoseconds(begin, end),
                    .checksum = checksum,
                });
            }
        }
    }

    return results;
}

void BenchmarkRunner::write_csv(std::ostream& output, std::span<const BenchmarkResult> results)
{
    output << "name,input_size,iteration,elapsed_ns,checksum\n";
    for (auto const& result : results) {
        output << result.name << ','
               << result.input_size << ','
               << result.iteration << ','
               << result.elapsed_ns << ','
               << result.checksum << '\n';
    }
}

bool BenchmarkRunner::should_run(std::string_view name) const
{
    return this->config_.only_name.empty() || this->config_.only_name == name;
}

std::vector<std::size_t> default_input_sizes()
{
    return {
        LAB00_SMALL_INPUT_SIZE,
        LAB00_MEDIUM_INPUT_SIZE,
        LAB00_LARGE_INPUT_SIZE,
    };
}

std::vector<std::size_t> parse_size_list(std::string_view text)
{
    std::vector<std::size_t> sizes;
    std::size_t begin = 0;

    while (begin <= text.size()) {
        std::size_t const delimiter = text.find(LAB00_SIZE_LIST_DELIMITER, begin);
        std::size_t const end = delimiter == std::string_view::npos ? text.size() : delimiter;
        std::string_view const token = text.substr(begin, end - begin);
        sizes.push_back(parse_size_token(token, "size list"));

        if (delimiter == std::string_view::npos) {
            break;
        }

        begin = delimiter + 1;
    }

    if (sizes.empty()) {
        throw std::invalid_argument("size list must not be empty");
    }

    return sizes;
}

std::size_t parse_positive_count(std::string_view text, std::string_view field_name)
{
    return parse_size_token(text, field_name);
}

} // namespace lab00
