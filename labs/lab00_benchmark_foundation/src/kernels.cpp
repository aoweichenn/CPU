#include <lab00/kernels.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace lab00 {
namespace {

inline constexpr std::int32_t LAB00_I32_PATTERN_MULTIPLIER = 17;
inline constexpr std::int32_t LAB00_I32_PATTERN_OFFSET = 11;
inline constexpr std::int32_t LAB00_I32_PATTERN_MODULUS = 257;
inline constexpr std::int32_t LAB00_I32_PATTERN_CENTER = 128;

inline constexpr float LAB00_F32_PATTERN_SCALE = 0.25F;
inline constexpr float LAB00_F32_PATTERN_BIAS = 3.0F;
inline constexpr float LAB00_F32_PATTERN_PERIOD = 31.0F;

inline constexpr std::uint8_t LAB00_FILL_BYTE = 0xA5U;
inline constexpr std::uint8_t LAB00_U8_PATTERN_MULTIPLIER = 37U;
inline constexpr std::uint8_t LAB00_U8_PATTERN_OFFSET = 19U;

[[nodiscard]] std::vector<std::int32_t> make_i32_input(std::size_t input_size)
{
    std::vector<std::int32_t> values(input_size);
    for (std::size_t index = 0; index < values.size(); ++index) {
        auto const raw = static_cast<std::int32_t>(
            (index * LAB00_I32_PATTERN_MULTIPLIER + LAB00_I32_PATTERN_OFFSET)
            % LAB00_I32_PATTERN_MODULUS);
        values[index] = raw - LAB00_I32_PATTERN_CENTER;
    }
    return values;
}

[[nodiscard]] std::vector<float> make_f32_input(std::size_t input_size, float phase)
{
    std::vector<float> values(input_size);
    for (std::size_t index = 0; index < values.size(); ++index) {
        float const wrapped = static_cast<float>(index % static_cast<std::size_t>(LAB00_F32_PATTERN_PERIOD));
        values[index] = (wrapped - LAB00_F32_PATTERN_BIAS + phase) * LAB00_F32_PATTERN_SCALE;
    }
    return values;
}

[[nodiscard]] std::vector<std::uint8_t> make_u8_input(std::size_t input_size)
{
    std::vector<std::uint8_t> values(input_size);
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(index) * LAB00_U8_PATTERN_MULTIPLIER + LAB00_U8_PATTERN_OFFSET);
    }
    return values;
}

[[nodiscard]] std::uint64_t checksum_u8_sample(std::span<const std::uint8_t> values)
{
    if (values.empty()) {
        return 0;
    }

    std::uint64_t checksum = values.size();
    checksum ^= static_cast<std::uint64_t>(values.front());
    checksum ^= static_cast<std::uint64_t>(values[values.size() / 2]) << 8U;
    checksum ^= static_cast<std::uint64_t>(values.back()) << 16U;
    return checksum;
}

[[nodiscard]] BenchmarkCase make_case(std::string name, BenchmarkFactory factory)
{
    return BenchmarkCase{
        .name = std::move(name),
        .make_workload = std::move(factory),
    };
}

} // namespace

std::int64_t sum_i32(std::span<const std::int32_t> values)
{
    std::int64_t sum = 0;
    for (std::int32_t value : values) {
        sum += value;
    }
    return sum;
}

float sum_f32(std::span<const float> values)
{
    float sum = 0.0F;
    for (float value : values) {
        sum += value;
    }
    return sum;
}

float dot_f32(std::span<const float> left, std::span<const float> right)
{
    if (left.size() != right.size()) {
        throw std::invalid_argument("dot_f32 requires equal input sizes");
    }

    float sum = 0.0F;
    for (std::size_t index = 0; index < left.size(); ++index) {
        sum += left[index] * right[index];
    }
    return sum;
}

std::uint64_t fill_u8(std::span<std::uint8_t> output, std::uint8_t value)
{
    std::ranges::fill(output, value);
    clobber_memory();
    return checksum_u8_sample(output);
}

std::uint64_t copy_u8(std::span<const std::uint8_t> input, std::span<std::uint8_t> output)
{
    if (input.size() != output.size()) {
        throw std::invalid_argument("copy_u8 requires equal input sizes");
    }

    std::ranges::copy(input, output.begin());
    clobber_memory();
    return checksum_u8_sample(output);
}

std::uint64_t checksum_i64(std::int64_t value) noexcept
{
    return static_cast<std::uint64_t>(value);
}

std::uint64_t checksum_f32(float value) noexcept
{
    return std::bit_cast<std::uint32_t>(value);
}

std::vector<BenchmarkCase> make_foundation_benchmarks()
{
    std::vector<BenchmarkCase> benchmarks;

    benchmarks.push_back(make_case("sum_i32", [](std::size_t input_size) {
        return [values = make_i32_input(input_size)]() -> std::uint64_t {
            return checksum_i64(sum_i32(values));
        };
    }));

    benchmarks.push_back(make_case("sum_f32", [](std::size_t input_size) {
        return [values = make_f32_input(input_size, 0.0F)]() -> std::uint64_t {
            return checksum_f32(sum_f32(values));
        };
    }));

    benchmarks.push_back(make_case("dot_f32", [](std::size_t input_size) {
        return [
            left = make_f32_input(input_size, 0.0F),
            right = make_f32_input(input_size, 1.0F)
        ]() -> std::uint64_t {
            return checksum_f32(dot_f32(left, right));
        };
    }));

    benchmarks.push_back(make_case("fill_u8", [](std::size_t input_size) {
        return [output = std::vector<std::uint8_t>(input_size)]() mutable -> std::uint64_t {
            return fill_u8(output, LAB00_FILL_BYTE);
        };
    }));

    benchmarks.push_back(make_case("copy_u8", [](std::size_t input_size) {
        return [
            input = make_u8_input(input_size),
            output = std::vector<std::uint8_t>(input_size)
        ]() mutable -> std::uint64_t {
            return copy_u8(input, output);
        };
    }));

    return benchmarks;
}

} // namespace lab00
