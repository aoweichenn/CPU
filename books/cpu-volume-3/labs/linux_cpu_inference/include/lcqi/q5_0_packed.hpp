#pragma once

#include <lcqi/ggml_matvec.hpp>
#include <lcqi/ggml_tensors.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace lcqi {

struct Q5_0PackedBlock {
    float d = 0.0F;
    std::array<std::uint8_t, LCQI_Q5_0_BLOCK_VALUES> qs{};
};

[[nodiscard]] std::vector<Q5_0PackedBlock> pack_q5_0_blocks(
    std::span<const std::uint8_t> bytes,
    std::int64_t element_count);

void pack_q5_0_blocks_to(std::span<const std::uint8_t> bytes,
                         std::span<Q5_0PackedBlock> output);

[[nodiscard]] bool q5_0_packed_q8_0_avx2_available() noexcept;

[[nodiscard]] const char* q5_0_packed_q8_0_active_backend() noexcept;

void matvec_q5_0_packed_q8_0(std::span<const Q5_0PackedBlock> rows,
                             std::int64_t row_count,
                             std::int64_t column_count,
                             std::span<const Q8_0InputBlock> input,
                             std::span<float> output);

void matvec_q5_0_packed_q8_0_rows_unchecked(std::span<const Q5_0PackedBlock> rows,
                                            std::int64_t row_count,
                                            std::int64_t column_count,
                                            std::span<const Q8_0InputBlock> input,
                                            std::span<float> output,
                                            std::int64_t row_begin,
                                            std::int64_t row_end) noexcept;

void matvec_q5_0_packed_q8_0_batch_rows_unchecked(
    std::span<const Q5_0PackedBlock> rows,
    std::int64_t row_count,
    std::int64_t column_count,
    std::span<const Q8_0InputBlock> input,
    std::int64_t batch_size,
    std::span<float> output,
    std::int64_t output_stride,
    std::int64_t row_begin,
    std::int64_t row_end) noexcept;

}  // namespace lcqi
