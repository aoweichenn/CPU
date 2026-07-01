#include <lcqi/q5_0_packed.hpp>

#if defined(LCQI_ENABLE_AVX2)

#include <immintrin.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace lcqi {
namespace {

constexpr std::int32_t LCQI_Q5_0_PACKED_ZERO_POINT = 16;
constexpr const char* LCQI_Q5_0_PACKED_BACKEND_ENV = "LCQI_Q5_0_PACKED_BACKEND";
constexpr const char* LCQI_Q5_0_PACKED_FORCE_SCALAR_ENV =
    "LCQI_Q5_0_PACKED_FORCE_SCALAR";
constexpr const char* LCQI_Q5_0_PACKED_BACKEND_SCALAR = "scalar";
constexpr const char* LCQI_Q5_0_PACKED_FORCE_TRUE = "1";
constexpr std::int64_t LCQI_Q5_0_PACKED_BATCH_WIDE = 8;
constexpr std::int64_t LCQI_Q5_0_PACKED_BATCH_NARROW = 4;

[[nodiscard]] float horizontal_sum_ps(__m256 values) noexcept {
    const __m128 low = _mm256_castps256_ps128(values);
    const __m128 high = _mm256_extractf128_ps(values, 1);
    const __m128 pair_sum = _mm_add_ps(low, high);
    const __m128 first_fold = _mm_hadd_ps(pair_sum, pair_sum);
    const __m128 second_fold = _mm_hadd_ps(first_fold, first_fold);
    return _mm_cvtss_f32(second_fold);
}

[[nodiscard]] __m256i dot_lanes_loaded_32_u8_i8_avx2(__m256i lhs_values,
                                                     const std::int8_t* rhs) noexcept {
    const __m256i rhs_values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs));
    const __m256i products_i16 = _mm256_maddubs_epi16(lhs_values, rhs_values);
    const __m256i ones = _mm256_set1_epi16(1);
    return _mm256_madd_epi16(products_i16, ones);
}

[[nodiscard]] float zero_point_correction(const Q5_0PackedBlock& block,
                                          const Q8_0InputBlock& input) noexcept {
    return -static_cast<float>(LCQI_Q5_0_PACKED_ZERO_POINT) *
           block.d *
           input.d *
           static_cast<float>(input.sum);
}

void accumulate_q5_0_packed_q8_0_lanes_avx2(__m256i weight_values,
                                            const Q5_0PackedBlock& block,
                                            const Q8_0InputBlock& input,
                                            __m256& lane_sum,
                                            float& correction_sum) noexcept {
    const __m256i weighted_lanes =
        dot_lanes_loaded_32_u8_i8_avx2(weight_values, input.qs.data());
    const __m256 scale = _mm256_set1_ps(block.d * input.d);
    lane_sum =
        _mm256_add_ps(lane_sum, _mm256_mul_ps(_mm256_cvtepi32_ps(weighted_lanes), scale));
    correction_sum += zero_point_correction(block, input);
}

template <std::int64_t BATCH_COUNT>
void accumulate_q5_0_packed_q8_0_batch_block_avx2(const Q5_0PackedBlock& block,
                                                  const Q8_0InputBlock* batch_input,
                                                  std::int64_t input_stride,
                                                  __m256* lane_sums,
                                                  float* correction_sums) noexcept {
    const __m256i weight_values =
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(block.qs.data()));
    for (std::int64_t batch = 0; batch < BATCH_COUNT; ++batch) {
        const Q8_0InputBlock& input = batch_input[batch * input_stride];
        accumulate_q5_0_packed_q8_0_lanes_avx2(
            weight_values,
            block,
            input,
            lane_sums[batch],
            correction_sums[batch]);
    }
}

template <std::int64_t BATCH_COUNT>
void matvec_q5_0_packed_q8_0_avx2_batch_group_unchecked(
    const Q5_0PackedBlock* row_blocks_begin,
    std::int64_t row_blocks,
    const Q8_0InputBlock* batch_input,
    std::int64_t input_stride,
    float* output,
    std::int64_t output_stride,
    std::int64_t row) noexcept {
    __m256 lane_sums[static_cast<std::size_t>(BATCH_COUNT)];
    float correction_sums[static_cast<std::size_t>(BATCH_COUNT)]{};
    for (std::int64_t batch = 0; batch < BATCH_COUNT; ++batch) {
        lane_sums[batch] = _mm256_setzero_ps();
    }
    for (std::int64_t block = 0; block < row_blocks; ++block) {
        accumulate_q5_0_packed_q8_0_batch_block_avx2<BATCH_COUNT>(
            row_blocks_begin[static_cast<std::size_t>(block)],
            batch_input + block,
            input_stride,
            lane_sums,
            correction_sums);
    }
    for (std::int64_t batch = 0; batch < BATCH_COUNT; ++batch) {
        output[batch * output_stride + row] =
            horizontal_sum_ps(lane_sums[batch]) + correction_sums[batch];
    }
}

