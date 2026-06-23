#include <lcqi/int8_kernels.hpp>

#if defined(LCQI_ENABLE_NEON)

#include <arm_neon.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr std::int32_t LCQI_NEON_OUTPUT_BLOCK = 8;
constexpr std::int32_t LCQI_NEON_HALF_OUTPUT_BLOCK = 4;

void require_positive(std::int32_t value, const char* name) {
    if (value <= 0) {
        throw std::runtime_error(std::string(name) + " must be positive");
    }
}

std::size_t checked_size(std::int32_t value, const char* name) {
    if (value < 0) {
        throw std::runtime_error(std::string(name) + " cannot be negative");
    }
    return static_cast<std::size_t>(value);
}

std::int32_t round_up_to_block(std::int32_t value, std::int32_t block_size) {
    require_positive(value, "value");
    require_positive(block_size, "block_size");
    return ((value + block_size - 1) / block_size) * block_size;
}

void validate_neon_contract(const PackedLinearI8& layer,
                            std::span<const float> input,
                            std::span<float> output) {
    require_positive(layer.input_size, "packed input_size");
    require_positive(layer.output_size, "packed output_size");
    if (layer.output_block_size != LCQI_NEON_OUTPUT_BLOCK) {
        throw std::runtime_error("NEON packed kernel requires output_block_size == 8");
    }
    if (input.size() != checked_size(layer.input_size, "input_size")) {
        throw std::runtime_error("NEON packed kernel input size mismatch");
    }
    if (output.size() != checked_size(layer.output_size, "output_size")) {
        throw std::runtime_error("NEON packed kernel output size mismatch");
    }
    if (layer.bias.size() != checked_size(layer.output_size, "output_size")) {
        throw std::runtime_error("NEON packed kernel bias size mismatch");
    }
    const std::int32_t padded_outputs =
        round_up_to_block(layer.output_size, layer.output_block_size);
    const std::size_t expected_packed_size =
        checked_size(padded_outputs, "padded_outputs") *
        checked_size(layer.input_size, "input_size");
    if (layer.packed_weights.size() != expected_packed_size) {
        throw std::runtime_error("NEON packed kernel packed weight size mismatch");
    }
}

}  // namespace

bool linear_i8_packed_neon_available() noexcept {
    return true;
}

void linear_i8_packed_neon(const PackedLinearI8& layer,
                           std::span<const float> input,
                           std::span<float> output) {
    validate_neon_contract(layer, input, output);

    const std::int32_t padded_outputs =
        round_up_to_block(layer.output_size, layer.output_block_size);
    std::size_t packed_index = 0;
    alignas(16) float block_output[LCQI_NEON_OUTPUT_BLOCK] = {};

    for (std::int32_t output_block = 0; output_block < padded_outputs;
         output_block += LCQI_NEON_OUTPUT_BLOCK) {
        for (std::int32_t offset = 0; offset < LCQI_NEON_OUTPUT_BLOCK; ++offset) {
            const std::int32_t output_index = output_block + offset;
            block_output[checked_size(offset, "offset")] =
                output_index < layer.output_size
                    ? layer.bias[checked_size(output_index, "output_index")]
                    : 0.0F;
        }

        float32x4_t accumulator_low = vld1q_f32(block_output);
        float32x4_t accumulator_high =
            vld1q_f32(block_output + LCQI_NEON_HALF_OUTPUT_BLOCK);

        for (std::int32_t input_index = 0; input_index < layer.input_size; ++input_index) {
            const int8x8_t weights_i8 = vld1_s8(layer.packed_weights.data() + packed_index);
            packed_index += LCQI_NEON_OUTPUT_BLOCK;
            const int16x8_t weights_i16 = vmovl_s8(weights_i8);
            const int32x4_t weights_i32_low = vmovl_s16(vget_low_s16(weights_i16));
            const int32x4_t weights_i32_high = vmovl_s16(vget_high_s16(weights_i16));
            const float32x4_t weights_f32_low = vcvtq_f32_s32(weights_i32_low);
            const float32x4_t weights_f32_high = vcvtq_f32_s32(weights_i32_high);
            const float32x4_t input_scaled =
                vdupq_n_f32(input[checked_size(input_index, "input_index")] * layer.scale);
            accumulator_low = vmlaq_f32(accumulator_low, weights_f32_low, input_scaled);
            accumulator_high = vmlaq_f32(accumulator_high, weights_f32_high, input_scaled);
        }

        vst1q_f32(block_output, accumulator_low);
        vst1q_f32(block_output + LCQI_NEON_HALF_OUTPUT_BLOCK, accumulator_high);
        for (std::int32_t offset = 0; offset < LCQI_NEON_OUTPUT_BLOCK; ++offset) {
            const std::int32_t output_index = output_block + offset;
            if (output_index < layer.output_size) {
                output[checked_size(output_index, "output_index")] =
                    block_output[checked_size(offset, "offset")];
            }
        }
    }
}

}  // namespace lcqi

#endif
