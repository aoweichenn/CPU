#include <lcqi/ggml_matvec.hpp>

#include <lcqi/ggml_tensors.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr std::uint32_t LCQI_GGML_MATVEC_BYTE_BITS = 8U;
constexpr std::uint8_t LCQI_GGML_MATVEC_LOW_NIBBLE_MASK = 0x0FU;
constexpr std::uint8_t LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK = 0x03U;
constexpr std::int32_t LCQI_GGML_MATVEC_Q5_ZERO_POINT = 16;
constexpr std::int32_t LCQI_GGML_MATVEC_Q6_ZERO_POINT = 32;
constexpr std::int32_t LCQI_GGML_MATVEC_Q6_HALF_BLOCK_VALUES = 128;
constexpr std::int32_t LCQI_GGML_MATVEC_Q6_QUARTER_BLOCK_VALUES = 64;
constexpr std::int32_t LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES = 32;
constexpr std::int32_t LCQI_GGML_MATVEC_Q6_SCALE_STRIDE = 8;
constexpr std::int32_t LCQI_GGML_MATVEC_Q8_QUANT_MAX = 127;

[[nodiscard]] std::size_t checked_size(std::int64_t value, const char* name) {
    if (value < 0 ||
        static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(name) + " is outside size_t range");
    }
    return static_cast<std::size_t>(value);
}

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

[[nodiscard]] std::uint16_t read_le_u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << LCQI_GGML_MATVEC_BYTE_BITS));
}

[[nodiscard]] std::uint32_t read_le_u32(const std::uint8_t* bytes) {
    std::uint32_t value = 0;
    for (std::uint32_t index = 0; index < sizeof(std::uint32_t); ++index) {
        value |= static_cast<std::uint32_t>(bytes[index]) <<
                 (LCQI_GGML_MATVEC_BYTE_BITS * index);
    }
    return value;
}

[[nodiscard]] float dot_q8_0_block_f32(const std::uint8_t* block, const float* input) {
    const float d = ggml_f16_to_f32(read_le_u16(block));
    const auto* qs = reinterpret_cast<const std::int8_t*>(block + sizeof(std::uint16_t));
    float sum = 0.0F;
    for (std::int32_t index = 0; index < LCQI_Q8_0_BLOCK_VALUES; ++index) {
        sum += static_cast<float>(qs[index]) * input[index];
    }
    return d * sum;
}

[[nodiscard]] float dot_q8_0_block_q8_0(const std::uint8_t* block,
                                        const Q8_0InputBlock& input) {
    const float d = ggml_f16_to_f32(read_le_u16(block));
    const auto* qs = reinterpret_cast<const std::int8_t*>(block + sizeof(std::uint16_t));
    std::int32_t sum = 0;
    for (std::int32_t index = 0; index < LCQI_Q8_0_BLOCK_VALUES; ++index) {
        sum += static_cast<std::int32_t>(qs[index]) *
               static_cast<std::int32_t>(input.qs[index]);
    }
    return d * input.d * static_cast<float>(sum);
}

[[nodiscard]] float dot_q5_0_block_f32(const std::uint8_t* block, const float* input) {
    const float d = ggml_f16_to_f32(read_le_u16(block));
    const std::uint32_t qh = read_le_u32(block + sizeof(std::uint16_t));
    const std::uint8_t* qs = block + sizeof(std::uint16_t) + sizeof(std::uint32_t);
    float sum = 0.0F;
    for (std::int32_t index = 0; index < LCQI_Q5_0_BLOCK_VALUES / 2; ++index) {
        const std::uint8_t high_0 =
            static_cast<std::uint8_t>(((qh >> index) & 1U) << 4U);
        const std::uint8_t high_1 =
            static_cast<std::uint8_t>(((qh >> (index + LCQI_Q5_0_BLOCK_VALUES / 2)) & 1U)
                                      << 4U);
        const std::int32_t first =
            static_cast<std::int32_t>((qs[index] & LCQI_GGML_MATVEC_LOW_NIBBLE_MASK) |
                                      high_0) -
            LCQI_GGML_MATVEC_Q5_ZERO_POINT;
        const std::int32_t second =
            static_cast<std::int32_t>((qs[index] >> 4U) | high_1) -
            LCQI_GGML_MATVEC_Q5_ZERO_POINT;
        sum += static_cast<float>(first) * input[index];
        sum += static_cast<float>(second) * input[index + LCQI_Q5_0_BLOCK_VALUES / 2];
    }
    return d * sum;
}

