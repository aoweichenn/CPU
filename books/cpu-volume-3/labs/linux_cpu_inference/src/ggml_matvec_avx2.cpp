#include <lcqi/ggml_matvec.hpp>

#if defined(LCQI_ENABLE_AVX2)

#include <lcqi/ggml_tensors.hpp>

#include <immintrin.h>

#include <array>
#include <cstdint>
#include <limits>
#include <span>

namespace lcqi {
namespace {

constexpr std::uint32_t LCQI_GGML_AVX2_BYTE_BITS = 8U;
constexpr std::uint8_t LCQI_GGML_AVX2_LOW_NIBBLE_MASK = 0x0FU;
constexpr std::int32_t LCQI_GGML_AVX2_Q5_ZERO_POINT = 16;
constexpr std::int64_t LCQI_GGML_AVX2_Q8_0_BLOCK_BYTES = 34;
constexpr std::int64_t LCQI_GGML_AVX2_Q5_0_BLOCK_BYTES = 22;

[[nodiscard]] std::uint16_t read_le_u16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << LCQI_GGML_AVX2_BYTE_BITS));
}

[[nodiscard]] std::uint32_t read_le_u32(const std::uint8_t* bytes) noexcept {
    std::uint32_t value = 0;
    for (std::uint32_t index = 0; index < sizeof(std::uint32_t); ++index) {
        value |= static_cast<std::uint32_t>(bytes[index]) <<
                 (LCQI_GGML_AVX2_BYTE_BITS * index);
    }
    return value;
}

[[nodiscard]] std::int32_t horizontal_sum_i32(__m256i values) noexcept {
    const __m128i low = _mm256_castsi256_si128(values);
    const __m128i high = _mm256_extracti128_si256(values, 1);
    __m128i sum = _mm_add_epi32(low, high);
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2)));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(sum);
}

[[nodiscard]] std::int32_t dot_32_i8_i8_avx2(const std::int8_t* lhs,
                                             const std::int8_t* rhs) noexcept {
    const __m256i lhs_values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
    const __m256i rhs_values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
    const __m256i absolute_lhs = _mm256_sign_epi8(lhs_values, lhs_values);
    const __m256i signed_rhs = _mm256_sign_epi8(rhs_values, lhs_values);
    const __m256i products_i16 = _mm256_maddubs_epi16(absolute_lhs, signed_rhs);
    const __m256i ones = _mm256_set1_epi16(1);
    const __m256i partial_i32 = _mm256_madd_epi16(products_i16, ones);
    return horizontal_sum_i32(partial_i32);
}

[[nodiscard]] std::int32_t dot_32_u8_i8_avx2(const std::uint8_t* lhs,
                                             const std::int8_t* rhs) noexcept {
    const __m256i lhs_values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs));
    const __m256i rhs_values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
    const __m256i products_i16 = _mm256_maddubs_epi16(lhs_values, rhs_values);
    const __m256i ones = _mm256_set1_epi16(1);
    const __m256i partial_i32 = _mm256_madd_epi16(products_i16, ones);
    return horizontal_sum_i32(partial_i32);
}

[[nodiscard]] float dot_q8_0_block_q8_0_avx2(const std::uint8_t* block,
                                             const Q8_0InputBlock& input) noexcept {
    const float d = ggml_f16_to_f32(read_le_u16(block));
    const auto* qs = reinterpret_cast<const std::int8_t*>(block + sizeof(std::uint16_t));
    return d * input.d * static_cast<float>(dot_32_i8_i8_avx2(qs, input.qs.data()));
}

