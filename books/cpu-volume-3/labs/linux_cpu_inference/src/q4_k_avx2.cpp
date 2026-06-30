#include <lcqi/q4_k.hpp>

#if defined(LCQI_ENABLE_AVX2)

#include <immintrin.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace lcqi {
namespace {

constexpr std::uint32_t LCQI_Q4K_F32_EXPONENT_MASK = 0x7F800000U;
constexpr std::uint32_t LCQI_Q4K_F16_SIGN_MASK = 0x8000U;
constexpr std::uint32_t LCQI_Q4K_F16_EXPONENT_MASK = 0x7C00U;
constexpr std::uint32_t LCQI_Q4K_F16_MANTISSA_MASK = 0x03FFU;
constexpr std::uint32_t LCQI_Q4K_F16_EXPONENT_BIAS = 15U;
constexpr std::uint32_t LCQI_Q4K_F32_EXPONENT_BIAS = 127U;
constexpr std::uint32_t LCQI_Q4K_F32_MANTISSA_BITS = 23U;
constexpr std::uint32_t LCQI_Q4K_F16_MANTISSA_BITS = 10U;
constexpr std::uint32_t LCQI_Q4K_F16_TO_F32_SIGN_SHIFT = 16U;
constexpr std::uint32_t LCQI_Q4K_BYTE_BITS = 8U;
constexpr std::uint8_t LCQI_Q4K_LOW_NIBBLE_MASK = 0x0FU;
constexpr std::uint8_t LCQI_Q4K_SIX_BIT_MASK = 63U;
constexpr std::int64_t LCQI_Q4K_QS_OFFSET = 16;
constexpr std::int64_t LCQI_Q4K_QS_BYTES_PER_PAIR = 32;
constexpr std::int64_t LCQI_Q4K_SUBBLOCK_PAIR_COUNT = 4;
constexpr const char* LCQI_Q4K_BACKEND_ENV = "LCQI_Q4K_BACKEND";
constexpr const char* LCQI_Q4K_FORCE_SCALAR_ENV = "LCQI_Q4K_FORCE_SCALAR";
constexpr const char* LCQI_Q4K_BACKEND_SCALAR = "scalar";
constexpr const char* LCQI_Q4K_FORCE_TRUE = "1";

std::uint16_t read_le_u16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << LCQI_Q4K_BYTE_BITS));
}

float bits_to_float(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

float f16_to_f32(std::uint16_t bits) noexcept {
    const std::uint32_t sign =
        (static_cast<std::uint32_t>(bits) & LCQI_Q4K_F16_SIGN_MASK)
        << LCQI_Q4K_F16_TO_F32_SIGN_SHIFT;
    const std::uint32_t exponent =
        static_cast<std::uint32_t>(bits) & LCQI_Q4K_F16_EXPONENT_MASK;
    const std::uint32_t mantissa =
        static_cast<std::uint32_t>(bits) & LCQI_Q4K_F16_MANTISSA_MASK;

    if (exponent == 0U) {
        if (mantissa == 0U) {
            return bits_to_float(sign);
        }
        float value = static_cast<float>(mantissa) /
                      static_cast<float>(1U << LCQI_Q4K_F16_MANTISSA_BITS);
        value = std::ldexp(value, 1 - static_cast<int>(LCQI_Q4K_F16_EXPONENT_BIAS));
        return sign == 0U ? value : -value;
    }

    if (exponent == LCQI_Q4K_F16_EXPONENT_MASK) {
        const std::uint32_t f32_mantissa =
            mantissa << (LCQI_Q4K_F32_MANTISSA_BITS - LCQI_Q4K_F16_MANTISSA_BITS);
        return bits_to_float(sign | LCQI_Q4K_F32_EXPONENT_MASK | f32_mantissa);
    }

    const std::uint32_t f32_exponent =
        ((exponent >> LCQI_Q4K_F16_MANTISSA_BITS) - LCQI_Q4K_F16_EXPONENT_BIAS +
         LCQI_Q4K_F32_EXPONENT_BIAS)
        << LCQI_Q4K_F32_MANTISSA_BITS;
    const std::uint32_t f32_mantissa =
        mantissa << (LCQI_Q4K_F32_MANTISSA_BITS - LCQI_Q4K_F16_MANTISSA_BITS);
    return bits_to_float(sign | f32_exponent | f32_mantissa);
}

void get_scale_min_k4(std::int32_t index,
                      const std::uint8_t* packed,
                      std::uint8_t& scale,
                      std::uint8_t& min) noexcept {
    if (index < LCQI_Q4K_SUBBLOCK_PAIR_COUNT) {
        scale = packed[index] & LCQI_Q4K_SIX_BIT_MASK;
        min = packed[index + LCQI_Q4K_SUBBLOCK_PAIR_COUNT] & LCQI_Q4K_SIX_BIT_MASK;
        return;
    }
    scale = static_cast<std::uint8_t>((packed[index + LCQI_Q4K_SUBBLOCK_PAIR_COUNT] &
                                       LCQI_Q4K_LOW_NIBBLE_MASK) |
                                      ((packed[index - LCQI_Q4K_SUBBLOCK_PAIR_COUNT] >> 6U)
                                       << 4U));
    min = static_cast<std::uint8_t>((packed[index + LCQI_Q4K_SUBBLOCK_PAIR_COUNT] >> 4U) |
                                    ((packed[index] >> 6U) << 4U));
}

std::int32_t horizontal_sum_i32(__m256i values) noexcept {
    const __m128i low = _mm256_castsi256_si128(values);
    const __m128i high = _mm256_extracti128_si256(values, 1);
    __m128i sum = _mm_add_epi32(low, high);
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2)));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(sum);
}