[[nodiscard]] float dot_q5_0_block_q8_0(const std::uint8_t* block,
                                        const Q8_0InputBlock& input) {
    const float d = ggml_f16_to_f32(read_le_u16(block));
    const std::uint32_t qh = read_le_u32(block + sizeof(std::uint16_t));
    const std::uint8_t* qs = block + sizeof(std::uint16_t) + sizeof(std::uint32_t);
    std::int32_t weighted_sum = 0;
    for (std::int32_t index = 0; index < LCQI_Q5_0_BLOCK_VALUES / 2; ++index) {
        const std::uint8_t high_0 =
            static_cast<std::uint8_t>(((qh >> index) & 1U) << 4U);
        const std::uint8_t high_1 =
            static_cast<std::uint8_t>(((qh >> (index + LCQI_Q5_0_BLOCK_VALUES / 2)) & 1U)
                                      << 4U);
        const std::int32_t first =
            static_cast<std::int32_t>((qs[index] & LCQI_GGML_MATVEC_LOW_NIBBLE_MASK) |
                                      high_0);
        const std::int32_t second =
            static_cast<std::int32_t>((qs[index] >> 4U) | high_1);
        weighted_sum += first * static_cast<std::int32_t>(input.qs[index]);
        weighted_sum += second *
                        static_cast<std::int32_t>(input.qs[index + LCQI_Q5_0_BLOCK_VALUES / 2]);
    }
    weighted_sum -= LCQI_GGML_MATVEC_Q5_ZERO_POINT * static_cast<std::int32_t>(input.sum);
    return d * input.d * static_cast<float>(weighted_sum);
}