[[nodiscard]] float dot_q5_0_block_q8_0_avx2(const std::uint8_t* block,
                                             const Q8_0InputBlock& input) noexcept {
    const float d = ggml_f16_to_f32(read_le_u16(block));
    const std::uint32_t qh = read_le_u32(block + sizeof(std::uint16_t));
    const std::uint8_t* qs = block + sizeof(std::uint16_t) + sizeof(std::uint32_t);

    alignas(32) std::array<std::uint8_t, LCQI_Q8_0_INPUT_BLOCK_VALUES> values{};
    for (std::int32_t index = 0; index < LCQI_Q8_0_INPUT_BLOCK_VALUES / 2; ++index) {
        const std::uint8_t high_0 =
            static_cast<std::uint8_t>(((qh >> index) & 1U) << 4U);
        const std::uint8_t high_1 = static_cast<std::uint8_t>(
            ((qh >> (index + LCQI_Q8_0_INPUT_BLOCK_VALUES / 2)) & 1U) << 4U);
        values[static_cast<std::size_t>(index)] =
            static_cast<std::uint8_t>((qs[index] & LCQI_GGML_AVX2_LOW_NIBBLE_MASK) | high_0);
        values[static_cast<std::size_t>(index + LCQI_Q8_0_INPUT_BLOCK_VALUES / 2)] =
            static_cast<std::uint8_t>((qs[index] >> 4U) | high_1);
    }

    const std::int32_t weighted_sum =
        dot_32_u8_i8_avx2(values.data(), input.qs.data()) -
        LCQI_GGML_AVX2_Q5_ZERO_POINT * static_cast<std::int32_t>(input.sum);
    return d * input.d * static_cast<float>(weighted_sum);
}

void matvec_q8_0_q8_0_avx2_unchecked(std::span<const std::uint8_t> rows,
                                     std::int64_t row_count,
                                     std::int64_t column_count,
                                     std::span<const Q8_0InputBlock> input,
                                     std::span<float> output,
                                     std::int64_t row_begin,
                                     std::int64_t row_end) {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES;
    const std::int64_t row_bytes = row_blocks * LCQI_GGML_AVX2_Q8_0_BLOCK_BYTES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        float sum = 0.0F;
        const std::uint8_t* row_data = rows.data() + row * row_bytes;
        for (std::int64_t block = 0; block < row_blocks; ++block) {
            sum += dot_q8_0_block_q8_0_avx2(
                row_data + block * LCQI_GGML_AVX2_Q8_0_BLOCK_BYTES,
                input[static_cast<std::size_t>(block)]);
        }
        output[static_cast<std::size_t>(row)] = sum;
    }
}

void matvec_q8_0_q8_0_avx2_batch_unchecked(std::span<const std::uint8_t> rows,
                                           std::int64_t row_count,
                                           std::int64_t column_count,
                                           std::span<const Q8_0InputBlock> input,
                                           std::int64_t batch_size,
                                           std::span<float> output,
                                           std::int64_t output_stride,
                                           std::int64_t row_begin,
                                           std::int64_t row_end) {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES;
    const std::int64_t row_bytes = row_blocks * LCQI_GGML_AVX2_Q8_0_BLOCK_BYTES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        const std::uint8_t* row_data = rows.data() + row * row_bytes;
        for (std::int64_t batch = 0; batch < batch_size; ++batch) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            float sum = 0.0F;
            for (std::int64_t block = 0; block < row_blocks; ++block) {
                sum += dot_q8_0_block_q8_0_avx2(
                    row_data + block * LCQI_GGML_AVX2_Q8_0_BLOCK_BYTES,
                    batch_input[static_cast<std::size_t>(block)]);
            }
            output[static_cast<std::size_t>(batch * output_stride + row)] = sum;
        }
    }
}

void matvec_q5_0_q8_0_avx2_unchecked(std::span<const std::uint8_t> rows,
                                     std::int64_t row_count,
                                     std::int64_t column_count,
                                     std::span<const Q8_0InputBlock> input,
                                     std::span<float> output,
                                     std::int64_t row_begin,
                                     std::int64_t row_end) {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES;
    const std::int64_t row_bytes = row_blocks * LCQI_GGML_AVX2_Q5_0_BLOCK_BYTES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        float sum = 0.0F;
        const std::uint8_t* row_data = rows.data() + row * row_bytes;
        for (std::int64_t block = 0; block < row_blocks; ++block) {
            sum += dot_q5_0_block_q8_0_avx2(
                row_data + block * LCQI_GGML_AVX2_Q5_0_BLOCK_BYTES,
                input[static_cast<std::size_t>(block)]);
        }
        output[static_cast<std::size_t>(row)] = sum;
    }
}

