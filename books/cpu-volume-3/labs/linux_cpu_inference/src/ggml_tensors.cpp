#include <lcqi/ggml_tensors.hpp>

#include <lcqi/q4_k.hpp>

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr std::uint32_t LCQI_GGML_F32_EXPONENT_MASK = 0x7F800000U;
constexpr std::uint32_t LCQI_GGML_F16_SIGN_MASK = 0x8000U;
constexpr std::uint32_t LCQI_GGML_F16_EXPONENT_MASK = 0x7C00U;
constexpr std::uint32_t LCQI_GGML_F16_MANTISSA_MASK = 0x03FFU;
constexpr std::uint32_t LCQI_GGML_F16_EXPONENT_BIAS = 15U;
constexpr std::uint32_t LCQI_GGML_F32_EXPONENT_BIAS = 127U;
constexpr std::uint32_t LCQI_GGML_F32_MANTISSA_BITS = 23U;
constexpr std::uint32_t LCQI_GGML_F16_MANTISSA_BITS = 10U;
constexpr std::uint32_t LCQI_GGML_F16_TO_F32_SIGN_SHIFT = 16U;
constexpr std::uint32_t LCQI_GGML_BYTE_BITS = 8U;
constexpr std::uint8_t LCQI_GGML_LOW_NIBBLE_MASK = 0x0FU;
constexpr std::uint8_t LCQI_GGML_Q5_HIGH_BIT_MASK = 0x10U;
constexpr std::uint32_t LCQI_GGML_Q5_HIGH_BIT_SHIFT = 4U;
constexpr std::int32_t LCQI_GGML_Q5_SECOND_HALF_BIT_OFFSET = 16;
constexpr std::uint8_t LCQI_GGML_Q6_HIGH_TWO_BITS_MASK = 0x03U;
constexpr std::int32_t LCQI_GGML_Q5_ZERO_POINT = 16;
constexpr std::int32_t LCQI_GGML_Q6_ZERO_POINT = 32;
constexpr std::int32_t LCQI_GGML_Q6_HALF_BLOCK_VALUES = 128;
constexpr std::int32_t LCQI_GGML_Q6_QUARTER_BLOCK_VALUES = 64;
constexpr std::int32_t LCQI_GGML_Q6_SUBBLOCK_VALUES = 32;
constexpr std::int32_t LCQI_GGML_Q6_SCALE_STRIDE = 8;
constexpr std::int32_t LCQI_GGML_F32_BYTES = 4;

[[nodiscard]] std::size_t checked_size(std::int64_t value, const char* name) {
    if (value < 0 ||
        static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(name) + " is outside size_t range");
    }
    return static_cast<std::size_t>(value);
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
        (static_cast<std::uint16_t>(bytes[1]) << LCQI_GGML_BYTE_BITS));
}

[[nodiscard]] std::uint32_t read_le_u32(const std::uint8_t* bytes) {
    std::uint32_t value = 0;
    for (std::uint32_t index = 0; index < LCQI_GGML_F32_BYTES; ++index) {
        value |= static_cast<std::uint32_t>(bytes[index]) <<
                 (LCQI_GGML_BYTE_BITS * index);
    }
    return value;
}