[[nodiscard]] bool scalar_backend_forced() noexcept {
    const char* backend = std::getenv(LCQI_Q5_0_PACKED_BACKEND_ENV);
    if (backend != nullptr && std::strcmp(backend, LCQI_Q5_0_PACKED_BACKEND_SCALAR) == 0) {
        return true;
    }
    const char* force_scalar = std::getenv(LCQI_Q5_0_PACKED_FORCE_SCALAR_ENV);
    return force_scalar != nullptr &&
           std::strcmp(force_scalar, LCQI_Q5_0_PACKED_FORCE_TRUE) == 0;
}

}  // namespace

bool q5_0_packed_q8_0_avx2_available() noexcept {
    if (scalar_backend_forced()) {
        return false;
    }
#if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
#else
    return true;
#endif
}

const char* q5_0_packed_q8_0_active_backend() noexcept {
    return q5_0_packed_q8_0_avx2_available() ? "avx2_maddubs" : "scalar";
}

void matvec_q5_0_packed_q8_0_avx2_rows_unchecked(
    std::span<const Q5_0PackedBlock> rows,
    std::int64_t row_count,
    std::int64_t column_count,
    std::span<const Q8_0InputBlock> input,
    std::span<float> output,
    std::int64_t row_begin,
    std::int64_t row_end) noexcept {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q5_0_BLOCK_VALUES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        __m256 lane_sum = _mm256_setzero_ps();
        float correction_sum = 0.0F;
        const Q5_0PackedBlock* row_blocks_begin =
            rows.data() + static_cast<std::size_t>(row * row_blocks);
        for (std::int64_t block = 0; block < row_blocks; ++block) {
            const Q5_0PackedBlock& packed_block =
                row_blocks_begin[static_cast<std::size_t>(block)];
            const __m256i weight_values =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(packed_block.qs.data()));
            accumulate_q5_0_packed_q8_0_lanes_avx2(weight_values,
                                                   packed_block,
                                                   input[static_cast<std::size_t>(block)],
                                                   lane_sum,
                                                   correction_sum);
        }
        output[static_cast<std::size_t>(row)] = horizontal_sum_ps(lane_sum) + correction_sum;
    }
}

void matvec_q5_0_packed_q8_0_avx2_batch_rows_unchecked(
    std::span<const Q5_0PackedBlock> rows,
    std::int64_t row_count,
    std::int64_t column_count,
    std::span<const Q8_0InputBlock> input,
    std::int64_t batch_size,
    std::span<float> output,
    std::int64_t output_stride,
    std::int64_t row_begin,
    std::int64_t row_end) noexcept {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_Q5_0_BLOCK_VALUES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        const Q5_0PackedBlock* row_blocks_begin =
            rows.data() + static_cast<std::size_t>(row * row_blocks);
        std::int64_t batch = 0;
        for (; batch + LCQI_Q5_0_PACKED_BATCH_WIDE <= batch_size;
             batch += LCQI_Q5_0_PACKED_BATCH_WIDE) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            matvec_q5_0_packed_q8_0_avx2_batch_group_unchecked
                <LCQI_Q5_0_PACKED_BATCH_WIDE>(row_blocks_begin,
                                              row_blocks,
                                              batch_input,
                                              row_blocks,
                                              output.data() +
                                                  static_cast<std::size_t>(
                                                      batch * output_stride),
                                              output_stride,
                                              row);
        }
        for (; batch + LCQI_Q5_0_PACKED_BATCH_NARROW <= batch_size;
             batch += LCQI_Q5_0_PACKED_BATCH_NARROW) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            matvec_q5_0_packed_q8_0_avx2_batch_group_unchecked
                <LCQI_Q5_0_PACKED_BATCH_NARROW>(row_blocks_begin,
                                                row_blocks,
                                                batch_input,
                                                row_blocks,
                                                output.data() +
                                                    static_cast<std::size_t>(
                                                        batch * output_stride),
                                                output_stride,
                                                row);
        }
        const std::int64_t remaining = batch_size - batch;
        if (remaining == 3) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            matvec_q5_0_packed_q8_0_avx2_batch_group_unchecked<3>(row_blocks_begin,
                                                                  row_blocks,
                                                                  batch_input,
                                                                  row_blocks,
                                                                  output.data() +
                                                                      static_cast<std::size_t>(
                                                                          batch * output_stride),
                                                                  output_stride,
                                                                  row);
            continue;
        }
        if (remaining == 2) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            matvec_q5_0_packed_q8_0_avx2_batch_group_unchecked<2>(row_blocks_begin,
                                                                  row_blocks,
                                                                  batch_input,
                                                                  row_blocks,
                                                                  output.data() +
                                                                      static_cast<std::size_t>(
                                                                          batch * output_stride),
                                                                  output_stride,
                                                                  row);
            continue;
        }
        if (remaining == 1) {
            const Q8_0InputBlock* batch_input =
                input.data() + static_cast<std::size_t>(batch * row_blocks);
            matvec_q5_0_packed_q8_0_avx2_batch_group_unchecked<1>(row_blocks_begin,
                                                                  row_blocks,
                                                                  batch_input,
                                                                  row_blocks,
                                                                  output.data() +
                                                                      static_cast<std::size_t>(
                                                                          batch * output_stride),
                                                                  output_stride,
                                                                  row);
        }
    }
}

}  // namespace lcqi

#endif