void matvec_q5_0_q8_0_avx2_batch_unchecked(std::span<const std::uint8_t> rows,
                                           std::int64_t row_count,
                                           std::int64_t column_count,
                                           std::span<const Q8_0InputBlock> input,
                                           std::int64_t batch_size,
                                           std::span<float> output,
                                           std::int64_t output_stride,
                                           std::int64_t row_begin,
                                           std::int64_t row_end) {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES;
    const std::int64_t row_bytes = row_blocks * LCQI_GGML_AVX2_Q5_0_BLOCK_BYTES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        const std::uint8_t* row_data = rows.data() + row * row_bytes;
        for (std::int64_t batch = 0; batch < batch_size; ++batch) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            float sum = 0.0F;
            for (std::int64_t block = 0; block < row_blocks; ++block) {
                sum += dot_q5_0_block_q8_0_avx2(
                    row_data + block * LCQI_GGML_AVX2_Q5_0_BLOCK_BYTES,
                    batch_input[static_cast<std::size_t>(block)]);
            }
            output[static_cast<std::size_t>(batch * output_stride + row)] = sum;
        }
    }
}

F32RowMax max_dot_q8_0_q8_0_avx2_rows_unchecked(std::span<const std::uint8_t> rows,
                                                std::int64_t row_count,
                                                std::int64_t column_count,
                                                std::span<const Q8_0InputBlock> input,
                                                std::int64_t row_begin,
                                                std::int64_t row_end) {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES;
    const std::int64_t row_bytes = row_blocks * LCQI_GGML_AVX2_Q8_0_BLOCK_BYTES;
    F32RowMax best;
    best.row = static_cast<std::size_t>(row_begin);
    best.value = -std::numeric_limits<float>::infinity();
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        const std::uint8_t* row_data = rows.data() + row * row_bytes;
        float sum = 0.0F;
        for (std::int64_t block = 0; block < row_blocks; ++block) {
            sum += dot_q8_0_block_q8_0_avx2(
                row_data + block * LCQI_GGML_AVX2_Q8_0_BLOCK_BYTES,
                input[static_cast<std::size_t>(block)]);
        }
        if (row == row_begin || sum > best.value) {
            best.row = static_cast<std::size_t>(row);
            best.value = sum;
        }
    }
    return best;
}

F32RowMax max_dot_q5_0_q8_0_avx2_rows_unchecked(std::span<const std::uint8_t> rows,
                                                std::int64_t row_count,
                                                std::int64_t column_count,
                                                std::span<const Q8_0InputBlock> input,
                                                std::int64_t row_begin,
                                                std::int64_t row_end) {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES;
    const std::int64_t row_bytes = row_blocks * LCQI_GGML_AVX2_Q5_0_BLOCK_BYTES;
    F32RowMax best;
    best.row = static_cast<std::size_t>(row_begin);
    best.value = -std::numeric_limits<float>::infinity();
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        const std::uint8_t* row_data = rows.data() + row * row_bytes;
        float sum = 0.0F;
        for (std::int64_t block = 0; block < row_blocks; ++block) {
            sum += dot_q5_0_block_q8_0_avx2(
                row_data + block * LCQI_GGML_AVX2_Q5_0_BLOCK_BYTES,
                input[static_cast<std::size_t>(block)]);
        }
        if (row == row_begin || sum > best.value) {
            best.row = static_cast<std::size_t>(row);
            best.value = sum;
        }
    }
    return best;
}

}  // namespace