[[nodiscard]] float dot_q6_k_block_f32(const std::uint8_t* block, const float* input) {
    const std::uint8_t* ql = block;
    const std::uint8_t* qh = ql + LCQI_Q6_K_BLOCK_VALUES / 2;
    const auto* scales =
        reinterpret_cast<const std::int8_t*>(qh + LCQI_Q6_K_BLOCK_VALUES / 4);
    const float d = ggml_f16_to_f32(
        read_le_u16(block + LCQI_Q6_K_BLOCK_BYTES - sizeof(std::uint16_t)));
    float sum = 0.0F;

    for (std::int32_t half = 0; half < LCQI_Q6_K_BLOCK_VALUES;
         half += LCQI_GGML_MATVEC_Q6_HALF_BLOCK_VALUES) {
        const float* x = input + half;
        for (std::int32_t lane = 0; lane < LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES; ++lane) {
            const std::int32_t scale_index =
                lane / (LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES / 2);
            const std::int32_t q1 =
                static_cast<std::int32_t>((ql[lane] & LCQI_GGML_MATVEC_LOW_NIBBLE_MASK) |
                                          (((qh[lane] >> 0U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;
            const std::int32_t q2 =
                static_cast<std::int32_t>((ql[lane + LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES] &
                                           LCQI_GGML_MATVEC_LOW_NIBBLE_MASK) |
                                          (((qh[lane] >> 2U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;
            const std::int32_t q3 =
                static_cast<std::int32_t>((ql[lane] >> 4U) |
                                          (((qh[lane] >> 4U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;
            const std::int32_t q4 =
                static_cast<std::int32_t>((ql[lane + LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES] >>
                                           4U) |
                                          (((qh[lane] >> 6U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;

            sum += static_cast<float>(scales[scale_index + 0]) *
                   static_cast<float>(q1) * x[lane];
            sum += static_cast<float>(scales[scale_index + 2]) *
                   static_cast<float>(q2) * x[lane + LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES];
            sum += static_cast<float>(scales[scale_index + 4]) *
                   static_cast<float>(q3) * x[lane + LCQI_GGML_MATVEC_Q6_QUARTER_BLOCK_VALUES];
            sum += static_cast<float>(scales[scale_index + 6]) *
                   static_cast<float>(q4) *
                   x[lane + LCQI_GGML_MATVEC_Q6_QUARTER_BLOCK_VALUES +
                     LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES];
        }
        ql += LCQI_GGML_MATVEC_Q6_QUARTER_BLOCK_VALUES;
        qh += LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES;
        scales += LCQI_GGML_MATVEC_Q6_SCALE_STRIDE;
    }
    return d * sum;
}

[[nodiscard]] float dot_q6_k_block_q8_0(const std::uint8_t* block,
                                        const Q8_0InputBlock* input) {
    const std::uint8_t* ql = block;
    const std::uint8_t* qh = ql + LCQI_Q6_K_BLOCK_VALUES / 2;
    const auto* scales =
        reinterpret_cast<const std::int8_t*>(qh + LCQI_Q6_K_BLOCK_VALUES / 4);
    const float d = ggml_f16_to_f32(
        read_le_u16(block + LCQI_Q6_K_BLOCK_BYTES - sizeof(std::uint16_t)));
    float sum = 0.0F;

    for (std::int32_t half = 0; half < LCQI_Q6_K_BLOCK_VALUES;
         half += LCQI_GGML_MATVEC_Q6_HALF_BLOCK_VALUES) {
        const std::int32_t input_half_block = half / LCQI_Q8_0_INPUT_BLOCK_VALUES;
        for (std::int32_t lane = 0; lane < LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES; ++lane) {
            const std::int32_t scale_index =
                lane / (LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES / 2);
            const std::int32_t q1 =
                static_cast<std::int32_t>((ql[lane] & LCQI_GGML_MATVEC_LOW_NIBBLE_MASK) |
                                          (((qh[lane] >> 0U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;
            const std::int32_t q2 =
                static_cast<std::int32_t>((ql[lane + LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES] &
                                           LCQI_GGML_MATVEC_LOW_NIBBLE_MASK) |
                                          (((qh[lane] >> 2U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;
            const std::int32_t q3 =
                static_cast<std::int32_t>((ql[lane] >> 4U) |
                                          (((qh[lane] >> 4U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;
            const std::int32_t q4 =
                static_cast<std::int32_t>((ql[lane + LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES] >>
                                           4U) |
                                          (((qh[lane] >> 6U) &
                                            LCQI_GGML_MATVEC_Q6_HIGH_TWO_BITS_MASK)
                                           << 4U)) -
                LCQI_GGML_MATVEC_Q6_ZERO_POINT;

            const Q8_0InputBlock& x1 = input[input_half_block + 0];
            const Q8_0InputBlock& x2 = input[input_half_block + 1];
            const Q8_0InputBlock& x3 = input[input_half_block + 2];
            const Q8_0InputBlock& x4 = input[input_half_block + 3];
            sum += x1.d * static_cast<float>(scales[scale_index + 0]) *
                   static_cast<float>(q1) * static_cast<float>(x1.qs[lane]);
            sum += x2.d * static_cast<float>(scales[scale_index + 2]) *
                   static_cast<float>(q2) * static_cast<float>(x2.qs[lane]);
            sum += x3.d * static_cast<float>(scales[scale_index + 4]) *
                   static_cast<float>(q3) * static_cast<float>(x3.qs[lane]);
            sum += x4.d * static_cast<float>(scales[scale_index + 6]) *
                   static_cast<float>(q4) * static_cast<float>(x4.qs[lane]);
        }
        ql += LCQI_GGML_MATVEC_Q6_QUARTER_BLOCK_VALUES;
        qh += LCQI_GGML_MATVEC_Q6_SUBBLOCK_VALUES;
        scales += LCQI_GGML_MATVEC_Q6_SCALE_STRIDE;
    }
    return d * sum;
}

[[nodiscard]] float dot_quantized_block_f32(GgmlType type,
                                            const std::uint8_t* block,
                                            const float* input) {
    switch (type) {
        case GgmlType::q8_0:
            return dot_q8_0_block_f32(block, input);
        case GgmlType::q5_0:
            return dot_q5_0_block_f32(block, input);
        case GgmlType::q6_k:
            return dot_q6_k_block_f32(block, input);
        default:
            throw std::runtime_error(std::string("LCQI has no direct F32 matvec for ") +
                                     ggml_type_name(type));
    }
}

[[nodiscard]] float dot_quantized_block_q8_0(GgmlType type,
                                             const std::uint8_t* block,
                                             const Q8_0InputBlock* input) {
    switch (type) {
        case GgmlType::q8_0:
            return dot_q8_0_block_q8_0(block, input[0]);
        case GgmlType::q5_0:
            return dot_q5_0_block_q8_0(block, input[0]);
        case GgmlType::q6_k:
            return dot_q6_k_block_q8_0(block, input);
        default:
            throw std::runtime_error(std::string("LCQI has no direct Q8_0 matvec for ") +
                                     ggml_type_name(type));
    }
}

void validate_q8_0_blocks(std::span<const Q8_0InputBlock> input, std::int64_t column_count) {
    require_multiple(column_count, LCQI_Q8_0_INPUT_BLOCK_VALUES, "Q8_0 input column count");
    if (input.size() !=
        checked_size(column_count / LCQI_Q8_0_INPUT_BLOCK_VALUES, "Q8_0 input block count")) {
        throw std::runtime_error("Q8_0 input block count mismatch");
    }
}

}  // namespace

bool ggml_type_has_f32_direct_matvec(GgmlType type) noexcept {
    return type == GgmlType::q8_0 || type == GgmlType::q5_0 || type == GgmlType::q6_k;
}

bool ggml_type_has_q8_0_direct_matvec(GgmlType type) noexcept {
    return type == GgmlType::q8_0 || type == GgmlType::q5_0 || type == GgmlType::q6_k;
}

#if defined(LCQI_ENABLE_AVX2)
bool ggml_q8_0_avx2_available() noexcept;

void matvec_ggml_quantized_q8_0_avx2_unchecked(GgmlType type,
                                               std::span<const std::uint8_t> rows,
                                               std::int64_t row_count,
                                               std::int64_t column_count,
                                               std::span<const Q8_0InputBlock> input,
                                               std::span<float> output);

void matvec_ggml_quantized_q8_0_avx2_rows_unchecked(GgmlType type,
                                                    std::span<const std::uint8_t> rows,
                                                    std::int64_t row_count,
                                                    std::int64_t column_count,
                                                    std::span<const Q8_0InputBlock> input,
                                                    std::span<float> output,
                                                    std::int64_t row_begin,
                                                    std::int64_t row_end);
#else
bool ggml_q8_0_avx2_available() noexcept {
    return false;
}

void matvec_ggml_quantized_q8_0_avx2_unchecked(GgmlType,
                                               std::span<const std::uint8_t>,
                                               std::int64_t,
                                               std::int64_t,
                                               std::span<const Q8_0InputBlock>,
                                               std::span<float>) {}

void matvec_ggml_quantized_q8_0_avx2_rows_unchecked(GgmlType,
                                                    std::span<const std::uint8_t>,
                                                    std::int64_t,
                                                    std::int64_t,
                                                    std::span<const Q8_0InputBlock>,
                                                    std::span<float>,
                                                    std::int64_t,
                                                    std::int64_t) {}
#endif

std::vector<Q8_0InputBlock> quantize_q8_0_input(std::span<const float> input) {
    require_multiple(static_cast<std::int64_t>(input.size()),
                     LCQI_Q8_0_INPUT_BLOCK_VALUES,
                     "Q8_0 input size");
    std::vector<Q8_0InputBlock> output(
        checked_size(static_cast<std::int64_t>(input.size()) / LCQI_Q8_0_INPUT_BLOCK_VALUES,
                     "Q8_0 input block count"));
    quantize_q8_0_input_to(input, output);
    return output;
}

void quantize_q8_0_input_to(std::span<const float> input,
                            std::span<Q8_0InputBlock> output) {
    require_multiple(static_cast<std::int64_t>(input.size()),
                     LCQI_Q8_0_INPUT_BLOCK_VALUES,
                     "Q8_0 input size");
    if (output.size() != checked_size(static_cast<std::int64_t>(input.size()) /
                                          LCQI_Q8_0_INPUT_BLOCK_VALUES,
                                      "Q8_0 input block count")) {
        throw std::runtime_error("Q8_0 output block count mismatch");
    }
    for (std::size_t block_index = 0; block_index < output.size(); ++block_index) {
        Q8_0InputBlock& block = output[block_index];
        const float* values =
            input.data() + block_index * checked_size(LCQI_Q8_0_INPUT_BLOCK_VALUES,
                                                      "Q8_0 input block values");
        float max_value = 0.0F;
        float max_abs = 0.0F;
        for (std::int32_t index = 0; index < LCQI_Q8_0_INPUT_BLOCK_VALUES; ++index) {
            const float abs_value = std::fabs(values[index]);
            if (abs_value > max_abs) {
                max_abs = abs_value;
                max_value = values[index];
            }
        }
        if (max_abs == 0.0F) {
            block.d = 0.0F;
            block.qs.fill(0);
            block.sum = 0;
            continue;
        }
        const float inverse_scale =
            -static_cast<float>(LCQI_GGML_MATVEC_Q8_QUANT_MAX) / max_value;
        std::int32_t sum = 0;
        for (std::int32_t index = 0; index < LCQI_Q8_0_INPUT_BLOCK_VALUES; ++index) {
            const int quantized = static_cast<int>(std::nearbyint(inverse_scale * values[index]));
            const int clamped = std::clamp(quantized,
                                           -LCQI_GGML_MATVEC_Q8_QUANT_MAX,
                                           LCQI_GGML_MATVEC_Q8_QUANT_MAX);
            block.qs[index] = static_cast<std::int8_t>(clamped);
            sum += clamped;
        }
        block.sum = static_cast<std::int16_t>(sum);
        block.d = 1.0F / inverse_scale;
    }
}

void matvec_ggml_quantized_f32(GgmlType type,
                               std::span<const std::uint8_t> rows,
                               std::int64_t row_count,
                               std::int64_t column_count,
                               std::span<const float> input,
                               std::span<float> output) {
    require_positive(row_count, "GGML matvec row count");
    const GgmlTypeLayout layout = ggml_type_layout(type);
    if (!ggml_type_has_f32_direct_matvec(type)) {
        throw std::runtime_error(std::string("LCQI has no direct F32 matvec for ") +
                                 ggml_type_name(type));
    }
    require_multiple(column_count, layout.block_size, "GGML matvec column count");
    if (input.size() != checked_size(column_count, "GGML matvec column count")) {
        throw std::runtime_error("GGML matvec input size mismatch");
    }
    if (output.size() != checked_size(row_count, "GGML matvec row count")) {
        throw std::runtime_error("GGML matvec output size mismatch");
    }
    const std::int64_t row_bytes =
        (column_count / layout.block_size) * static_cast<std::int64_t>(layout.type_size);
    if (rows.size() != checked_size(row_bytes * row_count, "GGML matvec matrix bytes")) {
        throw std::runtime_error("GGML matvec matrix byte size mismatch");
    }

    for (std::int64_t row = 0; row < row_count; ++row) {
        const std::uint8_t* row_bytes_begin =
            rows.data() + checked_size(row * row_bytes, "GGML matvec row offset");
        float sum = 0.0F;
        for (std::int64_t block = 0; block < column_count / layout.block_size; ++block) {
            sum += dot_quantized_block_f32(
                type,
                row_bytes_begin +
                    checked_size(block * static_cast<std::int64_t>(layout.type_size),
                                 "GGML matvec block offset"),
                input.data() + checked_size(block * layout.block_size,
                                            "GGML matvec input block offset"));
        }
        output[checked_size(row, "GGML matvec row")] = sum;
    }
}

void matvec_ggml_quantized_q8_0(GgmlType type,
                                std::span<const std::uint8_t> rows,
                                std::int64_t row_count,
                                std::int64_t column_count,
                                std::span<const Q8_0InputBlock> input,
                                std::span<float> output) {
    require_positive(row_count, "GGML Q8_0 matvec row count");
    const GgmlTypeLayout layout = ggml_type_layout(type);
    if (!ggml_type_has_q8_0_direct_matvec(type)) {
        throw std::runtime_error(std::string("LCQI has no direct Q8_0 matvec for ") +
                                 ggml_type_name(type));
    }
    require_multiple(column_count, layout.block_size, "GGML Q8_0 matvec column count");
    validate_q8_0_blocks(input, column_count);
    if (output.size() != checked_size(row_count, "GGML Q8_0 matvec row count")) {
        throw std::runtime_error("GGML Q8_0 matvec output size mismatch");
    }
    const std::int64_t row_bytes =
        (column_count / layout.block_size) * static_cast<std::int64_t>(layout.type_size);
    if (rows.size() != checked_size(row_bytes * row_count, "GGML Q8_0 matvec matrix bytes")) {
        throw std::runtime_error("GGML Q8_0 matvec matrix byte size mismatch");
    }

    matvec_ggml_quantized_q8_0_rows_unchecked(type,
                                              rows,
                                              row_count,
                                              column_count,
                                              input,
                                              output,
                                              0,
                                              row_count);
}

void matvec_ggml_quantized_q8_0_rows_unchecked(GgmlType type,
                                               std::span<const std::uint8_t> rows,
                                               std::int64_t row_count,
                                               std::int64_t column_count,
                                               std::span<const Q8_0InputBlock> input,
                                               std::span<float> output,
                                               std::int64_t row_begin,
                                               std::int64_t row_end) {
    const GgmlTypeLayout layout = ggml_type_layout(type);
#if defined(LCQI_ENABLE_AVX2)
    if ((type == GgmlType::q8_0 || type == GgmlType::q5_0) && ggml_q8_0_avx2_available()) {
        matvec_ggml_quantized_q8_0_avx2_rows_unchecked(type,
                                                       rows,
                                                       row_count,
                                                       column_count,
                                                       input,
                                                       output,
                                                       row_begin,
                                                       row_end);
        return;
    }
#endif
    (void)row_count;
    const std::int64_t input_blocks_per_weight_block =
        layout.block_size / LCQI_Q8_0_INPUT_BLOCK_VALUES;
    const std::int64_t row_bytes =
        (column_count / layout.block_size) * static_cast<std::int64_t>(layout.type_size);

    for (std::int64_t row = row_begin; row < row_end; ++row) {
        const std::uint8_t* row_bytes_begin =
            rows.data() + static_cast<std::size_t>(row * row_bytes);
        float sum = 0.0F;
        for (std::int64_t block = 0; block < column_count / layout.block_size; ++block) {
            sum += dot_quantized_block_q8_0(
                type,
                row_bytes_begin +
                    static_cast<std::size_t>(block * static_cast<std::int64_t>(layout.type_size)),
                input.data() +
                    static_cast<std::size_t>(block * input_blocks_per_weight_block));
        }
        output[static_cast<std::size_t>(row)] = sum;
    }
}

}  // namespace lcqi
