#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace lcqi {

constexpr std::int64_t LCQI_QK_K_BLOCK_VALUES = 256;
constexpr std::int64_t LCQI_Q4_K_BLOCK_BYTES = 144;
constexpr std::int64_t LCQI_Q8_K_BLOCK_BYTES = 292;
constexpr std::int64_t LCQI_Q4_K_SUBBLOCK_VALUES = 32;
constexpr std::int64_t LCQI_Q4_K_SUBBLOCKS = 8;
constexpr std::int64_t LCQI_Q8_K_BSUM_GROUPS = 16;

struct Q8KBlock {
    float d = 0.0F;
    std::array<std::int8_t, LCQI_QK_K_BLOCK_VALUES> qs{};
    std::array<std::int16_t, LCQI_Q8_K_BSUM_GROUPS> bsums{};
};

[[nodiscard]] std::vector<float> dequantize_q4_k(std::span<const std::uint8_t> bytes,
                                                 std::int64_t element_count);

void dequantize_q4_k_to(std::span<const std::uint8_t> bytes,
                        std::span<float> output);

[[nodiscard]] std::vector<Q8KBlock> quantize_q8_k_input(std::span<const float> input);

[[nodiscard]] bool q4_k_q8_avx2_available() noexcept;

[[nodiscard]] const char* q4_k_q8_active_backend() noexcept;

float dot_q4_k_f32(std::span<const std::uint8_t> q4_blocks,
                   std::span<const float> input);

float dot_q4_k_q8(std::span<const std::uint8_t> q4_blocks,
                  std::span<const Q8KBlock> input);

void matvec_q4_k_f32(std::span<const std::uint8_t> q4_rows,
                     std::int64_t row_count,
                     std::int64_t column_count,
                     std::span<const float> input,
                     std::span<float> output);

void matvec_q4_k_q8(std::span<const std::uint8_t> q4_rows,
                    std::int64_t row_count,
                    std::int64_t column_count,
                    std::span<const Q8KBlock> input,
                    std::span<float> output);

}  // namespace lcqi