bool ggml_q8_0_avx2_available() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
#else
    return true;
#endif
}

void matvec_ggml_quantized_q8_0_avx2_rows_unchecked(GgmlType type,
                                                    std::span<const std::uint8_t> rows,
                                                    std::int64_t row_count,
                                                    std::int64_t column_count,
                                                    std::span<const Q8_0InputBlock> input,
                                                    std::span<float> output,
                                                    std::int64_t row_begin,
                                                    std::int64_t row_end);

void matvec_ggml_quantized_q8_0_avx2_unchecked(GgmlType type,
                                               std::span<const std::uint8_t> rows,
                                               std::int64_t row_count,
                                               std::int64_t column_count,
                                               std::span<const Q8_0InputBlock> input,
                                               std::span<float> output) {
    matvec_ggml_quantized_q8_0_avx2_rows_unchecked(type,
                                                   rows,
                                                   row_count,
                                                   column_count,
                                                   input,
                                                   output,
                                                   0,
                                                   row_count);
}

void matvec_ggml_quantized_q8_0_avx2_rows_unchecked(GgmlType type,
                                                    std::span<const std::uint8_t> rows,
                                                    std::int64_t row_count,
                                                    std::int64_t column_count,
                                                    std::span<const Q8_0InputBlock> input,
                                                    std::span<float> output,
                                                    std::int64_t row_begin,
                                                    std::int64_t row_end) {
    if (type == GgmlType::q8_0) {
        matvec_q8_0_q8_0_avx2_unchecked(rows,
                                        row_count,
                                        column_count,
                                        input,
                                        output,
                                        row_begin,
                                        row_end);
        return;
    }
    if (type == GgmlType::q5_0) {
        matvec_q5_0_q8_0_avx2_unchecked(rows,
                                        row_count,
                                        column_count,
                                        input,
                                        output,
                                        row_begin,
                                        row_end);
    }
}

void matvec_ggml_quantized_q8_0_avx2_batch_rows_unchecked(
    GgmlType type,
    std::span<const std::uint8_t> rows,
    std::int64_t row_count,
    std::int64_t column_count,
    std::span<const Q8_0InputBlock> input,
    std::int64_t batch_size,
    std::span<float> output,
    std::int64_t output_stride,
    std::int64_t row_begin,
    std::int64_t row_end) {
    if (type == GgmlType::q8_0) {
        matvec_q8_0_q8_0_avx2_batch_unchecked(rows,
                                              row_count,
                                              column_count,
                                              input,
                                              batch_size,
                                              output,
                                              output_stride,
                                              row_begin,
                                              row_end);
        return;
    }
    if (type == GgmlType::q5_0) {
        matvec_q5_0_q8_0_avx2_batch_unchecked(rows,
                                              row_count,
                                              column_count,
                                              input,
                                              batch_size,
                                              output,
                                              output_stride,
                                              row_begin,
                                              row_end);
    }
}

F32RowMax max_dot_ggml_quantized_q8_0_avx2_rows_unchecked(
    GgmlType type,
    std::span<const std::uint8_t> rows,
    std::int64_t row_count,
    std::int64_t column_count,
    std::span<const Q8_0InputBlock> input,
    std::int64_t row_begin,
    std::int64_t row_end) {
    if (type == GgmlType::q8_0) {
        return max_dot_q8_0_q8_0_avx2_rows_unchecked(rows,
                                                     row_count,
                                                     column_count,
                                                     input,
                                                     row_begin,
                                                     row_end);
    }
    if (type == GgmlType::q5_0) {
        return max_dot_q5_0_q8_0_avx2_rows_unchecked(rows,
                                                     row_count,
                                                     column_count,
                                                     input,
                                                     row_begin,
                                                     row_end);
    }
    F32RowMax best;
    best.row = static_cast<std::size_t>(row_begin);
    best.value = -std::numeric_limits<float>::infinity();
    return best;
}

}  // namespace lcqi

#endif
