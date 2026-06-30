#include <lcqi/q4_k.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

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
constexpr std::int32_t LCQI_Q8K_QUANT_MAX = 127;
constexpr std::int64_t LCQI_Q4K_QS_OFFSET = 16;
constexpr std::int64_t LCQI_Q4K_MIN_GROUP_VALUES = 16;
constexpr std::int64_t LCQI_Q4K_QS_BYTES_PER_PAIR = 32;

void require_positive(std::int64_t value, const char* name) {
    if (value <= 0) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

void require_multiple(std::int64_t value, std::int64_t divisor, const char* name) {
    if (value <= 0 || value % divisor != 0) {
        throw std::runtime_error(std::string(name) + " must be a positive multiple of " +
                                 std::to_string(divisor));
    }
}

std::size_t checked_size(std::int64_t value, const char* name) {
    if (value < 0 ||
        static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(name) + " is outside size_t range");
    }
    return static_cast<std::size_t>(value);
}

std::uint16_t read_le_u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << LCQI_Q4K_BYTE_BITS));
}

float bits_to_float(std::uint32_t bits) {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

float f16_to_f32(std::uint16_t bits) {
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
                      std::uint8_t& min) {
    if (index < 4) {
        scale = packed[index] & LCQI_Q4K_SIX_BIT_MASK;
        min = packed[index + 4] & LCQI_Q4K_SIX_BIT_MASK;
        return;
    }
    scale = static_cast<std::uint8_t>((packed[index + 4] & LCQI_Q4K_LOW_NIBBLE_MASK) |
                                      ((packed[index - 4] >> 6U) << 4U));
    min = static_cast<std::uint8_t>((packed[index + 4] >> 4U) |
                                    ((packed[index] >> 6U) << 4U));
}

void validate_q4_k_bytes(std::span<const std::uint8_t> bytes, std::int64_t element_count) {
    require_multiple(element_count, LCQI_QK_K_BLOCK_VALUES, "Q4_K element_count");
    const std::int64_t expected_bytes =
        (element_count / LCQI_QK_K_BLOCK_VALUES) * LCQI_Q4_K_BLOCK_BYTES;
    if (bytes.size() != checked_size(expected_bytes, "Q4_K byte count")) {
        throw std::runtime_error("Q4_K byte count does not match element count");
    }
}

void validate_input_size(std::span<const float> input) {
    require_multiple(static_cast<std::int64_t>(input.size()),
                     LCQI_QK_K_BLOCK_VALUES,
                     "Q4_K input size");
}

void validate_q8_blocks(std::span<const Q8KBlock> input, std::int64_t column_count) {
    require_multiple(column_count, LCQI_QK_K_BLOCK_VALUES, "Q8_K column count");
    if (input.size() !=
        checked_size(column_count / LCQI_QK_K_BLOCK_VALUES, "Q8_K block count")) {
        throw std::runtime_error("Q8_K input block count mismatch");
    }
}

std::int8_t quantized_nibble(const std::uint8_t* qs,
                             std::int32_t subblock,
                             std::int32_t index) {
    const std::uint8_t packed =
        qs[(subblock / 2) * LCQI_Q4K_QS_BYTES_PER_PAIR + index];
    if ((subblock % 2) == 0) {
        return static_cast<std::int8_t>(packed & LCQI_Q4K_LOW_NIBBLE_MASK);
    }
    return static_cast<std::int8_t>(packed >> 4U);
}

void dequantize_block_to(const std::uint8_t* block, float* output) {
    const float d = f16_to_f32(read_le_u16(block));
    const float dmin = f16_to_f32(read_le_u16(block + 2));
    const std::uint8_t* scales = block + 4;
    const std::uint8_t* qs = block + LCQI_Q4K_QS_OFFSET;

    for (std::int32_t subblock = 0; subblock < LCQI_Q4_K_SUBBLOCKS; ++subblock) {
        std::uint8_t scale = 0;
        std::uint8_t min = 0;
        get_scale_min_k4(subblock, scales, scale, min);
        const float subblock_scale = d * static_cast<float>(scale);
        const float subblock_min = dmin * static_cast<float>(min);
        for (std::int32_t index = 0; index < LCQI_Q4_K_SUBBLOCK_VALUES; ++index) {
            output[subblock * LCQI_Q4_K_SUBBLOCK_VALUES + index] =
                subblock_scale * static_cast<float>(quantized_nibble(qs, subblock, index)) -
                subblock_min;
        }
    }
}

float dot_block_q4_k_f32(const std::uint8_t* block, const float* input) {
    const float d = f16_to_f32(read_le_u16(block));
    const float dmin = f16_to_f32(read_le_u16(block + 2));
    const std::uint8_t* scales = block + 4;
    const std::uint8_t* qs = block + LCQI_Q4K_QS_OFFSET;
    float sum = 0.0F;

    for (std::int32_t subblock = 0; subblock < LCQI_Q4_K_SUBBLOCKS; ++subblock) {
        std::uint8_t scale = 0;
        std::uint8_t min = 0;
        get_scale_min_k4(subblock, scales, scale, min);
        const float subblock_scale = d * static_cast<float>(scale);
        const float subblock_min = dmin * static_cast<float>(min);
        const std::int32_t base = subblock * LCQI_Q4_K_SUBBLOCK_VALUES;
        for (std::int32_t index = 0; index < LCQI_Q4_K_SUBBLOCK_VALUES; ++index) {
            const float weight =
                subblock_scale * static_cast<float>(quantized_nibble(qs, subblock, index)) -
                subblock_min;
            sum += weight * input[base + index];
        }
    }
    return sum;
}

float dot_block_q4_k_q8(const std::uint8_t* block, const Q8KBlock& input) {
    const float d = f16_to_f32(read_le_u16(block));
    const float dmin = f16_to_f32(read_le_u16(block + 2));
    const std::uint8_t* scales = block + 4;
    const std::uint8_t* qs = block + LCQI_Q4K_QS_OFFSET;
    std::int32_t weighted_sum = 0;
    std::int32_t min_sum = 0;

    for (std::int32_t subblock = 0; subblock < LCQI_Q4_K_SUBBLOCKS; ++subblock) {
        std::uint8_t scale = 0;
        std::uint8_t min = 0;
        get_scale_min_k4(subblock, scales, scale, min);
        const std::int32_t base = subblock * LCQI_Q4_K_SUBBLOCK_VALUES;
        std::int32_t subblock_sum = 0;
        for (std::int32_t index = 0; index < LCQI_Q4_K_SUBBLOCK_VALUES; ++index) {
            subblock_sum +=
                static_cast<std::int32_t>(quantized_nibble(qs, subblock, index)) *
                static_cast<std::int32_t>(input.qs[base + index]);
        }
        weighted_sum += static_cast<std::int32_t>(scale) * subblock_sum;
        min_sum += static_cast<std::int32_t>(min) *
                   (static_cast<std::int32_t>(input.bsums[subblock * 2]) +
                    static_cast<std::int32_t>(input.bsums[subblock * 2 + 1]));
    }

    return (d * input.d) * static_cast<float>(weighted_sum) -
           (dmin * input.d) * static_cast<float>(min_sum);
}

}  // namespace