std::int32_t dot_32_u4_i8_avx2(__m256i q4_values, const std::int8_t* q8_values) noexcept {
    const __m256i q8 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(q8_values));
    const __m256i pair_sums_i16 = _mm256_maddubs_epi16(q4_values, q8);
    const __m256i ones = _mm256_set1_epi16(1);
    const __m256i partial_i32 = _mm256_madd_epi16(pair_sums_i16, ones);
    return horizontal_sum_i32(partial_i32);
}

float dot_block_q4_k_q8_avx2(const std::uint8_t* block, const Q8KBlock& input) noexcept {
    const float d = f16_to_f32(read_le_u16(block));
    const float dmin = f16_to_f32(read_le_u16(block + 2));
    const std::uint8_t* scales = block + 4;
    const std::uint8_t* qs = block + LCQI_Q4K_QS_OFFSET;
    const __m256i nibble_mask = _mm256_set1_epi8(static_cast<char>(LCQI_Q4K_LOW_NIBBLE_MASK));
    std::int32_t weighted_sum = 0;
    std::int32_t min_sum = 0;

    for (std::int32_t pair = 0; pair < LCQI_Q4K_SUBBLOCK_PAIR_COUNT; ++pair) {
        const __m256i packed =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
                qs + pair * LCQI_Q4K_QS_BYTES_PER_PAIR));
        const __m256i low_nibbles = _mm256_and_si256(packed, nibble_mask);
        const __m256i high_nibbles =
            _mm256_and_si256(_mm256_srli_epi16(packed, 4), nibble_mask);

        const std::int32_t low_subblock = pair * 2;
        const std::int32_t high_subblock = low_subblock + 1;
        std::uint8_t low_scale = 0;
        std::uint8_t low_min = 0;
        std::uint8_t high_scale = 0;
        std::uint8_t high_min = 0;
        get_scale_min_k4(low_subblock, scales, low_scale, low_min);
        get_scale_min_k4(high_subblock, scales, high_scale, high_min);

        const std::int32_t low_dot =
            dot_32_u4_i8_avx2(low_nibbles,
                              input.qs.data() + low_subblock * LCQI_Q4_K_SUBBLOCK_VALUES);
        const std::int32_t high_dot =
            dot_32_u4_i8_avx2(high_nibbles,
                              input.qs.data() + high_subblock * LCQI_Q4_K_SUBBLOCK_VALUES);
        weighted_sum += static_cast<std::int32_t>(low_scale) * low_dot;
        weighted_sum += static_cast<std::int32_t>(high_scale) * high_dot;

        min_sum += static_cast<std::int32_t>(low_min) *
                   (static_cast<std::int32_t>(input.bsums[low_subblock * 2]) +
                    static_cast<std::int32_t>(input.bsums[low_subblock * 2 + 1]));
        min_sum += static_cast<std::int32_t>(high_min) *
                   (static_cast<std::int32_t>(input.bsums[high_subblock * 2]) +
                    static_cast<std::int32_t>(input.bsums[high_subblock * 2 + 1]));
    }

    return (d * input.d) * static_cast<float>(weighted_sum) -
           (dmin * input.d) * static_cast<float>(min_sum);
}

bool scalar_backend_forced() noexcept {
    const char* backend = std::getenv(LCQI_Q4K_BACKEND_ENV);
    if (backend != nullptr && std::strcmp(backend, LCQI_Q4K_BACKEND_SCALAR) == 0) {
        return true;
    }
    const char* force_scalar = std::getenv(LCQI_Q4K_FORCE_SCALAR_ENV);
    return force_scalar != nullptr && std::strcmp(force_scalar, LCQI_Q4K_FORCE_TRUE) == 0;
}

}  // namespace

bool q4_k_q8_avx2_available() noexcept {
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

const char* q4_k_q8_active_backend() noexcept {
    return q4_k_q8_avx2_available() ? "avx2_maddubs" : "scalar";
}

void matvec_q4_k_q8_avx2_rows_unchecked(std::span<const std::uint8_t> q4_rows,
                                        std::int64_t row_count,
                                        std::int64_t column_count,
                                        std::span<const Q8KBlock> input,
                                        std::span<float> output,
                                        std::int64_t row_begin,
                                        std::int64_t row_end) noexcept;

void matvec_q4_k_q8_avx2_unchecked(std::span<const std::uint8_t> q4_rows,
                                   std::int64_t row_count,
                                   std::int64_t column_count,
                                   std::span<const Q8KBlock> input,
                                   std::span<float> output) {
    matvec_q4_k_q8_avx2_rows_unchecked(q4_rows,
                                       row_count,
                                       column_count,
                                       input,
                                       output,
                                       0,
                                       row_count);
}

void matvec_q4_k_q8_avx2_rows_unchecked(std::span<const std::uint8_t> q4_rows,
                                        std::int64_t row_count,
                                        std::int64_t column_count,
                                        std::span<const Q8KBlock> input,
                                        std::span<float> output,
                                        std::int64_t row_begin,
                                        std::int64_t row_end) noexcept {
    (void)row_count;
    const std::int64_t row_blocks = column_count / LCQI_QK_K_BLOCK_VALUES;
    const std::int64_t row_bytes = row_blocks * LCQI_Q4_K_BLOCK_BYTES;
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        float sum = 0.0F;
        const std::uint8_t* row_data = q4_rows.data() + row * row_bytes;
        for (std::int64_t block = 0; block < row_blocks; ++block) {
            sum += dot_block_q4_k_q8_avx2(
                row_data + block * LCQI_Q4_K_BLOCK_BYTES,
                input[static_cast<std::size_t>(block)]);
        }
        output[static_cast<std::size_t>(row)] = sum;
    }
}

}  // namespace lcqi

#endif
