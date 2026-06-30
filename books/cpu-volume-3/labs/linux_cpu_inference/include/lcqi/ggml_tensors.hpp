#pragma once

#include <lcqi/gguf.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace lcqi {

constexpr std::int64_t LCQI_Q8_0_BLOCK_VALUES = 32;
constexpr std::int64_t LCQI_Q8_0_BLOCK_BYTES = 34;
constexpr std::int64_t LCQI_Q5_0_BLOCK_VALUES = 32;
constexpr std::int64_t LCQI_Q5_0_BLOCK_BYTES = 22;
constexpr std::int64_t LCQI_Q6_K_BLOCK_VALUES = 256;
constexpr std::int64_t LCQI_Q6_K_BLOCK_BYTES = 210;

[[nodiscard]] float ggml_f16_to_f32(std::uint16_t bits);

[[nodiscard]] std::vector<float> dequantize_ggml_tensor(
    GgmlType type,
    std::span<const std::uint8_t> bytes,
    std::int64_t element_count);

void dequantize_ggml_tensor_to(GgmlType type,
                               std::span<const std::uint8_t> bytes,
                               std::span<float> output);

}  // namespace lcqi