#if !defined(LCQI_ENABLE_AVX2)
bool q4_k_q8_avx2_available() noexcept {
    return false;
}

const char* q4_k_q8_active_backend() noexcept {
    return "scalar";
}
#endif

void matvec_q4_k_q8_avx2_unchecked(std::span<const std::uint8_t>,
                                   std::int64_t,
                                   std::int64_t,
                                   std::span<const Q8KBlock>,
                                   std::span<float>);

void matvec_q4_k_q8_avx2_rows_unchecked(std::span<const std::uint8_t>,
                                        std::int64_t,
                                        std::int64_t,
                                        std::span<const Q8KBlock>,
                                        std::span<float>,
                                        std::int64_t,
                                        std::int64_t) noexcept;

std::vector<float> dequantize_q4_k(std::span<const std::uint8_t> bytes,
                                   std::int64_t element_count) {
    validate_q4_k_bytes(bytes, element_count);
    std::vector<float> output(checked_size(element_count, "Q4_K element count"), 0.0F);
    dequantize_q4_k_to(bytes, output);
    return output;
}

void dequantize_q4_k_to(std::span<const std::uint8_t> bytes,
                        std::span<float> output) {
    validate_q4_k_bytes(bytes, static_cast<std::int64_t>(output.size()));
    const std::int64_t block_count =
        static_cast<std::int64_t>(output.size()) / LCQI_QK_K_BLOCK_VALUES;
    for (std::int64_t block_index = 0; block_index < block_count; ++block_index) {
        dequantize_block_to(bytes.data() + block_index * LCQI_Q4_K_BLOCK_BYTES,
                            output.data() + block_index * LCQI_QK_K_BLOCK_VALUES);
    }
}

