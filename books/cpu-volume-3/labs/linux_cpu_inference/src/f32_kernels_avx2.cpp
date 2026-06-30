#include <lcqi/f32_kernels.hpp>

#if defined(LCQI_ENABLE_AVX2)

#include <immintrin.h>

#include <array>
#include <cstddef>
#include <limits>

namespace lcqi {
namespace {

constexpr std::size_t LCQI_F32_AVX2_LANE_COUNT = 8;
constexpr std::size_t LCQI_F32_AVX2_UNROLL_COUNT = 4;
constexpr std::size_t LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT = 8;
constexpr std::size_t LCQI_F32_AVX2_UNROLLED_FLOATS =
    LCQI_F32_AVX2_LANE_COUNT * LCQI_F32_AVX2_UNROLL_COUNT;

[[nodiscard]] float horizontal_sum(__m256 values) noexcept {
    const __m128 low = _mm256_castps256_ps128(values);
    const __m128 high = _mm256_extractf128_ps(values, 1);
    const __m128 pair_sum = _mm_add_ps(low, high);
    const __m128 first_fold = _mm_hadd_ps(pair_sum, pair_sum);
    const __m128 second_fold = _mm_hadd_ps(first_fold, first_fold);
    return _mm_cvtss_f32(second_fold);
}

void compute_four_row_dot(const float* row0,
                          const float* row1,
                          const float* row2,
                          const float* row3,
                          const float* input,
                          std::size_t input_size,
                          float* sums) noexcept {
    std::size_t index = 0;
    __m256 accumulator0 = _mm256_setzero_ps();
    __m256 accumulator1 = _mm256_setzero_ps();
    __m256 accumulator2 = _mm256_setzero_ps();
    __m256 accumulator3 = _mm256_setzero_ps();

    for (; index + LCQI_F32_AVX2_LANE_COUNT <= input_size;
         index += LCQI_F32_AVX2_LANE_COUNT) {
        const __m256 input_values = _mm256_loadu_ps(input + index);
        accumulator0 =
            _mm256_fmadd_ps(_mm256_loadu_ps(row0 + index), input_values, accumulator0);
        accumulator1 =
            _mm256_fmadd_ps(_mm256_loadu_ps(row1 + index), input_values, accumulator1);
        accumulator2 =
            _mm256_fmadd_ps(_mm256_loadu_ps(row2 + index), input_values, accumulator2);
        accumulator3 =
            _mm256_fmadd_ps(_mm256_loadu_ps(row3 + index), input_values, accumulator3);
    }

    sums[0] = horizontal_sum(accumulator0);
    sums[1] = horizontal_sum(accumulator1);
    sums[2] = horizontal_sum(accumulator2);
    sums[3] = horizontal_sum(accumulator3);
    for (; index < input_size; ++index) {
        const float input_value = input[index];
        sums[0] += row0[index] * input_value;
        sums[1] += row1[index] * input_value;
        sums[2] += row2[index] * input_value;
        sums[3] += row3[index] * input_value;
    }
}

void compute_eight_row_dot(const float* const* rows,
                           const float* input,
                           std::size_t input_size,
                           float* sums) noexcept {
    std::size_t index = 0;
    __m256 accumulator0 = _mm256_setzero_ps();
    __m256 accumulator1 = _mm256_setzero_ps();
    __m256 accumulator2 = _mm256_setzero_ps();
    __m256 accumulator3 = _mm256_setzero_ps();
    __m256 accumulator4 = _mm256_setzero_ps();
    __m256 accumulator5 = _mm256_setzero_ps();
    __m256 accumulator6 = _mm256_setzero_ps();
    __m256 accumulator7 = _mm256_setzero_ps();

    for (; index + LCQI_F32_AVX2_LANE_COUNT <= input_size;
         index += LCQI_F32_AVX2_LANE_COUNT) {
        const __m256 input_values = _mm256_loadu_ps(input + index);
        accumulator0 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[0] + index), input_values, accumulator0);
        accumulator1 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[1] + index), input_values, accumulator1);
        accumulator2 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[2] + index), input_values, accumulator2);
        accumulator3 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[3] + index), input_values, accumulator3);
        accumulator4 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[4] + index), input_values, accumulator4);
        accumulator5 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[5] + index), input_values, accumulator5);
        accumulator6 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[6] + index), input_values, accumulator6);
        accumulator7 =
            _mm256_fmadd_ps(_mm256_loadu_ps(rows[7] + index), input_values, accumulator7);
    }

    sums[0] = horizontal_sum(accumulator0);
    sums[1] = horizontal_sum(accumulator1);
    sums[2] = horizontal_sum(accumulator2);
    sums[3] = horizontal_sum(accumulator3);
    sums[4] = horizontal_sum(accumulator4);
    sums[5] = horizontal_sum(accumulator5);
    sums[6] = horizontal_sum(accumulator6);
    sums[7] = horizontal_sum(accumulator7);
    for (; index < input_size; ++index) {
        const float input_value = input[index];
        for (std::size_t row = 0; row < LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT; ++row) {
            sums[row] += rows[row][index] * input_value;
        }
    }
}

}  // namespace

