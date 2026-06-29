#include <lcqi/f32_kernels.hpp>

#if defined(LCQI_ENABLE_AVX2)

#include <immintrin.h>

#include <cstddef>

namespace lcqi {
namespace {

constexpr std::size_t LCQI_F32_AVX2_LANE_COUNT = 8;
constexpr std::size_t LCQI_F32_AVX2_UNROLL_COUNT = 4;
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

}  // namespace lcqi

#endif