std::vector<Q8KBlock> quantize_q8_k_input(std::span<const float> input) {
    validate_input_size(input);
    const std::int64_t block_count =
        static_cast<std::int64_t>(input.size()) / LCQI_QK_K_BLOCK_VALUES;
    std::vector<Q8KBlock> output(checked_size(block_count, "Q8_K block count"));
    quantize_q8_k_input_to(input, output);
    return output;
}

void quantize_q8_k_input_to(std::span<const float> input,
                            std::span<Q8KBlock> output) {
    validate_input_size(input);
    const std::int64_t block_count =
        static_cast<std::int64_t>(input.size()) / LCQI_QK_K_BLOCK_VALUES;
    if (output.size() != checked_size(block_count, "Q8_K block count")) {
        throw std::runtime_error("Q8_K output block count mismatch");
    }
    for (std::int64_t block_index = 0; block_index < block_count; ++block_index) {
        Q8KBlock& block = output[checked_size(block_index, "Q8_K block index")];
        const float* values =
            input.data() + block_index * LCQI_QK_K_BLOCK_VALUES;
        float max_value = 0.0F;
        float max_abs = 0.0F;
        for (std::int32_t index = 0; index < LCQI_QK_K_BLOCK_VALUES; ++index) {
            const float abs_value = std::fabs(values[index]);
            if (abs_value > max_abs) {
                max_abs = abs_value;
                max_value = values[index];
            }
        }
        if (max_abs == 0.0F) {
            block.d = 0.0F;
            block.qs.fill(0);
            block.bsums.fill(0);
            continue;
        }

        const float inverse_scale = -static_cast<float>(LCQI_Q8K_QUANT_MAX) / max_value;
        for (std::int32_t index = 0; index < LCQI_QK_K_BLOCK_VALUES; ++index) {
            const int quantized = static_cast<int>(std::nearbyint(inverse_scale * values[index]));
            const int clamped = std::clamp(quantized,
                                           -LCQI_Q8K_QUANT_MAX,
                                           LCQI_Q8K_QUANT_MAX);
            block.qs[index] = static_cast<std::int8_t>(clamped);
        }
        for (std::int32_t group = 0; group < LCQI_Q8_K_BSUM_GROUPS; ++group) {
            std::int32_t sum = 0;
            for (std::int32_t index = 0; index < LCQI_Q4K_MIN_GROUP_VALUES; ++index) {
                sum += block.qs[group * LCQI_Q4K_MIN_GROUP_VALUES + index];
            }
            block.bsums[group] = static_cast<std::int16_t>(sum);
        }
        block.d = 1.0F / inverse_scale;
    }
}

float dot_q4_k_f32(std::span<const std::uint8_t> q4_blocks,
                   std::span<const float> input) {
    validate_input_size(input);
    validate_q4_k_bytes(q4_blocks, static_cast<std::int64_t>(input.size()));
    const std::int64_t block_count =
        static_cast<std::int64_t>(input.size()) / LCQI_QK_K_BLOCK_VALUES;
    float sum = 0.0F;
    for (std::int64_t block_index = 0; block_index < block_count; ++block_index) {
        sum += dot_block_q4_k_f32(
            q4_blocks.data() + block_index * LCQI_Q4_K_BLOCK_BYTES,
            input.data() + block_index * LCQI_QK_K_BLOCK_VALUES);
    }
    return sum;
}