bool dot_f32_avx2_available() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
    return true;
#endif
}

float dot_f32_avx2_unchecked(const float* lhs,
                             const float* rhs,
                             std::size_t size) noexcept {
    std::size_t index = 0;
    __m256 accumulator0 = _mm256_setzero_ps();
    __m256 accumulator1 = _mm256_setzero_ps();
    __m256 accumulator2 = _mm256_setzero_ps();
    __m256 accumulator3 = _mm256_setzero_ps();

    for (; index + LCQI_F32_AVX2_UNROLLED_FLOATS <= size;
         index += LCQI_F32_AVX2_UNROLLED_FLOATS) {
        const __m256 lhs0 = _mm256_loadu_ps(lhs + index);
        const __m256 rhs0 = _mm256_loadu_ps(rhs + index);
        const __m256 lhs1 = _mm256_loadu_ps(lhs + index + LCQI_F32_AVX2_LANE_COUNT);
        const __m256 rhs1 = _mm256_loadu_ps(rhs + index + LCQI_F32_AVX2_LANE_COUNT);
        const __m256 lhs2 = _mm256_loadu_ps(lhs + index + LCQI_F32_AVX2_LANE_COUNT * 2);
        const __m256 rhs2 = _mm256_loadu_ps(rhs + index + LCQI_F32_AVX2_LANE_COUNT * 2);
        const __m256 lhs3 = _mm256_loadu_ps(lhs + index + LCQI_F32_AVX2_LANE_COUNT * 3);
        const __m256 rhs3 = _mm256_loadu_ps(rhs + index + LCQI_F32_AVX2_LANE_COUNT * 3);

        accumulator0 = _mm256_fmadd_ps(lhs0, rhs0, accumulator0);
        accumulator1 = _mm256_fmadd_ps(lhs1, rhs1, accumulator1);
        accumulator2 = _mm256_fmadd_ps(lhs2, rhs2, accumulator2);
        accumulator3 = _mm256_fmadd_ps(lhs3, rhs3, accumulator3);
    }

    __m256 accumulator = _mm256_add_ps(
        _mm256_add_ps(accumulator0, accumulator1),
        _mm256_add_ps(accumulator2, accumulator3));
    for (; index + LCQI_F32_AVX2_LANE_COUNT <= size;
         index += LCQI_F32_AVX2_LANE_COUNT) {
        const __m256 lhs_values = _mm256_loadu_ps(lhs + index);
        const __m256 rhs_values = _mm256_loadu_ps(rhs + index);
        accumulator = _mm256_fmadd_ps(lhs_values, rhs_values, accumulator);
    }

    float sum = horizontal_sum(accumulator);
    for (; index < size; ++index) {
        sum += lhs[index] * rhs[index];
    }
    return sum;
}

