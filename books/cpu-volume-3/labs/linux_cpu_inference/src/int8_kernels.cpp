#include <lcqi/int8_kernels.hpp>

#include <lcqi/inference.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>

namespace lcqi {
namespace {

constexpr std::int32_t LCQI_I8_PATTERN_MODULUS = 23;
constexpr std::int32_t LCQI_I8_PATTERN_CENTER = 11;
constexpr std::int32_t LCQI_INPUT_PATTERN_MODULUS = 17;
constexpr std::int32_t LCQI_INPUT_PATTERN_CENTER = 8;
constexpr float LCQI_INPUT_PATTERN_SCALE = 0.125F;
constexpr double LCQI_SECONDS_TO_MICROSECONDS = 1000000.0;
constexpr std::int32_t LCQI_SIMD_OUTPUT_BLOCK = 8;
constexpr std::size_t LCQI_BENCHMARK_BACKEND_COUNT = 4;
constexpr const char* LCQI_BACKEND_SCALAR = "scalar";
constexpr const char* LCQI_BACKEND_PACKED_SCALAR = "packed_scalar";
constexpr const char* LCQI_BACKEND_AVX2 = "avx2";
constexpr const char* LCQI_BACKEND_NEON = "neon";

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

void validate_quantized_layer(const QuantizedLinearLayer& layer) {
    require_positive(layer.input_size, "input_size");
    require_positive(layer.output_size, "output_size");
    const std::size_t expected_weights =
        checked_size(layer.input_size, "input_size") *
        checked_size(layer.output_size, "output_size");
    if (layer.weights.size() != expected_weights) {
        throw std::runtime_error("quantized layer weight size mismatch");
    }
    if (layer.bias.size() != checked_size(layer.output_size, "output_size")) {
        throw std::runtime_error("quantized layer bias size mismatch");
    }
}

std::int32_t round_up_to_block(std::int32_t value, std::int32_t block_size) {
    require_positive(value, "value");
    require_positive(block_size, "block_size");
    return ((value + block_size - 1) / block_size) * block_size;
}

float checksum(std::span<const float> values) {
    float sum = 0.0F;
    for (const float value : values) {
        sum += value;
    }
    return sum;
}

void add_unavailable_backend(KernelBenchmarkResult& result, const char* name) {
    result.backends.push_back(KernelBackendBenchmark{
        .name = name,
        .available = false,
        .average_us = 0.0,
        .max_abs_diff = 0.0F,
        .checksum = 0.0F,
    });
}

template <typename Callable>
double measure_average_us(std::int32_t repeat, Callable&& callable) {
    require_positive(repeat, "repeat");
    const auto begin = std::chrono::steady_clock::now();
    for (std::int32_t index = 0; index < repeat; ++index) {
        callable();
    }
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = end - begin;
    return elapsed.count() * LCQI_SECONDS_TO_MICROSECONDS / static_cast<double>(repeat);
}

}  // namespace

PackedLinearI8 pack_linear_i8(const QuantizedLinearLayer& layer,
                              std::int32_t output_block_size) {
    validate_quantized_layer(layer);
    require_positive(output_block_size, "output_block_size");

    PackedLinearI8 packed;
    packed.input_size = layer.input_size;
    packed.output_size = layer.output_size;
    packed.output_block_size = output_block_size;
    packed.scale = layer.scale;
    packed.bias = layer.bias;

    const std::int32_t padded_outputs =
        round_up_to_block(layer.output_size, output_block_size);
    packed.packed_weights.assign(
        checked_size(padded_outputs, "padded_outputs") *
            checked_size(layer.input_size, "input_size"),
        0);

    std::size_t packed_index = 0;
    for (std::int32_t output_block = 0; output_block < padded_outputs;
         output_block += output_block_size) {
        for (std::int32_t input_index = 0; input_index < layer.input_size; ++input_index) {
            for (std::int32_t offset = 0; offset < output_block_size; ++offset) {
                const std::int32_t output_index = output_block + offset;
                if (output_index < layer.output_size) {
                    const std::size_t source =
                        checked_size(output_index, "output_index") *
                            checked_size(layer.input_size, "input_size") +
                        checked_size(input_index, "input_index");
                    packed.packed_weights[packed_index] = layer.weights[source];
                }
                ++packed_index;
            }
        }
    }
    return packed;
}

void linear_i8_packed(const PackedLinearI8& layer,
                      std::span<const float> input,
                      std::span<float> output) {
    std::vector<float> accumulators(
        checked_size(layer.output_block_size, "output_block_size"),
        0.0F);
    linear_i8_packed_with_workspace(layer, input, output, accumulators);
}

void linear_i8_packed_with_workspace(const PackedLinearI8& layer,
                                     std::span<const float> input,
                                     std::span<float> output,
                                     std::span<float> accumulator_workspace) {
    require_positive(layer.input_size, "packed input_size");
    require_positive(layer.output_size, "packed output_size");
    require_positive(layer.output_block_size, "packed output_block_size");
    if (input.size() != checked_size(layer.input_size, "input_size")) {
        throw std::runtime_error("linear_i8_packed input size mismatch");
    }
    if (output.size() != checked_size(layer.output_size, "output_size")) {
        throw std::runtime_error("linear_i8_packed output size mismatch");
    }
    if (layer.bias.size() != checked_size(layer.output_size, "output_size")) {
        throw std::runtime_error("linear_i8_packed bias size mismatch");
    }

    const std::int32_t padded_outputs =
        round_up_to_block(layer.output_size, layer.output_block_size);
    const std::size_t expected_packed_size =
        checked_size(padded_outputs, "padded_outputs") *
        checked_size(layer.input_size, "input_size");
    if (layer.packed_weights.size() != expected_packed_size) {
        throw std::runtime_error("linear_i8_packed packed weight size mismatch");
    }
    if (accumulator_workspace.size() != checked_size(layer.output_block_size, "output_block_size")) {
        throw std::runtime_error("linear_i8_packed accumulator workspace size mismatch");
    }

    std::size_t packed_index = 0;
    for (std::int32_t output_block = 0; output_block < padded_outputs;
         output_block += layer.output_block_size) {
        std::fill(accumulator_workspace.begin(), accumulator_workspace.end(), 0.0F);
        for (std::int32_t offset = 0; offset < layer.output_block_size; ++offset) {
            const std::int32_t output_index = output_block + offset;
            if (output_index < layer.output_size) {
                accumulator_workspace[checked_size(offset, "offset")] =
                    layer.bias[checked_size(output_index, "output_index")];
            }
        }

        for (std::int32_t input_index = 0; input_index < layer.input_size; ++input_index) {
            const float input_value = input[checked_size(input_index, "input_index")];
            for (std::int32_t offset = 0; offset < layer.output_block_size; ++offset) {
                const std::int32_t output_index = output_block + offset;
                const std::int8_t weight = layer.packed_weights[packed_index++];
                if (output_index < layer.output_size) {
                    accumulator_workspace[checked_size(offset, "offset")] +=
                        static_cast<float>(weight) * layer.scale * input_value;
                }
            }
        }

        for (std::int32_t offset = 0; offset < layer.output_block_size; ++offset) {
            const std::int32_t output_index = output_block + offset;
            if (output_index < layer.output_size) {
                output[checked_size(output_index, "output_index")] =
                    accumulator_workspace[checked_size(offset, "offset")];
            }
        }
    }
}

QuantizedLinearLayer make_deterministic_i8_layer(std::int32_t input_size,
                                                 std::int32_t output_size,
                                                 float scale) {
    require_positive(input_size, "input_size");
    require_positive(output_size, "output_size");
    QuantizedLinearLayer layer;
    layer.input_size = input_size;
    layer.output_size = output_size;
    layer.scale = scale;
    const std::size_t weight_count =
        checked_size(input_size, "input_size") * checked_size(output_size, "output_size");
    layer.weights.resize(weight_count);
    layer.bias.resize(checked_size(output_size, "output_size"));
    for (std::int32_t output_index = 0; output_index < output_size; ++output_index) {
        layer.bias[checked_size(output_index, "output_index")] =
            static_cast<float>((output_index % LCQI_INPUT_PATTERN_MODULUS) -
                               LCQI_INPUT_PATTERN_CENTER) *
            LCQI_INPUT_PATTERN_SCALE;
        for (std::int32_t input_index = 0; input_index < input_size; ++input_index) {
            const std::int32_t raw =
                ((output_index * input_size + input_index) % LCQI_I8_PATTERN_MODULUS) -
                LCQI_I8_PATTERN_CENTER;
            layer.weights[checked_size(output_index, "output_index") *
                              checked_size(input_size, "input_size") +
                          checked_size(input_index, "input_index")] =
                static_cast<std::int8_t>(raw);
        }
    }
    return layer;
}

std::vector<float> make_deterministic_input(std::int32_t input_size) {
    require_positive(input_size, "input_size");
    std::vector<float> input(checked_size(input_size, "input_size"));
    for (std::int32_t input_index = 0; input_index < input_size; ++input_index) {
        input[checked_size(input_index, "input_index")] =
            static_cast<float>((input_index % LCQI_INPUT_PATTERN_MODULUS) -
                               LCQI_INPUT_PATTERN_CENTER) *
            LCQI_INPUT_PATTERN_SCALE;
    }
    return input;
}

float max_abs_diff(std::span<const float> lhs, std::span<const float> rhs) {
    if (lhs.size() != rhs.size()) {
        throw std::runtime_error("max_abs_diff size mismatch");
    }
    float diff = 0.0F;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        diff = std::max(diff, std::fabs(lhs[index] - rhs[index]));
    }
    return diff;
}