[[nodiscard]] float bits_to_float(std::uint32_t bits) {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void validate_quantized_bytes(std::span<const std::uint8_t> bytes,
                              std::int64_t element_count,
                              std::int64_t block_values,
                              std::int64_t block_bytes,
                              const char* name) {
    require_multiple(element_count, block_values, name);
    const std::int64_t expected_bytes = (element_count / block_values) * block_bytes;
    if (bytes.size() != checked_size(expected_bytes, name)) {
        throw std::runtime_error(std::string(name) + " byte count does not match element count");
    }
}

void dequantize_f32_to(std::span<const std::uint8_t> bytes, std::span<float> output) {
    const std::size_t expected_bytes = output.size() * LCQI_GGML_F32_BYTES;
    if (bytes.size() != expected_bytes) {
        throw std::runtime_error("F32 byte count does not match element count");
    }
    for (std::size_t index = 0; index < output.size(); ++index) {
        output[index] = bits_to_float(
            read_le_u32(bytes.data() + index * LCQI_GGML_F32_BYTES));
    }
}

void dequantize_q8_0_to(std::span<const std::uint8_t> bytes, std::span<float> output) {
    validate_quantized_bytes(bytes,
                             static_cast<std::int64_t>(output.size()),
                             LCQI_Q8_0_BLOCK_VALUES,
                             LCQI_Q8_0_BLOCK_BYTES,
                             "Q8_0");
    const std::int64_t block_count =
        static_cast<std::int64_t>(output.size()) / LCQI_Q8_0_BLOCK_VALUES;
    for (std::int64_t block_index = 0; block_index < block_count; ++block_index) {
        const std::uint8_t* block =
            bytes.data() + checked_size(block_index * LCQI_Q8_0_BLOCK_BYTES, "Q8_0 offset");
        const float d = ggml_f16_to_f32(read_le_u16(block));
        const std::uint8_t* qs = block + sizeof(std::uint16_t);
        float* target =
            output.data() + checked_size(block_index * LCQI_Q8_0_BLOCK_VALUES, "Q8_0 target");
        for (std::int32_t index = 0; index < LCQI_Q8_0_BLOCK_VALUES; ++index) {
            target[index] = static_cast<float>(static_cast<std::int8_t>(qs[index])) * d;
        }
    }
}

void dequantize_q5_0_to(std::span<const std::uint8_t> bytes, std::span<float> output) {
    validate_quantized_bytes(bytes,
                             static_cast<std::int64_t>(output.size()),
                             LCQI_Q5_0_BLOCK_VALUES,
                             LCQI_Q5_0_BLOCK_BYTES,
                             "Q5_0");
    const std::int64_t block_count =
        static_cast<std::int64_t>(output.size()) / LCQI_Q5_0_BLOCK_VALUES;
    for (std::int64_t block_index = 0; block_index < block_count; ++block_index) {
        const std::uint8_t* block =
            bytes.data() + checked_size(block_index * LCQI_Q5_0_BLOCK_BYTES, "Q5_0 offset");
        const float d = ggml_f16_to_f32(read_le_u16(block));
        const std::uint32_t qh = read_le_u32(block + sizeof(std::uint16_t));
        const std::uint8_t* qs = block + sizeof(std::uint16_t) + sizeof(std::uint32_t);
        float* target =
            output.data() + checked_size(block_index * LCQI_Q5_0_BLOCK_VALUES, "Q5_0 target");
        for (std::int32_t index = 0; index < LCQI_Q5_0_BLOCK_VALUES / 2; ++index) {
            const std::uint8_t high_0 =
                static_cast<std::uint8_t>(((qh >> index) & 1U)
                                          << LCQI_GGML_Q5_HIGH_BIT_SHIFT);
            const std::uint8_t high_1 =
                static_cast<std::uint8_t>(((qh >> (index + LCQI_GGML_Q5_SECOND_HALF_BIT_OFFSET)) &
                                           1U)
                                          << LCQI_GGML_Q5_HIGH_BIT_SHIFT);
            const std::int32_t low =
                static_cast<std::int32_t>((qs[index] & LCQI_GGML_LOW_NIBBLE_MASK) | high_0) -
                LCQI_GGML_Q5_ZERO_POINT;
            const std::int32_t high =
                static_cast<std::int32_t>((qs[index] >> 4U) | high_1) -
                LCQI_GGML_Q5_ZERO_POINT;
            target[index] = static_cast<float>(low) * d;
            target[index + LCQI_Q5_0_BLOCK_VALUES / 2] = static_cast<float>(high) * d;
        }
    }
}

void dequantize_q6_k_to(std::span<const std::uint8_t> bytes, std::span<float> output) {
    validate_quantized_bytes(bytes,
                             static_cast<std::int64_t>(output.size()),
                             LCQI_Q6_K_BLOCK_VALUES,
                             LCQI_Q6_K_BLOCK_BYTES,
                             "Q6_K");
    const std::int64_t block_count =
        static_cast<std::int64_t>(output.size()) / LCQI_Q6_K_BLOCK_VALUES;
    for (std::int64_t block_index = 0; block_index < block_count; ++block_index) {
        const std::uint8_t* block =
            bytes.data() + checked_size(block_index * LCQI_Q6_K_BLOCK_BYTES, "Q6_K offset");
        const std::uint8_t* ql = block;
        const std::uint8_t* qh = ql + LCQI_Q6_K_BLOCK_VALUES / 2;
        const auto* scales =
            reinterpret_cast<const std::int8_t*>(qh + LCQI_Q6_K_BLOCK_VALUES / 4);
        const float d = ggml_f16_to_f32(
            read_le_u16(block + LCQI_Q6_K_BLOCK_BYTES - sizeof(std::uint16_t)));
        float* y =
            output.data() + checked_size(block_index * LCQI_Q6_K_BLOCK_VALUES, "Q6_K target");

        for (std::int32_t half = 0; half < LCQI_Q6_K_BLOCK_VALUES;
             half += LCQI_GGML_Q6_HALF_BLOCK_VALUES) {
            for (std::int32_t lane = 0; lane < LCQI_GGML_Q6_SUBBLOCK_VALUES; ++lane) {
                const std::int32_t scale_index = lane / (LCQI_GGML_Q6_SUBBLOCK_VALUES / 2);
                const std::int32_t q1 =
                    static_cast<std::int32_t>((ql[lane] & LCQI_GGML_LOW_NIBBLE_MASK) |
                                              (((qh[lane] >> 0U) &
                                                LCQI_GGML_Q6_HIGH_TWO_BITS_MASK)
                                               << 4U)) -
                    LCQI_GGML_Q6_ZERO_POINT;
                const std::int32_t q2 =
                    static_cast<std::int32_t>((ql[lane + LCQI_GGML_Q6_SUBBLOCK_VALUES] &
                                               LCQI_GGML_LOW_NIBBLE_MASK) |
                                              (((qh[lane] >> 2U) &
                                                LCQI_GGML_Q6_HIGH_TWO_BITS_MASK)
                                               << 4U)) -
                    LCQI_GGML_Q6_ZERO_POINT;
                const std::int32_t q3 =
                    static_cast<std::int32_t>((ql[lane] >> 4U) |
                                              (((qh[lane] >> 4U) &
                                                LCQI_GGML_Q6_HIGH_TWO_BITS_MASK)
                                               << 4U)) -
                    LCQI_GGML_Q6_ZERO_POINT;
                const std::int32_t q4 =
                    static_cast<std::int32_t>((ql[lane + LCQI_GGML_Q6_SUBBLOCK_VALUES] >> 4U) |
                                              (((qh[lane] >> 6U) &
                                                LCQI_GGML_Q6_HIGH_TWO_BITS_MASK)
                                               << 4U)) -
                    LCQI_GGML_Q6_ZERO_POINT;

                y[lane] = d * static_cast<float>(scales[scale_index + 0]) *
                          static_cast<float>(q1);
                y[lane + LCQI_GGML_Q6_SUBBLOCK_VALUES] =
                    d * static_cast<float>(scales[scale_index + 2]) *
                    static_cast<float>(q2);
                y[lane + LCQI_GGML_Q6_QUARTER_BLOCK_VALUES] =
                    d * static_cast<float>(scales[scale_index + 4]) *
                    static_cast<float>(q3);
                y[lane + LCQI_GGML_Q6_QUARTER_BLOCK_VALUES +
                  LCQI_GGML_Q6_SUBBLOCK_VALUES] =
                    d * static_cast<float>(scales[scale_index + 6]) *
                    static_cast<float>(q4);
            }
            y += LCQI_GGML_Q6_HALF_BLOCK_VALUES;
            ql += LCQI_GGML_Q6_QUARTER_BLOCK_VALUES;
            qh += LCQI_GGML_Q6_SUBBLOCK_VALUES;
            scales += LCQI_GGML_Q6_SCALE_STRIDE;
        }
    }
}

}  // namespace

