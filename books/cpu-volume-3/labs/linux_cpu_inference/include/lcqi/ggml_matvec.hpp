#pragma once

#include <lcqi/gguf.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace lcqi {

constexpr std::int64_t LCQI_Q8_0_INPUT_BLOCK_VALUES = 32;

struct Q8_0InputBlock {
    float d = 0.0F;
    std::array<std::int8_t, LCQI_Q8_0_INPUT_BLOCK_VALUES> qs{};
    std::int16_t sum = 0;
};

[[nodiscard]] bool ggml_type_has_f32_direct_matvec(GgmlType type) noexcept;

[[nodiscard]] bool ggml_type_has_q8_0_direct_matvec(GgmlType type) noexcept;

[[nodiscard]] std::vector<Q8_0InputBlock> quantize_q8_0_input(std::span<const float> input);

void quantize_q8_0_input_to(std::span<const float> input,
                            std::span<Q8_0InputBlock> output);

void matvec_ggml_quantized_f32(GgmlType type,
                               std::span<const std::uint8_t> rows,
                               std::int64_t row_count,
                               std::int64_t column_count,
                               std::span<const float> input,
                               std::span<float> output);

void matvec_ggml_quantized_q8_0(GgmlType type,
                                std::span<const std::uint8_t> rows,
                                std::int64_t row_count,
                                std::int64_t column_count,
                                std::span<const Q8_0InputBlock> input,
                                std::span<float> output);

void matvec_ggml_quantized_q8_0_rows_unchecked(GgmlType type,
                                               std::span<const std::uint8_t> rows,
                                               std::int64_t row_count,
                                               std::int64_t column_count,
                                               std::span<const Q8_0InputBlock> input,
                                               std::span<float> output,
                                               std::int64_t row_begin,
                                               std::int64_t row_end);

}  // namespace lcqi