void linear_f32_rows_avx2_unchecked(const float* weights,
                                    const float* input,
                                    const float* bias,
                                    std::size_t input_size,
                                    std::size_t row_begin,
                                    std::size_t row_end,
                                    float* output) noexcept {
    std::size_t row = row_begin;
    for (; row + LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT <= row_end;
         row += LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT) {
        std::array<const float*, LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT> rows{};
        rows[0] = weights + row * input_size;
        for (std::size_t offset = 1; offset < rows.size(); ++offset) {
            rows[offset] = rows[offset - 1] + input_size;
        }
        std::array<float, LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT> sums{};
        compute_eight_row_dot(rows.data(), input, input_size, sums.data());
        for (std::size_t offset = 0; offset < LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT; ++offset) {
            output[row + offset] =
                sums[offset] + (bias == nullptr ? 0.0F : bias[row + offset]);
        }
    }
    for (; row + LCQI_F32_AVX2_UNROLL_COUNT <= row_end;
         row += LCQI_F32_AVX2_UNROLL_COUNT) {
        const float* row0 = weights + row * input_size;
        const float* row1 = row0 + input_size;
        const float* row2 = row1 + input_size;
        const float* row3 = row2 + input_size;
        std::array<float, LCQI_F32_AVX2_UNROLL_COUNT> sums{};
        compute_four_row_dot(row0, row1, row2, row3, input, input_size, sums.data());
        for (std::size_t offset = 0; offset < LCQI_F32_AVX2_UNROLL_COUNT; ++offset) {
            output[row + offset] =
                sums[offset] + (bias == nullptr ? 0.0F : bias[row + offset]);
        }
    }
    for (; row < row_end; ++row) {
        const float* weight_row = weights + row * input_size;
        output[row] = dot_f32_avx2_unchecked(weight_row, input, input_size) +
                      (bias == nullptr ? 0.0F : bias[row]);
    }
}

F32RowMax max_dot_f32_rows_avx2_unchecked(const float* weights,
                                          const float* input,
                                          std::size_t input_size,
                                          std::size_t row_begin,
                                          std::size_t row_end) noexcept {
    F32RowMax best;
    best.row = row_begin;
    best.value = -std::numeric_limits<float>::infinity();
    std::size_t row = row_begin;
    for (; row + LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT <= row_end;
         row += LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT) {
        std::array<const float*, LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT> rows{};
        rows[0] = weights + row * input_size;
        for (std::size_t offset = 1; offset < rows.size(); ++offset) {
            rows[offset] = rows[offset - 1] + input_size;
        }
        std::array<float, LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT> sums{};
        compute_eight_row_dot(rows.data(), input, input_size, sums.data());
        for (std::size_t offset = 0; offset < LCQI_F32_AVX2_WIDE_ROW_UNROLL_COUNT; ++offset) {
            if (row + offset == row_begin || sums[offset] > best.value) {
                best.row = row + offset;
                best.value = sums[offset];
            }
        }
    }
    for (; row + LCQI_F32_AVX2_UNROLL_COUNT <= row_end;
         row += LCQI_F32_AVX2_UNROLL_COUNT) {
        const float* row0 = weights + row * input_size;
        const float* row1 = row0 + input_size;
        const float* row2 = row1 + input_size;
        const float* row3 = row2 + input_size;
        std::array<float, LCQI_F32_AVX2_UNROLL_COUNT> sums{};
        compute_four_row_dot(row0, row1, row2, row3, input, input_size, sums.data());
        for (std::size_t offset = 0; offset < LCQI_F32_AVX2_UNROLL_COUNT; ++offset) {
            if (row + offset == row_begin || sums[offset] > best.value) {
                best.row = row + offset;
                best.value = sums[offset];
            }
        }
    }
    for (; row < row_end; ++row) {
        const float* weight_row = weights + row * input_size;
        const float value = dot_f32_avx2_unchecked(weight_row, input, input_size);
        if (row == row_begin || value > best.value) {
            best.row = row;
            best.value = value;
        }
    }
    return best;
}

}  // namespace lcqi

#endif