float ggml_f16_to_f32(std::uint16_t bits) {
    const std::uint32_t sign =
        (static_cast<std::uint32_t>(bits) & LCQI_GGML_F16_SIGN_MASK)
        << LCQI_GGML_F16_TO_F32_SIGN_SHIFT;
    const std::uint32_t exponent =
        static_cast<std::uint32_t>(bits) & LCQI_GGML_F16_EXPONENT_MASK;
    const std::uint32_t mantissa =
        static_cast<std::uint32_t>(bits) & LCQI_GGML_F16_MANTISSA_MASK;

    if (exponent == 0U) {
        if (mantissa == 0U) {
            return bits_to_float(sign);
        }
        float value = static_cast<float>(mantissa) /
                      static_cast<float>(1U << LCQI_GGML_F16_MANTISSA_BITS);
        value = std::ldexp(value, 1 - static_cast<int>(LCQI_GGML_F16_EXPONENT_BIAS));
        return sign == 0U ? value : -value;
    }

    if (exponent == LCQI_GGML_F16_EXPONENT_MASK) {
        const std::uint32_t f32_mantissa =
            mantissa << (LCQI_GGML_F32_MANTISSA_BITS - LCQI_GGML_F16_MANTISSA_BITS);
        return bits_to_float(sign | LCQI_GGML_F32_EXPONENT_MASK | f32_mantissa);
    }

    const std::uint32_t f32_exponent =
        ((exponent >> LCQI_GGML_F16_MANTISSA_BITS) - LCQI_GGML_F16_EXPONENT_BIAS +
         LCQI_GGML_F32_EXPONENT_BIAS)
        << LCQI_GGML_F32_MANTISSA_BITS;
    const std::uint32_t f32_mantissa =
        mantissa << (LCQI_GGML_F32_MANTISSA_BITS - LCQI_GGML_F16_MANTISSA_BITS);
    return bits_to_float(sign | f32_exponent | f32_mantissa);
}

std::vector<float> dequantize_ggml_tensor(GgmlType type,
                                          std::span<const std::uint8_t> bytes,
                                          std::int64_t element_count) {
    if (element_count < 0) {
        throw std::runtime_error("GGML element_count cannot be negative");
    }
    std::vector<float> output(checked_size(element_count, "GGML element_count"), 0.0F);
    dequantize_ggml_tensor_to(type, bytes, output);
    return output;
}

void dequantize_ggml_tensor_to(GgmlType type,
                               std::span<const std::uint8_t> bytes,
                               std::span<float> output) {
    switch (type) {
        case GgmlType::f32:
            dequantize_f32_to(bytes, output);
            return;
        case GgmlType::q8_0:
            dequantize_q8_0_to(bytes, output);
            return;
        case GgmlType::q5_0:
            dequantize_q5_0_to(bytes, output);
            return;
        case GgmlType::q6_k:
            dequantize_q6_k_to(bytes, output);
            return;
        case GgmlType::q4_k:
            dequantize_q4_k_to(bytes, output);
            return;
        default:
            throw std::runtime_error(std::string("LCQI cannot dequantize GGML type ") +
                                     ggml_type_name(type));
    }
}

}  // namespace lcqi
