#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <lab00/benchmark.hpp>

namespace lab00 {

[[nodiscard]] std::int64_t sum_i32(std::span<const std::int32_t> values);
[[nodiscard]] float sum_f32(std::span<const float> values);
[[nodiscard]] float dot_f32(std::span<const float> left, std::span<const float> right);
[[nodiscard]] std::uint64_t fill_u8(std::span<std::uint8_t> output, std::uint8_t value);
[[nodiscard]] std::uint64_t copy_u8(std::span<const std::uint8_t> input, std::span<std::uint8_t> output);

[[nodiscard]] std::uint64_t checksum_i64(std::int64_t value) noexcept;
[[nodiscard]] std::uint64_t checksum_f32(float value) noexcept;

[[nodiscard]] std::vector<BenchmarkCase> make_foundation_benchmarks();

} // namespace lab00