float dot_q4_k_q8(std::span<const std::uint8_t> q4_blocks,
                  std::span<const Q8KBlock> input) {
    const std::int64_t element_count =
        static_cast<std::int64_t>(input.size()) * LCQI_QK_K_BLOCK_VALUES;
    validate_q4_k_bytes(q4_blocks, element_count);
    float sum = 0.0F;
    for (std::int64_t block_index = 0;
         block_index < static_cast<std::int64_t>(input.size());
         ++block_index) {
        sum += dot_block_q4_k_q8(
            q4_blocks.data() + block_index * LCQI_Q4_K_BLOCK_BYTES,
            input[checked_size(block_index, "Q8_K block index")]);
    }
    return sum;
}

void matvec_q4_k_f32(std::span<const std::uint8_t> q4_rows,
                     std::int64_t row_count,
                     std::int64_t column_count,
                     std::span<const float> input,
                     std::span<float> output) {
    require_positive(row_count, "Q4_K row count");
    require_multiple(column_count, LCQI_QK_K_BLOCK_VALUES, "Q4_K column count");
    if (input.size() != checked_size(column_count, "Q4_K column count")) {
        throw std::runtime_error("Q4_K matvec input size mismatch");
    }
    if (output.size() != checked_size(row_count, "Q4_K row count")) {
        throw std::runtime_error("Q4_K matvec output size mismatch");
    }
    const std::int64_t row_bytes =
        (column_count / LCQI_QK_K_BLOCK_VALUES) * LCQI_Q4_K_BLOCK_BYTES;
    if (q4_rows.size() != checked_size(row_bytes * row_count, "Q4_K matrix bytes")) {
        throw std::runtime_error("Q4_K matvec matrix byte size mismatch");
    }
    for (std::int64_t row = 0; row < row_count; ++row) {
        output[checked_size(row, "Q4_K row")] =
            dot_q4_k_f32(q4_rows.subspan(checked_size(row * row_bytes, "Q4_K row offset"),
                                         checked_size(row_bytes, "Q4_K row bytes")),
                         input);
    }
}

void matvec_q4_k_q8(std::span<const std::uint8_t> q4_rows,
                    std::int64_t row_count,
                    std::int64_t column_count,
                    std::span<const Q8KBlock> input,
                    std::span<float> output) {
    require_positive(row_count, "Q4_K row count");
    validate_q8_blocks(input, column_count);
    if (output.size() != checked_size(row_count, "Q4_K row count")) {
        throw std::runtime_error("Q4_K matvec output size mismatch");
    }
    const std::int64_t row_bytes =
        (column_count / LCQI_QK_K_BLOCK_VALUES) * LCQI_Q4_K_BLOCK_BYTES;
    if (q4_rows.size() != checked_size(row_bytes * row_count, "Q4_K matrix bytes")) {
        throw std::runtime_error("Q4_K matvec matrix byte size mismatch");
    }
#if defined(LCQI_ENABLE_AVX2)
    if (q4_k_q8_avx2_available()) {
        matvec_q4_k_q8_avx2_unchecked(q4_rows, row_count, column_count, input, output);
        return;
    }
#endif
    matvec_q4_k_q8_rows_unchecked(q4_rows, row_count, column_count, input, output, 0, row_count);
}

void matvec_q4_k_q8_rows_unchecked(std::span<const std::uint8_t> q4_rows,
                                   std::int64_t row_count,
                                   std::int64_t column_count,
                                   std::span<const Q8KBlock> input,
                                   std::span<float> output,
                                   std::int64_t row_begin,
                                   std::int64_t row_end) noexcept {
    const std::int64_t row_bytes =
        (column_count / LCQI_QK_K_BLOCK_VALUES) * LCQI_Q4_K_BLOCK_BYTES;
#if defined(LCQI_ENABLE_AVX2)
    if (q4_k_q8_avx2_available()) {
        matvec_q4_k_q8_avx2_rows_unchecked(q4_rows,
                                           row_count,
                                           column_count,
                                           input,
                                           output,
                                           row_begin,
                                           row_end);
        return;
    }
#else
    (void)row_count;
#endif
    for (std::int64_t row = row_begin; row < row_end; ++row) {
        output[static_cast<std::size_t>(row)] =
            dot_q4_k_q8(q4_rows.subspan(static_cast<std::size_t>(row * row_bytes),
                                        static_cast<std::size_t>(row_bytes)),
                        input);
    }
}

}  // namespace lcqi