KernelBenchmarkResult benchmark_linear_i8_case(const KernelBenchmarkCase& benchmark_case) {
    require_positive(benchmark_case.input_size, "benchmark input_size");
    require_positive(benchmark_case.output_size, "benchmark output_size");
    require_positive(benchmark_case.output_block_size, "benchmark output_block_size");
    require_positive(benchmark_case.repeat, "benchmark repeat");

    const QuantizedLinearLayer layer =
        make_deterministic_i8_layer(benchmark_case.input_size,
                                    benchmark_case.output_size,
                                    0.015625F);
    const PackedLinearI8 packed = pack_linear_i8(layer, benchmark_case.output_block_size);
    const PackedLinearI8 packed_simd = pack_linear_i8(layer, LCQI_SIMD_OUTPUT_BLOCK);
    const std::vector<float> input = make_deterministic_input(benchmark_case.input_size);
    std::vector<float> scalar_output(checked_size(benchmark_case.output_size, "output_size"),
                                     0.0F);
    std::vector<float> packed_output(scalar_output.size(), 0.0F);
    std::vector<float> simd_output(scalar_output.size(), 0.0F);
    std::vector<float> packed_workspace(
        checked_size(benchmark_case.output_block_size, "output_block_size"),
        0.0F);

    linear_i8(layer, input, scalar_output);
    linear_i8_packed_with_workspace(packed, input, packed_output, packed_workspace);

    KernelBenchmarkResult result;
    result.benchmark_case = benchmark_case;
    result.backends.reserve(LCQI_BENCHMARK_BACKEND_COUNT);
    result.backends.push_back(KernelBackendBenchmark{
        .name = LCQI_BACKEND_SCALAR,
        .available = true,
        .average_us = measure_average_us(benchmark_case.repeat,
                                         [&]() { linear_i8(layer, input, scalar_output); }),
        .max_abs_diff = 0.0F,
        .checksum = checksum(scalar_output),
    });
    result.backends.push_back(KernelBackendBenchmark{
        .name = LCQI_BACKEND_PACKED_SCALAR,
        .available = true,
        .average_us = measure_average_us(
            benchmark_case.repeat,
            [&]() { linear_i8_packed_with_workspace(packed,
                                                    input,
                                                    packed_output,
                                                    packed_workspace); }),
        .max_abs_diff = max_abs_diff(scalar_output, packed_output),
        .checksum = checksum(packed_output),
    });
    if (linear_i8_packed_avx2_available()) {
        linear_i8_packed_avx2(packed_simd, input, simd_output);
        result.backends.push_back(KernelBackendBenchmark{
            .name = LCQI_BACKEND_AVX2,
            .available = true,
            .average_us = measure_average_us(
                benchmark_case.repeat,
                [&]() { linear_i8_packed_avx2(packed_simd, input, simd_output); }),
            .max_abs_diff = max_abs_diff(scalar_output, simd_output),
            .checksum = checksum(simd_output),
        });
    } else {
        add_unavailable_backend(result, LCQI_BACKEND_AVX2);
    }
    if (linear_i8_packed_neon_available()) {
        linear_i8_packed_neon(packed_simd, input, simd_output);
        result.backends.push_back(KernelBackendBenchmark{
            .name = LCQI_BACKEND_NEON,
            .available = true,
            .average_us = measure_average_us(
                benchmark_case.repeat,
                [&]() { linear_i8_packed_neon(packed_simd, input, simd_output); }),
            .max_abs_diff = max_abs_diff(scalar_output, simd_output),
            .checksum = checksum(simd_output),
        });
    } else {
        add_unavailable_backend(result, LCQI_BACKEND_NEON);
    }
    return result;
}

}  // namespace lcqi

#if !defined(LCQI_ENABLE_AVX2)
namespace lcqi {

bool linear_i8_packed_avx2_available() noexcept {
    return false;
}

void linear_i8_packed_avx2(const PackedLinearI8&, std::span<const float>, std::span<float>) {
    throw std::runtime_error("AVX2 packed kernel is not enabled in this build");
}

}  // namespace lcqi
#endif

#if !defined(LCQI_ENABLE_NEON)
namespace lcqi {

bool linear_i8_packed_neon_available() noexcept {
    return false;
}

void linear_i8_packed_neon(const PackedLinearI8&, std::span<const float>, std::span<float>) {
    throw std::runtime_error("NEON packed kernel is not enabled in this build");
}

}  // namespace lcqi
#endif
