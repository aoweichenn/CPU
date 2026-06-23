#include <lcqi/int8_kernels.hpp>

#if defined(LCQI_ENABLE_AVX2)

#include <immintrin.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr std::int32_t LCQI_AVX2_OUTPUT_BLOCK = 8;

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

void validate_avx2_contract(const PackedLinearI8& layer,
                            std::span<const float> input,
                            std::span<float> output) {
    require_positive(layer.input_size, "packed input_size");
    require_positive(layer.output_size, "packed output_size");
    if (layer.output_block_size != LCQI_AVX2_OUTPUT_BLOCK) {
        throw std::runtime_error("AVX2 packed kernel requires output_block_size == 8");
    }
    if (input.size() != checked_size(layer.input_size, "input_size")) {
        throw std::runtime_error("AVX2 packed kernel input size mismatch");
    }
    if (output.size() != checked_size(layer.output_size, "output_size")) {
        throw std::runtime_error("AVX2 packed kernel output size mismatch");
    }
    if (layer.bias.size() != checked_size(layer.output_size, "output_size")) {
        throw std::runtime_error("AVX2 packed kernel bias size mismatch");
    }
    const std::int32_t padded_outputs =
        round_up_to_block(layer.output_size, layer.output_block_size);
    const std::size_t expected_packed_size =
        checked_size(padded_outputs, "padded_outputs") *
        checked_size(layer.input_size, "input_size");
    if (layer.packed_weights.size() != expected_packed_size) {
        throw std::runtime_error("AVX2 packed kernel packed weight size mismatch");
    }
}

}  // namespace

bool linear_i8_packed_avx2_available() noexcept {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
    return true;
#endif
}

void linear_i8_packed_avx2(const PackedLinearI8& layer,
                           std::span<const float> input,
                           std::span<float> output) {
    if (!linear_i8_packed_avx2_available()) {
        throw std::runtime_error("AVX2/FMA is not available on this CPU");
    }
    validate_avx2_contract(layer, input, output);

    const std::int32_t padded_outputs =
        round_up_to_block(layer.output_size, layer.output_block_size);
    std::size_t packed_index = 0;
    alignas(32) float block_output[LCQI_AVX2_OUTPUT_BLOCK] = {};

    for (std::int32_t output_block = 0; output_block < padded_outputs;
         output_block += LCQI_AVX2_OUTPUT_BLOCK) {
        __m256 accumulator = _mm256_setzero_ps();

        for (std::int32_t offset = 0; offset < LCQI_AVX2_OUTPUT_BLOCK; ++offset) {
            const std::int32_t output_index = output_block + offset;
            block_output[checked_size(offset, "offset")] =
                output_index < layer.output_size
                    ? layer.bias[checked_size(output_index, "output_index")]
                    : 0.0F;
        }
        accumulator = _mm256_load_ps(block_output);

        for (std::int32_t input_index = 0; input_index < layer.input_size; ++input_index) {
            std::uint64_t packed_bytes = 0;
            std::memcpy(&packed_bytes,
                        layer.packed_weights.data() + packed_index,
                        sizeof(packed_bytes));
            const __m128i packed_i8 =
                _mm_cvtsi64_si128(static_cast<long long>(packed_bytes));
            packed_index += LCQI_AVX2_OUTPUT_BLOCK;
            const __m256i weights_i32 = _mm256_cvtepi8_epi32(packed_i8);
            const __m256 weights_f32 = _mm256_cvtepi32_ps(weights_i32);
            const __m256 input_scaled =
                _mm256_set1_ps(input[checked_size(input_index, "input_index")] * layer.scale);
            accumulator = _mm256_fmadd_ps(weights_f32, input_scaled, accumulator);
        }

        _mm256_store_ps(block_output, accumulator);
        for (std::int32_t offset = 0; offset < LCQI_AVX2_OUTPUT_BLOCK; ++offset) {
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
